LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := libktx
LOCAL_CFLAGS    := -Werror -DKTX_OPENGL_ES2=1
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SRC_FILES := /lib/checkheader.c \
                   /lib/errstr.c \
                   /lib/etcdec.cxx \
                   /lib/etcunpack.cxx \
                   /lib/hashtable.c \
                   /lib/loader.c \
                   /lib/swap.c \
                   /lib/writer.c \

LOCAL_LDLIBS    := -llog -lGLESv2

include $(BUILD_STATIC_LIBRARY)
