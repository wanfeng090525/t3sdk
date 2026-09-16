# T3 验证 SDK - Android NDK 构建
# 使用官方 T3Example_AndroidJNI 源码（C++）+ 精简 JNI 绑定
# 使用 ndk-build 编译为多架构 .so

LOCAL_PATH := $(call my-dir)

# ===== libt3sdk.so =====
include $(CLEAR_VARS)
LOCAL_MODULE    := t3sdk
LOCAL_SRC_FILES := t3sdk/t3sdk.cpp t3sdk_jni.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_CPPFLAGS  := -std=c++17 -Wall -O2 -fexceptions -frtti
LOCAL_LDLIBS    := -llog
# 关键：禁用 --gc-sections。NDK 默认启用函数级 GC，在部分架构(O2+LLVM
# 优化)下会把 httpExchange 的响应解析代码误判为未引用而回收，导致
# chunked/content-length 检测逻辑缺失、App 报"响应不是有效的JSON格式"。
LOCAL_LDFLAGS   := -Wl,--no-gc-sections
include $(BUILD_SHARED_LIBRARY)
