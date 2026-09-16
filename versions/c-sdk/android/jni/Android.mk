# T3 验证 SDK - Android NDK 构建
# 使用 ndk-build 编译为多架构 .so

LOCAL_PATH := $(call my-dir)

# ===== libt3sdk.so =====
include $(CLEAR_VARS)
LOCAL_MODULE    := t3sdk
LOCAL_SRC_FILES := ../../src/t3sdk.c ../../src/t3sdk_jni.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include
LOCAL_CFLAGS    := -Wall -O2 -fPIC
LOCAL_LDLIBS    := -llog
include $(BUILD_SHARED_LIBRARY)
