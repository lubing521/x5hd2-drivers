/******************************************************************************

  Copyright (C), 2001-2011, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : hi_drv_disp.h
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2009/12/15
  Description   :
  History       :
  1.Date        : 2009/12/15
    Author      : w58735
    Modification: Created file

******************************************************************************/
#ifndef __HI_DRV_DISP_H__
#define __HI_DRV_DISP_H__

#include "hi_type.h"
#include "hi_common.h"
#include "hi_debug.h"
#include "hi_drv_video.h"


#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif
#ifndef CONFIG_SUPPORT_CA_RELEASE
#define HI_FATAL_DISP(fmt...) \
            HI_FATAL_PRINT(HI_ID_DISP, fmt)

#define HI_ERR_DISP(fmt...) \
            HI_ERR_PRINT(HI_ID_DISP, fmt)

#define HI_WARN_DISP(fmt...) \
            HI_WARN_PRINT(HI_ID_DISP, fmt)

#define HI_INFO_DISP(fmt...) \
            HI_INFO_PRINT(HI_ID_DISP, fmt)
#else

#define HI_FATAL_DISP(fmt...)
#define HI_ERR_DISP(fmt...)
#define HI_WARN_DISP(fmt...)
#define HI_INFO_DISP(fmt...) 
#endif


/* display ID */
typedef enum hiDRV_DISPLAY_E
{
    HI_DRV_DISPLAY_0 = 0,
    HI_DRV_DISPLAY_1,
    HI_DRV_DISPLAY_2,
    HI_DRV_DISPLAY_BUTT
}HI_DRV_DISPLAY_E;

typedef enum hiDRV_DISP_LAYER_E
{
    HI_DRV_DISP_LAYER_NONE = 0,
    HI_DRV_DISP_LAYER_VIDEO0,
    HI_DRV_DISP_LAYER_VIDEO1,
    HI_DRV_DISP_LAYER_VIDEO2,

    HI_DRV_DISP_LAYER_GFX0,
    HI_DRV_DISP_LAYER_GFX1,
    HI_DRV_DISP_LAYER_GFX2,
    HI_DRV_DISP_LAYER_GFX3,
    
    HI_DRV_DISP_LAYER_BUTT
}HI_DRV_DISP_LAYER_E;


typedef enum hiDRV_DISP_STEREO_MODE_E
{
    HI_DRV_DISP_STEREO_NONE = 0, /* 2D Mode */
    HI_DRV_DISP_STEREO_FRAME_PACKING,
    HI_DRV_DISP_STEREO_SBS_HALF,
    HI_DRV_DISP_STEREO_TAB,
    HI_DRV_DISP_STEREO_FIELD_ALT,
    HI_DRV_DISP_STEREO_LINE_ALT,
    HI_DRV_DISP_STEREO_SBS_FULL,
    HI_DRV_DISP_STEREO_L_DEPTH,
    HI_DRV_DISP_STEREO_L_DEPTH_G_DEPTH,
    HI_DRV_DISP_STEREO_MODE_BUTT
}HI_DRV_DISP_STEREO_MODE_E;

typedef enum hiDRV_DISP_ZORDER_E
{
    HI_DRV_DISP_ZORDER_MOVETOP = 0,
    HI_DRV_DISP_ZORDER_MOVEUP,
    HI_DRV_DISP_ZORDER_MOVEBOTTOM,
    HI_DRV_DISP_ZORDER_MOVEDOWN,
    HI_DRV_DISP_ZORDER_BUTT
}HI_DRV_DISP_ZORDER_E;

typedef enum hiDRV_DISP_FMT_E
{
    HI_DRV_DISP_FMT_1080P_60 = 0,         /**<1080p 60 Hz*/
    HI_DRV_DISP_FMT_1080P_50,         /**<1080p 50 Hz*/
    HI_DRV_DISP_FMT_1080P_30,         /**<1080p 30 Hz*/
    HI_DRV_DISP_FMT_1080P_25,         /**<1080p 25 Hz*/
    HI_DRV_DISP_FMT_1080P_24,         /**<1080p 24 Hz*/

    HI_DRV_DISP_FMT_1080i_60,         /**<1080i 60 Hz*/
    HI_DRV_DISP_FMT_1080i_50,         /**<1080i 60 Hz*/

    HI_DRV_DISP_FMT_720P_60,          /**<720p 60 Hz*/
    HI_DRV_DISP_FMT_720P_50,          /**<720p 50 Hz */

    HI_DRV_DISP_FMT_576P_50,          /**<576p 50 Hz*/
    HI_DRV_DISP_FMT_480P_60,          /**<480p 60 Hz*/

    HI_DRV_DISP_FMT_PAL,              /* B D G H I PAL */
    HI_DRV_DISP_FMT_PAL_B,            /* B PAL�� Australia */
    HI_DRV_DISP_FMT_PAL_B1,           /* B1 PAL, Hungary */
    HI_DRV_DISP_FMT_PAL_D,            /* D PAL, China */
    HI_DRV_DISP_FMT_PAL_D1,           /* D1 PAL�� Poland */
    HI_DRV_DISP_FMT_PAL_G,            /* G PAL�� Europe */
    HI_DRV_DISP_FMT_PAL_H,            /* H PAL�� Europe */
    HI_DRV_DISP_FMT_PAL_K,            /* K PAL�� Europe */
    HI_DRV_DISP_FMT_PAL_I,            /* I PAL��U.K. */
    HI_DRV_DISP_FMT_PAL_N,            /* N PAL, Jamaica/Uruguay */
    HI_DRV_DISP_FMT_PAL_Nc,           /* Nc PAL, Argentina */

    HI_DRV_DISP_FMT_PAL_M,            /* M PAL, 525 lines */
    HI_DRV_DISP_FMT_PAL_60,           /* 60 PAL */
    HI_DRV_DISP_FMT_NTSC,             /* (M)NTSC       */
    HI_DRV_DISP_FMT_NTSC_J,           /* NTSC-J        */
    HI_DRV_DISP_FMT_NTSC_443,          /* (M)PAL        */

    HI_DRV_DISP_FMT_SECAM_SIN,      /**< SECAM_SIN*/
    HI_DRV_DISP_FMT_SECAM_COS,      /**< SECAM_COS*/
    HI_DRV_DISP_FMT_SECAM_L,        /**< France*/
    HI_DRV_DISP_FMT_SECAM_B,        /**< Middle East*/
    HI_DRV_DISP_FMT_SECAM_G,        /**< Middle East*/
    HI_DRV_DISP_FMT_SECAM_D,        /**< Eastern Europe*/
    HI_DRV_DISP_FMT_SECAM_K,        /**< Eastern Europe*/
    HI_DRV_DISP_FMT_SECAM_H,        /**< Line SECAM*/

    HI_DRV_DISP_FMT_1440x576i_50,
    HI_DRV_DISP_FMT_1440x480i_60,

    HI_DRV_DISP_FMT_1080P_24_FP,
    HI_DRV_DISP_FMT_720P_60_FP,
    HI_DRV_DISP_FMT_720P_50_FP,

    HI_DRV_DISP_FMT_861D_640X480_60,
    HI_DRV_DISP_FMT_VESA_800X600_60,
    HI_DRV_DISP_FMT_VESA_1024X768_60,
    HI_DRV_DISP_FMT_VESA_1280X720_60,
    HI_DRV_DISP_FMT_VESA_1280X800_60,
    HI_DRV_DISP_FMT_VESA_1280X1024_60,
    HI_DRV_DISP_FMT_VESA_1360X768_60,         //Rowe
    HI_DRV_DISP_FMT_VESA_1366X768_60,
    HI_DRV_DISP_FMT_VESA_1400X1050_60,        //Rowe
    HI_DRV_DISP_FMT_VESA_1440X900_60,
    HI_DRV_DISP_FMT_VESA_1440X900_60_RB,
    HI_DRV_DISP_FMT_VESA_1600X900_60_RB,
    HI_DRV_DISP_FMT_VESA_1600X1200_60,
    HI_DRV_DISP_FMT_VESA_1680X1050_60,       //Rowe
    HI_DRV_DISP_FMT_VESA_1920X1080_60,
    HI_DRV_DISP_FMT_VESA_1920X1200_60,
    HI_DRV_DISP_FMT_VESA_2048X1152_60,

    //ADD
    HI_DRV_DISP_FMT_VESA_2560X1600_60,
    HI_DRV_DISP_FMT_xxxx_3840X2160_60,

    //ADD
    HI_DRV_DISP_FMT_CUSTOM,

    HI_DRV_DISP_FMT_BUTT
}HI_DRV_DISP_FMT_E;

typedef enum hiDRV_DISP_VDAC_SIGNAL_E
{
    HI_DRV_DISP_VDAC_NONE = 0,
    HI_DRV_DISP_VDAC_CVBS,
    HI_DRV_DISP_VDAC_Y,
    HI_DRV_DISP_VDAC_PB,
    HI_DRV_DISP_VDAC_PR,
    HI_DRV_DISP_VDAC_SV_Y,
    HI_DRV_DISP_VDAC_SV_C,
    HI_DRV_DISP_VDAC_R,
    HI_DRV_DISP_VDAC_G,
    HI_DRV_DISP_VDAC_B,
    HI_DRV_DISP_VDAC_G_NOSYNC,
    HI_DRV_DISP_VDAC_SIGNAL_BUTT
}HI_DRV_DISP_VDAC_SIGNAL_E;

typedef enum hiDRV_DISP_INTF_ID_E
{
    HI_DRV_DISP_INTF_YPBPR0 = 0,
    HI_DRV_DISP_INTF_SVIDEO0,
    HI_DRV_DISP_INTF_CVBS0,
    HI_DRV_DISP_INTF_VGA0,

    HI_DRV_DISP_INTF_HDMI0,
    HI_DRV_DISP_INTF_HDMI1,
    HI_DRV_DISP_INTF_HDMI2,

    HI_DRV_DISP_INTF_BT656_0,
    HI_DRV_DISP_INTF_BT656_1,
    HI_DRV_DISP_INTF_BT656_2,

    HI_DRV_DISP_INTF_BT1120_0,
    HI_DRV_DISP_INTF_BT1120_1,
    HI_DRV_DISP_INTF_BT1120_2,

    HI_DRV_DISP_INTF_LCD0,
    HI_DRV_DISP_INTF_LCD1,
    HI_DRV_DISP_INTF_LCD2,

    HI_DRV_DISP_INTF_ID_MAX
}HI_DRV_DISP_INTF_ID_E;


#define HI_DISP_VDAC_MAX_NUMBER 4
#define HI_DISP_VDAC_INVALID_ID 0xff
typedef struct hiDRV_DISP_INTF_S
{
    HI_DRV_DISP_INTF_ID_E eID;
    HI_U8 u8VDAC_Y_G;
    HI_U8 u8VDAC_Pb_B;
    HI_U8 u8VDAC_Pr_R;
}HI_DRV_DISP_INTF_S;

typedef enum hiDRV_DISP_INTF_DATA_FMT
{
    HI_DRV_DISP_INTF_DATA_FMT_YUV422 = 0,
    HI_DRV_DISP_INTF_DATA_FMT_RGB565,
    HI_DRV_DISP_INTF_DATA_FMT_RGB444,
    HI_DRV_DISP_INTF_DATA_FMT_RGB666,
    HI_DRV_DISP_INTF_DATA_FMT_RGB888,
    HI_DRV_DISP_INTF_DATA_FMT_BUTT
}HI_DRV_DISP_INTF_DATA_FMT_E;

typedef struct hiDRV_DISP_TIMING_S
{
    HI_U32  u32VFB;
    HI_U32  u32VBB;
    HI_U32  u32VACT;
    HI_U32  u32HFB;
    HI_U32  u32HBB;
    HI_U32  u32HACT;
    HI_U32  u32VPW;
    HI_U32  u32HPW;
    HI_BOOL bIDV;
    HI_BOOL bIHS;
    HI_BOOL bIVS;
    HI_BOOL bClkReversal;
    HI_U32  u32DataWidth;
    HI_DRV_DISP_INTF_DATA_FMT_E eDataFmt;

    HI_BOOL bDitherEnable;
    HI_U32  u32ClkPara0;
    HI_U32  u32ClkPara1;

    HI_BOOL bInterlace;
    HI_U32  u32PixFreq;
    HI_U32  u32VertFreq;
    HI_U32  u32AspectRatioW;
    HI_U32  u32AspectRatioH;

    HI_BOOL u32bUseGamma;

    HI_U32  u32Reserve0; 
    HI_U32  u32Reserve1; 
}HI_DRV_DISP_TIMING_S;

typedef struct hiDRV_DISP_COLOR_S
{
    HI_U8 u8Red;
    HI_U8 u8Green;
    HI_U8 u8Blue;
}HI_DRV_DISP_COLOR_S;

typedef enum hiDISP_STEREO_E
{
    DISP_STEREO_NONE = 0,
    DISP_STEREO_FPK,
    DISP_STEREO_SBS_HALF,
    DISP_STEREO_TAB,
    DISP_STEREO_FIELD_ALTE,
    DISP_STEREO_LINE_ALTE,
    DISP_STEREO_SBS_FULL,
    DISP_STEREO_L_DEPT,
    DISP_STEREO_L_DEPT_G_DEPT,
    DISP_STEREO_BUTT
}HI_DRV_DISP_STEREO_E;

typedef struct hiDISP_CROP_S
{
    HI_U32 u32LeftOffset;
    HI_U32 u32TopOffset;
    HI_U32 u32RightOffset;
    HI_U32 u32BottomOffset;
}HI_DRV_DISP_CROP_S;

 
typedef struct hiDISP_DISPLAY_INFO_S
{
    HI_BOOL bIsMaster;
    HI_BOOL bIsSlave;
    HI_DRV_DISPLAY_E enAttachedDisp;
    
    HI_DRV_DISP_STEREO_E eDispMode;
    HI_BOOL bRightEyeFirst;
    HI_BOOL bInterlace;
    HI_BOOL bIsBottomField;
    HI_U32 u32Vline;

    HI_BOOL bUseAdjRect;
    HI_RECT_S stAdjRect;
    HI_RECT_S stOrgRect;
    HI_RECT_S stRefRect;
    HI_DRV_ASPECT_RATIO_S stAR;

    HI_U32 u32RefreshRate;
    HI_DRV_COLOR_SPACE_E eColorSpace;

    HI_U32 u32Bright;
    HI_U32 u32Hue;
    HI_U32 u32Satur;
    HI_U32 u32Contrst;

    HI_U32 u32Kr;
    HI_U32 u32Kg;
    HI_U32 u32Kb;
}HI_DISP_DISPLAY_INFO_S;

/*************** IP protect ***************/
//Macrovision
typedef enum hiDRV_DISP_MACROVISION_E
{
    HI_DRV_DISP_MACROVISION_TYPE0,
    HI_DRV_DISP_MACROVISION_TYPE1,
    HI_DRV_DISP_MACROVISION_TYPE2,
    HI_DRV_DISP_MACROVISION_TYPE3,
    HI_DRV_DISP_MACROVISION_CUSTOMER,
    HI_DRV_DISP_MACROVISION_BUTT
}HI_DRV_DISP_MACROVISION_E;


//CGMS-A
typedef enum hiDRV_DISP_CGMSA_TYPE_E
{
    HI_DRV_DISP_CGMSA_A = 0x00,
    HI_DRV_DISP_CGMSA_B,
    HI_DRV_DISP_CGMSA_TYPE_BUTT
}HI_DRV_DISP_CGMSA_TYPE_E;

typedef enum hiDRV_DISP_CGMSA_MODE_E
{
   HI_DRV_DISP_CGMSA_COPY_FREELY  = 0,
   HI_DRV_DISP_CGMSA_COPY_NO_MORE = 0x01,
   HI_DRV_DISP_CGMSA_COPY_ONCE    = 0x02,
   HI_DRV_DISP_CGMSA_COPY_NEVER   = 0x03,
      
   HI_DRV_DISP_CGMSA_MODE_BUTT
}HI_DRV_DISP_CGMSA_MODE_E;

typedef struct hiDRV_DISP_CGMSA_CFG_S
{
    HI_BOOL           bEnable;
    HI_DRV_DISP_CGMSA_TYPE_E  enType;
    HI_DRV_DISP_CGMSA_MODE_E  enMode;
}HI_DRV_DISP_CGMSA_CFG_S;


/*************** about VBI ***************/
typedef enum hiDRV_DISP_VBI_TYPE_E
{
    HI_DRV_DISP_VBI_TTX=0,
    HI_DRV_DISP_VBI_CC,
    HI_DRV_DISP_VBI_VCHIP,
    HI_DRV_DISP_VBI_WSS,
    HI_DRV_DISP_VBI_TYPE_BUTT
}HI_DRV_DISP_VBI_TYPE_E;

typedef struct hiDRV_DISP_VBI_CFG_S
{
    HI_DRV_DISP_VBI_TYPE_E eType;
    HI_U32  u32InBufferSize;
    HI_U32  u32WorkBufferSize;
}HI_DRV_DISP_VBI_CFG_S;

typedef struct hiDRV_DISP_VBI_DATA_S
{
    HI_DRV_DISP_VBI_TYPE_E eType;
    HI_U8  *pu8DataAddr;
    HI_U32  u32DataLen;
}HI_DRV_DISP_VBI_DATA_S;

typedef struct hiDRV_DISP_WSS_DATA_S
{
    HI_BOOL bEnable;
    HI_U16  u16Data;
}HI_DRV_DISP_WSS_DATA_S;

typedef struct hiDRV_DISP_HDMI_S
{
    HI_BOOL bHsyncNegative; 
    HI_BOOL bVsyncNegative; 
    HI_BOOL bDvNegative;    
    HI_S32 s32SyncType;    

    HI_S32 s32CompNumber;  //0,10bit; 1, 20bit; 2, 30bit
    HI_S32 s32DataFmt;     //0:YCbCr;1: RGB
}HI_DRV_DISP_HDMI_S;

typedef struct hiDRV_DISP_COLOR_SETTING_S
{
    HI_DRV_COLOR_SPACE_E enInCS;
    HI_DRV_COLOR_SPACE_E enOutCS;

    HI_U32 u32Bright;      //bright adjust value,range[0,100],default setting 50; 
    HI_U32 u32Hue;         //hue adjust value,range[0,100],default setting 50;
    HI_U32 u32Satur;       //saturation adjust value,range[0,100],default setting 50;
    HI_U32 u32Contrst;     //contrast adjust value,range[0,100],default setting 50;

    HI_U32 u32Kr;
    HI_U32 u32Kg;
    HI_U32 u32Kb;    
    HI_BOOL bGammaEnable;
    HI_BOOL bUseCustGammaTable; 

    HI_VOID *pReserve;    /* must be 0 */
    HI_U32   u32Reserve;  /* must be 0 */
}HI_DRV_DISP_COLOR_SETTING_S;

typedef struct hiDRV_DISP_INIT_PARAM_S
{
    HI_U32                u32Version;
    HI_BOOL               bSelfStart;
    HI_BOOL               bIsMaster;
    HI_BOOL               bIsSlave;
    HI_DRV_DISPLAY_E      enAttachedDisp;
    HI_DRV_DISP_FMT_E     enFormat;   
    HI_U32                u32Brightness;
    HI_U32                u32Contrast;
    HI_U32                u32Saturation;
    HI_U32                u32HuePlus;
    HI_BOOL               bGammaEnable; 
    HI_U32                u32ScreenXpos;
    HI_U32                u32ScreenYpos;
    HI_U32                u32ScreenWidth;
    HI_U32                u32ScreenHeight; 
    HI_DRV_DISP_COLOR_S   stBgColor;
    HI_BOOL               bCustomRatio;
    HI_U32                u32CustomRatioWidth;
    HI_U32                u32CustomRatioHeight;
    HI_DRV_DISP_INTF_S    stIntf[HI_DRV_DISP_INTF_ID_MAX];
    HI_DRV_DISP_TIMING_S  stDispTiming;

    HI_U32  u32Reseve;
    HI_VOID *pRevData;
}HI_DRV_DISP_INIT_PARAM_S;

typedef struct hiDRV_DISP_SETTING_S
{
    HI_U32  u32BootVersion;
    HI_BOOL bGetPDMParam;
    HI_BOOL bSelfStart;
    HI_BOOL bIsMaster;
    HI_BOOL bIsSlave;
    HI_DRV_DISPLAY_E enAttachedDisp;

    /* output format */
    HI_DRV_DISP_STEREO_E eDispMode;
    HI_DRV_DISP_FMT_E enFormat;
    HI_DRV_DISP_TIMING_S stCustomTimg;

    /* about color */
    HI_DRV_DISP_COLOR_SETTING_S stColor;

    /* background color */
    HI_DRV_DISP_COLOR_S stBgColor;

    //HI_BOOL bCGMSAEnable;
    //HI_DRV_DISP_CGMSA_TYPE_E  eCGMSAType;
    //HI_DRV_DISP_CGMSA_MODE_E  eCGMSAMode;

    //HI_DRV_DISP_MACROVISION_E eMcvnType;

    /* interface setting */
    HI_U32 u32IntfNumber;
    HI_DRV_DISP_INTF_S stIntf[HI_DRV_DISP_INTF_ID_MAX];

    HI_U32 u32LayerNumber;
    HI_DRV_DISP_LAYER_E enLayer[HI_DRV_DISP_LAYER_BUTT]; /* Z-order is from bottom to top */

    /* about sink display screen */
    HI_BOOL bAdjRect;
    HI_RECT_S stAdjRect;

    HI_BOOL bCustomRatio;
    HI_U32 u32CustomRatioWidth;
    HI_U32 u32CustomRatioHeight;

    HI_U32  u32Reseve;
    HI_VOID *pRevData;
}HI_DRV_DISP_SETTING_S;

#define DEF_HI_DRV_DISP_MIRROR_BUFFER_MAX_NUMBER 16

typedef struct hiDRV_DISP_CAST_CFG_S
{
    /* frame config */
    HI_DRV_PIX_FORMAT_E eFormat; /* Support ... */
    HI_U32 u32Width;
    HI_U32 u32Height;

    /* buffer config */
    HI_U32  u32BufNumber; /* not more than HI_DISP_MIRROR_BUFFER_MAX_NUMBER */

    HI_BOOL bUserAlloc;  /* TRUE: user alloc buffers; FALSE: enDisp alloc buffers */

    HI_U32 u32BufSize;    /* every buffer size in Byte */
    HI_U32 u32BufStride;  /* only for 'bUserAlloc = TRUE' */
    HI_U32 u32BufPhyAddr[DEF_HI_DRV_DISP_MIRROR_BUFFER_MAX_NUMBER]; /* only for 'bUserAlloc = TRUE' */
} HI_DRV_DISP_CAST_CFG_S;


/*************** about alg ***************/
typedef enum hiDRV_DISP_ALG_TYPE_E
{
    HI_DRV_DISP_ALG_DEI = 0,
    HI_DRV_DISP_ALG_ACC,
    HI_DRV_DISP_ALG_ACM,
    HI_DRV_DISP_ALG_SHARPNESS,
    HI_DRV_DISP_ALG_DNR,
    HI_DRV_DISP_ALG_TYPE_BUTT
}HI_DRV_DISP_ALG_TYPE_E;

typedef struct hiDRV_DISP_ALG_CFG_S
{
    HI_DRV_DISP_ALG_TYPE_E eType;
    HI_BOOL bEnable;

    HI_U32   u32Reserved;
    HI_VOID *pPrivate;
}HI_DRV_DISP_ALG_CFG_S;


#define DEF_HI_DRV_DISP_MAX_LAYER_NUMBER 8

typedef struct hiDRV_DISP_VERSION_S
{
    HI_U32 u32VersionPartL;
    HI_U32 u32VersionPartH;
}HI_DRV_DISP_VERSION_S;


#define DEF_DISP_CALLBACK_MAX_EVENT  8

typedef enum hiDRV_DISP_CALLBACK_TYPE_E
{
    /* �жϷ���λ�ã�������Ч��Ϊ�ο���0Ϊ��һ�У�100Ϊ���һ�н��� */
    HI_DRV_DISP_C_TYPE_NONE = 0,
    //HI_DRV_DISP_C_SHOW_MODE,
    HI_DRV_DISP_C_INTPOS_0_PERCENT,
    HI_DRV_DISP_C_INTPOS_90_PERCENT,
    HI_DRV_DISP_C_GFX_WBC,
    HI_DRV_DISP_C_REG_UP,
    HI_DRV_DISP_C_TYPE_BUTT
}HI_DRV_DISP_CALLBACK_TYPE_E;

typedef enum hiDRV_DISP_CALLBACK_EVENT_E
{
    HI_DRV_DISP_C_EVET_NONE = 0,
    HI_DRV_DISP_C_PREPARE_CLOSE,
    //HI_DRV_DISP_C_CLOSE            = 0x2,
    //HI_DRV_DISP_CALLBACK_PREPARE_TO_OPEN,
    HI_DRV_DISP_C_OPEN,
    
    HI_DRV_DISP_C_PREPARE_TO_PEND  = 0x10,
    //HI_DRV_DISP_C_PEND             = 0x20,
    //HI_DRV_DISP_C_PREPARE_TO_RESUME,
    HI_DRV_DISP_C_RESUME,

    HI_DRV_DISP_C_DISPLAY_SETTING_CHANGE = 0x20,
    HI_DRV_DISP_C_ADJUCT_SCREEN_AREA,

    HI_DRV_DISP_C_VT_INT = 0x100,
    HI_DRV_DISP_C_EVENT_BUTT
}HI_DRV_DISP_CALLBACK_EVENT_E;

typedef enum hiDRV_DISP_FIELD_FLAG_E
{
    HI_DRV_DISP_FIELD_PROGRESSIVE = 0,
    HI_DRV_DISP_FIELD_TOP,
    HI_DRV_DISP_FIELD_BOTTOM,
    HI_DRV_DISP_FIELD_FLAG_BUTT
}HI_DRV_DISP_FIELD_FLAG_E;

typedef struct hiDRV_DISP_CALLBACK_INFO_S
{
    HI_DRV_DISP_CALLBACK_EVENT_E eEventType;
    HI_DRV_DISP_FIELD_FLAG_E eField;

    HI_DISP_DISPLAY_INFO_S stDispInfo;
}HI_DRV_DISP_CALLBACK_INFO_S;

typedef struct hiDRV_DISP_CALLBACK_S
{
    HI_VOID (* pfDISP_Callback)(HI_HANDLE hDst, const HI_DRV_DISP_CALLBACK_INFO_S *pstInfo);
    HI_HANDLE hDst;
}HI_DRV_DISP_CALLBACK_S;







#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
