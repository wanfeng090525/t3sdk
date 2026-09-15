# T3验证SDK - Android NDK 命令行可执行文件
# 使用 ndk-build 编译

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := t3_cli
LOCAL_SRC_FILES := main.cpp t3sdk/t3sdk.cpp


LOCAL_CPPFLAGS := -std=c++17 -Wall -O2

# 构建为命令行可执行文件（非动态库）
include $(BUILD_EXECUTABLE)
