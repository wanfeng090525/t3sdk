# T3 SDK - Android NDK CMake 构建脚本
# 用于生成可在 Android 应用中加载的 libt3sdk.so

cmake_minimum_required(VERSION 3.10.2)
project(t3sdk)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fPIC -O2")

# 源文件
set(T3_SOURCES
    ../../src/md5.c
    ../../src/crypto.c
    ../../src/http.c
    ../../src/t3json.c
    ../../src/t3sdk.c
    ../../src/jni.c
)

# 头文件目录
include_directories(
    ../../include
)

# 生成动态库
add_library(t3sdk SHARED ${T3_SOURCES})

# 链接系统库（Android NDK 自带，无需额外依赖）
target_link_libraries(t3sdk
    log
)
