LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := libgl2jni
LOCAL_CFLAGS    := -Werror -DKTX_OPENGL_ES2=1
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../libktx/include
LOCAL_SRC_FILES := /gl_code.cpp
LOCAL_LDLIBS    := -llog -lGLESv2

LOCAL_STATIC_LIBRARIES := ktx

include $(BUILD_SHARED_LIBRARY)
