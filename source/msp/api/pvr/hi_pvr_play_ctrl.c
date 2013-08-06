/******************************************************************************

  Copyright (C), 2001-2011, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : hi_pvr_play_ctrl.c
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2008/04/14
  Description   : PLAY module
  History       :
  1.Date        : 2008/04/14
    Author      : q46153
    Modification: Created file

******************************************************************************/

#include <malloc.h>
#include <sys/time.h>
#include <pthread.h>

#include "hi_type.h"

#include "hi_mpi_pvr.h"
#include "hi_mpi_avplay.h"

#include "pvr_debug.h"
#include "hi_pvr_play_ctrl.h"
#include "hi_pvr_index.h"
#include "hi_pvr_rec_ctrl.h"
#include "hi_pvr_intf.h"
#include "hi_pvr_priv.h"
#include "hi_mpi_demux.h"
#include "hi_drv_pvr.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#define TIMESHIFT_INVALID_CHN           0XFF
#define PVR_PLAY_MAX_SEND_BUF_SIZE      (256*1024)

#define PVR_DMX_TS_BUFFER_GAP           0x100   /*DMX_TS_BUFFER_GAP*/

extern HI_S32   g_s32PvrFd;      /*PVR module file description */
extern char api_pathname_pvr[];

/* initial flag for play module                                             */
STATIC PVR_PLAY_COMM_S g_stPlayInit;

/* all information of play channel                                          */
STATIC PVR_PLAY_CHN_S g_stPvrPlayChns[PVR_PLAY_MAX_CHN_NUM];

#ifdef PVR_PROC_SUPPORT
static PVR_PLAY_CHN_PROC_S *paPlayProcInfo[PVR_PLAY_MAX_CHN_NUM];  /* PROC info pointer of record channel*/
#endif

static FILE *g_pvrfpSend = NULL; /* handle of file */

extern HI_S32 HI_MPI_VO_GetWindowDelay(HI_HANDLE hWindow, HI_DRV_WIN_PLAY_INFO_S *pDelay);


#define PVR_GET_STATE_BY_SPEED(state, speed) \
do {\
    switch (speed)\
    {\
        case HI_UNF_PVR_PLAY_SPEED_NORMAL           :\
            state = HI_UNF_PVR_PLAY_STATE_PLAY;\
            break;\
        case HI_UNF_PVR_PLAY_SPEED_2X_FAST_FORWARD  :\
        case HI_UNF_PVR_PLAY_SPEED_4X_FAST_FORWARD  :\
        case HI_UNF_PVR_PLAY_SPEED_8X_FAST_FORWARD  :\
        case HI_UNF_PVR_PLAY_SPEED_16X_FAST_FORWARD :\
        case HI_UNF_PVR_PLAY_SPEED_32X_FAST_FORWARD :\
        case HI_UNF_PVR_PLAY_SPEED_64X_FAST_FORWARD :\
            state = HI_UNF_PVR_PLAY_STATE_FF;\
            break;\
        case HI_UNF_PVR_PLAY_SPEED_1X_FAST_BACKWARD :\
        case HI_UNF_PVR_PLAY_SPEED_2X_FAST_BACKWARD :\
        case HI_UNF_PVR_PLAY_SPEED_4X_FAST_BACKWARD :\
        case HI_UNF_PVR_PLAY_SPEED_8X_FAST_BACKWARD :\
        case HI_UNF_PVR_PLAY_SPEED_16X_FAST_BACKWARD:\
        case HI_UNF_PVR_PLAY_SPEED_32X_FAST_BACKWARD:\
        case HI_UNF_PVR_PLAY_SPEED_64X_FAST_BACKWARD:\
            state = HI_UNF_PVR_PLAY_STATE_FB;\
            break;\
        case HI_UNF_PVR_PLAY_SPEED_2X_SLOW_FORWARD  :\
        case HI_UNF_PVR_PLAY_SPEED_4X_SLOW_FORWARD  :\
        case HI_UNF_PVR_PLAY_SPEED_8X_SLOW_FORWARD  :\
        case HI_UNF_PVR_PLAY_SPEED_16X_SLOW_FORWARD :\
        case HI_UNF_PVR_PLAY_SPEED_32X_SLOW_FORWARD :\
            state = HI_UNF_PVR_PLAY_STATE_SF;\
            break;\
        case HI_UNF_PVR_PLAY_SPEED_2X_SLOW_BACKWARD :\
        case HI_UNF_PVR_PLAY_SPEED_4X_SLOW_BACKWARD :\
        case HI_UNF_PVR_PLAY_SPEED_8X_SLOW_BACKWARD :\
        case HI_UNF_PVR_PLAY_SPEED_16X_SLOW_BACKWARD:\
        case HI_UNF_PVR_PLAY_SPEED_32X_SLOW_BACKWARD:\
            state = HI_UNF_PVR_PLAY_STATE_INVALID;\
            break;\
        default:\
            state = HI_UNF_PVR_PLAY_STATE_INVALID;\
            break;\
    }\
}while(0)

/*play direction whether it is forward or not: current or before pause the state is play, fast forward, slow forward or step forward */
#define PVR_IS_PLAY_FORWARD(StateNow, StateLast)\
         (  (HI_UNF_PVR_PLAY_STATE_PLAY == StateNow) \
         || (HI_UNF_PVR_PLAY_STATE_FF == StateNow) \
         || (HI_UNF_PVR_PLAY_STATE_SF == StateNow) \
         || (HI_UNF_PVR_PLAY_STATE_STEPF == StateNow)\
         || ((HI_UNF_PVR_PLAY_STATE_PAUSE == StateNow) \
             && ( (HI_UNF_PVR_PLAY_STATE_PLAY == StateLast) \
               || (HI_UNF_PVR_PLAY_STATE_FF == StateLast) \
               || (HI_UNF_PVR_PLAY_STATE_SF == StateLast) \
               || (HI_UNF_PVR_PLAY_STATE_STEPF == StateLast) \
          )))

/*play direction whether it is backward or not: current or before puase the state is fast rewind or step rewind */
#define PVR_IS_PLAY_BACKWARD(StateNow, StateLast)\
         (  (HI_UNF_PVR_PLAY_STATE_FB == StateNow) \
         || (HI_UNF_PVR_PLAY_STATE_STEPB == StateNow)\
         || ((HI_UNF_PVR_PLAY_STATE_PAUSE == StateNow) \
             && ( (HI_UNF_PVR_PLAY_STATE_FB == StateLast) \
               || (HI_UNF_PVR_PLAY_STATE_STEPB == StateLast) \
          )))


STATIC INLINE HI_S32 PVRPlayDevInit(HI_VOID)
{
    int fd;

#ifdef PVR_PROC_SUPPORT
    HI_S32 ret;
    HI_U32 PhyAddr;
    HI_U32 u32BufSize;
    HI_U32 u32VirAddr;
    HI_U32 i;
#endif

    if (g_s32PvrFd == -1)
    {
        fd = open (api_pathname_pvr, O_RDWR , 0);

        if(fd <= 0)
        {
            HI_FATAL_PVR("Cannot open '%s'\n", api_pathname_pvr);
            return HI_FAILURE;
        }
        g_s32PvrFd = fd;

    }

#ifdef PVR_PROC_SUPPORT
    ret = ioctl(g_s32PvrFd, CMD_PVR_INIT_PLAY, (HI_S32)&PhyAddr);
    if (HI_SUCCESS != ret)
    {
        HI_FATAL_PVR("pvr play init error\n");
        return HI_FAILURE;
    }


    u32BufSize = sizeof(PVR_PLAY_CHN_PROC_S) * PVR_PLAY_MAX_CHN_NUM;
    u32BufSize = (u32BufSize%4096 == 0) ? u32BufSize : (u32BufSize / 4096 + 1) * 4096;

    u32VirAddr = (HI_U32)HI_MMAP(PhyAddr, u32BufSize);
    if (0 == u32VirAddr)
    {
        HI_FATAL_PVR("play proc buffer mmap error\n");
        return HI_FAILURE;
    }

    for (i = 0 ; i < PVR_PLAY_MAX_CHN_NUM; i++)
    {
        paPlayProcInfo[i] = (PVR_PLAY_CHN_PROC_S*)(u32VirAddr + sizeof(PVR_PLAY_CHN_PROC_S)*i);
    }
#endif

    return HI_SUCCESS;
}

STATIC INLINE HI_BOOL PVRPlayIsVoEmpty(PVR_PLAY_CHN_S  *pChnAttr)
{
    HI_S32 ret = HI_FAILURE;
    HI_HANDLE hWindow;
    HI_DRV_WIN_PLAY_INFO_S stWinDelay = {0};

    ret = HI_MPI_AVPLAY_GetWindowHandle(pChnAttr->hAvplay, &hWindow);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("HI_MPI_AVPLAY_GetWindowHandle fail:0x%x\n", ret);
        return HI_TRUE;

    }

    /* never set VO DieMode disable!
    ret = HI_MPI_VO_DisableDieMode(hWindow);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("HI_MPI_VO_DisableDieMode fail:0x%x\n", ret);
        return HI_TRUE;

    }
    */
    ret = HI_MPI_VO_GetWindowDelay(hWindow, &stWinDelay);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("HI_MPI_VO_GetWindowDelay fail:0x%x\n", ret);
        return HI_TRUE;
    }

    HI_INFO_PVR("WinDelay=%d\n", stWinDelay.u32DelayTime);

    if (stWinDelay.u32DelayTime <= 40) /* less-equal than 40 is OK, less-equal than 1 frame */
    {
        return HI_TRUE;
    }
    else
    {
        return HI_FALSE;
    }
}

STATIC INLINE HI_BOOL PVRPlayIsAoEmpty(PVR_PLAY_CHN_S  *pChnAttr)
{
    HI_S32 ret = HI_FAILURE;
    HI_HANDLE hSnd;
    HI_U32 SndDelay = 0;

    ret = HI_MPI_AVPLAY_GetSndHandle(pChnAttr->hAvplay, &hSnd);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("HI_MPI_AVPLAY_GetSndHandle fail:0x%x\n", ret);
        return HI_TRUE;

    }

    ret = HI_MPI_AO_Track_GetDelayMs(hSnd, &SndDelay);
//    ret = HI_MPI_HIAO_GetDelayMs(hSnd, &SndDelay);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("HI_MPI_HIAO_GetDelayMs fail:0x%x\n", ret);
        return HI_TRUE;
    }

    if (0 == SndDelay)
    {
        return HI_TRUE;
    }
    else
    {
        return HI_FALSE;
    }
}


STATIC INLINE HI_BOOL PVRPlayIsEOS(PVR_PLAY_CHN_S  *pChnAttr)
{
    HI_BOOL Eof = HI_TRUE;
    HI_S32 ret = HI_FAILURE;
    HI_UNF_AVPLAY_BUFID_E bufID;
    HI_UNF_AVPLAY_STATUS_INFO_S info;
    HI_UNF_DMX_TSBUF_STATUS_S  stTsBufStat;
    HI_U32 u32BufLowSize = 0;
    HI_U32 u32CurPts = PVR_INDEX_INVALID_PTSMS;

    /* the audio index should set audio esbuffer, video index set video esbuffer */
    if (PVR_INDEX_IS_TYPE_AUDIO(pChnAttr->IndexHandle))
    {
        bufID = HI_UNF_AVPLAY_BUF_ID_ES_AUD;
        u32BufLowSize = 1024;

        /* audio haven't completely end the play, until to play over */
        if (!PVRPlayIsAoEmpty(pChnAttr))
        {
            Eof = HI_FALSE;
            return Eof;
        }
    }
    else /* video */
    {
        bufID = HI_UNF_AVPLAY_BUF_ID_ES_VID;
        u32BufLowSize = 8*1024;

        /* video haven't completely end the play, until to play over */
        if (!PVRPlayIsVoEmpty(pChnAttr))
        {
            Eof = HI_FALSE;
            return Eof;
        }
    }

    ret = HI_UNF_DMX_GetTSBufferStatus(pChnAttr->hTsBuffer, &stTsBufStat);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("HI_UNF_DMX_GetTSBufferStatus fail:0x%x\n", ret);
        return HI_TRUE;
    }

    ret = HI_UNF_AVPLAY_GetStatusInfo(pChnAttr->hAvplay, &info);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("HI_UNF_AVPLAY_GetStatusInfo fail:0x%x\n", ret);
        return HI_TRUE;
    }

    if (PVR_INDEX_IS_TYPE_AUDIO(pChnAttr->IndexHandle))
    {
        u32CurPts = info.stSyncStatus.u32LastAudPts;
    }
    else /* video */
    {
        u32CurPts = info.stSyncStatus.u32LastVidPts;
    }

    /* pts invariable, whether size is invariable or not or es buffer size less than some byte */
    if ((stTsBufStat.u32UsedSize < (188 + PVR_DMX_TS_BUFFER_GAP))
        && (pChnAttr->u32LastPtsMs == u32CurPts)
        && ((info.stBufStatus[bufID].u32UsedSize == pChnAttr->u32LastEsBufSize)
          || (info.stBufStatus[bufID].u32UsedSize < u32BufLowSize)))
    {
        Eof = HI_TRUE;
        HI_INFO_PVR("BUF EMPTY NOW. ES in buf:%u,lst:%u, PTS:%d,lst:%d\n",
                info.stBufStatus[bufID].u32UsedSize,pChnAttr->u32LastEsBufSize, u32CurPts, pChnAttr->u32LastPtsMs);
    }
    else
    {
        Eof = HI_FALSE;

        HI_INFO_PVR("ES in buf:%u, PTS:%d lst:%d\n", info.stBufStatus[bufID].u32UsedSize, u32CurPts, pChnAttr->u32LastPtsMs);
        pChnAttr->u32LastEsBufSize = info.stBufStatus[bufID].u32UsedSize;
        pChnAttr->u32LastPtsMs = u32CurPts;
    }

    return Eof;
}

STATIC INLINE HI_S32 PVRPlayWaitForEndOfFile(PVR_PLAY_CHN_S  *pChnAttr,  HI_U32 timeOutMs)
{
    HI_BOOL Eof = HI_TRUE;
    HI_U32 u32time = 0;

    /* On step mode, just need wait next command, that is it. regardless of whether it is end or not in main thread*/
    if (HI_UNF_PVR_PLAY_STATE_STEPF == pChnAttr->enState)
    {
        usleep(200 * 1000);
        return HI_FALSE;
    }

    do {
        Eof = PVRPlayIsEOS(pChnAttr);
        if (Eof)
        {
            break;
        }
        else
        {
            /* look up interval 200ms */
            usleep(200 * 1000);
            u32time += 200;
            continue;
        }
    } while(u32time < timeOutMs);

    HI_INFO_PVR("Eof=%d\n", Eof);

    return Eof;
}


STATIC INLINE PVR_PLAY_CHN_S * PVRPlayFindFreeChn(HI_VOID)
{
    PVR_PLAY_CHN_S * pChnAttr = NULL;

    /* find a free play channel */
#if 0 /*not support multi-thread */
    HI_U32 i;
    for (i = 0; i < PVR_PLAY_MAX_CHN_NUM; i++)
    {
        if (g_stPvrPlayChns[i].enState == HI_UNF_PVR_PLAY_STATE_INVALID)
        {
            pChnAttr = &g_stPvrPlayChns[i];
            pChnAttr->enState = HI_UNF_PVR_PLAY_STATE_INIT;
            break;
        }
    }
#else /* manage the resources by kernel driver */
    HI_U32 ChanId;
    if (HI_SUCCESS != ioctl(g_s32PvrFd, CMD_PVR_CREATE_PLAY_CHN, (HI_S32)&ChanId))
    {
        HI_FATAL_PVR("pvr play creat channel error\n");
        return HI_NULL;
    }

    HI_ASSERT(g_stPvrPlayChns[ChanId].enState == HI_UNF_PVR_PLAY_STATE_INVALID);
    pChnAttr = &g_stPvrPlayChns[ChanId];
    pChnAttr->enState = HI_UNF_PVR_PLAY_STATE_INIT;
    pChnAttr->enLastState = HI_UNF_PVR_PLAY_STATE_INIT;

#endif

    return pChnAttr;
}


STATIC INLINE HI_S32 PVRPlayCheckUserCfg(const HI_UNF_PVR_PLAY_ATTR_S *pUserCfg, HI_HANDLE hAvplay, HI_HANDLE hTsBuffer)
{
    HI_S32 ret;
    HI_UNF_AVPLAY_ATTR_S         AVPlayAttr;
    HI_UNF_AVPLAY_STATUS_INFO_S  StatusInfo;
    HI_UNF_DMX_TSBUF_STATUS_S    TsBufStatus;
    HI_CHAR szIndexName[PVR_MAX_FILENAME_LEN + 4];
    HI_U32 i;

    if (HI_UNF_PVR_STREAM_TYPE_TS != pUserCfg->enStreamType )
    {
        HI_ERR_PVR("invalid play enStreamType:%d\n", pUserCfg->enStreamType );
        return HI_ERR_PVR_INVALID_PARA;
    }

    PVR_CHECK_CIPHER_CFG(&pUserCfg->stDecryptCfg);

    /*  if play file name ok */
    if (!((pUserCfg->u32FileNameLen > 0)
        && (strlen(pUserCfg->szFileName) == pUserCfg->u32FileNameLen)))
    {
        HI_ERR_PVR("Invalid file name!\n");
        return HI_ERR_PVR_FILE_INVALID_FNAME;
    }

    /* check if stream exist!                                               */
    if (!PVR_CHECK_FILE_EXIST64(pUserCfg->szFileName))
    {
        HI_ERR_PVR("Stream file %s doesn't exist!\n", pUserCfg->szFileName);
        return HI_ERR_PVR_FILE_NOT_EXIST;
    }
    sprintf(szIndexName, "%s.%s", pUserCfg->szFileName, "idx");
    if (!PVR_CHECK_FILE_EXIST(szIndexName))
    {
        HI_ERR_PVR("can NOT find index file for '%s'!\n", pUserCfg->szFileName);
        return HI_ERR_PVR_FILE_NOT_EXIST;
    }

    for (i = 0; i < PVR_PLAY_MAX_CHN_NUM; i++)
    {
        /* check whether demux id is used or not */
        if (HI_UNF_PVR_PLAY_STATE_INVALID != g_stPvrPlayChns[i].enState)
        {
            /* check whether the same file is playing or not*/
            if (0 == strcmp(g_stPvrPlayChns[i].stUserCfg.szFileName, pUserCfg->szFileName))
            {
                HI_ERR_PVR("file %s was exist to be playing.\n", pUserCfg->szFileName);
                return HI_ERR_PVR_FILE_EXIST;
            }

            if (g_stPvrPlayChns[i].hAvplay == hAvplay)
            {
                HI_ERR_PVR("avplay 0x%x already has been used to play.\n", hAvplay);
                return HI_ERR_PVR_ALREADY;
            }
            if (g_stPvrPlayChns[i].hTsBuffer == hTsBuffer)
            {
                HI_ERR_PVR("Ts buffer 0x%x already has been used to play.\n", hTsBuffer);
                return HI_ERR_PVR_ALREADY;
            }
        }

    }

    ret = HI_UNF_AVPLAY_GetStatusInfo(hAvplay, &StatusInfo);
    if (ret != HI_SUCCESS)
    {
        HI_ERR_PVR("check hAvplay for PVR failed:%#x\n", ret);
        return HI_FAILURE;
    }
    if (StatusInfo.enRunStatus != HI_UNF_AVPLAY_STATUS_STOP)
    {
        HI_U32 u32Stop = HI_UNF_AVPLAY_MEDIA_CHAN_VID;

        u32Stop |= HI_UNF_AVPLAY_MEDIA_CHAN_AUD;
        
        HI_WARN_PVR("the hAvplay is not stopped\n");

        ret = HI_UNF_AVPLAY_Stop(hAvplay, (HI_UNF_AVPLAY_MEDIA_CHAN_E)u32Stop, NULL);
        if (ret != HI_SUCCESS)
        {
            HI_ERR_PVR("can NOT stop hAvplay for pvr replay\n");
            return ret;
        }
    }

    ret = HI_UNF_AVPLAY_GetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_STREAM_MODE, &AVPlayAttr);
    if (ret != HI_SUCCESS)
    {
        HI_ERR_PVR("check hAvplay attr failed\n");
        return HI_FAILURE;
    }
    if (AVPlayAttr.stStreamAttr.enStreamType != HI_UNF_AVPLAY_STREAM_TYPE_TS)
    {
        HI_ERR_PVR("hAvplay's enStreamType is NOT TS.\n");
        return HI_ERR_PVR_INVALID_PARA;
    }

    ret = HI_UNF_DMX_GetTSBufferStatus(hTsBuffer, &TsBufStatus);
    if (ret != HI_SUCCESS)
    {
        HI_ERR_PVR("check hTsBuffer failed.\n");
        return HI_ERR_PVR_INVALID_PARA;
    }

    return HI_SUCCESS;
}

STATIC INLINE HI_S32 PVRPlayPrepareCipher(PVR_PLAY_CHN_S  *pChnAttr)
{
    HI_S32 ret;
    HI_UNF_PVR_CIPHER_S *pCipherCfg;
    HI_UNF_CIPHER_CTRL_S ctrl;
    HI_UNF_CIPHER_ATTS_S stCipherAttr;

    pCipherCfg = &(pChnAttr->stUserCfg.stDecryptCfg);
    if (!pCipherCfg->bDoCipher)
    {
        return HI_SUCCESS;
    }

    /* get cipher handle */
    stCipherAttr.enCipherType = HI_UNF_CIPHER_TYPE_NORMAL;
    ret = HI_UNF_CIPHER_CreateHandle(&pChnAttr->hCipher, &stCipherAttr);
    if (ret != HI_SUCCESS)
    {
        HI_ERR_PVR("HI_UNF_CIPHER_CreateHandle failed:%#x\n", ret);
        return ret;
    }

    ctrl.enAlg = pCipherCfg->enType;
    ctrl.bKeyByCA = HI_FALSE;
    memcpy(ctrl.u32Key, pCipherCfg->au8Key, sizeof(ctrl.u32Key));
    memset(ctrl.u32IV, 0, sizeof(ctrl.u32IV));

    if (HI_UNF_CIPHER_ALG_AES ==  pCipherCfg->enType )
    {
        ctrl.enBitWidth = PVR_CIPHER_AES_BIT_WIDTH;
        ctrl.enWorkMode = PVR_CIPHER_AES_WORK_MODD;
        ctrl.enKeyLen = PVR_CIPHER_AES_KEY_LENGTH;
    }
    else if (HI_UNF_CIPHER_ALG_DES ==  pCipherCfg->enType )
    {
        ctrl.enBitWidth = PVR_CIPHER_DES_BIT_WIDTH;
        ctrl.enWorkMode = PVR_CIPHER_DES_WORK_MODD;
        ctrl.enKeyLen = PVR_CIPHER_DES_KEY_LENGTH;
    }
    else
    {
        ctrl.enBitWidth = PVR_CIPHER_3DES_BIT_WIDTH;
        ctrl.enWorkMode = PVR_CIPHER_3DES_WORK_MODD;
        ctrl.enKeyLen = PVR_CIPHER_3DES_KEY_LENGTH;
    }

    ret = HI_UNF_CIPHER_ConfigHandle(pChnAttr->hCipher, &ctrl);
    if (ret != HI_SUCCESS)
    {
        HI_ERR_PVR("HI_UNF_CIPHER_ConfigHandle failed:%#x\n", ret);
        (HI_VOID)HI_UNF_CIPHER_DestroyHandle(pChnAttr->hCipher);
        return ret;
    }

    return HI_SUCCESS;
}

STATIC INLINE HI_S32 PVRPlayReleaseCipher(PVR_PLAY_CHN_S  *pChnAttr)
{
    HI_S32 ret = HI_SUCCESS;

    /* free cipher handle */
    if ( (pChnAttr->stUserCfg.stDecryptCfg.bDoCipher) && (pChnAttr->hCipher) )
    {
        ret = HI_UNF_CIPHER_DestroyHandle(pChnAttr->hCipher);
        if (ret != HI_SUCCESS)
        {
            HI_ERR_PVR("release Cipher handle failed! erro:%#x\n", ret);
        }
        pChnAttr->hCipher = 0;
    }

    return ret;
}

/*
table 2-3 -- ITU-T Rec. H.222.0 | ISO/IEC 13818 transport packet
        Syntax                    No. of bits        Mnemonic
transport_packet(){
    sync_byte                        8            bslbf
    transport_error_indicator        1            bslbf
    payload_unit_start_indicator     1            bslbf
    transport_priority               1            bslbf
    PID                              13            uimsbf
    transport_scrambling_control     2            bslbf
    *********************************************************
***    adaptation_field_control      2            bslbf ****** the 2bit we want
    *********************************************************
    continuity_counter               4            uimsbf
    if(adaptation_field_control=='10'  || adaptation_field_control=='11'){
        adaptation_field()
    }
    if(adaptation_field_control=='01' || adaptation_field_control=='11') {
        for (i=0;i<N;i++){
            data_byte                8            bslbf
        }
    }
}


"2.4.3.2 Transport Stream packet layer" of ISO 13818-1

adaptation_field_control
    -- This 2 bit field indicates whether this Transport Stream packet header
        is followed by an adaptation field and/or payload.

Table 2-6 -- Adaptation field control values
value    description
00    reserved for future use by ISO/IEC
01    no adaptation_field, payload only
10    adaptation_field only, no payload
11    adaptation_field followed by payload
*/
STATIC INLINE HI_VOID RVRPlaySetAdptFiled(HI_U8 *pTsHead, HI_U32 flag)
{
    HI_U8 byte4 = pTsHead[3];

    HI_ASSERT(flag <= 0x3);

    byte4 = byte4 & 0xcf;/* clear adp field bits */
    byte4 = byte4 | ((flag & 0x3) << 4); /* set adp field bits */

    pTsHead[3] = byte4;
}

STATIC INLINE HI_U8 RVRPlayGetAdptFiled(const HI_U8 *pTsHead)
{
    HI_U8 byte4 = pTsHead[3];

    byte4 = (byte4 >> 4) & 0x3;

    return byte4;
}

/*****************************************************************************
 Prototype       : RVRPlayDisOrderCnt,RVRPlayDisOrderCntEnd
 Description     : disorderly number, prevent from making the demux regard repeatly number  as lost packet
 Input           : pTsHead  **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/7/21
    Author       : fd
    Modification : Created function

*****************************************************************************/
STATIC INLINE HI_VOID RVRPlayDisOrderCnt(HI_U8 *pTsHead)
{
    HI_U8 oldCnt;
    HI_U8 byte4 = pTsHead[3];

    oldCnt = byte4 & 0xf;

    oldCnt += 5; /* 5 is one random number, make the calu discontinuous */

    byte4 = byte4 & 0xf0;
    byte4 = byte4 | (oldCnt & 0xf);

    pTsHead[3] = byte4;
}

STATIC INLINE HI_VOID RVRPlayDisOrderCntEnd(HI_U8 *pTsHead)
{
    HI_U8 oldCnt;
    HI_U8 byte4 = pTsHead[3];

    oldCnt = byte4 & 0xf;

    oldCnt += 3; /* 3 is one random number, make the calu discontinuous */

    byte4 = byte4 & 0xf0;
    byte4 = byte4 | (oldCnt & 0xf);

    pTsHead[3] = byte4;
}

/* modify adaptation_field_control field, and set that length, pading with 0xff */
STATIC INLINE HI_VOID PVRPlaySetTsHead(HI_U8 *pTsHead, HI_U32 dataStartPos)
{
    HI_U8 adapt_flag;
    if (PVR_TS_HEAD_SIZE == dataStartPos)
    {
        return;
    }

    if (0 != (pTsHead[1] & 0x40))
    {
        return;
    }

    if (dataStartPos >= PVR_TS_HEAD_SIZE + PVR_TS_MIN_PD_SIZE) /* modify the padding area length */
    {
        adapt_flag = RVRPlayGetAdptFiled(pTsHead);
        /*AI7D02961 the dataStartPos is not equal the size of ts head, it should be both */
        RVRPlaySetAdptFiled(pTsHead, PVR_TS_ADAPT_BOTH);

        pTsHead[PVR_TS_PD_SIZE_POS] = (HI_U8)(dataStartPos - (PVR_TS_HEAD_SIZE + 1));

        if (!PVR_TS_ADAPT_HAVE_ADAPT(adapt_flag))
        {
            pTsHead[PVR_TS_PD_FLAG_POS] = 0;
        }

        memset(pTsHead + PVR_TS_HEAD_SIZE + PVR_TS_MIN_PD_SIZE, 0xff,
                    dataStartPos - (PVR_TS_HEAD_SIZE + PVR_TS_MIN_PD_SIZE));
    }
    else /* only 1Byte Adapt_len */
    {
        RVRPlaySetAdptFiled(pTsHead, PVR_TS_ADAPT_BOTH);
        pTsHead[PVR_TS_PD_SIZE_POS] = 0;
    }

    RVRPlayDisOrderCnt(pTsHead);
    return;
}

/*
dataEnd: position of valid data end in last TS pkg(Byte)
*                               dataEnd
*                                 |
original:                         V
* -----------------------------------------------------------
*| TS head | pending | valid data |  invalid data     |
*------------------------------------------------------------

0 == PVR_TS_MOVE_TO_END:
* -----------------------------------------------------------
*| TS head | pending | valid data |  0xff 0xff  ...  |
*------------------------------------------------------------

1 == PVR_TS_MOVE_TO_END:
* -----------------------------------------------------------
*| TS head | pending |   0xff 0xff  ... | valid data |
*------------------------------------------------------------
*/
STATIC INLINE HI_VOID PVRPlayAddTsEnd(HI_U8 *pBufAddr, HI_U32 dataEnd,  HI_U32 endToAdd)
{
    HI_U8 *pLastTsHead;
    HI_U8  adapt_flag;
    HI_U32 dataInLastTs ;

   if (0 == endToAdd)
   {
        return;
   }
#if 0 == PVR_TS_MOVE_TO_END
    memset((HI_U8*)pBufAddr + dataEnd , 0xff, endToAdd);
#else

    pLastTsHead = pBufAddr + dataEnd + endToAdd - PVR_TS_LEN;

    if (0 != (pLastTsHead[1] & 0x40))
    {
        pLastTsHead[1] = 0x1f;
        pLastTsHead[2] = 0xff;
        return;
    }

    adapt_flag = RVRPlayGetAdptFiled(pLastTsHead);

    /* TODO: 2B or 1B adapt-head */
    RVRPlaySetAdptFiled(pLastTsHead,
            ((endToAdd + PVR_TS_HEAD_SIZE) == PVR_TS_LEN)?
            PVR_TS_ADAPT_ADAPT_ONLY : PVR_TS_ADAPT_BOTH);
    RVRPlayDisOrderCntEnd(pLastTsHead);
    /* AI7D04104 event if it should have adaptation, we need to check whether it length is zero or not */
    if (PVR_TS_ADAPT_HAVE_ADAPT(adapt_flag) && 0 != pLastTsHead[PVR_TS_PD_SIZE_POS]) /* existent the padding field */
    {
        if (endToAdd + PVR_TS_HEAD_SIZE + PVR_TS_MIN_PD_SIZE > PVR_TS_LEN)
        {
            endToAdd = PVR_TS_LEN - (PVR_TS_HEAD_SIZE + PVR_TS_MIN_PD_SIZE);
        }

        dataInLastTs = PVR_TS_LEN - (endToAdd + PVR_TS_HEAD_SIZE + PVR_TS_MIN_PD_SIZE);

        memmove(pBufAddr + dataEnd  + endToAdd - dataInLastTs,
            pBufAddr + dataEnd  - dataInLastTs,
            dataInLastTs);

        memset(pBufAddr + dataEnd  - dataInLastTs, 0xff, endToAdd);
        pBufAddr[dataEnd  - (dataInLastTs + PVR_TS_MIN_PD_SIZE)] += (HI_U8)(endToAdd);
    }
    else /* nonexistent padding field */
    {
        if (endToAdd + PVR_TS_HEAD_SIZE > PVR_TS_LEN)
        {
            endToAdd = PVR_TS_LEN - (PVR_TS_HEAD_SIZE);
        }

        dataInLastTs = PVR_TS_LEN - (endToAdd + PVR_TS_HEAD_SIZE);

        memmove(pLastTsHead + PVR_TS_HEAD_SIZE + endToAdd,
            pLastTsHead + PVR_TS_HEAD_SIZE,
            dataInLastTs);

        memset(pLastTsHead + PVR_TS_HEAD_SIZE, 0xff, endToAdd);
        pLastTsHead[PVR_TS_PD_SIZE_POS] = (HI_U8)(endToAdd - 1);
        pLastTsHead[PVR_TS_PD_FLAG_POS] = 0;
    }
#endif

    return ;
}
#if 0

/*****************************************************************************
 Prototype       : PVRPlayReadStream
 Description     : read ts from file
 Input           : pChnAttr  **channel attribute, pointed by user.
 Input           : offset ** read offset
 Input           : size   **read size
 Input           : pu8Addr**data buffer
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/22
    Author       : quyaxin 46153
    Modification : Created function

*****************************************************************************/

static HI_S32 PVRPlayReadStream(HI_U8 *pu8Addr, HI_U32 offset, HI_U32 size, PVR_PLAY_CHN_S *pChnAttr)
{
    ssize_t  n;
    do
    {
        if ((n = PVR_PREAD64(pu8Addr, (size), pChnAttr->s32DataFile, (HI_U64)offset)) == -1)
        {
            if (EINTR == errno)
            {
                continue;
            }
            else if (errno)
            {
                perror("read ts error: ");
                return HI_ERR_PVR_FILE_CANT_READ;
            }
            else
            {
                HI_ERR_PVR("read err1,  want:%u, off:%llu \n", (size), offset);
                return HI_ERR_PVR_FILE_TILL_END;
            }
        }
        if ((0 == n) && (0 != (size)))
        {
            HI_WARN_PVR("read 0,    want:%u, off:%llu \n", (size), offset);
            return HI_ERR_PVR_FILE_TILL_END;
        }
        offset += (size);
   }while(EINTR == errno);
   return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : PVRPlayFindCorretEndIndex
 Description     :make sure the index of the last frame.
 Input           : pChnAttr  **channel attribute, pointed by user.
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/22
    Author       : quyaxin 46153
    Modification : Created function

*****************************************************************************/
static HI_S32 PVRPlayFindCorretEndIndex(PVR_PLAY_CHN_S  *pChnAttr)
{
    HI_S32 ret;
    HI_U32 StartFrame;
    HI_U32 EndFrame;
    HI_U32 i;
    HI_U32 before_pos,size1,size2;
    PVR_INDEX_ENTRY_S pframe;
    HI_U8 scdbuf[4];

    StartFrame = pChnAttr->IndexHandle->stCycMgr.u32StartFrame;
    EndFrame = pChnAttr->IndexHandle->stCycMgr.u32EndFrame-1;
    for(i=EndFrame;i>=StartFrame;i--)
    {
        ret = PVR_Index_GetFrameByNum(pChnAttr->IndexHandle,&pframe,i);
        if(HI_SUCCESS!=ret)
        {
            HI_ERR_PVR("Get %d Frame failed!\n",i);
            return ret;
        }

        size1 = PVR_PLAY_PICTURE_HEADER_LEN;
        size2 = 0;

        if (PVR_INDEX_IS_REWIND(pChnAttr->IndexHandle))
        {
            before_pos = (HI_U32)(pframe.u64Offset % PVR_INDEX_MAX_FILE_SIZE(pChnAttr->IndexHandle));

            if (before_pos + size1 > (HI_U32)(PVR_INDEX_MAX_FILE_SIZE(pChnAttr->IndexHandle))) /* stride rewind */
            {
                size1 = (HI_U32)PVR_INDEX_MAX_FILE_SIZE(pChnAttr->IndexHandle) - before_pos;
                size2 = PVR_PLAY_PICTURE_HEADER_LEN - size1;
            }
        }
        else
        {
            before_pos = (HI_U32)pframe.u64Offset;
        }
        ret = PVRPlayReadStream((HI_U8 *)scdbuf, before_pos, size1, pChnAttr);
        if(HI_SUCCESS!=ret)
        {
            continue;
        }
        if(size2>0)
        {
             before_pos = 0;

ret = PVRPlayReadStream(&scdbuf[size1], before_pos, size2, pChnAttr);
             if(HI_SUCCESS!=ret)
             {
                continue;
             }
        }
        if (scdbuf[0]!=0x00||scdbuf[1]!=0x00||scdbuf[2]!=0x01)
        {
            HI_ERR_PVR("frame %d error!\n",i);
            continue;
        }
        else
        {
            pChnAttr->IndexHandle->stCycMgr.u32EndFrame = i+1;
            HI_INFO_PVR("endframe index %d\n",i);
            return HI_SUCCESS;
        }
    }
    return HI_FAILURE;
}
#endif
/*****************************************************************************
 Prototype       : PVRPlaySendData
 Description     : by TS packet align mode, send pointed size data to demux, and the data must be cotinuious and valid
 Input           : pChnAttr     **the attribute of channel
                   offSet       ** the data offset from start in ts
                   bytesToSend  ** the data size
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/23
    Author       : quyaxin 46153
    Modification : Created function

*****************************************************************************/
/*
PVR_CIPHER_PKG_LEN aligned, read file from here        PVR_CIPHER_PKG_LEN aligned, read file to here
|                                                                             |
|     188 aligned        offSet(picture header,StartCode)    188 aligned                                        |
|              |             |                                  |             |
V              V             V                                  V             V
-------------------------------------------------------------------------------
| xxx          | TS head |xx |       valid data    |    xxx     |0x47...      |
-------------------------------------------------------------------------------
|<-cipherHead->|<-headToAdd->|<---bytesToSend----->|<-endToAdd->|<-cipherEnd->|
|<---------------------------alignSize--------------------------------------->|
*/
#if 0
STATIC INLINE HI_S32 PVRPlaySendData(PVR_PLAY_CHN_S *pChnAttr, HI_U64 offSet, HI_U32 bytesToSend)
{
    HI_S32   ret;
    HI_U32   alignSize;
    HI_U32   u32BytesRead = 0;
    HI_U32   u32PhyAddr;
    HI_UNF_STREAM_BUF_S demuxBuf;

    HI_U32 headToAdd;  /* the length from beginning of data to ts header *//*CNcomment:������ʼ��TSͷ�ľ��� */
    HI_U32 endToAdd;   /* the length from end of data to ts end *//*CNcomment:���ݽ�β��TSβ�ľ��� */
    HI_U32 cipherHead; /* from the first ts header to the lastest cipher array*//*CNcomment:��ʼTSͷ����ǰ�����һ�����ܷ���(ͬʱҲ��O_DIRECT��ȡλ��)�ľ��� */
    HI_U32 cipherEnd;  /* from the end of data to the lastest cipher array *//*CNcomment:���ݽ�β��������һ�����ܷ���(ͬʱҲ��O_DIRECT��ȡλ��)�ľ��� */
    HI_U32 u32BytesRealSend;  /*real length be sent to buffer *//*CNcomment:�����͵�TS Buffer�еĳ���*/
    HI_U64 u64ReadOffset;     /*read offset address of stream file*//*CNcomment:�����ļ��Ķ�ƫ�Ƶ�ַ */

    if (0 == bytesToSend)
    {
        return HI_SUCCESS;
    }

    headToAdd = (HI_U32)(offSet % PVR_TS_LEN);
    endToAdd = PVR_TS_LEN - (HI_U32)((offSet + bytesToSend) % PVR_TS_LEN);

    if (pChnAttr->stUserCfg.stDecryptCfg.bDoCipher)
    {
        cipherHead = (HI_U32)((offSet - headToAdd)  % PVR_CIPHER_PKG_LEN);
        cipherEnd = PVR_CIPHER_PKG_LEN - (HI_U32)((offSet + bytesToSend + endToAdd) % PVR_CIPHER_PKG_LEN);
    }
    else
    {
        cipherHead = 0;
        cipherEnd = 0;
    }

    alignSize = bytesToSend + headToAdd + endToAdd + cipherHead + cipherEnd;

    //HI_INFO_PVR("%u = %u + %u + %u + %u + %u\n", alignSize ,bytesToSend , headToAdd, endToAdd , cipherHead , cipherEnd);

    ret = HI_MPI_DMX_GetTSBuffer(pChnAttr->hTsBuffer, alignSize, &demuxBuf, &u32PhyAddr, 0);
    while (HI_SUCCESS != ret)
    {
        if ((pChnAttr->bQuickUpdateStatus) || (pChnAttr->bPlayMainThreadStop))
        {
            return HI_SUCCESS;
        }

        if (HI_ERR_DMX_NOAVAILABLE_BUF != ret)
        {
            HI_ERR_PVR("HI_MPI_DMX_GetTSBuffer failed:%#x!\n", ret);
            return ret;
        }
        else
        {
            HI_INFO_PVR("HI_MPI_DMX_GetTSBuffer busy:%#x!\n", ret);
            PVR_UNLOCK(&(pChnAttr->stMutex));
            usleep(40000);

            PVR_LOCK(&(pChnAttr->stMutex));
            if ((pChnAttr->bQuickUpdateStatus) || (pChnAttr->bPlayMainThreadStop))
            {
                return HI_SUCCESS;
            }
            ret = HI_MPI_DMX_GetTSBuffer(pChnAttr->hTsBuffer, alignSize, &demuxBuf, &u32PhyAddr, 0);
        }
    }

    u64ReadOffset = ((offSet - headToAdd) - cipherHead);
    HI_INFO_PVR("cur read pos:%llu, offset:%llu, cipherHead:%#x, bytesToSend:%#x, headToAdd:%#x alignSize:%#x!\n",
                u64ReadOffset,
                offSet,
                cipherHead,
                bytesToSend,
                headToAdd,
                alignSize);

    while ((u32BytesRead + PVR_PLAY_MAX_SEND_BUF_SIZE) < alignSize)
    {
        PVR_PLAY_READ_FILE(pChnAttr->pu8DataBuf, u64ReadOffset, PVR_PLAY_MAX_SEND_BUF_SIZE, pChnAttr);
        memcpy(demuxBuf.pu8Data + u32BytesRead, pChnAttr->pu8DataBuf, PVR_PLAY_MAX_SEND_BUF_SIZE);
        u32BytesRead += PVR_PLAY_MAX_SEND_BUF_SIZE;
        //pChnAttr->u64CurReadPos += PVR_PLAY_MAX_SEND_BUF_SIZE;
    }
    PVR_PLAY_READ_FILE(pChnAttr->pu8DataBuf, u64ReadOffset, alignSize - u32BytesRead, pChnAttr);
    memcpy(demuxBuf.pu8Data + u32BytesRead, pChnAttr->pu8DataBuf, alignSize - u32BytesRead);
    //pChnAttr->u64CurReadPos += (alignSize - u32BytesRead);

    /*if decipher is necessary,do it *//*CNcomment:�����Ҫ��������н���*/
    if (pChnAttr->stUserCfg.stDecryptCfg.bDoCipher)
    {
        ret = HI_UNF_CIPHER_Decrypt(pChnAttr->hCipher, u32PhyAddr, u32PhyAddr, alignSize);
        if (ret != HI_SUCCESS)
        {
            HI_ERR_PVR("HI_UNF_CIPHER_Decrypt failed:%#x!\n", ret);
            return ret;
        }
    }

    /*if index file don't exist, checking is unnecessary *//*CNcomment:���������Ų���Ҫ���*/
    if (!pChnAttr->bPlayingTsNoIdx)
    {
        HI_ASSERT(demuxBuf.pu8Data[cipherHead] == 0x47);
    }

    if ((HI_UNF_PVR_PLAY_STATE_PLAY != pChnAttr->enState)
       &&  (HI_UNF_PVR_PLAY_STATE_SF != pChnAttr->enState))
    {
        PVRPlaySetTsHead(demuxBuf.pu8Data + cipherHead, headToAdd);
        PVRPlayAddTsEnd(demuxBuf.pu8Data, cipherHead + headToAdd + bytesToSend, endToAdd);
        u32BytesRealSend = headToAdd + bytesToSend + endToAdd;
    }
    else /* normally or slow play,send whole of stream,do not add stream any more,only cut off the divicese between TS package*//*CNcomment:�������ź����ţ����������������ٲ�������ֻҪ�������ĵط��п��Ϳ�����*/
    {
        u32BytesRealSend = headToAdd + bytesToSend - ((offSet + bytesToSend) % PVR_TS_LEN);
    }

    HI_ASSERT(u32BytesRealSend <= alignSize);

    ret = HI_MPI_DMX_PutTSBuffer(pChnAttr->hTsBuffer, u32BytesRealSend, cipherHead);
    if (ret != HI_SUCCESS)
    {
        HI_ERR_PVR("HI_MPI_DMX_PutTSBuffer failed:%#x!\n", ret);
        return ret;
    }
    else
    {
        if (g_pvrfpSend)
        {
            fwrite(demuxBuf.pu8Data + cipherHead, 1, u32BytesRealSend, g_pvrfpSend);
        }
    }

    return HI_SUCCESS;
}

#else

/* send new command, when break or execute error, return failure, otherwise, return success.
   u32BytesSend must be aligned by 512 byte */
STATIC INLINE HI_S32 PVRPlaySendToTsBuffer(PVR_PLAY_CHN_S *pChnAttr, HI_U64 u64ReadOffset,
                HI_U32 u32BytesSend, HI_BOOL IsHead, HI_BOOL IsEnd,
                HI_U32 cipherHead, HI_U32 cipherEnd,
                HI_U32 headToAdd,  HI_U32 endToAdd)
{
    HI_S32   ret;
    HI_U32   u32BytesRealSend = u32BytesSend;
    HI_U32   u32StartPos = 0;
    HI_U32   u32PhyAddr;
    HI_UNF_STREAM_BUF_S demuxBuf;
    HI_UNF_PVR_DATA_ATTR_S stDataAttr;
    PVR_INDEX_ENTRY_S stStartFrame;
    PVR_INDEX_ENTRY_S stEndFrame;
    HI_U32            u32EndFrm;
    HI_U64            u64LenAdp = 0;
    HI_U64            u64OffsetAdp = 0;

    /*find out ts buffer size u32ReadOnce*/
    ret = HI_MPI_DMX_GetTSBuffer(pChnAttr->hTsBuffer, u32BytesSend, &demuxBuf, &u32PhyAddr, 0);
    while (HI_SUCCESS != ret)
    {
        if ((pChnAttr->bQuickUpdateStatus) || (pChnAttr->bPlayMainThreadStop))
        {
            return HI_FAILURE;
        }

        if (HI_ERR_DMX_NOAVAILABLE_BUF != ret)
        {
            HI_ERR_PVR("HI_MPI_DMX_GetTSBuffer failed:%#x!\n", ret);
            return HI_FAILURE;
        }
        else
        {
//            HI_INFO_PVR("HI_MPI_DMX_GetTSBuffer busy:%#x!\n", ret);
            PVR_UNLOCK(&(pChnAttr->stMutex));
            usleep(40000);

            PVR_LOCK(&(pChnAttr->stMutex));
            if ((pChnAttr->bQuickUpdateStatus) || (pChnAttr->bPlayMainThreadStop))
            {
                return HI_FAILURE;
            }
            ret = HI_MPI_DMX_GetTSBuffer(pChnAttr->hTsBuffer, u32BytesSend, &demuxBuf, &u32PhyAddr, 0);
        }
    }

    /* read ts to buffer */
    PVR_PLAY_READ_FILE(demuxBuf.pu8Data, u64ReadOffset, u32BytesSend, pChnAttr);

    if (NULL != pChnAttr->readCallBack)
    {
        ret = PVR_Index_GetFrameByNum(pChnAttr->IndexHandle,&stStartFrame,pChnAttr->IndexHandle->stCycMgr.u32StartFrame);
        if(HI_SUCCESS!=ret)
        {
            HI_ERR_PVR("Get Start Frame failed,ret=%d\n",ret);
            stStartFrame.u64Offset = 0;
        }

        if (pChnAttr->IndexHandle->stCycMgr.u32EndFrame > 0)
        {
            u32EndFrm = pChnAttr->IndexHandle->stCycMgr.u32EndFrame - 1;
        }
        else
        {
            u32EndFrm = pChnAttr->IndexHandle->stCycMgr.u32LastFrame - 1;
        }

        ret = PVR_Index_GetFrameByNum(pChnAttr->IndexHandle,&stEndFrame,u32EndFrm);
        if(HI_SUCCESS!=ret)
        {
            HI_ERR_PVR("Get End Frame failed,ret=%d\n",ret);
            stEndFrame.u64Offset = 0;
        }

        stDataAttr.u32ChnID        = pChnAttr->u32chnID;
        stDataAttr.u64FileStartPos = stStartFrame.u64Offset;
        stDataAttr.u64FileEndPos   = stEndFrame.u64Offset;
        stDataAttr.u64GlobalOffset = u64ReadOffset;

        memset(stDataAttr.CurFileName, 0, sizeof(stDataAttr.CurFileName));
        PVRFileGetOffsetFName(pChnAttr->s32DataFile, u64ReadOffset, stDataAttr.CurFileName);
        PVR_Index_GetIdxFileName(stDataAttr.IdxFileName, pChnAttr->stUserCfg.szFileName);

        u64OffsetAdp = u64ReadOffset;
        u64LenAdp = (HI_U32)u32BytesSend;
        PVRFileGetRealOffset(pChnAttr->s32DataFile, &u64OffsetAdp, &u64LenAdp);
        if(u64LenAdp < u32BytesSend)
        {
            ret = pChnAttr->readCallBack(&stDataAttr, demuxBuf.pu8Data, u32PhyAddr, (HI_U32)u64OffsetAdp, (HI_U32)u64LenAdp);
            if(HI_SUCCESS!=ret)
            {
                HI_ERR_PVR("readCallBack failed,ret=%d\n",ret);
                return HI_FAILURE;
            }
            u64OffsetAdp = u64ReadOffset + u64LenAdp;
            PVRFileGetRealOffset(pChnAttr->s32DataFile, &u64OffsetAdp, NULL);
            PVRFileGetOffsetFName(pChnAttr->s32DataFile, u64ReadOffset + u32BytesSend, stDataAttr.CurFileName);
            demuxBuf.pu8Data += u64LenAdp;

            ret = pChnAttr->readCallBack(&stDataAttr, demuxBuf.pu8Data, u32PhyAddr, (HI_U32)u64OffsetAdp, u32BytesSend - (HI_U32)u64LenAdp);
            if(HI_SUCCESS!=ret)
            {
                HI_ERR_PVR("readCallBack failed,ret=%d\n",ret);
                return HI_FAILURE;
            }
        }
        else
        {
            ret = pChnAttr->readCallBack(&stDataAttr, demuxBuf.pu8Data, u32PhyAddr, (HI_U32)u64OffsetAdp, (HI_U32)u64LenAdp);
            if(HI_SUCCESS!=ret)
            {
                HI_ERR_PVR("readCallBack failed,ret=%d\n",ret);
                return HI_FAILURE;
            }
        }
    }

    /*if need to decrypt, decrypt it */
    if (pChnAttr->stUserCfg.stDecryptCfg.bDoCipher)
    {
        ret = HI_UNF_CIPHER_Decrypt(pChnAttr->hCipher, u32PhyAddr, u32PhyAddr, u32BytesSend);
        if (ret != HI_SUCCESS)
        {
            HI_ERR_PVR("HI_UNF_CIPHER_Decrypt failed:%#x!\n", ret);
            return HI_FAILURE;
        }
    }

    /* none header or end, send u32BytesSend size data to ts buffer */
    u32BytesRealSend = u32BytesSend;
    u32StartPos = 0;
    if ((HI_UNF_PVR_PLAY_STATE_PLAY != pChnAttr->enState)
       &&  (HI_UNF_PVR_PLAY_STATE_SF != pChnAttr->enState)
       &&  (HI_UNF_PVR_PLAY_STATE_STEPF != pChnAttr->enState))
    {
        HI_INFO_PVR("===FF/FB:head=%d, end=%d\n", IsHead, IsEnd);
        if (IsHead)
        {
            PVRPlaySetTsHead(demuxBuf.pu8Data + cipherHead, headToAdd);
            u32BytesRealSend -= cipherHead;
            u32StartPos = cipherHead;
        }
        if(IsEnd)
        {
            PVRPlayAddTsEnd(demuxBuf.pu8Data, u32BytesSend - cipherEnd - endToAdd, endToAdd);
            u32BytesRealSend -= cipherEnd;
        }
    }
    else /* both normal play and slow play, send all stream,  no longer patch stream, just depart it at PVR_TS_LEN size times */
    {
        //HI_INFO_PVR("===PLAY:head=%d, end=%d\n", IsHead, IsEnd);
        if (IsHead)
        {
            u32BytesRealSend -= cipherHead;
            u32StartPos = cipherHead;
        }
        if (IsEnd && (endToAdd != 0))
        {
            u32BytesRealSend -= (cipherEnd + PVR_TS_LEN);
        }
    }
    //HI_INFO_PVR("====cipherHead:0x%x cipherEnd:0x%x, headToAdd:0x%x, endToAdd:0x%x \n", cipherHead, cipherEnd, headToAdd, endToAdd);
    //HI_INFO_PVR("====u32StartPos:0x%x u32BytesRealSend:0x%x, u64ReadOffset:0x%llx, u32BytesSend:0x%x \n", u32StartPos, u32BytesRealSend, u64ReadOffset, u32BytesSend);

    ret = HI_MPI_DMX_PutTSBuffer(pChnAttr->hTsBuffer, u32BytesRealSend, u32StartPos);
    if (ret != HI_SUCCESS)
    {
        HI_ERR_PVR("HI_MPI_DMX_PutTSBuffer failed:%#x!\n", ret);
        return HI_FAILURE;
    }
    else
    {
        if (g_pvrfpSend)
        {
            fwrite(demuxBuf.pu8Data + u32StartPos, 1, u32BytesRealSend, g_pvrfpSend);
        }
    }

    return HI_SUCCESS;
}



STATIC INLINE HI_S32 PVRPlaySendData(PVR_PLAY_CHN_S *pChnAttr, HI_U64 offSet, HI_U32 bytesToSend)
{
    HI_S32   ret;
    HI_U32   alignSize;
    HI_S32   s32ReadTimes = 0;
    HI_U32   i;
    HI_U32 headToAdd;  /* distance from data start to TS header */
    HI_U32 endToAdd;   /* distance from data end to TS end */
    HI_U32 cipherHead; /* distance from start ts header forward to the closest cipher group(meanwhile, imply read position of O_DIRECT, too) */
    HI_U32 cipherEnd;  /* distance data end backward to the closest cipher group(meanwhile, imply read position of O_DIRECT, too) */
    HI_U64 u64ReadOffset;     /* read offset address of ts file*/

    if (0 == bytesToSend)
    {
        return HI_SUCCESS;
    }

    headToAdd = (HI_U32)(offSet % PVR_TS_LEN);
    endToAdd = PVR_TS_LEN - (HI_U32)((offSet + bytesToSend) % PVR_TS_LEN);

    cipherHead = (HI_U32)((offSet - headToAdd)  % PVR_CIPHER_PKG_LEN);
    cipherEnd = PVR_CIPHER_PKG_LEN - (HI_U32)((offSet + bytesToSend + endToAdd) % PVR_CIPHER_PKG_LEN);

    alignSize = bytesToSend + headToAdd + endToAdd + cipherHead + cipherEnd;

    u64ReadOffset = ((offSet - headToAdd) - cipherHead);

    /*
    HI_INFO_PVR("cur read pos:%llu, offset:%llu, cipherHead:%#x, bytesToSend:%#x, headToAdd:%#x alignSize:%#x!\n",
                u64ReadOffset,
                offSet,
                cipherHead,
                bytesToSend,
                headToAdd,
                alignSize); */

    //HI_INFO_PVR("%u = %u + %u + %u + %u + %u\n", alignSize ,bytesToSend , headToAdd, endToAdd , cipherHead , cipherEnd);

    /* per time send 256k byte, at last, send the not enough 256k byte along with the previous 256k byte */
    s32ReadTimes = (HI_S32)alignSize/PVR_PLAY_MAX_SEND_BUF_SIZE + 1;

    /* less than 512k byte, which need to deal with once */
    if (s32ReadTimes < 3)
    {
        ret = PVRPlaySendToTsBuffer(pChnAttr, u64ReadOffset, alignSize, HI_TRUE, HI_TRUE,
                    cipherHead, cipherEnd, headToAdd, endToAdd);

        return ret;
    }

    /* more than 512k byte, at first, send the first head */
    ret =  PVRPlaySendToTsBuffer(pChnAttr, u64ReadOffset, PVR_PLAY_MAX_SEND_BUF_SIZE,
                    HI_TRUE, HI_FALSE, cipherHead, cipherEnd, headToAdd, endToAdd);
    if (HI_SUCCESS != ret)
    {
        return ret;
    }

    /*more than 512k byte, then send the middle block data*/
    for (i = 1; i < (HI_U32)s32ReadTimes-2; i++)
    {

        ret =  PVRPlaySendToTsBuffer(pChnAttr, u64ReadOffset + (HI_U64)i*PVR_PLAY_MAX_SEND_BUF_SIZE,
                    PVR_PLAY_MAX_SEND_BUF_SIZE, HI_FALSE, HI_FALSE,
                    cipherHead, cipherEnd, headToAdd, endToAdd);
        if (HI_SUCCESS != ret)
        {
            return ret;
        }

    }

    /*more  than 512k byte, send the last two block data, the last data less than or equal 256k(end) */
    ret =  PVRPlaySendToTsBuffer(pChnAttr, u64ReadOffset+(HI_U64)i*PVR_PLAY_MAX_SEND_BUF_SIZE,
                     alignSize - i*PVR_PLAY_MAX_SEND_BUF_SIZE, HI_FALSE, HI_TRUE,
                     cipherHead, cipherEnd, headToAdd, endToAdd);
    if (HI_SUCCESS != ret)
    {
        return ret;
    }

    return HI_SUCCESS;
}

#endif

/*****************************************************************************
 Prototype       : PVRPlaySendAframe
 Description     : send one frame data to demux
 Input           : pChnAttr     **the attribute of play channel
                   pframe       ** frame index info
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/22
    Author       : quyaxin 46153
    Modification : Created function

*****************************************************************************/
STATIC INLINE HI_S32 PVRPlaySendAframe(PVR_PLAY_CHN_S  *pChnAttr, const PVR_INDEX_ENTRY_S *pframe)
{
    HI_S32 ret = HI_SUCCESS;

    HI_U64 before_pos;
    HI_U32 size1;
    HI_U32 size2= 0;

    HI_ASSERT_RET(NULL != pframe);
    HI_ASSERT_RET(NULL != pChnAttr);

    if (0 == pframe->u32FrameSize) /* the abnormal, index file content is zero, AI7D02622 */
    {
        return HI_SUCCESS;
    }
    size1 = pframe->u32FrameSize;
    HI_INFO_PVR("Frame to send: Offset/Size/PTS(ms): %llu, %u, %u.\n",
                    pframe->u64Offset, pframe->u32FrameSize, pframe->u32PtsMs);

    if (pframe->u32FrameSize > PVR_PLAY_MAX_FRAME_SIZE)
    {
        HI_WARN_PVR("Frame size too large, drop it(Size:%u, offset=%llu).\n",
                    pframe->u32FrameSize, pframe->u64Offset);
        return HI_SUCCESS;
    }

#if 0  /* not need it, meanless to check it */
    /* if playing,repeat sending the beginning of stream one time.If rewind back, do not play*/
    /*CNcomment:���ڲ���״̬����������ʼ�Ĳ���(��һ֮֡ǰ)Ҳ��һ�£�ֻ��һ�Σ����˻����ǲ����ٲ��ŵ� */
    if ((HI_UNF_PVR_PLAY_STATE_PLAY == pChnAttr->enState)
       // && (!PVR_INDEX_IS_REWIND(pChnAttr->IndexHandle))
        && (HI_FALSE == pChnAttr->bCAStreamHeadSent)
        && (0 != pframe->u64Offset))
    {
        HI_INFO_PVR("SEND:0->%llu.\n", pframe->u64Offset);

        /*
        ret |= PVRPlaySendData(pChnAttr, 0, (HI_U32)pframe->u64Offset);
        pChnAttr->bCAStreamHeadSent = HI_TRUE;

    }
#endif

    before_pos = pframe->u64Offset;

    if(pChnAttr->IndexHandle->stCycMgr.u64MaxCycSize > 0)
    {
        if (PVR_INDEX_IS_REWIND(pChnAttr->IndexHandle))
        {
            before_pos = pframe->u64Offset % PVR_INDEX_MAX_FILE_SIZE(pChnAttr->IndexHandle);

            if (before_pos + pframe->u32FrameSize > PVR_INDEX_MAX_FILE_SIZE(pChnAttr->IndexHandle)) /* stride rewind */
            {
                size1 = PVR_INDEX_MAX_FILE_SIZE(pChnAttr->IndexHandle) - before_pos;
                size2 = pframe->u32FrameSize - size1;
            }
        }
    }

    /* send frame data. if rewind, depart two and send it */
    ret = PVRPlaySendData(pChnAttr, before_pos, (HI_U32)size1);
    if ((size2) && (HI_SUCCESS == ret))
    {
        PVRPlaySendData(pChnAttr, (HI_U64)0, size2);
    }

    return HI_SUCCESS;
}

STATIC HI_S32 PVRPlaySendEmptyPacketToTsBuffer(PVR_PLAY_CHN_S *pChnAttr)
{
    HI_S32 ret;
    HI_U32 u32Bytes2Send = 188 * 10;
    HI_U32 u32PhyAddr;
    HI_UNF_STREAM_BUF_S DataBuf;
    HI_U32 i;

    ret = HI_MPI_DMX_GetTSBuffer(pChnAttr->hTsBuffer, u32Bytes2Send, &DataBuf, &u32PhyAddr, 0);
    while (HI_SUCCESS != ret)
    {
        if ((pChnAttr->bQuickUpdateStatus) || (pChnAttr->bPlayMainThreadStop))
        {
            return HI_FAILURE;
        }

        if (HI_ERR_DMX_NOAVAILABLE_BUF != ret)
        {
            HI_ERR_PVR("HI_MPI_DMX_GetTSBuffer failed:%#x!\n", ret);
            return HI_FAILURE;
        }
        else
        {
            usleep(40000);
            if ((pChnAttr->bQuickUpdateStatus) || (pChnAttr->bPlayMainThreadStop))
            {
                return HI_FAILURE;
            }

            ret = HI_MPI_DMX_GetTSBuffer(pChnAttr->hTsBuffer, u32Bytes2Send, &DataBuf, &u32PhyAddr, 0);
        }
    }

    memset(DataBuf.pu8Data, 0xff, DataBuf.u32Size);
    for (i = 0; i < (u32Bytes2Send / 188); i++)
    {
        DataBuf.pu8Data[i * 188] = 0x47;
        DataBuf.pu8Data[i * 188 + 1] = 0x1f;
        DataBuf.pu8Data[i * 188 + 2] = 0xff;
        DataBuf.pu8Data[i * 188 + 3] = 0x10;
    }

    ret = HI_MPI_DMX_PutTSBuffer(pChnAttr->hTsBuffer, u32Bytes2Send, 0);
    if (ret != HI_SUCCESS)
    {
        HI_ERR_PVR("HI_MPI_DMX_PutTSBuffer failed:%#x!\n", ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : PVRPlayCheckError
 Description     : Check return value, if not success, trigger callback event
 Input           : pChnAttr  **
                   ret     **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/5/26
    Author       : q46153
    Modification : Created function

*****************************************************************************/
STATIC INLINE HI_VOID PVRPlayCheckError(const PVR_PLAY_CHN_S  *pChnAttr,  HI_S32 ret)
{
    if (HI_SUCCESS  == ret)
    {
        return;
    }

    /* after pause, stop play, notice error play event, AI7D02621 */
    if (HI_ERR_DMX_NOAVAILABLE_BUF == ret
        || HI_ERR_DMX_NOAVAILABLE_DATA == ret)
    {
        return;
    }

    HI_INFO_PVR("====callback occured, ret=0x%x\n", ret);
    switch (ret)
    {
        case HI_ERR_PVR_FILE_TILL_END:
            if (PVR_Rec_IsFileSaving(pChnAttr->stUserCfg.szFileName))
            {
                PVR_Intf_DoEventCallback(pChnAttr->u32chnID, HI_UNF_PVR_EVENT_PLAY_REACH_REC, 0);
            }
            else
            {
                PVR_Intf_DoEventCallback(pChnAttr->u32chnID, HI_UNF_PVR_EVENT_PLAY_EOF, 0);
            }
            break;
        case HI_ERR_PVR_FILE_TILL_START:
            //if (!PVR_Rec_IsFileSaving(pChnAttr->stUserCfg.szFileName))
            {
                PVR_Intf_DoEventCallback(pChnAttr->u32chnID, HI_UNF_PVR_EVENT_PLAY_SOF, 0);
            }
            break;
        default:
            PVR_Intf_DoEventCallback(pChnAttr->u32chnID, HI_UNF_PVR_EVENT_PLAY_ERROR, ret);
    }

    return;
}

/* play catch up to the record, how long wait to retry */
/*
STATIC INLINE HI_U32 PVRPlayCalcWaitTimeForPlayEnd(PVR_PLAY_CHN_S  *pChnAttr)
{
#define PVR_MIN_HDD_SIZE  10000
#define PVR_MID_HDD_SIZE  400000
#define PVR_DFT_WAIT_TIME 40000

    HI_U32 u32BufSizeHdd = 0;
    HI_U32 u32WaitTime = 0;
    HI_S32 ret = 0;
    HI_UNF_DMX_TSBUF_STATUS_S  tsBufStatus;

    ret = HI_UNF_DMX_GetTSBufferStatus(pChnAttr->hTsBuffer, &tsBufStatus);
    if (HI_SUCCESS != ret)
    {
        return PVR_DFT_WAIT_TIME;
    }
    u32BufSizeHdd = tsBufStatus.u32UsedSize;
    if (u32BufSizeHdd < PVR_MIN_HDD_SIZE)
        u32WaitTime = (800000 - u32BufSizeHdd);
    else if (u32BufSizeHdd >= PVR_MIN_HDD_SIZE && u32BufSizeHdd < PVR_MID_HDD_SIZE)
        u32WaitTime = (800000 - u32BufSizeHdd) / 100;
    else
        u32WaitTime = 1000;

    return u32WaitTime;
}

*/
#if 1
/* when fast forward and backward, calculate the continual time of current frame by next frame
  return : wait time
      0, no need to wait
      -1, next frame will not output
      other value, the vale need to wait, in millisecond.

   calculate the frame bumber by between I frame, and then get the display time by frame-rate, last, calculate the send frame delay of trick mode play

*/

STATIC INLINE HI_S32 PVRPlayCalcCurFrmStayTime(PVR_PLAY_CHN_S  *pChnAttr, PVR_INDEX_ENTRY_S *pNextFrame)
{
    HI_U32 u32NextPlayTimeMs;
    HI_S32 speedx;

    /* when next frame is the first frame, not need to wait, and set next frame to the reference frame */
    if (PVR_INDEX_INVALID_PTSMS == pChnAttr->stTplayCtlInfo.u32RefFrmPtsMs)
    {
        PVR_LOCK(&pChnAttr->IndexHandle->stMutex);
        pChnAttr->stTplayCtlInfo.u32RefFrmSysTimeMs = pNextFrame->u32DisplayTimeMs;///PVRIndexGetCurTimeMs();
        pChnAttr->stTplayCtlInfo.u32RefFrmPtsMs = pNextFrame->u32PtsMs;
        pChnAttr->IndexHandle->u32FrameDistance = 0;
        PVR_UNLOCK(&pChnAttr->IndexHandle->stMutex);
        return 0;
    }

    /* fast forward state */
    if (HI_UNF_PVR_PLAY_STATE_FF == pChnAttr->enState)
    {
        PVR_LOCK(&pChnAttr->IndexHandle->stMutex);
        speedx = pChnAttr->enSpeed / HI_UNF_PVR_PLAY_SPEED_NORMAL;
        u32NextPlayTimeMs = (pChnAttr->IndexHandle->u32FrameDistance*PVR_TPLAY_MIN_FRAME_RATE)/(HI_U32)speedx;
        if(u32NextPlayTimeMs < PVR_TPLAY_FRAME_SHOW_TIME)
        {
            HI_INFO_PVR("drop a frame, NextPlayTimeMs %u, FrameDistance %u, Pts:%u, ReadFrame %u\n",
                u32NextPlayTimeMs,
                pChnAttr->IndexHandle->u32FrameDistance,
                pNextFrame->u32PtsMs,
                pChnAttr->IndexHandle->u32ReadFrame);

            //pChnAttr->IndexHandle->u32FrameDistance = 0;
            PVR_UNLOCK(&pChnAttr->IndexHandle->stMutex);
            return -1;
        }
        else
        {
            HI_INFO_PVR("---------FF:u32NextPlayTimeMs %d,u32FrameDistance %d,u32PtsMs %d,u32ReadFrame %d\n",
                u32NextPlayTimeMs,
                pChnAttr->IndexHandle->u32FrameDistance,
                pNextFrame->u32PtsMs,
                pChnAttr->IndexHandle->u32ReadFrame);

            pChnAttr->IndexHandle->u32FrameDistance = pChnAttr->IndexHandle->u32FrameDistance % (HI_U32)speedx;
            PVR_UNLOCK(&pChnAttr->IndexHandle->stMutex);
            return (HI_S32)u32NextPlayTimeMs;
        }
    }

    /*fast backward state */
    if (HI_UNF_PVR_PLAY_STATE_FB == pChnAttr->enState)
    {
        PVR_LOCK(&pChnAttr->IndexHandle->stMutex);
        speedx = (0 - pChnAttr->enSpeed) / HI_UNF_PVR_PLAY_SPEED_NORMAL;
        u32NextPlayTimeMs = (pChnAttr->IndexHandle->u32FrameDistance*PVR_TPLAY_MIN_FRAME_RATE)/(HI_U32)speedx;
        if(u32NextPlayTimeMs < PVR_TPLAY_FRAME_SHOW_TIME)
        {
            HI_INFO_PVR("less than min distance, drop a frame, NextPlayTimeMs %d, FrameDistance %d,PtsMs %d, ReadFrame %d\n",
                u32NextPlayTimeMs,
                pChnAttr->IndexHandle->u32FrameDistance,
                pNextFrame->u32PtsMs,
                pChnAttr->IndexHandle->u32ReadFrame);

            //pChnAttr->IndexHandle->u32FrameDistance = 0;
            PVR_UNLOCK(&pChnAttr->IndexHandle->stMutex);
            return -1;
        }
        else
        {
            HI_INFO_PVR("----------FB:u32NextPlayTimeMs %d,u32FrameDistance %d,u32PtsMs %d,u32ReadFrame %d\n",
                u32NextPlayTimeMs,
                pChnAttr->IndexHandle->u32FrameDistance,
                pNextFrame->u32PtsMs,
                pChnAttr->IndexHandle->u32ReadFrame);

            pChnAttr->IndexHandle->u32FrameDistance = pChnAttr->IndexHandle->u32FrameDistance % (HI_U32)speedx;
            PVR_UNLOCK(&pChnAttr->IndexHandle->stMutex);
            return (HI_S32)u32NextPlayTimeMs;
        }
    }
    HI_ERR_PVR("invalid state.\n");
    return -1;
}

#endif

#if 0
/*according to next frame, calculate the last time of current frame when forward play or rewind play *//*CNcomment:����Ϳ���ʱ������һ֡��������㵱ǰ֡��Ҫ����ʱ��*/

STATIC INLINE HI_S32 PVRPlayCalcCurFrmStayTime(PVR_PLAY_CHN_S  *pChnAttr, PVR_INDEX_ENTRY_S *pNextFrame)
{
    HI_U32 u32CurPlayTimeMs;
    HI_U32 u32NextPlayTimeMs;
    HI_S32 speedx;

    /*if the next frame is the first frame,do not wait and set next frame to refrence frame*//*CNcomment:��һ֡Ϊ��һ֡���ȴ���������һ֡����Ϊ�ο�֡*/
    if (PVR_INDEX_INVALID_PTSMS == pChnAttr->stTplayCtlInfo.u32RefFrmPtsMs)
    {
        pChnAttr->stTplayCtlInfo.u32RefFrmPtsMs = pNextFrame->u32DisplayTimeMs;
        pChnAttr->stTplayCtlInfo.u32RefFrmSysTimeMs = PVRIndexGetCurTimeMs();
        return 0;
    }

    /*system time of current program played already *//*CNcomment: ��ǰ��Ŀ�Ѿ����ŵ�ϵͳʱ��*/
    u32CurPlayTimeMs = PVRIndexGetCurTimeMs() - pChnAttr->stTplayCtlInfo.u32RefFrmSysTimeMs;

    /*forward *//*CNcomment:���״̬*/
    if (HI_UNF_PVR_PLAY_STATE_FF == pChnAttr->enState)
    {
        speedx = pChnAttr->enSpeed / HI_UNF_PVR_PLAY_SPEED_NORMAL;
        u32NextPlayTimeMs = (pNextFrame->u32DisplayTimeMs - pChnAttr->stTplayCtlInfo.u32RefFrmPtsMs)
                / speedx;

        /* if frame's time less than 40ms,do not play *//*CNcomment: ���ڵ�֡�Ͳ����� */
        if (u32NextPlayTimeMs < (u32CurPlayTimeMs + PVR_TPLAY_MIN_DISTANCE))
        {
            HI_INFO_PVR("less than min distance, donn't send this frame, PTS=%u, playtime=%d, cur+%d=%d.\n",
                pNextFrame->u32DisplayTimeMs, u32NextPlayTimeMs, PVR_TPLAY_MIN_DISTANCE,
                (u32CurPlayTimeMs + PVR_TPLAY_MIN_DISTANCE));
            return -1;
        }
        else
        {
            HI_INFO_PVR("FF: 1st frm:%u, cur frm:%u,  cur time:%u, nxt time:%u.\n",
                pChnAttr->stTplayCtlInfo.u32RefFrmPtsMs, pNextFrame->u32DisplayTimeMs,
                u32CurPlayTimeMs, u32NextPlayTimeMs);
            HI_INFO_PVR("u32NextPlayTimeMs %d,u32ReadFrame %d\n",(u32NextPlayTimeMs - u32CurPlayTimeMs),pChnAttr->IndexHandle->u32ReadFrame);
            return (u32NextPlayTimeMs - u32CurPlayTimeMs);
        }
    }

    /*rewind  *//*CNcomment: ����״̬*/
    if (HI_UNF_PVR_PLAY_STATE_FB == pChnAttr->enState)
    {
        speedx = (0 - pChnAttr->enSpeed) / HI_UNF_PVR_PLAY_SPEED_NORMAL;
        u32NextPlayTimeMs = (pChnAttr->stTplayCtlInfo.u32RefFrmPtsMs - pNextFrame->u32DisplayTimeMs)
                / speedx;

        if (u32NextPlayTimeMs < (u32CurPlayTimeMs + PVR_TPLAY_MIN_DISTANCE))
        {
            HI_INFO_PVR("less than min distance, donn't send this frame, PTS=%d.\n", pNextFrame->u32DisplayTimeMs);
            return -1;
        }
        else
        {

            return (u32NextPlayTimeMs - u32CurPlayTimeMs);
        }
    }

    HI_ERR_PVR("invalid state.\n");
    return -1;
}

#endif

/* seek the read pointer of index to the output one frame, so that, seek or play state can get the correct reference position*/
STATIC INLINE HI_S32 PVRPlaySeekToCurFrame(PVR_INDEX_HANDLE handle, PVR_PLAY_CHN_S  *pChnAttr,
            HI_UNF_PVR_PLAY_STATE_E enCurState, HI_UNF_PVR_PLAY_STATE_E enNextState)
{
    HI_UNF_AVPLAY_STATUS_INFO_S stInfo;
    HI_S32 ret;
    HI_U32 IsForword;
    HI_U32 IsNextForword;
    HI_U32 u32SeekToPTS;

    /* if current play to the start or end of file, seek it to start or end directly. no longer find it by current frame. */
    if (pChnAttr->bEndOfFile)
    {
        if (pChnAttr->bTillStartOfFile)
        {
            return PVR_Index_SeekToStart(handle);
        }
        else
        {
            return PVR_Index_SeekToEnd(handle);
        }
    }

    if ((HI_UNF_PVR_PLAY_STATE_FB == enCurState)
        || (HI_UNF_PVR_PLAY_STATE_STEPB == enCurState))
    {
        IsForword = 1;
    }
    else
    {
        IsForword = 0;
    }

    if ((HI_UNF_PVR_PLAY_STATE_FB == enNextState)
        || (HI_UNF_PVR_PLAY_STATE_STEPB == enNextState))
    {
        IsNextForword = 1;
    }
    else
    {
        IsNextForword = 0;
    }

    ret = HI_MPI_AVPLAY_GetStatusInfo(pChnAttr->hAvplay, &stInfo);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("HI_MPI_AVPLAY_GetStatusInfo failed!\n");
        return HI_FAILURE;
    }

    if (PVR_INDEX_IS_TYPE_AUDIO(handle))
    {
         u32SeekToPTS = stInfo.stSyncStatus.u32LocalTime;
    }
    else
    {
         u32SeekToPTS = stInfo.stSyncStatus.u32LastVidPts;
    }
    /* evade case */
    if ((PVR_INDEX_INVALID_PTSMS == u32SeekToPTS)||((stInfo.stSyncStatus.u32LastVidPts-stInfo.stSyncStatus.u32FirstVidPts)<500))
    {
        HI_WARN_PVR("current pts invalid(-1), do not seek to it!\n");
        return HI_SUCCESS;
    }

    ret = PVR_Index_SeekToPTS(handle, u32SeekToPTS, IsForword, IsNextForword);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("PVR_Index_SeekToPTS not found the PTS failed!\n");
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

/* reset buffer and player, seek ts position to the current play frame, if that frame invalid, mean to reset it already, the decoder no longer reset it */
STATIC INLINE HI_S32 PVRPlayResetToCurFrame(PVR_INDEX_HANDLE handle, PVR_PLAY_CHN_S  *pChnAttr, HI_UNF_PVR_PLAY_STATE_E enNextState)
{
    HI_S32 ret = HI_SUCCESS;
    HI_UNF_PVR_PLAY_STATE_E enPreState;

    /* failed to get current frame, no longer reset player and ts buffer, imply has reset it already*/
    if (HI_UNF_PVR_PLAY_STATE_PAUSE == pChnAttr->enState)
    {
        enPreState = pChnAttr->enLastState;
    }
    else
    {
        enPreState = pChnAttr->enState;
    }

    ret = PVRPlaySeekToCurFrame(pChnAttr->IndexHandle, pChnAttr, enPreState, enNextState);
    if (HI_SUCCESS == ret)
    {
        HI_INFO_PVR("to reset buffer and player.\n");
        PVR_Index_ChangePlayMode(pChnAttr->IndexHandle);


        pChnAttr->bTsBufReset = HI_TRUE;
        ret = HI_UNF_DMX_ResetTSBuffer(pChnAttr->hTsBuffer);
        if (HI_SUCCESS != ret)
        {
            HI_ERR_PVR("ts buffer reset failed!\n");
            return HI_FAILURE;
        }

        ret = HI_UNF_AVPLAY_Reset(pChnAttr->hAvplay, NULL);
        if (HI_SUCCESS != ret)
        {
            HI_ERR_PVR("AVPLAY reset failed!\n");
            return HI_FAILURE;
        }
    }

    UNUSED(handle);
    return HI_SUCCESS;
}

STATIC INLINE HI_S32 PVRPlayCheckIfTsOverByRec(PVR_PLAY_CHN_S  *pChnAttr)
{
    HI_S32 ret;

    if (PVR_Index_QureyClearRecReachPlay(pChnAttr->IndexHandle))
    {
        if (HI_UNF_PVR_PLAY_STATE_FB == pChnAttr->enState) /* ts over play when FB, generate a SOF event */
        {
            return HI_ERR_PVR_FILE_TILL_START;
        }

        PVR_Index_SeekToStart(pChnAttr->IndexHandle);
        pChnAttr->bTsBufReset = HI_TRUE;
        ret = HI_UNF_DMX_ResetTSBuffer(pChnAttr->hTsBuffer);
        if (HI_SUCCESS != ret)
        {
            HI_ERR_PVR("ts buffer reset failed!\n");
            return ret;
        }

        ret = HI_UNF_AVPLAY_Reset(pChnAttr->hAvplay, NULL);
        if (HI_SUCCESS != ret)
        {
            HI_ERR_PVR("AVPLAY reset failed!\n");
            return ret;
        }
    }

    return HI_SUCCESS;
}

STATIC INLINE HI_S32 PVRPlayIsChnTplay(HI_U32 u32Chn)
{
    PVR_PLAY_CHN_S  *pChnAttr;

    PVR_PLAY_CHECK_INIT(&g_stPlayInit);

    PVR_PLAY_CHECK_CHN(u32Chn);
    pChnAttr = &g_stPvrPlayChns[u32Chn];
    PVR_PLAY_CHECK_CHN_INIT(pChnAttr->enState);

    if ( pChnAttr->enState > HI_UNF_PVR_PLAY_STATE_PAUSE
        && pChnAttr->enState < HI_UNF_PVR_PLAY_STATE_STOP)
    {
        return HI_SUCCESS;
    }
    else
    {
        return HI_ERR_PVR_PLAY_INVALID_STATE;
    }
}

/*
check if the frame to play is saved to ts file
*/
STATIC INLINE HI_BOOL PVRPlayIsTsSaved(PVR_PLAY_CHN_S *pChnAttr, PVR_INDEX_ENTRY_S *pFrameToPlay)
{
    if (pChnAttr->IndexHandle->bIsRec)
    {
        if (pChnAttr->IndexHandle->u64FileSizeGlobal >= (HI_U64)pFrameToPlay->u64GlobalOffset)
        {
            return HI_TRUE;
        }
        else
        {
            HI_ERR_PVR("Play Over Rec when timeshift: R/W/Real: %lld, %llu, %llu.\n",pFrameToPlay->u64GlobalOffset,pChnAttr->IndexHandle->u64GlobalOffset, pChnAttr->IndexHandle->u64FileSizeGlobal );
            return HI_FALSE;
        }
    }
    else
    {
        return HI_TRUE;
    }
}

/*****************************************************************************
 Prototype       : PVRPlayMainRoute
 Description     : the main control thread of player
 Input           : args  ** the attribute of play channel
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/22
    Author       : quyaxin 46153
    Modification : Created function

*****************************************************************************/
STATIC void* PVRPlayMainRoute(void *args)
{
    HI_S32              ret = HI_SUCCESS;
    HI_S32              ret_sent = HI_SUCCESS;
    PVR_INDEX_ENTRY_S   frame ={0};
    PVR_PLAY_CHN_S      *pChnAttr = (PVR_PLAY_CHN_S*)args;
    HI_S32              s32StayTimeMs;
    HI_U32              waittime = 200000; /*200ms*/
    HI_BOOL             bCallBack = HI_FALSE;
    HI_BOOL             bLastFrameSent = HI_TRUE;

    if (!pChnAttr)
    {
        return HI_NULL;
    }

    if (NULL == g_pvrfpSend)
    {
        HI_CHAR saveName[256];

        snprintf(saveName, 255, "%s_send.ts", pChnAttr->stUserCfg.szFileName);
        //g_pvrfpSend = fopen(saveName, "wb");
    }

    while (!pChnAttr->bPlayMainThreadStop)
    {
        /* read file to the end, so wait until new comman incomming */
        if (pChnAttr->bEndOfFile && !pChnAttr->bQuickUpdateStatus)
        {
            //HI_ERR_PVR("End of file, wait new commond.\n");
            usleep(10000);
            continue;
        }

        PVR_LOCK(&(pChnAttr->stMutex));

        ret = PVRPlayCheckIfTsOverByRec(pChnAttr);
        if (HI_SUCCESS != ret)
        {
            PVR_UNLOCK(&(pChnAttr->stMutex));
            PVRPlayCheckError(pChnAttr, ret);
            continue;
        }

        pChnAttr->bTsBufReset = HI_FALSE;
        switch (pChnAttr->enState)
        {
            case HI_UNF_PVR_PLAY_STATE_PLAY:
            case HI_UNF_PVR_PLAY_STATE_SF:
            case HI_UNF_PVR_PLAY_STATE_STEPF:
            {
                /* noexistent index file, read fixed size per-time*/
                if (pChnAttr->bPlayingTsNoIdx || !pChnAttr->stUserCfg.bIsClearStream)
                {
                    if (pChnAttr->u64CurReadPos >= pChnAttr->u64TsFileSize)
                    {
                        ret = HI_ERR_PVR_FILE_TILL_END;
                    }
                    else
                    {
                        frame.u16UpFlowFlag = 0;
                        frame.u64Offset = pChnAttr->u64CurReadPos;
                        frame.u32FrameSize = PVR_FIFO_WRITE_BLOCK_SIZE;
                    }
                }
                else
                {
                    /*if (pChnAttr->bQuickUpdateStatus)
                    {
                        ret = PVR_Index_GetNextIFrame(pChnAttr->IndexHandle, &frame);
                    }
                    else*/

                    /* normal play, not need to read data from the first I frame, send all the data is ok */
                    {
                        ret = PVR_Index_GetNextFrame(pChnAttr->IndexHandle, &frame);
                        if (HI_SUCCESS == ret)
                        {
                            bLastFrameSent = HI_FALSE;
                        }
                    }

                    if (!PVRPlayIsTsSaved(pChnAttr, &frame))
                    {
                        ret = HI_ERR_PVR_FILE_TILL_END;
                    }
#if 0
                    if (HI_SUCCESS == PVR_Index_GetCurrentFrame(pChnAttr->IndexHandle, &frameNext))
                    {
                        /* think it as increase case, as for rewind, use the old length of frame */
                        if (frameNext.s64GlobalOffset > frame.s64GlobalOffset)
                        {
                            frame.u32FrameSize = frameNext.s64GlobalOffset - frame.s64GlobalOffset;
                        }
                    }
#endif
                }

                pChnAttr->bQuickUpdateStatus = HI_FALSE;
                break;
            }

            case HI_UNF_PVR_PLAY_STATE_FF:
            {
                pChnAttr->bQuickUpdateStatus = HI_FALSE;

               /* noexistent index file, read fixed size per-time*/
                if (pChnAttr->bPlayingTsNoIdx)
                {
                    frame.u16UpFlowFlag = 0;
                    frame.u64Offset = pChnAttr->u64CurReadPos;
                    frame.u32FrameSize = PVR_FIFO_WRITE_BLOCK_SIZE;
                }
                else
                {
                    ret = PVR_Index_GetNextIFrame(pChnAttr->IndexHandle, &frame);
                }
                break;
            }

            case HI_UNF_PVR_PLAY_STATE_FB:
            {
                pChnAttr->bQuickUpdateStatus = HI_FALSE;

                /* noexistent index file, read fixed size per-time*/
                if (pChnAttr->bPlayingTsNoIdx)
                {
                    frame.u16UpFlowFlag = 0;
                    frame.u64Offset = pChnAttr->u64CurReadPos;
                    frame.u32FrameSize = PVR_FIFO_WRITE_BLOCK_SIZE;
                }
                else
                {
                    ret = PVR_Index_GetPreIFrame(pChnAttr->IndexHandle, &frame);
                }
                break;
            }

            case HI_UNF_PVR_PLAY_STATE_PAUSE:
            {
                pChnAttr->bQuickUpdateStatus = HI_FALSE;
                PVR_UNLOCK(&(pChnAttr->stMutex));
                (HI_VOID)usleep(10000);
                continue;
            }

            case HI_UNF_PVR_PLAY_STATE_STEPB:
            {
                pChnAttr->bQuickUpdateStatus = HI_FALSE;
                pChnAttr->bEndOfFile = HI_FALSE;
                HI_ERR_PVR("not support status.\n");
                PVR_UNLOCK(&(pChnAttr->stMutex));
                (HI_VOID)usleep(10000);
                continue;
            }

            default:
            {
                HI_INFO_PVR("Stop or invalid status: State=%d\n", pChnAttr->enState);
                PVR_UNLOCK(&(pChnAttr->stMutex));
                usleep(10000);
                continue;
            }
        } /* end switch */


        if (HI_SUCCESS == ret)  /* read index OK  */
        {
            pChnAttr->bEndOfFile = HI_FALSE;
            pChnAttr->u64CurReadPos = frame.u64Offset + (HI_U64)frame.u32FrameSize;

#ifdef PVR_PROC_SUPPORT
            paPlayProcInfo[pChnAttr->u32chnID]->u64CurReadPos = pChnAttr->u64CurReadPos;
            paPlayProcInfo[pChnAttr->u32chnID]->u32StartFrame = pChnAttr->IndexHandle->stCycMgr.u32StartFrame;
            paPlayProcInfo[pChnAttr->u32chnID]->u32EndFrame = pChnAttr->IndexHandle->stCycMgr.u32EndFrame;
            paPlayProcInfo[pChnAttr->u32chnID]->u32LastFrame = pChnAttr->IndexHandle->stCycMgr.u32LastFrame;
            paPlayProcInfo[pChnAttr->u32chnID]->u32ReadFrame = pChnAttr->IndexHandle->u32ReadFrame;
#endif
        }
        else /* read index error */
        {
            /* on normally playing, catch up the record, wait a momnet, and then continue, the wait time up to content size of TS buffer */
            if (PVR_Rec_IsFileSaving(pChnAttr->stUserCfg.szFileName)
                && (HI_UNF_PVR_PLAY_STATE_PLAY == pChnAttr->enState)
                && (HI_ERR_PVR_FILE_TILL_END == ret))
            {
                /* not need to calculate it, probe it constantly, obtain the next frame and play it, no frame, wait 20 millisecond*/
                waittime = 200000; //PVRPlayCalcWaitTimeForPlayEnd(pChnAttr);
            }
            else
            {
                HI_WARN_PVR("read idx err:%#x.\n", ret);
            }
        }

        PVR_UNLOCK(&(pChnAttr->stMutex));

        /* successfully read frame, send it to ts buffer */
        if (HI_SUCCESS == ret)
        {
            if (pChnAttr->bPlayMainThreadStop)
            {
                break;
            }

            /* for fast forward and backward, by waiting the control the speed of playing, no matter of timeshift play, event if rewind rewrite the reference frame, still can reference the time */
            if ((HI_UNF_PVR_PLAY_STATE_FF == pChnAttr->enState)
                || (HI_UNF_PVR_PLAY_STATE_FB == pChnAttr->enState))
            {
                s32StayTimeMs = PVRPlayCalcCurFrmStayTime(pChnAttr, &frame);
                if (-1 == s32StayTimeMs)
                {
                    if (PVRPlayIsVoEmpty(pChnAttr))
                    {
                        HI_INFO_PVR("VO empty now, send the frame directly.\n");
                    }
                    else
                    {
                        HI_INFO_PVR("Not send the frame.\n");
                        continue;
                    }
                }
                else if (s32StayTimeMs != 0)
                {
                    HI_INFO_PVR("TPLAY delay %d ms, to play next frame.\n", s32StayTimeMs);
                    s32StayTimeMs = (s32StayTimeMs > 1000) ? 1000 : s32StayTimeMs;
                    usleep((HI_U32)s32StayTimeMs*1000);
                }
                else
                {
                    HI_INFO_PVR("s32StayTimeMs:%d. send.\n", s32StayTimeMs);
                }
            }
            //getchar();
            PVR_LOCK(&(pChnAttr->stMutex));
            if (HI_FALSE == pChnAttr->bTsBufReset)
            {
                ret_sent = PVRPlaySendAframe(pChnAttr, &frame);
                if (HI_SUCCESS != ret_sent)
                {
                    HI_ERR_PVR("======== send a frame err:%x ==========\n", ret);
                }
                bLastFrameSent = HI_TRUE;
            }
            PVR_UNLOCK(&(pChnAttr->stMutex));

            /* send all the frame case, send one frame wait a moment, so that make a sleep for other schedule */
            if ((HI_SUCCESS == ret_sent)
              && ((HI_UNF_PVR_PLAY_STATE_PLAY == pChnAttr->enState)
                || (HI_UNF_PVR_PLAY_STATE_SF == pChnAttr->enState)
                || (HI_UNF_PVR_PLAY_STATE_STEPF == pChnAttr->enState)))
            {
                //usleep(100);
            }
        }
        else
        {
            if (HI_ERR_PVR_FILE_TILL_START == ret)
            {
                pChnAttr->bTillStartOfFile = HI_TRUE;
            }
            else
            {
                pChnAttr->bTillStartOfFile = HI_FALSE;
            }

            /* on normally playing, catch up the record, wait a momnet, and then continue, the wait time up to content size of TS buffer */
            if (PVR_Rec_IsFileSaving(pChnAttr->stUserCfg.szFileName))
            {
                if ( ((HI_UNF_PVR_PLAY_STATE_PLAY == pChnAttr->enState)
                      || ( HI_UNF_PVR_PLAY_STATE_STEPF == pChnAttr->enState)
                      || ( HI_UNF_PVR_PLAY_STATE_SF == pChnAttr->enState))
                      && (HI_ERR_PVR_FILE_TILL_END == ret)) /* NO EOF when play speed < 1x */
                {
                    bCallBack = HI_FALSE;
                    ret = HI_SUCCESS;
                }
                else
                {
                    bCallBack = HI_TRUE;
                }

                if (HI_UNF_PVR_PLAY_STATE_FB == pChnAttr->enState)
                {
                    pChnAttr->bEndOfFile = HI_TRUE;
                }
                usleep(waittime);
            }
            else  /* NOT timeshift, playback only */
            {
                /* notice the complete event after waiting Vid/Aud play over */
                if ((HI_ERR_PVR_FILE_TILL_END == ret) || (HI_ERR_PVR_FILE_TILL_START == ret))
                {
#if 1 /* not do this, wait the automatic exit of decoder, otherwise, the last frame will shake */
                    /* for fast forward and backward, send the first frame or the last frame multi-times, prevent it from being not output , that make sure it will output the first or the last frame */
                    if ((HI_UNF_PVR_PLAY_STATE_FB == pChnAttr->enState)
                        || (HI_UNF_PVR_PLAY_STATE_FF == pChnAttr->enState))
                    {
                        PVR_LOCK(&(pChnAttr->stMutex));
                        if (HI_FALSE == pChnAttr->bTsBufReset)
                        {
                            ret_sent = PVRPlaySendAframe(pChnAttr, &pChnAttr->IndexHandle->stCurPlayFrame);
                            ret_sent |= PVRPlaySendAframe(pChnAttr, &pChnAttr->IndexHandle->stCurPlayFrame);
                            ret_sent |= PVRPlaySendAframe(pChnAttr, &pChnAttr->IndexHandle->stCurPlayFrame);
                            ret_sent |= PVRPlaySendAframe(pChnAttr, &pChnAttr->IndexHandle->stCurPlayFrame);
                            ret_sent |= PVRPlaySendAframe(pChnAttr, &pChnAttr->IndexHandle->stCurPlayFrame);
                            if (HI_SUCCESS != ret_sent)
                            {
                                HI_ERR_PVR("======== send a frame err:%x ==========\n", ret_sent);
                            }
                        }
                        PVR_UNLOCK(&(pChnAttr->stMutex));
                    }
#endif
                    /*after finishing send ts, all the way, check whether play over or not, not finished, switch state and not callback */
                    bCallBack = HI_TRUE;
                    pChnAttr->u32LastPtsMs = 0; /* prevent from the first frame -1 being abnormal exit */
                    pChnAttr->u32LastEsBufSize = 0;
		            PVRPlaySendEmptyPacketToTsBuffer(pChnAttr);/*send some empty packet,prevent the demux ptrs do not update in case of error in ts file*/
                    usleep(200000);
                    while (HI_TRUE != PVRPlayWaitForEndOfFile(pChnAttr, 200))
                    {
                        if ((pChnAttr->bQuickUpdateStatus) || (pChnAttr->bPlayMainThreadStop))
                        {
                            bCallBack = HI_FALSE;
                            break;
                        }
                    }

                    if (bCallBack)
                    {
                       pChnAttr->bEndOfFile = HI_TRUE;
                    }
                }
            }

            /* callback by error */
            if ((bCallBack) && !((pChnAttr->bQuickUpdateStatus) || (pChnAttr->bPlayMainThreadStop)))
            {
                PVRPlayCheckError(pChnAttr, ret);
            }
        }

        ret = HI_SUCCESS;
    } /* end while */

    HI_INFO_PVR("<<----------PVRPlayMainRoute exit---------------\n");
    if (NULL != g_pvrfpSend)
    {
        fclose(g_pvrfpSend);

        g_pvrfpSend = NULL;
    }

    UNUSED(bLastFrameSent);
    return NULL;
}


/*the following case, the record can catch up to the live stream */
HI_BOOL PVR_Play_IsFilePlayingSlowPauseBack(const HI_CHAR *pFileName)
{
    HI_U32 i;

    for (i = 0; i < PVR_PLAY_MAX_CHN_NUM; i++)
    {
        if (( g_stPvrPlayChns[i].enState == HI_UNF_PVR_PLAY_STATE_PAUSE)
            || ( g_stPvrPlayChns[i].enState == HI_UNF_PVR_PLAY_STATE_SF)
            || ( g_stPvrPlayChns[i].enState == HI_UNF_PVR_PLAY_STATE_STEPB)
            || ( g_stPvrPlayChns[i].enState == HI_UNF_PVR_PLAY_STATE_STEPF))
        {
            if  ( !strcmp(g_stPvrPlayChns[i].stUserCfg.szFileName, pFileName) )
            {
                return HI_TRUE;
            }
        }
    }

    return HI_FALSE;
}

/*****************************************************************************
 Prototype       : HI_PVR_PlayInit
 Description     : play module initializde
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/14
    Author       : q46153
    Modification : Created function

*****************************************************************************/
HI_S32 HI_PVR_PlayInit(HI_VOID)
{
    HI_U32 i;
    HI_S32 ret;
    PVR_PLAY_CHN_S *pChnAttr;

    if (HI_TRUE == g_stPlayInit.bInit)
    {
        HI_WARN_PVR("Play Module has been Initialized!\n");
        return HI_SUCCESS;
    }
    else
    {
        /*initialize whole of  index */
        PVR_Index_Init();

        ret = PVRPlayDevInit();
        if (HI_SUCCESS != ret)
        {
            return ret;
        }

        ret = PVRIntfInitEvent();
        if (HI_SUCCESS != ret)
        {
            return ret;
        }

        /* set all play channel as INVALID status                            */
        for (i = 0 ; i < PVR_PLAY_MAX_CHN_NUM; i++)
        {
            pChnAttr = &g_stPvrPlayChns[i];
            pChnAttr->enState = HI_UNF_PVR_PLAY_STATE_INVALID;
            pChnAttr->enLastState = HI_UNF_PVR_PLAY_STATE_INVALID;
            pChnAttr->bPlayMainThreadStop = HI_TRUE;
            pChnAttr->u32chnID = i;
            pChnAttr->s32DataFile = PVR_FILE_INVALID_FILE;
            pChnAttr->u64CurReadPos = 0;
            pChnAttr->IndexHandle = NULL;
            pChnAttr->hCipher = 0;
            pChnAttr->PlayStreamThread = 0;
            pChnAttr->bCAStreamHeadSent = HI_FALSE;
            pChnAttr->u64LastSeqHeadOffset = PVR_INDEX_INVALID_SEQHEAD_OFFSET;
            pChnAttr->readCallBack = NULL;
            memset(&pChnAttr->stUserCfg, 0, sizeof(HI_UNF_PVR_PLAY_ATTR_S));
            memset(&pChnAttr->stCipherBuf, 0, sizeof(PVR_PHY_BUF_S));

            if(-1 == pthread_mutex_init(&(pChnAttr->stMutex), NULL))
            {
                PVRIntfDeInitEvent();
                HI_ERR_PVR("init mutex lock for PVR play chn%d failed \n", i);
                return HI_FAILURE;
            }
        }

        g_stPlayInit.bInit = HI_TRUE;

        return HI_SUCCESS;
    }
}

/*****************************************************************************
 Prototype       : HI_PVR_PlayDeInit
 Description     : play module de-initialize
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/14
    Author       : q46153
    Modification : Created function

*****************************************************************************/
HI_S32 HI_PVR_PlayDeInit(HI_VOID)
{
    HI_U32 i;

    if ( HI_FALSE == g_stPlayInit.bInit )
    {
        HI_WARN_PVR("Play Module is not Initialized!\n");
        return HI_SUCCESS;
    }
    else
    {
        /* set all play channel as INVALID status                            */
        for (i = 0 ; i < PVR_PLAY_MAX_CHN_NUM; i++)
        {
            if (g_stPvrPlayChns[i].enState != HI_UNF_PVR_PLAY_STATE_INVALID)
            {
                HI_ERR_PVR("play chn%d is in use, can NOT deInit PLAY!\n", i);
                return HI_ERR_PVR_BUSY;
            }

            (HI_VOID)pthread_mutex_destroy(&(g_stPvrPlayChns[i].stMutex));
        }

        g_stPlayInit.bInit = HI_FALSE;
        PVRIntfDeInitEvent();
        return HI_SUCCESS;
    }
}

/*****************************************************************************
 Prototype       : HI_PVR_PlayCreateChn
 Description     : create one play channel
 Input           : pAttr  **the attribute of channel
 Output          : pchn   **play channel number
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/22
    Author       : quyaxin 46153
    Modification : Created function

*****************************************************************************/
HI_S32 HI_PVR_PlayCreateChn(HI_U32 *pChn, const HI_UNF_PVR_PLAY_ATTR_S *pAttr, HI_HANDLE hAvplay, HI_HANDLE hTsBuffer)
{
    HI_S32 ret;
    PVR_PLAY_CHN_S *pChnAttr =NULL;

    PVR_CHECK_POINTER(pAttr);
    PVR_CHECK_POINTER(pChn);

    PVR_PLAY_CHECK_INIT(&g_stPlayInit);

    ret = PVRPlayCheckUserCfg(pAttr, hAvplay, hTsBuffer);
    if (HI_SUCCESS != ret)
    {
        return ret;
    }

    pChnAttr = PVRPlayFindFreeChn();
    if (NULL == pChnAttr)
    {
        HI_ERR_PVR("Not enough channel to be used!\n");
        return HI_ERR_PVR_NO_CHN_LEFT;
    }

    PVR_LOCK(&(pChnAttr->stMutex));

    pChnAttr->s32DataFile = PVR_FILE_INVALID_FILE;
    pChnAttr->IndexHandle = NULL;
    pChnAttr->hCipher = 0;
    pChnAttr->PlayStreamThread = 0;
    pChnAttr->bCAStreamHeadSent = HI_FALSE;
    pChnAttr->u64LastSeqHeadOffset = PVR_INDEX_INVALID_SEQHEAD_OFFSET;
    pChnAttr->bPlayMainThreadStop = HI_TRUE;
    pChnAttr->u64CurReadPos = 0;
    //pChnAttr->bAdecStoped = HI_FALSE;
    pChnAttr->enSpeed = HI_UNF_PVR_PLAY_SPEED_NORMAL;
    pChnAttr->bPlayingTsNoIdx = HI_FALSE;
    pChnAttr->bTsBufReset = HI_TRUE;
    memcpy(&pChnAttr->stUserCfg, pAttr, sizeof(HI_UNF_PVR_PLAY_ATTR_S));
    //pChnAttr->stUserCfg.bIsClearStream = HI_FALSE;

    /* initialize cipher module */
    ret = PVRPlayPrepareCipher(pChnAttr);
    if (ret != HI_SUCCESS)
    {
        goto ErrorExit;
    }

    /*  check whether current to open file is recording or not
        if recording, return the recording file index handle, regard it as timeshift play channel to manage
        or alone play channel, which not support record the playing file.
    */
    pChnAttr->IndexHandle = PVR_Index_CreatPlay(pChnAttr->u32chnID, pAttr, &pChnAttr->bPlayingTsNoIdx);
    if (NULL == pChnAttr->IndexHandle)
    {
        HI_ERR_PVR("index init failed.\n");
        ret = HI_ERR_PVR_FILE_CANT_READ;
        goto ErrorExit;
    }

    /* open ts file */
    pChnAttr->s32DataFile = PVR_OPEN64(pAttr->szFileName,PVR_FOPEN_MODE_DATA_READ);
    if (PVR_FILE_INVALID_FILE == pChnAttr->s32DataFile)
    {
        ret = HI_ERR_PVR_FILE_CANT_OPEN;
        goto ErrorExit;
    }

    pChnAttr->u64TsFileSize = PVR_FILE_GetFileSize64(pAttr->szFileName);

    pChnAttr->hAvplay = hAvplay;
    pChnAttr->hTsBuffer = hTsBuffer;
    *pChn = pChnAttr->u32chnID;

#ifdef PVR_PROC_SUPPORT
    /* proc information initialize */
    strcpy(paPlayProcInfo[pChnAttr->u32chnID]->szFileName, pAttr->szFileName);
    paPlayProcInfo[pChnAttr->u32chnID]->u32DmxId = pChnAttr->u32chnID;
    paPlayProcInfo[pChnAttr->u32chnID]->hAvplay = pChnAttr->hAvplay;
    paPlayProcInfo[pChnAttr->u32chnID]->hTsBuffer = pChnAttr->hTsBuffer;
    paPlayProcInfo[pChnAttr->u32chnID]->hCipher = pChnAttr->hCipher;
    paPlayProcInfo[pChnAttr->u32chnID]->enState = pChnAttr->enState;
    paPlayProcInfo[pChnAttr->u32chnID]->enSpeed = pChnAttr->enSpeed;
    paPlayProcInfo[pChnAttr->u32chnID]->u64CurReadPos = pChnAttr->u64CurReadPos;
#endif

    PVR_UNLOCK(&(pChnAttr->stMutex));

    HI_INFO_PVR("\n--------PVR.PLAY.Ver:%s_%s\n\n", __DATE__, __TIME__);

    return HI_SUCCESS;

ErrorExit:
    if (PVR_FILE_INVALID_FILE != pChnAttr->s32DataFile)
    {
        (HI_VOID)PVR_CLOSE64(pChnAttr->s32DataFile);
        pChnAttr->s32DataFile = PVR_FILE_INVALID_FILE;
    }

    if (pChnAttr->IndexHandle)
    {
       (HI_VOID)PVR_Index_Destroy(pChnAttr->IndexHandle, PVR_INDEX_PLAY);
       pChnAttr->IndexHandle = NULL;
    }
    
    (HI_VOID)PVRPlayReleaseCipher(pChnAttr);

    pChnAttr->enState = HI_UNF_PVR_PLAY_STATE_INVALID;
    ioctl(g_s32PvrFd, CMD_PVR_DESTROY_PLAY_CHN, (HI_S32)&(pChnAttr->u32chnID));
    PVR_UNLOCK(&(pChnAttr->stMutex));
    return ret;
}

/*****************************************************************************
 Prototype       : HI_PVR_PlayDestroyChn
 Description     : destroy one play channel
 Input           : u32Chn  **channel number
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/22
    Author       : quyaxin 46153
    Modification : Created function

*****************************************************************************/
HI_S32 HI_PVR_PlayDestroyChn(HI_U32 u32Chn)
{
    PVR_PLAY_CHN_S  *pChnAttr;

    PVR_PLAY_CHECK_CHN(u32Chn);
    pChnAttr = &g_stPvrPlayChns[u32Chn];
    PVR_LOCK(&(pChnAttr->stMutex));
    PVR_PLAY_CHECK_CHN_INIT_UNLOCK(pChnAttr);

    /* check channel state */
    if (!(HI_UNF_PVR_PLAY_STATE_STOP == pChnAttr->enState
        || HI_UNF_PVR_PLAY_STATE_INIT == pChnAttr->enState))
    {
        HI_ERR_PVR("You should stop channel first!\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_ERR_PVR_PLAY_INVALID_STATE;
    }

    pChnAttr->enState = HI_UNF_PVR_PLAY_STATE_INVALID;
    if (HI_SUCCESS != ioctl(g_s32PvrFd, CMD_PVR_DESTROY_PLAY_CHN, (HI_S32)&u32Chn))
    {
        HI_FATAL_PVR("pvr play destroy channel error.\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_FAILURE;
    }

    /* close stream file                                                    */
    (HI_VOID)PVR_CLOSE64(pChnAttr->s32DataFile);

    pChnAttr->IndexHandle->u32ReadFrame = 0; //zyy 2009.05.22 AI7D05498
    (HI_VOID)PVR_Index_Destroy(pChnAttr->IndexHandle, PVR_INDEX_PLAY);
    pChnAttr->IndexHandle = NULL;

    (HI_VOID)PVRPlayReleaseCipher(pChnAttr);

    pChnAttr->u64LastSeqHeadOffset = PVR_INDEX_INVALID_SEQHEAD_OFFSET;
    pChnAttr->PlayStreamThread = 0;

#ifdef PVR_PROC_SUPPORT
    memset(paPlayProcInfo[u32Chn], 0, sizeof(PVR_PLAY_CHN_PROC_S));
#endif

    PVR_UNLOCK(&(pChnAttr->stMutex));

    return HI_SUCCESS;
}


/*****************************************************************************
 Prototype       : HI_PVR_PlaySetChn
 Description     : set play channle attributes
 Input           : chn    **
                   pAttr  **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/29
    Author       : q46153
    Modification : Created function

*****************************************************************************/
HI_S32  HI_PVR_PlaySetChn(HI_U32 u32ChnID, const HI_UNF_PVR_PLAY_ATTR_S *pstPlayAttr)
{
    PVR_PLAY_CHN_S  *pChnAttr;

    PVR_PLAY_CHECK_CHN(u32ChnID);
    pChnAttr = &g_stPvrPlayChns[u32ChnID];
    PVR_PLAY_CHECK_CHN_INIT(pChnAttr->enState);

    PVR_CHECK_POINTER(pstPlayAttr);

    /* TODO: we set several attributes which can be set dynamically. */

    return HI_ERR_PVR_NOT_SUPPORT;
}

/*****************************************************************************
 Prototype       : HI_PVR_PlayGetChn
 Description     : get play channel attribute
 Input           : chn    **
                   pAttr  **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/29
    Author       : q46153
    Modification : Created function

*****************************************************************************/
HI_S32 HI_PVR_PlayGetChn(HI_U32 u32ChnID, HI_UNF_PVR_PLAY_ATTR_S *pstPlayAttr)
{
    PVR_PLAY_CHN_S  *pChnAttr;

    PVR_PLAY_CHECK_CHN(u32ChnID);
    pChnAttr = &g_stPvrPlayChns[u32ChnID];
    PVR_PLAY_CHECK_CHN_INIT(pChnAttr->enState);

    PVR_CHECK_POINTER(pstPlayAttr);

    memcpy(pstPlayAttr, &pChnAttr->stUserCfg, sizeof(HI_UNF_PVR_PLAY_ATTR_S));

    return HI_SUCCESS;
}


/*****************************************************************************
 Prototype       : HI_PVR_PlayStartChn
 Description     : start play channel
 Input           : u32ChnID **channel number
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/22
    Author       : quyaxin 46153
    Modification : Created function

*****************************************************************************/
HI_S32 HI_PVR_PlayStartChn(HI_U32 u32ChnID) /* pause when end of file */
{
    HI_S32                      ret;
    PVR_PLAY_CHN_S              *pChnAttr;
    HI_U32                      pid;
    HI_HANDLE                   hWindow;

    PVR_PLAY_CHECK_CHN(u32ChnID);
    pChnAttr = &g_stPvrPlayChns[u32ChnID];
    PVR_LOCK(&(pChnAttr->stMutex));
    PVR_PLAY_CHECK_CHN_INIT_UNLOCK(pChnAttr);

    if (!(HI_UNF_PVR_PLAY_STATE_STOP == pChnAttr->enState
          || HI_UNF_PVR_PLAY_STATE_INIT == pChnAttr->enState))
    {
        PVR_UNLOCK(&(pChnAttr->stMutex));
        HI_ERR_PVR("Can't start play channel at current state!\n");
        return HI_ERR_PVR_PLAY_INVALID_STATE;
    }

    ret = HI_MPI_AVPLAY_GetWindowHandle(pChnAttr->hAvplay, &hWindow);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("AVPLAY get window handle failed!\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_FAILURE;
    }

#if 0
    /* set window to step mode */
    ret = HI_MPI_VO_SetWindowStepMode(hWindow, HI_FALSE);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("Set window step mode failed!\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_FAILURE;
    }
#endif

    ret = HI_UNF_AVPLAY_SetDecodeMode(pChnAttr->hAvplay, HI_UNF_VCODEC_MODE_NORMAL);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("set vdec normal mode error!\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_FAILURE;
    }

    ret = HI_UNF_AVPLAY_GetAttr(pChnAttr->hAvplay, HI_UNF_AVPLAY_ATTR_ID_AUD_PID, &pid);
    if ((HI_SUCCESS != ret) || (0x1fff == pid))
    {
        HI_ERR_PVR("has not audio stream! ret=%#x pid=%d\n", ret, pid);
    }
    else
    {
        ret = HI_UNF_AVPLAY_Start(pChnAttr->hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD, NULL);
        if (HI_SUCCESS != ret)
        {
            PVR_UNLOCK(&(pChnAttr->stMutex));
            HI_ERR_PVR("Can't start audio, error:%#x!\n", ret);
            return ret;
        }
        else
        {
            HI_INFO_PVR("HI_UNF_AVPLAY_start audio ok!\n");
        }
    }

    ret = HI_UNF_AVPLAY_GetAttr(pChnAttr->hAvplay, HI_UNF_AVPLAY_ATTR_ID_VID_PID, &pid);
    if ((HI_SUCCESS != ret) || (0x1fff == pid))
    {
        HI_ERR_PVR("has not video stream! ret=%#x pid=%d\n", ret, pid);
    }
    else
    {
        ret = HI_UNF_AVPLAY_Start(pChnAttr->hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_VID, NULL);
        if (HI_SUCCESS != ret)
        {
            PVR_UNLOCK(&(pChnAttr->stMutex));
            HI_ERR_PVR("Can't start video, error:%#x!\n", ret);
            return ret;
        }
        else
        {
            HI_INFO_PVR("HI_UNF_AVPLAY_start video ok!\n");
        }
    }

    ret = HI_UNF_AVPLAY_Resume(pChnAttr->hAvplay, NULL);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("AVPLAY resume failed!\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_FAILURE;
    }

    pChnAttr->enLastState = pChnAttr->enState;
    pChnAttr->enState = HI_UNF_PVR_PLAY_STATE_PLAY;
    pChnAttr->enSpeed = HI_UNF_PVR_PLAY_SPEED_NORMAL;
    pChnAttr->bPlayMainThreadStop = HI_FALSE;
    pChnAttr->bQuickUpdateStatus = HI_TRUE;
    pChnAttr->u64LastSeqHeadOffset = PVR_INDEX_INVALID_SEQHEAD_OFFSET;
    pChnAttr->bEndOfFile = HI_FALSE;
    memset(&pChnAttr->stLastStatus, 0, sizeof(HI_UNF_PVR_PLAY_STATUS_S));

    PVR_Index_ResetPlayAttr(pChnAttr->IndexHandle);

    /*if(HI_SUCCESS!=PVRPlayFindCorretEndIndex(pChnAttr))
    {
        HI_ERR_PVR("find endframe failed!\n");
    }*/

    if (HI_SUCCESS != PVR_Index_SeekToPauseOrStart(pChnAttr->IndexHandle))
    {
        HI_ERR_PVR("seek to pause frame failed!\n");
    }

    if (pthread_create(&pChnAttr->PlayStreamThread, NULL, PVRPlayMainRoute, pChnAttr))
    {
        HI_U32 u32Stop = HI_UNF_AVPLAY_MEDIA_CHAN_VID;

        u32Stop |= HI_UNF_AVPLAY_MEDIA_CHAN_AUD;
        
        HI_ERR_PVR("create play thread failed!\n");
        HI_UNF_AVPLAY_Stop(pChnAttr->hAvplay, (HI_UNF_AVPLAY_MEDIA_CHAN_E)u32Stop, NULL);
        /* and also reset play state to init                                      */
        pChnAttr->enState = HI_UNF_PVR_PLAY_STATE_INIT ;
        pChnAttr->enLastState = HI_UNF_PVR_PLAY_STATE_INIT ;
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_FAILURE;
    }

#ifdef PVR_PROC_SUPPORT
    paPlayProcInfo[u32ChnID]->enState = pChnAttr->enState;
    paPlayProcInfo[u32ChnID]->enSpeed = pChnAttr->enSpeed;
#endif

    PVR_UNLOCK(&(pChnAttr->stMutex));

    return HI_SUCCESS;
}


/*****************************************************************************
 Prototype       : HI_PVR_PlayStopChn
 Description     : stop play channel
 Input           : u32Chn  **channel number
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/22
    Author       : q46153
    Modification : Created function

*****************************************************************************/
HI_S32 HI_PVR_PlayStopChn(HI_U32 u32Chn, const HI_UNF_AVPLAY_STOP_OPT_S *pstStopOpt)
{
    HI_S32 ret;
    PVR_PLAY_CHN_S  *pChnAttr;
    HI_HANDLE       hWindow;
    HI_U32 u32Stop = 0;

    PVR_PLAY_CHECK_CHN(u32Chn);

    pChnAttr = &g_stPvrPlayChns[u32Chn];
    PVR_LOCK(&(pChnAttr->stMutex));
    PVR_PLAY_CHECK_CHN_INIT_UNLOCK(pChnAttr);

    if ((HI_UNF_PVR_PLAY_STATE_STOP == pChnAttr->enState)
        || (HI_UNF_PVR_PLAY_STATE_INIT == pChnAttr->enState))
    {
        PVR_UNLOCK(&(pChnAttr->stMutex));
        HI_ERR_PVR("Play channel is stopped already!\n");
        return HI_ERR_PVR_ALREADY;
    }

    PVR_UNLOCK(&(pChnAttr->stMutex));

    HI_INFO_PVR("wait Play thread.\n");
    pChnAttr->bPlayMainThreadStop = HI_TRUE;
    (HI_VOID)pthread_join(pChnAttr->PlayStreamThread, NULL);
    HI_INFO_PVR("wait Play thread OK.\n");

    PVR_LOCK(&(pChnAttr->stMutex));

    ret = HI_MPI_AVPLAY_GetWindowHandle(pChnAttr->hAvplay, &hWindow);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("AVPLAY get window handle failed!\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_FAILURE;
    }
#if 0
    /* set window to step mode */
    ret = HI_MPI_VO_SetWindowStepMode(hWindow, HI_FALSE);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("Set window step mode failed!\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_FAILURE;
    }
#endif
    ret = HI_UNF_AVPLAY_SetDecodeMode(pChnAttr->hAvplay, HI_UNF_VCODEC_MODE_NORMAL);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("set vdec normal mode error!\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_FAILURE;
    }

    u32Stop = HI_UNF_AVPLAY_MEDIA_CHAN_VID;

    u32Stop |= HI_UNF_AVPLAY_MEDIA_CHAN_AUD;

    ret = HI_UNF_AVPLAY_Stop(pChnAttr->hAvplay, (HI_UNF_AVPLAY_MEDIA_CHAN_E)u32Stop, pstStopOpt);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("HI_UNF_AVPLAY_Stop failed:%#x, force PVR stop!\n", ret);
    }

    ret = HI_UNF_DMX_ResetTSBuffer(pChnAttr->hTsBuffer);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("ts buffer reset failed!\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_FAILURE;
    }

    pChnAttr->enLastState = HI_UNF_PVR_PLAY_STATE_STOP;
    pChnAttr->enState = HI_UNF_PVR_PLAY_STATE_STOP;
    pChnAttr->bPlayMainThreadStop = HI_FALSE;
    pChnAttr->bQuickUpdateStatus = HI_FALSE;
    pChnAttr->u64LastSeqHeadOffset = PVR_INDEX_INVALID_SEQHEAD_OFFSET;
    pChnAttr->bEndOfFile = HI_FALSE;
    memset(&pChnAttr->stLastStatus, 0, sizeof(HI_UNF_PVR_PLAY_STATUS_S));

#ifdef PVR_PROC_SUPPORT
    paPlayProcInfo[u32Chn]->enState = pChnAttr->enState;
    paPlayProcInfo[u32Chn]->enSpeed = pChnAttr->enSpeed;
#endif

    PVR_Index_ResetPlayAttr(pChnAttr->IndexHandle);

    PVR_UNLOCK(&(pChnAttr->stMutex));
    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : HI_PVR_PlayStartTimeShift
 Description     : start TimeShift
 Input           : pu32PlayChnID   **
                   u32DemuxID   **
                   u32RecChnID  **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/10
    Author       : q46153
    Modification : Created function

*****************************************************************************/
HI_S32 HI_PVR_PlayStartTimeShift(HI_U32 *pu32PlayChnID, HI_U32 u32RecChnID, HI_HANDLE hAvplay, HI_HANDLE hTsBuffer)
{
    HI_S32 ret;
    HI_U32 u32PlayChnID;
    HI_UNF_PVR_REC_ATTR_S RecAttr;
    HI_UNF_PVR_PLAY_ATTR_S PlayAttr;

    PVR_CHECK_POINTER(pu32PlayChnID);

    /* get record channel attribute                                            */
    ret = HI_PVR_RecGetChn(u32RecChnID, &RecAttr);
    if (HI_SUCCESS != ret)
    {
        return ret;
    }

    /* configure play channel with record channel attributes                   */
    PlayAttr.enStreamType = RecAttr.enStreamType;
    PlayAttr.u32FileNameLen = RecAttr.u32FileNameLen;
    PlayAttr.bIsClearStream = RecAttr.bIsClearStream;
    strcpy(PlayAttr.szFileName, RecAttr.szFileName);
    PlayAttr.stDecryptCfg.bDoCipher = RecAttr.stEncryptCfg.bDoCipher;
    PlayAttr.stDecryptCfg.enType = RecAttr.stEncryptCfg.enType;
    PlayAttr.stDecryptCfg.u32KeyLen = RecAttr.stEncryptCfg.u32KeyLen;
    memcpy(PlayAttr.stDecryptCfg.au8Key, RecAttr.stEncryptCfg.au8Key, PVR_MAX_CIPHER_KEY_LEN);

    /* apply a new play channel for timeshift playing                          */
    ret = HI_PVR_PlayCreateChn(&u32PlayChnID, &PlayAttr, hAvplay, hTsBuffer);
    if ( HI_SUCCESS != ret)
    {
        return ret;
    }

    /* start timeshift playing                                                 */
    ret = HI_PVR_PlayStartChn(u32PlayChnID);
    if ( HI_SUCCESS != ret)
    {
        (HI_VOID)HI_PVR_PlayDestroyChn(u32PlayChnID);
        return ret;
    }

    *pu32PlayChnID = u32PlayChnID;
    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : HI_PVR_PlayStopTimeShift
 Description     : stop TimeShift
 Input           : u32PlayChnID  **
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/10
    Author       : q46153
    Modification : Created function

*****************************************************************************/
HI_S32 HI_PVR_PlayStopTimeShift(HI_U32 u32PlayChnID, const HI_UNF_AVPLAY_STOP_OPT_S *pstStopOpt)
{
    HI_S32 ret;

    PVR_PLAY_CHN_S  *pChnAttr;
    PVR_PLAY_CHECK_CHN(u32PlayChnID);
    pChnAttr = &g_stPvrPlayChns[u32PlayChnID];
    PVR_PLAY_CHECK_CHN_INIT(pChnAttr->enState);

    /* stop timeshift play channel                                             */
    ret = HI_PVR_PlayStopChn(u32PlayChnID, pstStopOpt);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("stop play chn failed:%#x!\n", ret);
        return ret;
    }

    ret = HI_PVR_PlayDestroyChn(u32PlayChnID);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("free play chn failed:%#x!\n", ret);
        return ret;
    }
 
    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : HI_PVR_PlayPauseChn
 Description     : pause play channel
 Input           : u32Chn  ** channel number
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/22
    Author       : quyaxin 46153
    Modification : Created function

*****************************************************************************/
HI_S32 HI_PVR_PlayPauseChn(HI_U32 u32Chn)
{
    HI_S32 ret;
    PVR_PLAY_CHN_S  *pChnAttr;

    /* when u32Chn is record channel, mean to pause the live stream */
    if (PVR_Rec_IsChnRecording(u32Chn))
    {
        return PVR_Rec_MarkPausePos(u32Chn);
    }

    PVR_PLAY_CHECK_CHN(u32Chn);
    pChnAttr = &g_stPvrPlayChns[u32Chn];
    PVR_LOCK(&(pChnAttr->stMutex));
    PVR_PLAY_CHECK_CHN_INIT_UNLOCK(pChnAttr);

    if (HI_UNF_PVR_PLAY_STATE_PAUSE == pChnAttr->enState)
    {
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_SUCCESS;
    }

    if (!((HI_SUCCESS == PVRPlayIsChnTplay(u32Chn))
          || (HI_UNF_PVR_PLAY_STATE_PLAY == pChnAttr->enState)))
    {
        HI_ERR_PVR("state:%d NOT support Pause!\n", pChnAttr->enState);
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_ERR_PVR_PLAY_INVALID_STATE;
    }

    ret = HI_UNF_AVPLAY_Pause(pChnAttr->hAvplay, NULL);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("AVPLAY_Pause ERR:%#x!\n", ret);
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return ret;
    }

    pChnAttr->bQuickUpdateStatus = HI_TRUE;
    pChnAttr->enLastState = pChnAttr->enState;
    pChnAttr->enState = HI_UNF_PVR_PLAY_STATE_PAUSE;
    pChnAttr->enSpeed = HI_UNF_PVR_PLAY_SPEED_NORMAL;

#ifdef PVR_PROC_SUPPORT
    paPlayProcInfo[u32Chn]->enState = pChnAttr->enState;
    paPlayProcInfo[u32Chn]->enSpeed = pChnAttr->enSpeed;
#endif

    HI_WARN_PVR("pause OK!\n");
    PVR_UNLOCK(&(pChnAttr->stMutex));
    return HI_SUCCESS;

}

/*****************************************************************************
 Prototype       : HI_PVR_PlayResumeChn
 Description     : resume the play channel
 Input           : u32Chn  **channel number
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/22
    Author       : quyaxin 46153
    Modification : Created function

*****************************************************************************/
HI_S32 HI_PVR_PlayResumeChn(HI_U32 u32Chn)
{
    HI_S32 ret;
    PVR_PLAY_CHN_S  *pChnAttr;
    HI_HANDLE hWindow;
    HI_U32  pid;

    PVR_PLAY_CHECK_CHN(u32Chn);

    pChnAttr = &g_stPvrPlayChns[u32Chn];
    PVR_LOCK(&(pChnAttr->stMutex));
    PVR_PLAY_CHECK_CHN_INIT_UNLOCK(pChnAttr);

    if (HI_UNF_PVR_PLAY_STATE_PLAY == pChnAttr->enState)
    {
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_SUCCESS;
    }

    /* among forward play mode, just notice the event when reach to the start, not switch status  */
    if ((pChnAttr->bEndOfFile) && (PVR_IS_PLAY_FORWARD(pChnAttr->enState, pChnAttr->enLastState)))
    {
        HI_WARN_PVR("need not start main rout, laststate=%d!\n", pChnAttr->enState);
        PVR_Intf_DoEventCallback(pChnAttr->u32chnID, HI_UNF_PVR_EVENT_PLAY_EOF, 0);
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_SUCCESS;
    }

    ret = HI_MPI_AVPLAY_GetWindowHandle(pChnAttr->hAvplay, &hWindow);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("AVPLAY get window handle failed!\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_FAILURE;
    }
#if 0
    /* set window to step mode */
    ret = HI_MPI_VO_SetWindowStepMode(hWindow, HI_FALSE);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("Set window step mode failed!\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_FAILURE;
    }
#endif
    ret = HI_UNF_AVPLAY_Resume(pChnAttr->hAvplay, NULL);

    //ret = PVRPlaySetWinRate(hWindow, (HI_UNF_PVR_PLAY_SPEED_E)(HI_UNF_PVR_PLAY_SPEED_NORMAL/4));
    //HI_INFO_PVR("resume play PVRPlaySetWinRate result is %d\n", ret);

    ret = HI_UNF_AVPLAY_SetDecodeMode(pChnAttr->hAvplay, HI_UNF_VCODEC_MODE_NORMAL);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("set vdec normal mode error!\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_FAILURE;
    }

    ret = PVRPlayCheckIfTsOverByRec(pChnAttr);
    if (HI_SUCCESS != ret)
    {
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_FAILURE;
    }

    /*except of pause state, all the other state switch to play need to reset it to current playing frame */
    else if ((HI_UNF_PVR_PLAY_STATE_PAUSE != pChnAttr->enState)
      || ((HI_UNF_PVR_PLAY_STATE_PAUSE == pChnAttr->enState)
        && (HI_UNF_PVR_PLAY_STATE_PLAY != pChnAttr->enLastState)))
    {
        HI_INFO_PVR("to reset buffer and player.\n");

        ret = PVRPlayResetToCurFrame(pChnAttr->IndexHandle, pChnAttr, HI_UNF_PVR_PLAY_STATE_PLAY);
        if (HI_SUCCESS != ret)
        {
            HI_ERR_PVR("reset to current frame failed!\n");
        }
    }

    ret = HI_UNF_AVPLAY_GetAttr(pChnAttr->hAvplay, HI_UNF_AVPLAY_ATTR_ID_AUD_PID, &pid);
    if ((HI_SUCCESS != ret) || (0x1fff == pid))
    {
        HI_WARN_PVR("has not audio stream!\n");
    }
    else
    {
        ret = HI_UNF_AVPLAY_Start(pChnAttr->hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD, NULL);
        if (HI_SUCCESS != ret)
        {
            PVR_UNLOCK(&(pChnAttr->stMutex));
            HI_ERR_PVR("Can't start audio, error:%#x!\n", ret);
            return ret;
        }
        else
        {
            HI_INFO_PVR("HI_UNF_AVPLAY_start audio ok!\n");
        }
    }

    ret = HI_UNF_AVPLAY_Resume(pChnAttr->hAvplay, NULL);
    if (ret != HI_SUCCESS)
    {
        HI_ERR_PVR("AVPLAY_Resume failed:%#x\n", ret);
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_FAILURE;
    }

    pChnAttr->bQuickUpdateStatus = HI_TRUE;
    pChnAttr->enLastState = pChnAttr->enState;
    pChnAttr->enState = HI_UNF_PVR_PLAY_STATE_PLAY;
    pChnAttr->enSpeed = HI_UNF_PVR_PLAY_SPEED_NORMAL;

#ifdef PVR_PROC_SUPPORT
    paPlayProcInfo[u32Chn]->enState = pChnAttr->enState;
    paPlayProcInfo[u32Chn]->enSpeed = pChnAttr->enSpeed;
#endif

    HI_WARN_PVR("resume OK!\n");
    PVR_UNLOCK(&(pChnAttr->stMutex));
    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : HI_PVR_PlayTrickMode
 Description     : set the play mode of play channel
 Input           : u32Chn         **channel number
                   pTrickMode  **play mode, trick mode
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/22
    Author       : quyaxin 46153
    Modification : Created function

*****************************************************************************/
HI_S32 HI_PVR_PlayTrickMode(HI_U32 u32Chn, const HI_UNF_PVR_PLAY_MODE_S *pTrickMode)
{
    HI_S32 ret;
    PVR_PLAY_CHN_S  *pChnAttr;
    HI_UNF_PVR_PLAY_STATE_E stateToSet;

    HI_UNF_AVPLAY_TPLAY_OPT_S stTPlayOpt;
    
    PVR_CHECK_POINTER(pTrickMode);
    PVR_PLAY_CHECK_CHN(u32Chn);

    /*set the mode is rate one, that should be equal to resume */
    if (HI_UNF_PVR_PLAY_SPEED_NORMAL == pTrickMode->enSpeed) /* switch to the normal mode */
    {
        return HI_PVR_PlayResumeChn(u32Chn);
    }

    pChnAttr = &g_stPvrPlayChns[u32Chn];
    PVR_LOCK(&(pChnAttr->stMutex));
    PVR_PLAY_CHECK_CHN_INIT_UNLOCK(pChnAttr);

    /* not support slow backward play */
    PVR_GET_STATE_BY_SPEED(stateToSet, pTrickMode->enSpeed);
    if (HI_UNF_PVR_PLAY_STATE_INVALID == stateToSet)
    {
        HI_ERR_PVR("NOT support this trick mode.\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_ERR_PVR_NOT_SUPPORT;
    }

    /* after playing, any play state can be switched to Tplay */
    if ((pChnAttr->enState < HI_UNF_PVR_PLAY_STATE_PLAY)
        || (pChnAttr->enState > HI_UNF_PVR_PLAY_STATE_STEPB))
    {
        HI_ERR_PVR("State now:%d, NOT support TPlay!\n", pChnAttr->enState);
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_ERR_PVR_PLAY_INVALID_STATE;
    }

    if (HI_TRUE == pChnAttr->bPlayingTsNoIdx)
    {
        HI_ERR_PVR("No index file, NOT support trick mode.\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_ERR_PVR_NOT_SUPPORT;
    }

    /* audio index file not support Tplay*/
    if (PVR_INDEX_IS_TYPE_AUDIO(pChnAttr->IndexHandle))
    {
        HI_ERR_PVR("audio indexed stream NOT support trick mode.\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_ERR_PVR_NOT_SUPPORT;
    }

    /* scramble stream not support Tplay */
    if (!pChnAttr->stUserCfg.bIsClearStream)
    {
        HI_ERR_PVR("scrambed stream NOT support trick mode !\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_ERR_PVR_NOT_SUPPORT;
    }

    if ((pChnAttr->enState == stateToSet)
        && (pChnAttr->enSpeed == pTrickMode->enSpeed))
    {
        HI_WARN_PVR("Set the same speed: %d\n", pTrickMode->enSpeed);
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_SUCCESS;
    }

    /* between forward play and  backword play, just notice the event when reach to the start, not switch status  */
    if ((pChnAttr->bEndOfFile)
        && (PVR_IS_PLAY_BACKWARD(pChnAttr->enState, pChnAttr->enLastState))
        && (HI_UNF_PVR_PLAY_STATE_FB == stateToSet))
    {
        HI_INFO_PVR("need not start main rout, state=%d, laststate=%d!\n", stateToSet, pChnAttr->enState);
        PVR_Intf_DoEventCallback(pChnAttr->u32chnID, HI_UNF_PVR_EVENT_PLAY_SOF, 0);
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_SUCCESS;
    }
    else if ((pChnAttr->bEndOfFile)
        && (PVR_IS_PLAY_FORWARD(pChnAttr->enState, pChnAttr->enLastState))
        && ((HI_UNF_PVR_PLAY_STATE_FF == stateToSet)
            || (HI_UNF_PVR_PLAY_STATE_SF == stateToSet)))
    {
        HI_INFO_PVR("need not start main rout, state=%d, laststate=%d!\n", stateToSet, pChnAttr->enState);
        PVR_Intf_DoEventCallback(pChnAttr->u32chnID, HI_UNF_PVR_EVENT_PLAY_EOF, 0);
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_SUCCESS;
    }

    /* record rewrite the file, reset it to start */
    ret = PVRPlayCheckIfTsOverByRec(pChnAttr);
    if (HI_SUCCESS != ret)
    {
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_FAILURE;
    }

    /* state invariable, not need to reset */
    else if (pChnAttr->enState == stateToSet)
    {
        HI_WARN_PVR("State not change:state=%d!\n", pChnAttr->enState);
    }
    /* before and after pause, state invariable, not need to reset */
    else if ((HI_UNF_PVR_PLAY_STATE_PAUSE == pChnAttr->enState)
        && (pChnAttr->enLastState == stateToSet))
    {
        HI_WARN_PVR("State not change before and after pause:state=%d!\n", pChnAttr->enState);
    }
    /* among PLAY/SF/STEPF state, not need to reset */
    else if (((HI_UNF_PVR_PLAY_STATE_PLAY == pChnAttr->enState)
        && (HI_UNF_PVR_PLAY_STATE_SF == stateToSet))
      || ((HI_UNF_PVR_PLAY_STATE_STEPF == pChnAttr->enState)
        && (HI_UNF_PVR_PLAY_STATE_SF == stateToSet)))
    {
        HI_WARN_PVR("Change state from %d to %d!\n", pChnAttr->enState, stateToSet);
    }
    /* before and after pause,among PLAY/SF/STEPF state,not need to reset  */
    else if (((HI_UNF_PVR_PLAY_STATE_PAUSE == pChnAttr->enState)
        && (HI_UNF_PVR_PLAY_STATE_PLAY == pChnAttr->enLastState)
        && (HI_UNF_PVR_PLAY_STATE_SF == stateToSet))
      || ((HI_UNF_PVR_PLAY_STATE_PAUSE == pChnAttr->enState)
        && (HI_UNF_PVR_PLAY_STATE_STEPF == pChnAttr->enLastState)
        && (HI_UNF_PVR_PLAY_STATE_SF == stateToSet)))
    {
        HI_WARN_PVR("Change state from %d to %d, after pause!\n", pChnAttr->enLastState, stateToSet);
    }
    /* other state need to reset the current playing frame */
    else
    {
        HI_WARN_PVR("to reset buffer and player Last state:%d, to set state:%d, play state:%d.\n", pChnAttr->enLastState, stateToSet, pChnAttr->enState);
        ret = PVRPlayResetToCurFrame(pChnAttr->IndexHandle, pChnAttr, stateToSet);
        if (HI_SUCCESS != ret)
        {
            HI_ERR_PVR("reset to current frame failed!\n");
        }
    }

    /* forward or backward trick mode need to set I frame mode */
    if ((HI_UNF_PVR_PLAY_STATE_FF == stateToSet)
        || (HI_UNF_PVR_PLAY_STATE_FB == stateToSet))
    {

        if (HI_UNF_PVR_PLAY_STATE_FB == stateToSet)
        {
            stTPlayOpt.enTplayDirect = HI_UNF_AVPLAY_TPLAY_DIRECT_BACKWARD;
        }
        else
        {
            stTPlayOpt.enTplayDirect = HI_UNF_AVPLAY_TPLAY_DIRECT_FORWARD;
        }

        stTPlayOpt.u32SpeedInteger = abs(pTrickMode->enSpeed/HI_UNF_PVR_PLAY_SPEED_NORMAL);
        stTPlayOpt.u32SpeedDecimal = 0;
        
        ret = HI_UNF_AVPLAY_Tplay(pChnAttr->hAvplay, &stTPlayOpt);
        if (HI_SUCCESS != ret)
        {
            HI_ERR_PVR("AVPLAY Tplay failed!\n");
            PVR_UNLOCK(&(pChnAttr->stMutex));
            return HI_FAILURE;
        }
    }
    else if (HI_UNF_PVR_PLAY_STATE_SF == stateToSet)
    {
        /* set none I frame mode */
        ret = HI_UNF_AVPLAY_SetDecodeMode(pChnAttr->hAvplay, HI_UNF_VCODEC_MODE_NORMAL);
        if (HI_SUCCESS != ret)
        {
            HI_ERR_PVR("set vdec normal mode error!\n");
            PVR_UNLOCK(&(pChnAttr->stMutex));
            return HI_FAILURE;
        }

        /* set frame repeat number */
        stTPlayOpt.enTplayDirect = HI_UNF_AVPLAY_TPLAY_DIRECT_FORWARD;

        stTPlayOpt.u32SpeedInteger = 0;
        stTPlayOpt.u32SpeedDecimal = (pTrickMode->enSpeed *1000) / HI_UNF_PVR_PLAY_SPEED_NORMAL;

        /* sf use the normal play mode */
        ret = HI_UNF_AVPLAY_Tplay(pChnAttr->hAvplay, &stTPlayOpt);
        if (HI_SUCCESS != ret)
        {
            HI_ERR_PVR("AVPLAY resume failed!\n");
            PVR_UNLOCK(&(pChnAttr->stMutex));
            return HI_FAILURE;
        }
    }
    else
    {
        HI_ERR_PVR("NOT support this trick mode.\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_ERR_PVR_NOT_SUPPORT;
    }

    pChnAttr->bQuickUpdateStatus = HI_TRUE;
    pChnAttr->stTplayCtlInfo.u32RefFrmPtsMs = PVR_INDEX_INVALID_PTSMS;
    pChnAttr->enLastState = pChnAttr->enState;
    pChnAttr->enState = stateToSet;
    pChnAttr->enSpeed = pTrickMode->enSpeed;

#ifdef PVR_PROC_SUPPORT
    paPlayProcInfo[u32Chn]->enState = pChnAttr->enState;
    paPlayProcInfo[u32Chn]->enSpeed = pChnAttr->enSpeed;
#endif

    HI_WARN_PVR("set trickMode Ok: speed=%d!\n", pTrickMode->enSpeed);
    PVR_UNLOCK(&(pChnAttr->stMutex));
    return ret;
}

/*****************************************************************************
 Prototype       : HI_PVR_PlaySeek
 Description     : seek to play
 Input           : u32Chn        **channel number
                   pPosition  **the position to play
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/22
    Author       : quyaxin 46153
    Modification : Created function

*****************************************************************************/
HI_S32 HI_PVR_PlaySeek(HI_U32 u32Chn, const HI_UNF_PVR_PLAY_POSITION_S *pPosition)
{
    HI_S32 ret = HI_SUCCESS;
    PVR_PLAY_CHN_S  *pChnAttr;

    PVR_CHECK_POINTER(pPosition);
    PVR_PLAY_CHECK_CHN(u32Chn);
    pChnAttr = &g_stPvrPlayChns[u32Chn];

    PVR_LOCK(&(pChnAttr->stMutex));
    PVR_PLAY_CHECK_CHN_INIT_UNLOCK(pChnAttr);

    /* for scramble stream, not support seek, presently */
    if (!pChnAttr->stUserCfg.bIsClearStream)
    {
        PVR_UNLOCK(&(pChnAttr->stMutex));
        /* TODO: about the scramble stream */
        HI_ERR_PVR("Not support scram ts to seek!\n");
        return HI_ERR_PVR_NOT_SUPPORT;
    }

    if (HI_UNF_PVR_PLAY_POS_TYPE_SIZE == pPosition->enPositionType)
    {
        PVR_UNLOCK(&(pChnAttr->stMutex));
        HI_ERR_PVR("Not support seek by size!\n");
        return HI_ERR_PVR_NOT_SUPPORT;
    }

    if (HI_TRUE == pChnAttr->bPlayingTsNoIdx)
    {
        HI_ERR_PVR("No index file, NOT support seek.\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_ERR_PVR_NOT_SUPPORT;
    }

    if ((pPosition->s32Whence == SEEK_SET) && (pPosition->s64Offset < 0))
    {
        PVR_UNLOCK(&(pChnAttr->stMutex));
        HI_ERR_PVR("seek from start, offset error: %d!\n", pPosition->s64Offset);
        return HI_ERR_PVR_INVALID_PARA;
    }

    if ((pPosition->s32Whence == SEEK_END) && (pPosition->s64Offset > 0))
    {
        PVR_UNLOCK(&(pChnAttr->stMutex));
        HI_ERR_PVR("seek from end, offset error: %d!\n", pPosition->s64Offset);
        return HI_ERR_PVR_INVALID_PARA;
    }

    if ((pPosition->s32Whence == SEEK_CUR) && (pPosition->s64Offset == 0))
    {
        PVR_UNLOCK(&(pChnAttr->stMutex));
        HI_WARN_PVR("seek from current, offset is %lld!\n", pPosition->s64Offset);
        return HI_SUCCESS;
    }

    HI_INFO_PVR("Seek: type:%s, whence:%s, offset:%lld. \n",
        HI_UNF_PVR_PLAY_POS_TYPE_TIME == pPosition->enPositionType ? "TIME" : "Frame",
        WHENCE_STRING(pPosition->s32Whence),
        pPosition->s64Offset);

    if ((HI_UNF_PVR_PLAY_STATE_PLAY == pChnAttr->enState)
        || (HI_UNF_PVR_PLAY_STATE_PAUSE == pChnAttr->enState)
        || (HI_UNF_PVR_PLAY_STATE_FF == pChnAttr->enState)
        || (HI_UNF_PVR_PLAY_STATE_FB == pChnAttr->enState)
        || (HI_UNF_PVR_PLAY_STATE_SF == pChnAttr->enState)
        || (HI_UNF_PVR_PLAY_STATE_STEPF == pChnAttr->enState)
        || (HI_UNF_PVR_PLAY_STATE_STEPB == pChnAttr->enState))
    {
        HI_INFO_PVR("to reset buffer and player.\n");
        pChnAttr->stTplayCtlInfo.u32RefFrmPtsMs = PVR_INDEX_INVALID_PTSMS;

        ret = PVRPlayResetToCurFrame(pChnAttr->IndexHandle, pChnAttr, pChnAttr->enState);
        if (HI_SUCCESS != ret)
        {
            HI_ERR_PVR("reset to current frame failed!\n");
        }
    }

    switch ( pPosition->enPositionType )
    {
        case HI_UNF_PVR_PLAY_POS_TYPE_SIZE:
            /*
            ret = PVR_Index_SeekByByte(pChnAttr->IndexHandle,
                        pPosition->s64Offset, pPosition->s32Whence);
            */
            ret = HI_ERR_PVR_NOT_SUPPORT;
            break;
        case HI_UNF_PVR_PLAY_POS_TYPE_FRAME:
            /*
            if (pPosition->s64Offset > 0x7fffffff)
            {
                ret = HI_ERR_PVR_INVALID_PARA;
            }
            else
            {
                ret = PVR_Index_SeekByFrame2I(pChnAttr->IndexHandle,
                        (HI_S32)pPosition->s64Offset, pPosition->s32Whence);
            }
            */
            ret = HI_ERR_PVR_NOT_SUPPORT;
            break;
        case HI_UNF_PVR_PLAY_POS_TYPE_TIME:
            ret = PVR_Index_SeekByTime(pChnAttr->IndexHandle, pPosition->s64Offset, pPosition->s32Whence);
            if(ret==HI_SUCCESS)
            {
                if(pChnAttr->bEndOfFile==HI_TRUE)
                    pChnAttr->bEndOfFile = HI_FALSE;
            }
            break;
        default:
            ret = HI_ERR_PVR_INVALID_PARA;
    }

    pChnAttr->bQuickUpdateStatus = HI_TRUE;

    HI_INFO_PVR("SEEK OK!\n");
    PVR_UNLOCK(&(pChnAttr->stMutex));

    return ret;
}

/*****************************************************************************
 Prototype       : HI_PVR_PlayStep
 Description     : play by step frame
 Input           : u32Chn        **channel number
                   direction  ** direction:forward or backward. presently, just only support backward.
 Output          : None
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/22
    Author       : quyaxin 46153
    Modification : Created function

*****************************************************************************/
HI_S32 HI_PVR_PlayStep(HI_U32 u32Chn, HI_S32 direction)
{
    HI_S32 ret = HI_SUCCESS;
    PVR_PLAY_CHN_S  *pChnAttr;
    HI_HANDLE hWindow;

    PVR_PLAY_CHECK_CHN(u32Chn);
    pChnAttr = &g_stPvrPlayChns[u32Chn];

    PVR_LOCK(&(pChnAttr->stMutex));
    PVR_PLAY_CHECK_CHN_INIT_UNLOCK(pChnAttr);

    HI_INFO_PVR("PVR step once, channel=%d!\n", u32Chn);

    if (direction < 0)
    {
        HI_ERR_PVR("PVR Play: NOT support step back!\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_ERR_PVR_NOT_SUPPORT;
    }

    if (direction == 0)
    {
        HI_WARN_PVR("PVR Play: step no direction!\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_SUCCESS;
    }

    /* audio type index file, which not support step forward play */
    if (PVR_INDEX_IS_TYPE_AUDIO(pChnAttr->IndexHandle))
    {
        HI_ERR_PVR("audio stream NOT support step play!\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_ERR_PVR_NOT_SUPPORT;
    }

    if ((pChnAttr->bEndOfFile)
        && (PVR_IS_PLAY_FORWARD(pChnAttr->enState, pChnAttr->enLastState))
        && (direction > 0))
    {
        HI_INFO_PVR("till end, need not start main rout, state=%d, laststate=%d!\n",  pChnAttr->enState);
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_SUCCESS;
    }

    /* record rewrite the file, so reset it to the start */
    if (PVR_Index_QureyClearRecReachPlay(pChnAttr->IndexHandle))
    {
        PVR_Index_SeekToStart(pChnAttr->IndexHandle);
        pChnAttr->bTsBufReset = HI_TRUE;
        ret = HI_UNF_DMX_ResetTSBuffer(pChnAttr->hTsBuffer);
        if (HI_SUCCESS != ret)
        {
            HI_ERR_PVR("ts buffer reset failed!\n");
            PVR_UNLOCK(&(pChnAttr->stMutex));
            return HI_FAILURE;
        }

        ret = HI_UNF_AVPLAY_Reset(pChnAttr->hAvplay, NULL);
        if (HI_SUCCESS != ret)
        {
            HI_ERR_PVR("AVPLAY reset failed!\n");
            PVR_UNLOCK(&(pChnAttr->stMutex));
            return HI_FAILURE;
        }
    }
    else if (((HI_UNF_PVR_PLAY_STATE_PLAY != pChnAttr->enState)
        && (HI_UNF_PVR_PLAY_STATE_SF != pChnAttr->enState)
        && (HI_UNF_PVR_PLAY_STATE_PAUSE != pChnAttr->enState)
        && (HI_UNF_PVR_PLAY_STATE_STEPF != pChnAttr->enState))
     || ((HI_UNF_PVR_PLAY_STATE_PAUSE == pChnAttr->enState)
        && (HI_UNF_PVR_PLAY_STATE_PLAY != pChnAttr->enLastState)
        && (HI_UNF_PVR_PLAY_STATE_SF != pChnAttr->enLastState)
        && (HI_UNF_PVR_PLAY_STATE_STEPF != pChnAttr->enLastState)))
    {
        HI_INFO_PVR("to reset buffer and player.\n");
        ret = PVRPlayResetToCurFrame(pChnAttr->IndexHandle, pChnAttr, HI_UNF_PVR_PLAY_STATE_STEPF);
        if (HI_SUCCESS != ret)
        {
            HI_ERR_PVR("reset to current frame failed!\n");
        }
    }

    ret = HI_MPI_AVPLAY_GetWindowHandle(pChnAttr->hAvplay, &hWindow);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("AVPLAY get window handle failed!\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_FAILURE;
    }

    /* the first incoming step play mode, set the player */
    if (HI_UNF_PVR_PLAY_STATE_STEPF != pChnAttr->enState)
    {
        ret = HI_UNF_AVPLAY_SetDecodeMode(pChnAttr->hAvplay, HI_UNF_VCODEC_MODE_NORMAL);
        if (HI_SUCCESS != ret)
        {
            HI_ERR_PVR("set vdec normal mode error!\n");
            PVR_UNLOCK(&(pChnAttr->stMutex));
            return HI_FAILURE;
        }

        /*video trick mode, stop the audio */
        ret = HI_UNF_AVPLAY_Stop(pChnAttr->hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_AUD, NULL);
        if (HI_SUCCESS != ret)
        {
            HI_ERR_PVR("AVPLAY stop audio failed!\n");
            PVR_UNLOCK(&(pChnAttr->stMutex));
            return HI_FAILURE;
        }
#if 0
        /* set window to step mode */
        ret = HI_MPI_VO_SetWindowStepMode(hWindow, HI_TRUE);
        if (HI_SUCCESS != ret)
        {
            HI_ERR_PVR("Set window step mode failed!\n");
            PVR_UNLOCK(&(pChnAttr->stMutex));
            return HI_FAILURE;
        }
#endif
        /* on step mode, resume the player to normal mode */
        ret = HI_UNF_AVPLAY_Resume(pChnAttr->hAvplay, NULL);
        if (ret != HI_SUCCESS)
        {
            HI_ERR_PVR("AVPLAY_Resume failed:%#x\n", ret);
            PVR_UNLOCK(&(pChnAttr->stMutex));
            return HI_FAILURE;
        }

        pChnAttr->bQuickUpdateStatus = HI_TRUE;
        pChnAttr->enLastState = pChnAttr->enState;
        pChnAttr->enState = HI_UNF_PVR_PLAY_STATE_STEPF;

#ifdef PVR_PROC_SUPPORT
        paPlayProcInfo[u32Chn]->enState = pChnAttr->enState;
        paPlayProcInfo[u32Chn]->enSpeed = pChnAttr->enSpeed;
#endif
    }
    
    /* on step mode forward one frame */
    /*HI_MPI_VO_SetWindowStepPlay(hWindow);  */
    ret = HI_UNF_AVPLAY_Step(pChnAttr->hAvplay, NULL);
    if (HI_SUCCESS != ret)
    {
        HI_ERR_PVR("Step window failed!\n");
        PVR_UNLOCK(&(pChnAttr->stMutex));
        return HI_FAILURE;
    }
    
    if (PVRPlayIsEOS(pChnAttr))
    {
        HI_INFO_PVR("till end, need not start main rout, state=%d, laststate=%d!\n",  pChnAttr->enState);
        PVR_Intf_DoEventCallback(pChnAttr->u32chnID, HI_UNF_PVR_EVENT_PLAY_EOF, 0);
    }

    PVR_UNLOCK(&(pChnAttr->stMutex));

    return HI_SUCCESS;
}

HI_S32 HI_PVR_PlayRegisterReadCallBack(HI_U32 u32Chn, ExtraCallBack readCallBack)
{
    PVR_PLAY_CHN_S              *pChnAttr;
    PVR_PLAY_CHECK_CHN(u32Chn);
    pChnAttr = &g_stPvrPlayChns[u32Chn];
    PVR_LOCK(&(pChnAttr->stMutex));
    PVR_PLAY_CHECK_CHN_INIT_UNLOCK(pChnAttr);

    pChnAttr->readCallBack = readCallBack;
    PVR_UNLOCK(&(pChnAttr->stMutex));
    return HI_SUCCESS;
}

HI_S32 HI_PVR_PlayUnRegisterReadCallBack(HI_U32 u32Chn)
{
    PVR_PLAY_CHN_S              *pChnAttr;

    PVR_PLAY_CHECK_CHN(u32Chn);
    pChnAttr = &g_stPvrPlayChns[u32Chn];
    PVR_LOCK(&(pChnAttr->stMutex));
    PVR_PLAY_CHECK_CHN_INIT_UNLOCK(pChnAttr);

    pChnAttr->readCallBack = NULL;

    PVR_UNLOCK(&(pChnAttr->stMutex));
    return HI_SUCCESS;
}

/*****************************************************************************
 Prototype       : HI_PVR_PlayGetStatus
 Description     : get the status of play channel
 Input           : u32Chn      **channel number
 Output          : pStatus  **the status of channel
 Return Value    :
 Global Variable
    Read Only    :
    Read & Write :
  History
  1.Date         : 2008/4/22
    Author       : quyaxin 46153
    Modification : Created function

*****************************************************************************/
HI_S32 HI_PVR_PlayGetStatus(HI_U32 u32Chn, HI_UNF_PVR_PLAY_STATUS_S *pStatus)
{
    PVR_PLAY_CHN_S  *pChnAttr;
    HI_UNF_AVPLAY_STATUS_INFO_S stInfo;
    HI_S32 ret;
    HI_U32 u32SearchPTS;
    HI_UNF_PVR_PLAY_STATE_E enCurState;
    HI_U32 IsForword;
    PVR_INDEX_ENTRY_S    stCurPlayFrame;   /* the current displaying frame info  */
    HI_U32 u32PtsPos;
    HI_U32 u32LoopTime = 0;

    PVR_PLAY_CHECK_CHN(u32Chn);
    pChnAttr = &g_stPvrPlayChns[u32Chn];

    PVR_LOCK(&(pChnAttr->stMutex));
    PVR_PLAY_CHECK_CHN_INIT_UNLOCK(pChnAttr);

    enCurState = pChnAttr->enState;

    if ((enCurState < HI_UNF_PVR_PLAY_STATE_PLAY)
        || (enCurState > HI_UNF_PVR_PLAY_STATE_STEPB)
        || (pChnAttr->bEndOfFile)) /*reach to the start or end of the file, return the previous status */
    {
        memcpy(pStatus, &pChnAttr->stLastStatus, sizeof(HI_UNF_PVR_PLAY_STATUS_S));
        pStatus->enState = pChnAttr->enState;
        pStatus->enSpeed = pChnAttr->enSpeed;
        PVR_UNLOCK(&(pChnAttr->stMutex));

        return HI_SUCCESS;
    }

    if ((HI_UNF_PVR_PLAY_STATE_FB == enCurState)
        || (HI_UNF_PVR_PLAY_STATE_STEPB == enCurState))
    {
        IsForword = 1;
    }
    else
    {
        IsForword = 0;
    }
    PVR_UNLOCK(&(pChnAttr->stMutex));
    do
    {
        ret = HI_MPI_AVPLAY_GetStatusInfo(pChnAttr->hAvplay, &stInfo);
        if (HI_SUCCESS != ret)
        {
            HI_ERR_PVR("HI_MPI_AVPLAY_GetStatusInfo failed!\n");

            return HI_FAILURE;
        }

        if (PVR_INDEX_IS_TYPE_AUDIO(pChnAttr->IndexHandle))
        {
            u32SearchPTS = stInfo.stSyncStatus.u32LocalTime;
            HI_INFO_PVR("====audio cur pts:%d!\n", u32SearchPTS);
        }
        else
        {
            u32SearchPTS = stInfo.stSyncStatus.u32LastVidPts;
            HI_INFO_PVR("====video cur pts:%d!\n", u32SearchPTS);
        }
        u32LoopTime ++;
        if (PVR_INDEX_INVALID_PTSMS == u32SearchPTS)
        {
            if (u32LoopTime > 10)
            {
                break;
            }
            usleep(100*1000);
        }

    } while(PVR_INDEX_INVALID_PTSMS == u32SearchPTS);
    PVR_LOCK(&(pChnAttr->stMutex));

    if (PVR_INDEX_INVALID_PTSMS == u32SearchPTS)
    {
        memcpy(pStatus, &pChnAttr->stLastStatus, sizeof(HI_UNF_PVR_PLAY_STATUS_S));
        pStatus->enState = pChnAttr->enState;
        pStatus->enSpeed = pChnAttr->enSpeed;
    }
    else
    {
        if (HI_SUCCESS == PVR_Index_QueryFrameByPTS(pChnAttr->IndexHandle, u32SearchPTS, &stCurPlayFrame, &u32PtsPos, IsForword))
        {
            pStatus->u32CurPlayFrame = u32PtsPos;
            pStatus->u64CurPlayPos =  stCurPlayFrame.u64Offset;
            pStatus->u32CurPlayTimeInMs = stCurPlayFrame.u32DisplayTimeMs;
        }
        else
        {
            memcpy(pStatus, &pChnAttr->stLastStatus, sizeof(HI_UNF_PVR_PLAY_STATUS_S));
            pStatus->enState = pChnAttr->enState;
            pStatus->enSpeed = pChnAttr->enSpeed;
        }
    }
    pStatus->enState = pChnAttr->enState;
    pStatus->enSpeed = pChnAttr->enSpeed;

    memcpy(&pChnAttr->stLastStatus, pStatus, sizeof(HI_UNF_PVR_PLAY_STATUS_S));

    HI_WARN_PVR("========cur time:%d!\n", pStatus->u32CurPlayTimeInMs);
    PVR_UNLOCK(&(pChnAttr->stMutex));

    return HI_SUCCESS;
}

HI_S32 HI_PVR_PlayGetFileAttr(HI_U32 u32Chn, HI_UNF_PVR_FILE_ATTR_S *pAttr)
{
    HI_S32 ret;
    PVR_PLAY_CHN_S  *pChnAttr;

    PVR_CHECK_POINTER(pAttr);
    PVR_PLAY_CHECK_INIT(&g_stPlayInit);

    PVR_PLAY_CHECK_CHN(u32Chn);
    pChnAttr = &g_stPvrPlayChns[u32Chn];

    PVR_LOCK(&(pChnAttr->stMutex));
    PVR_PLAY_CHECK_CHN_INIT_UNLOCK(pChnAttr);

    ret = PVR_Index_PlayGetFileAttrByFileName(pChnAttr->stUserCfg.szFileName, pAttr);
    PVR_UNLOCK(&(pChnAttr->stMutex));

    return ret;
}

HI_S32 HI_PVR_GetFileAttrByFileName(const HI_CHAR *pFileName, HI_UNF_PVR_FILE_ATTR_S *pAttr)
{
    HI_S32 ret;

    ret = PVR_Index_PlayGetFileAttrByFileName(pFileName, pAttr);
    if (HI_SUCCESS != ret)
    {
        return ret;
    }

    return HI_SUCCESS;
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
