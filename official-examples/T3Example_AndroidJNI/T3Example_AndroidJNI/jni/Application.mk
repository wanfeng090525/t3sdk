# T3验证SDK - Android NDK 应用配置

# 目标 ABI（根据需要修改，常用 arm64-v8a）
APP_ABI := arm64-v8a armeabi-v7a x86_64

# 最低 API 等级
APP_PLATFORM := android-21

# 使用 C++ STL
APP_STL := c++_static

# C++17 + 启用异常和RTTI（SDK使用了throw/try/catch）
APP_CPPFLAGS := -std=c++17 -fexceptions -frtti
