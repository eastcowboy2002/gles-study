LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/../SDL/include \
	$(LOCAL_PATH)/../libktx/include \

LOCAL_SRC_FILES := \
	$(subst $(LOCAL_PATH)/,, \
    $(wildcard $(LOCAL_PATH)/*.cpp))

LOCAL_SRC_FILES += \
	../SDL/src/main/android/SDL_android_main.c

LOCAL_LDLIBS := -ldl -lGLESv2 -llog -landroid

LOCAL_CFLAGS := -DKTX_OPENGL_ES2=1
LOCAL_ARM_NEON := true

LOCAL_SHARED_LIBRARIES := SDL2 SDL2_image
LOCAL_STATIC_LIBRARIES := ktx

include $(BUILD_SHARED_LIBRARY)
