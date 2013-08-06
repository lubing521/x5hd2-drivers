/******************************************************************************

  Copyright (C), 2001-2011, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : drv_disp_adp2unf.c
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2012/09/20
  Description   :
  History       :
  1.Date        : 
  Author        : 
  Modification  : Created file

*******************************************************************************/

#include "hi_unf_disp.h"
#include "hi_drv_video.h"
#include "hi_drv_disp.h"
#include "mpi_disp_tran.h"

#include "hi_drv_pdm.h"
#include "drv_pdm_ext.h"
#include "drv_module_ext.h"

#include "drv_display.h"
#include "drv_disp_com.h"


#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

HI_S32 DISP_PrintParam(HI_UNF_DISP_E enDisp, HI_DISP_PARAM_S *pP)
{
    HI_S32 i;
    
    DISP_PRINT("DISP%d PDM Param:\n", enDisp);

    DISP_PRINT("Fmt=%d, B=%d, C=%d, S=%d,H=%d,GM=%d\n", 
                pP->enFormat,
                pP->u32Brightness,
                pP->u32Contrast,
                pP->u32Saturation,
                pP->u32HuePlus,
                pP->bGammaEnable);
#if 0
    DISP_PRINT("x=%d, y=%d, w=%d, h=%d\n", 
                pP->u32ScreenXpos,
                pP->u32ScreenYpos,
                pP->u32ScreenWidth,
                pP->u32ScreenHeight);

    DISP_PRINT("BGC=%d, =%d, =%d, AR=%d,WvsH=%dvs%d\n", 
                pP->stBgColor.u8Red,
                pP->stBgColor.u8Green,
                pP->stBgColor.u8Blue,
                pP->stAspectRatio.enDispAspectRatio,
                pP->stAspectRatio.u32UserAspectWidth,
                pP->stAspectRatio.u32UserAspectHeight);
#endif

    for(i=0; i<HI_UNF_DISP_INTF_TYPE_BUTT; i++)
    {
        //DISP_PRINT("INTF %d, type=%d : ", i, pP->stIntf[i].enIntfType);
        if (pP->stIntf[i].enIntfType == HI_UNF_DISP_INTF_TYPE_HDMI)
        {
            DISP_PRINT("HDMI ID=%d\n", pP->stIntf[i].enIntfType);
        }
        else if (pP->stIntf[i].enIntfType == HI_UNF_DISP_INTF_TYPE_YPBPR)
        {
            DISP_PRINT("Y=%d, Pb=%d, Pr=%d\n", 
                        pP->stIntf[i].unIntf.stYPbPr.u8DacY,
                        pP->stIntf[i].unIntf.stYPbPr.u8DacPb,
                        pP->stIntf[i].unIntf.stYPbPr.u8DacPr);
        }
        else if (pP->stIntf[i].enIntfType == HI_UNF_DISP_INTF_TYPE_CVBS)
        {
            DISP_PRINT("CVBS=%d\n", 
                        pP->stIntf[i].unIntf.stCVBS.u8Dac);
        }
        else
        {
            //DISP_PRINT("NULL\n");
        }
        
    }
#if 0
    DISP_PRINT("TIMING===============================\n");
    DISP_PRINT(" VFB=%d,VBB=%d,VACT=%d\n HFB=%d,HBB=%d,HACT=%d\n VPW=%d,HPW=%d\n IDV=%d,IHS=%d,IVS=%d\n",
            pP->stDispTiming.VFB,
            pP->stDispTiming.VBB,
            pP->stDispTiming.VACT,
            pP->stDispTiming.HFB,
            pP->stDispTiming.HBB,
            pP->stDispTiming.HACT,
            pP->stDispTiming.VPW,
            pP->stDispTiming.HPW,
            pP->stDispTiming.IDV,
            pP->stDispTiming.IHS,
            pP->stDispTiming.IVS);
#endif

    return HI_SUCCESS;
}



HI_S32 DISP_CheckParam(HI_UNF_DISP_E enDisp, HI_DISP_PARAM_S *pP)
{
    HI_S32 i;

    //DISP_PrintParam(enDisp, pP);

    if (enDisp == HI_UNF_DISPLAY1)
    {
        if (pP->enFormat >= HI_UNF_ENC_FMT_BUTT)
        {
            DISP_ERROR("invalid enformt!\n");
            return HI_FAILURE;
        }

        for(i=0; i < HI_UNF_DISP_INTF_TYPE_BUTT; i++)
        {   
            if (pP->stIntf[i].enIntfType != HI_UNF_DISP_INTF_TYPE_BUTT)
            {
                if (pP->stIntf[i].enIntfType == HI_UNF_DISP_INTF_TYPE_YPBPR)
                {
                    if (   ( pP->stIntf[i].unIntf.stYPbPr.u8DacY > 3)
                        || ( pP->stIntf[i].unIntf.stYPbPr.u8DacPb > 3)
                        || ( pP->stIntf[i].unIntf.stYPbPr.u8DacPr > 3)
                        || ( pP->stIntf[i].unIntf.stYPbPr.u8DacY == pP->stIntf[i].unIntf.stYPbPr.u8DacPb)
                        || ( pP->stIntf[i].unIntf.stYPbPr.u8DacY == pP->stIntf[i].unIntf.stYPbPr.u8DacPr)
                        || ( pP->stIntf[i].unIntf.stYPbPr.u8DacPb == pP->stIntf[i].unIntf.stYPbPr.u8DacPr)
                       )
                    {
                        DISP_ERROR("invalid vadc id!\n");
                        return HI_FAILURE;
                    }
                }
            }
        }

    }
    else
    {
        //HI_UNF_DISPLAY1
        if ( (pP->enFormat > HI_UNF_ENC_FMT_SECAM_COS) || (pP->enFormat < HI_UNF_ENC_FMT_PAL) )
        {
            DISP_ERROR("invalid enformt!\n");
            return HI_FAILURE;
        }

        for(i=0; i < HI_UNF_DISP_INTF_TYPE_BUTT; i++)
        {   
            if (pP->stIntf[i].enIntfType != HI_UNF_DISP_INTF_TYPE_BUTT)
            {
                if (pP->stIntf[i].enIntfType == HI_UNF_DISP_INTF_TYPE_CVBS)
                {
                    if( pP->stIntf[i].unIntf.stCVBS.u8Dac > 3)
                    {
                        DISP_ERROR("invalid vadc id!\n");
                        return HI_FAILURE;
                    }
                }
            }
        }

    }

    if (  (pP->u32Brightness > 100 )
        ||(pP->u32Contrast > 100) 
        ||(pP->u32Saturation > 100) 
        ||(pP->u32HuePlus > 100) )
    {
        DISP_ERROR("invalid color param!\n");
        return HI_FAILURE;
    }

/*
    pP->bGammaEnable; 
    pP->u32ScreenXpos;
    pP->u32ScreenYpos;
    pP->u32ScreenWidth;
    pP->u32ScreenHeight; 
    pP->stBgColor;
*/
    if (pP->stAspectRatio.enDispAspectRatio > HI_UNF_DISP_ASPECT_RATIO_USER)
    {
        DISP_ERROR("invalid aspect ratio param!\n");
        return HI_FAILURE;
    }

    if (pP->stAspectRatio.enDispAspectRatio == HI_UNF_DISP_ASPECT_RATIO_USER)
    {
        if (  (pP->stAspectRatio.u32UserAspectWidth > (pP->stAspectRatio.u32UserAspectHeight * 16))
            || (pP->stAspectRatio.u32UserAspectHeight > (pP->stAspectRatio.u32UserAspectWidth * 16)))
        {
            DISP_ERROR("invalid aspect ratio param!\n");
            return HI_FAILURE;
        }
    }
   
    //pP->stDispTiming;

    return HI_SUCCESS;
}


HI_S32 HI_PDM_GetDispParamTEST(HI_UNF_DISP_E enDisp, HI_DISP_PARAM_S *pP)
{
    HI_S32 i;
    
    if (enDisp == HI_UNF_DISPLAY1)
    {
        pP->enFormat = HI_UNF_ENC_FMT_1080i_50;   
        pP->u32Brightness = 50;
        pP->u32Contrast   = 50;
        pP->u32Saturation = 50;
        pP->u32HuePlus    = 50;
        pP->bGammaEnable  = HI_FALSE; 
        pP->u32ScreenXpos = 0;
        pP->u32ScreenYpos = 0;
        pP->u32ScreenWidth  = 0;
        pP->u32ScreenHeight = 0; 
        pP->stBgColor.u8Red   = 0;
        pP->stBgColor.u8Green = 0;
        pP->stBgColor.u8Blue  = 0;
        pP->stAspectRatio.enDispAspectRatio = HI_UNF_DISP_ASPECT_RATIO_16TO9;

        pP->stIntf[0].enIntfType = HI_UNF_DISP_INTF_TYPE_YPBPR;
        pP->stIntf[0].unIntf.stYPbPr.u8DacY  = 0;
        pP->stIntf[0].unIntf.stYPbPr.u8DacPb = 1;
        pP->stIntf[0].unIntf.stYPbPr.u8DacPr = 3;

        pP->stIntf[1].enIntfType = HI_UNF_DISP_INTF_TYPE_HDMI;
        pP->stIntf[1].unIntf.enHdmi = HI_UNF_HDMI_ID_0;
        
        for(i=1; i<HI_UNF_DISP_INTF_TYPE_BUTT; i++)
        {
            pP->stIntf[i].enIntfType = HI_UNF_DISP_INTF_TYPE_BUTT;
        }
        //pP->stDispTiming;
    }
    else
    {
        //HI_UNF_DISPLAY1
        pP->enFormat = HI_UNF_ENC_FMT_PAL;   
        pP->u32Brightness = 50;
        pP->u32Contrast   = 50;
        pP->u32Saturation = 50;
        pP->u32HuePlus    = 50;
        pP->bGammaEnable  = HI_FALSE; 
        pP->u32ScreenXpos = 0;
        pP->u32ScreenYpos = 0;
        pP->u32ScreenWidth  = 0;
        pP->u32ScreenHeight = 0; 
        pP->stBgColor.u8Red   = 0;
        pP->stBgColor.u8Green = 0;
        pP->stBgColor.u8Blue  = 0;
        pP->stAspectRatio.enDispAspectRatio = HI_UNF_DISP_ASPECT_RATIO_4TO3;

        pP->stIntf[0].enIntfType = HI_UNF_DISP_INTF_TYPE_CVBS;
        pP->stIntf[0].unIntf.stCVBS.u8Dac = 2;
        
        for(i=1; i<HI_UNF_DISP_INTF_TYPE_BUTT; i++)
        {
            pP->stIntf[i].enIntfType = HI_UNF_DISP_INTF_TYPE_BUTT;
        }
    }

    return HI_SUCCESS;
}

HI_S32 HI_UNF_DISP_Init(HI_VOID)
{
    HI_S32 nRet;
    // disp init
    nRet = DISP_Init();

    return nRet;
}

HI_S32 HI_UNF_DISP_DeInit(HI_VOID)
{
    //HI_S32 nRet;

    // disp deinit
    DISP_DeInit();
    return HI_SUCCESS;
}

HI_S32 HI_UNF_DISP_Open (HI_UNF_DISP_E enDisp)
{
    HI_S32 nRet;

    nRet = DISP_Open((HI_DRV_DISPLAY_E)(enDisp));

    return nRet;
}

HI_S32 HI_UNF_DISP_Close(HI_UNF_DISP_E enDisp)
{
    HI_S32 nRet;

    nRet =  DISP_Close((HI_DRV_DISPLAY_E)(enDisp));
    return nRet;
}


HI_DRV_DISP_FMT_E DISP_TVHDFmtU2V(HI_UNF_ENC_FMT_E U)
{
    if (U <= HI_UNF_ENC_FMT_480P_60)
    {
        return (HI_DRV_DISP_FMT_E)(HI_DRV_DISP_FMT_1080P_60 + (U - HI_UNF_ENC_FMT_1080P_60));
    }
    else
    {
        return HI_DRV_DISP_FMT_1080i_50;
    }
}



HI_DRV_DISP_FMT_E DISP_TVSDFmtU2V(HI_UNF_ENC_FMT_E U)
{
    switch (U)
    {
        case HI_UNF_ENC_FMT_PAL:
            return HI_DRV_DISP_FMT_PAL;
        case HI_UNF_ENC_FMT_PAL_N:
            return HI_DRV_DISP_FMT_PAL_N;
        case HI_UNF_ENC_FMT_PAL_Nc:
            return HI_DRV_DISP_FMT_PAL_Nc;
        case HI_UNF_ENC_FMT_NTSC:
            return HI_DRV_DISP_FMT_NTSC;
        case HI_UNF_ENC_FMT_NTSC_J:
            return HI_DRV_DISP_FMT_NTSC_J;
        case HI_UNF_ENC_FMT_NTSC_PAL_M:
            return HI_DRV_DISP_FMT_PAL_M;
        case HI_UNF_ENC_FMT_SECAM_SIN:
            return HI_DRV_DISP_FMT_SECAM_SIN;
        case HI_UNF_ENC_FMT_SECAM_COS:
            return HI_DRV_DISP_FMT_SECAM_COS;
        default:
            return HI_DRV_DISP_FMT_PAL;
    }
}

HI_DRV_DISP_FMT_E DISP_3DFmtU2V(HI_UNF_ENC_FMT_E U)
{
    if (U <= HI_UNF_ENC_FMT_720P_50_FRAME_PACKING)
    {
        return (HI_DRV_DISP_FMT_E)(HI_DRV_DISP_FMT_1080P_24_FP + (U - HI_UNF_ENC_FMT_1080P_24_FRAME_PACKING));
    }
    else
    {
        return HI_DRV_DISP_FMT_1080P_24_FP;
    }
}

HI_DRV_DISP_FMT_E DISP_DVIFmtU2V(HI_UNF_ENC_FMT_E U)
{
    if (U <= HI_UNF_ENC_FMT_VESA_2048X1152_60)
    {
        return (HI_DRV_DISP_FMT_E)(HI_DRV_DISP_FMT_861D_640X480_60 + (U - HI_UNF_ENC_FMT_861D_640X480_60));
    }
    else
    {
        return HI_DRV_DISP_FMT_861D_640X480_60;
    }
}

HI_DRV_DISP_FMT_E DISP_GetEncFmt(HI_UNF_ENC_FMT_E enUnFmt)
{
    if (enUnFmt <= HI_UNF_ENC_FMT_480P_60)
    {
        return DISP_TVHDFmtU2V(enUnFmt);
    }
    else if (enUnFmt <= HI_UNF_ENC_FMT_SECAM_COS)
    {
        return DISP_TVSDFmtU2V(enUnFmt);
    }
    else if (enUnFmt <= HI_UNF_ENC_FMT_720P_50_FRAME_PACKING)
    {
        return DISP_3DFmtU2V(enUnFmt);
    }
    else if (enUnFmt <= HI_UNF_ENC_FMT_VESA_2048X1152_60)
    {
        return DISP_DVIFmtU2V(enUnFmt);
    }
    else if (enUnFmt == HI_UNF_ENC_FMT_BUTT)
    {
        return HI_DRV_DISP_FMT_CUSTOM;
        return HI_SUCCESS;
    }
    else
    {
        return HI_DRV_DISP_FMT_PAL;
    }
}

HI_U8 DISP_GetVdacIdFromPinIDForMPW(HI_U8 PinId)
{
    switch (PinId)
    {
        case 0:
            return (HI_U8)0;
        case 1:
            return (HI_U8)1;
        case 2:
            return (HI_U8)2;
        case 3:
            return (HI_U8)3;
        default:
            return (HI_U8)0xff;
    }
}


HI_S32 DISP_GetDrvIntf(HI_UNF_DISP_INTF_S *pU, HI_DRV_DISP_INTF_S *pM, HI_BOOL bu2m)
{
    if (bu2m)
    {
        /* set invalid value */
        pM->u8VDAC_Y_G  = HI_DISP_VDAC_INVALID_ID;
        pM->u8VDAC_Pb_B = HI_DISP_VDAC_INVALID_ID;
        pM->u8VDAC_Pr_R = HI_DISP_VDAC_INVALID_ID;

        switch (pU->enIntfType)
        {
            case HI_UNF_DISP_INTF_TYPE_HDMI:
                pM->eID = HI_DRV_DISP_INTF_HDMI0 + (pU->unIntf.enHdmi - HI_UNF_HDMI_ID_0);
                if (pM->eID > HI_DRV_DISP_INTF_HDMI2)
                {
                    return HI_FAILURE;
                }
                break;
            case HI_UNF_DISP_INTF_TYPE_YPBPR:
                pM->eID = HI_DRV_DISP_INTF_YPBPR0;
                pM->u8VDAC_Y_G  = DISP_GetVdacIdFromPinIDForMPW(pU->unIntf.stYPbPr.u8DacY);
                pM->u8VDAC_Pb_B = DISP_GetVdacIdFromPinIDForMPW(pU->unIntf.stYPbPr.u8DacPb);
                pM->u8VDAC_Pr_R = DISP_GetVdacIdFromPinIDForMPW(pU->unIntf.stYPbPr.u8DacPr);
                DISP_INFO("=================Y=%d,Pb=%d, Pr=%d\n", 
                       pM->u8VDAC_Y_G, pM->u8VDAC_Pb_B, pM->u8VDAC_Pr_R);
                break;
            case HI_UNF_DISP_INTF_TYPE_SVIDEO:
                pM->eID = HI_DRV_DISP_INTF_SVIDEO0;
                pM->u8VDAC_Y_G  = DISP_GetVdacIdFromPinIDForMPW(pU->unIntf.stSVideo.u8DacY);
                pM->u8VDAC_Pb_B = DISP_GetVdacIdFromPinIDForMPW(pU->unIntf.stSVideo.u8DacC);
                break;
            case HI_UNF_DISP_INTF_TYPE_CVBS:
                pM->eID = HI_DRV_DISP_INTF_CVBS0;
                pM->u8VDAC_Y_G  = DISP_GetVdacIdFromPinIDForMPW(pU->unIntf.stCVBS.u8Dac);
                DISP_INFO("=================CVBS=%d\n", 
                       pM->u8VDAC_Y_G);
                break;
/*
            case HI_UNF_DISP_INTF_TYPE_LCD:
                pM->eID =  + (pU->unIntf.enLCD - );
                if (pM->eID > )
                {
                    return HI_FAILURE;
                }
                break;
            case HI_UNF_DISP_INTF_TYPE_BT1120:
                pM->eID =  + (pU->unIntf.enHDMI - );
                if (pM->eID > )
                {
                    return HI_FAILURE;
                }
                break;
            case HI_UNF_DISP_INTF_TYPE_BT656:
                pM->eID =  + (pU->unIntf.enHDMI - );
                if (pM->eID > )
                {
                    return HI_FAILURE;
                }
                break;
            case HI_UNF_DISP_INTF_TYPE_RGB:
                pM->eID =  + (pU->unIntf.enHDMI - );
                if (pM->eID > )
                {
                    return HI_FAILURE;
                }
                break;
*/
            default:
                return HI_FAILURE;
        }
    }

    return HI_SUCCESS;
}



#define DISP_VERSION_HI3716CV200_MPW 0x20130417ul

HI_S32 DispGetInitParam(HI_DRV_DISPLAY_E enDisp, HI_DRV_DISP_INIT_PARAM_S *pstSetting)
{
    HI_DISP_PARAM_S stDispParam, *pstDispParam;
    PDM_EXPORT_FUNC_S *pst_PDMFunc;
    HI_UNF_DISP_E enUnfDisp;
    HI_S32 i, j, nRet;
    
    if (enDisp > HI_DRV_DISPLAY_1)
    {
        return HI_FAILURE;
    }
    else
    {
        // get PDM data
#if 1
        nRet = HI_DRV_MODULE_GetFunction(HI_ID_PDM, (HI_VOID**)&pst_PDMFunc);
        if (nRet || !pst_PDMFunc->pfnPDM_GetDispParam)
        {
            DISP_ERROR("DISP_get PDM funt failed!");
            return HI_FAILURE;;
        }

        enUnfDisp = (HI_DRV_DISPLAY_0 == enDisp) ? HI_UNF_DISPLAY0 : HI_UNF_DISPLAY1;
        nRet = pst_PDMFunc->pfnPDM_GetDispParam(enUnfDisp, &stDispParam);
        nRet |= DISP_CheckParam(enUnfDisp, &stDispParam);
        if (nRet)
        {
            DISP_ERROR("DISP0 Param invalid!\n");
            return HI_FAILURE;
        }
#else
        enUnfDisp = (HI_DRV_DISPLAY_0 == enDisp) ? HI_UNF_DISPLAY0 : HI_UNF_DISPLAY1;

        nRet = HI_PDM_GetDispParamTEST(enUnfDisp, &stDispParam);
        nRet |= DISP_CheckParam(enUnfDisp, &stDispParam);
        if (nRet)
        {
            DISP_INFO("DISP0 Param invalid!\n");
            return HI_FAILURE;
        }


#endif

    }

    pstDispParam = &stDispParam;

    pstSetting->u32Version = DISP_VERSION_HI3716CV200_MPW;
    pstSetting->bSelfStart = HI_TRUE;
    pstSetting->enFormat   = DISP_GetEncFmt(pstDispParam->enFormat);

    if (enDisp == HI_DRV_DISPLAY_1)
    {
        pstSetting->bIsMaster = HI_TRUE;
        //pstSetting->bIsMaster = HI_FALSE;
        pstSetting->bIsSlave  = HI_FALSE;
        pstSetting->enAttachedDisp = HI_DRV_DISPLAY_0;
    }
    else
    {
        pstSetting->bIsMaster = HI_FALSE;
        pstSetting->bIsSlave  = HI_TRUE;
        //pstSetting->bIsSlave  = HI_FALSE;
        pstSetting->enAttachedDisp = HI_DRV_DISPLAY_1;
    }
    pstSetting->u32Brightness = pstDispParam->u32Brightness;
    pstSetting->u32Contrast   = pstDispParam->u32Contrast;
    pstSetting->u32Saturation = pstDispParam->u32Saturation;
    pstSetting->u32HuePlus    = pstDispParam->u32HuePlus;
    pstSetting->bGammaEnable  = pstDispParam->bGammaEnable; 
    pstSetting->u32ScreenXpos   = pstDispParam->u32ScreenXpos;
    pstSetting->u32ScreenYpos   = pstDispParam->u32ScreenYpos;
    pstSetting->u32ScreenWidth  = pstDispParam->u32ScreenWidth;
    pstSetting->u32ScreenHeight = pstDispParam->u32ScreenHeight; 
    pstSetting->stBgColor.u8Red   = pstDispParam->stBgColor.u8Red;
    pstSetting->stBgColor.u8Green = pstDispParam->stBgColor.u8Green;
    pstSetting->stBgColor.u8Blue  = pstDispParam->stBgColor.u8Blue;

    switch(pstDispParam->stAspectRatio.enDispAspectRatio)
    {
        case HI_UNF_DISP_ASPECT_RATIO_4TO3:
            pstSetting->bCustomRatio = HI_TRUE;
            pstSetting->u32CustomRatioWidth  = 4;
            pstSetting->u32CustomRatioHeight = 3;
            break;
        case HI_UNF_DISP_ASPECT_RATIO_16TO9:
            pstSetting->bCustomRatio = HI_TRUE;
            pstSetting->u32CustomRatioWidth  = 16;
            pstSetting->u32CustomRatioHeight = 9;
            break;
        case HI_UNF_DISP_ASPECT_RATIO_221TO1:
            pstSetting->bCustomRatio = HI_TRUE;
            pstSetting->u32CustomRatioWidth  = 221;
            pstSetting->u32CustomRatioHeight = 100;
            break;
        case HI_UNF_DISP_ASPECT_RATIO_USER:
            pstSetting->bCustomRatio = HI_TRUE;
            pstSetting->u32CustomRatioWidth  = pstDispParam->stAspectRatio.u32UserAspectWidth;
            pstSetting->u32CustomRatioHeight = pstDispParam->stAspectRatio.u32UserAspectHeight;
            break;
        case HI_UNF_DISP_ASPECT_RATIO_AUTO:
        default:
            pstSetting->bCustomRatio = HI_FALSE;
            pstSetting->u32CustomRatioWidth  = 0;
            pstSetting->u32CustomRatioHeight = 0;
            break;
    }

    for(i=0, j=0; i<HI_UNF_DISP_INTF_TYPE_BUTT; i++)
    {
        //printk(" i=%d,  intf type=%d\n", i, pstDispParam->stIntf[i].enIntfType);
        if (pstDispParam->stIntf[i].enIntfType < HI_UNF_DISP_INTF_TYPE_BUTT)
        {
            DISP_GetDrvIntf(&(pstDispParam->stIntf[i]), &pstSetting->stIntf[j], HI_TRUE);
            j++;
            //printk("================= i=%d,j=%d\n", i, j);
        }
    }

    for(; j<HI_DRV_DISP_INTF_ID_MAX; j++)
    {
        pstSetting->stIntf[j].eID = HI_DRV_DISP_INTF_ID_MAX;
    }


    //pstSetting->stDispTiming; 

    return HI_SUCCESS;
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif