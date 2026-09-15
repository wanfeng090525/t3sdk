# T3验证SDK C语言版本 Makefile
# 自动检测：有哪个源文件就编译哪个，都有就都编译
# 编译: make    运行: make run

CC = gcc
CFLAGS = -Wall -O2

# 检测操作系统
UNAME_S := $(shell uname -s)
ifeq ($(OS),Windows_NT)
    LIBS = -lws2_32 -liphlpapi
    EXE = .exe
    RM = del /Q
else
    LIBS =
    EXE =
    RM = rm -f
endif

# 自动检测存在的源文件
TARGETS =
ifneq ($(wildcard main.c),)
    TARGETS += t3sdk_demo$(EXE)
endif
ifneq ($(wildcard example_all_api.c),)
    TARGETS += example_all_api$(EXE)
endif

all: $(TARGETS)

t3sdk_demo$(EXE): main.c t3sdk/t3sdk.c t3sdk/t3sdk.h
	$(CC) $(CFLAGS) main.c t3sdk/t3sdk.c -o $@ $(LIBS)
	@echo "编译完成: $@"

example_all_api$(EXE): example_all_api.c t3sdk/t3sdk.c t3sdk/t3sdk.h
	$(CC) $(CFLAGS) example_all_api.c t3sdk/t3sdk.c -o $@ $(LIBS)
	@echo "编译完成: $@"

clean:
	$(RM) t3sdk_demo$(EXE) example_all_api$(EXE)
	@echo "清理完成"

run:
ifneq ($(wildcard main.c),)
	./t3sdk_demo$(EXE)
else
	./example_all_api$(EXE)
endif

.PHONY: all clean run
