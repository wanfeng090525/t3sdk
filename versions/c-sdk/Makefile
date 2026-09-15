# T3 验证 SDK - Makefile
# 构建 Linux 共享库 (.so) 和静态库 (.a)
#
# 用法:
#   make          - 构建共享库和静态库
#   make shared   - 仅构建共享库
#   make static   - 仅构建静态库
#   make test     - 构建并运行测试程序
#   make install  - 安装到 /usr/local
#   make clean    - 清理

CC      = gcc
CFLAGS  = -Wall -O2 -fPIC -Iinclude
LDFLAGS = -shared

# 平台检测
UNAME_S := $(shell uname -s)
ifeq ($(OS),Windows_NT)
    LIBS = -lws2_32 -liphlpapi
    EXT  = .dll
else
    LIBS =
    EXT  = .so
endif

SRC_DIR = src
INC_DIR = include
BUILD   = build
OBJ     = $(BUILD)/obj

SRCS    = $(SRC_DIR)/t3sdk.c
OBJS    = $(OBJ)/t3sdk.o
TARGET_SO = $(BUILD)/libt3sdk$(EXT)
TARGET_A  = $(BUILD)/libt3sdk.a

.PHONY: all shared static test install clean

all: shared static

$(OBJ):
	mkdir -p $(OBJ)

$(OBJ)/t3sdk.o: $(SRCS) $(INC_DIR)/t3sdk.h | $(OBJ)
	$(CC) $(CFLAGS) -c $(SRCS) -o $@

shared: $(TARGET_SO)

$(TARGET_SO): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
	@echo "共享库已生成: $@"

static: $(TARGET_A)

$(TARGET_A): $(OBJS)
	ar rcs $@ $(OBJS)
	@echo "静态库已生成: $@"

test: $(TARGET_SO)
	@echo "构建测试程序..."
	$(CC) -Wall -O2 -I$(INC_DIR) test/test.c -L$(BUILD) -lt3sdk $(LIBS) -o $(BUILD)/test_t3sdk
	@echo "测试程序已生成: $(BUILD)/test_t3sdk"
	@echo ""
	@echo "运行测试 (LD_LIBRARY_PATH=$(BUILD) ./build/test_t3sdk)"

install: $(TARGET_SO) $(TARGET_A)
	cp $(TARGET_SO) /usr/local/lib/
	cp $(TARGET_A) /usr/local/lib/
	cp $(INC_DIR)/t3sdk.h /usr/local/include/
	ldconfig
	@echo "安装完成: /usr/local/lib/libt3sdk$(EXT), /usr/local/include/t3sdk.h"

clean:
	rm -rf $(BUILD)
	@echo "清理完成"
