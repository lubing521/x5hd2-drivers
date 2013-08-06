LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(SDK_DIR)/Android.def

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE := libhi_vdec
ALL_DEFAULT_INSTALLED_MODULES += $(LOCAL_MODULE)

LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS := $(CFG_HI_CFLAGS)
LOCAL_CFLAGS += -DLOG_TAG=\"$(LOCAL_MODULE)\"

ifeq (y,$(CFG_HI_VDEC_REG_CODEC_SUPPORT))
LOCAL_CFLAGS += -DHI_VDEC_REG_CODEC_SUPPORT=1
else
LOCAL_CFLAGS += -DHI_VDEC_REG_CODEC_SUPPORT=0
endif

#LOCAL_SRC_FILES := $(sort $(call all-c-files-under, ./))
LOCAL_SRC_FILES := hi_codec.c  mpi_vdec_adapter.c  mpi_vdec.c

LOCAL_C_INCLUDES := $(COMMON_UNF_INCLUDE)
LOCAL_C_INCLUDES += $(COMMON_DRV_INCLUDE)
LOCAL_C_INCLUDES += $(COMMON_API_INCLUDE)
LOCAL_C_INCLUDES += $(MSP_UNF_INCLUDE)
LOCAL_C_INCLUDES += $(MSP_DRV_INCLUDE)
LOCAL_C_INCLUDES += $(MSP_API_INCLUDE)
LOCAL_C_INCLUDES += $(MSP_DIR)/drv/vdec
LOCAL_C_INCLUDES += $(MSP_DIR)/drv/vfmw/vfmw_release
LOCAL_C_INCLUDES += $(MSP_DIR)/api/jpeg/include
LOCAL_C_INCLUDES += $(MSP_DIR)/api/jpeg/jpeg6b/include
LOCAL_C_INCLUDES += $(MSP_DIR)/api/jpeg/jpegfmw/include

ifeq ($(CFG_HI_CHIP_TYPE),hi3712)
LOCAL_C_INCLUDES += $(MSP_DIR)/drv/vfmw/vfmw_v4.0/firmware/product/HiX6V300
else
LOCAL_C_INCLUDES += $(MSP_DIR)/drv/vfmw/vfmw_v4.0/firmware/product/HiX5HD
endif

LOCAL_SHARED_LIBRARIES := libcutils libutils libhi_common libdl libhi_demux

include $(BUILD_SHARED_LIBRARY)

ifeq (y,$(CFG_HI_VDEC_REG_CODEC_SUPPORT))

include $(CLEAR_VARS)
include $(SDK_DIR)/$(SDK_CFGFILE)
include $(SDK_DIR)/Android.def

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE := libhi_codec.MJPEG

LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS := $(CFG_HI_CFLAGS)
LOCAL_CFLAGS += -DLOG_TAG=\"$(LOCAL_MODULE)\"

ifeq (y,$(CFG_HI_VDEC_REG_CODEC_SUPPORT))
LOCAL_CFLAGS += -DHI_VDEC_REG_CODEC_SUPPORT=1
else
LOCAL_CFLAGS += -DHI_VDEC_REG_CODEC_SUPPORT=0
endif

LOCAL_SRC_FILES := mjpeg.c

LOCAL_C_INCLUDES := $(COMMON_UNF_INCLUDE)
LOCAL_C_INCLUDES += $(COMMON_DRV_INCLUDE)
LOCAL_C_INCLUDES += $(COMMON_API_INCLUDE)
LOCAL_C_INCLUDES += $(MSP_UNF_INCLUDE)
LOCAL_C_INCLUDES += $(MSP_DRV_INCLUDE)
LOCAL_C_INCLUDES += $(MSP_API_INCLUDE)
LOCAL_C_INCLUDES += $(MSP_DIR)/drv/vdec
LOCAL_C_INCLUDES += $(MSP_DIR)/drv/vfmw/vfmw_release
LOCAL_C_INCLUDES += $(MSP_DIR)/api/jpeg/include
LOCAL_C_INCLUDES += $(MSP_DIR)/api/jpeg/jpeg6b/include
LOCAL_C_INCLUDES += $(MSP_DIR)/api/jpeg/jpegfmw/include

ifeq ($(CFG_HI_CHIP_TYPE),hi3712)
LOCAL_C_INCLUDES += $(MSP_DIR)/drv/vfmw/vfmw_v4.0/firmware/product/HiX6V300
else
LOCAL_C_INCLUDES += $(MSP_DIR)/drv/vfmw/vfmw_v4.0/firmware/product/HiX5HD
endif

LOCAL_SHARED_LIBRARIES := libcutils libutils libhi_common libdl libhi_demux libhi_jpeg

include $(BUILD_SHARED_LIBRARY)

endif