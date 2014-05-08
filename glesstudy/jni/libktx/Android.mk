LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := ktx

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \

LOCAL_SRC_FILES := \
	$(subst $(LOCAL_PATH)/,, \
    $(wildcard $(LOCAL_PATH)/lib/*.c) \
    $(wildcard $(LOCAL_PATH)/lib/*.cxx) \
    )

LOCAL_CFLAGS := -DKTX_OPENGL_ES2=1 -DSUPPORT_SOFTWARE_ETC_UNPACK=1

include $(BUILD_STATIC_LIBRARY)
