# T3验证SDK C++版本 Makefile
# 自动检测：有哪个源文件就编译哪个，都有就都编译
# 编译: make    运行: make run

CXX = g++
CXXFLAGS = -Wall -O2 -std=c++17

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
ifneq ($(wildcard main.cpp),)
    TARGETS += t3sdk_demo$(EXE)
endif
ifneq ($(wildcard example_all_api.cpp),)
    TARGETS += example_all_api$(EXE)
endif

all: $(TARGETS)

t3sdk_demo$(EXE): main.cpp t3sdk/t3sdk.cpp t3sdk/t3sdk.h
	$(CXX) $(CXXFLAGS) main.cpp t3sdk/t3sdk.cpp -o $@ $(LIBS)
	@echo "编译完成: $@"

example_all_api$(EXE): example_all_api.cpp t3sdk/t3sdk.cpp t3sdk/t3sdk.h
	$(CXX) $(CXXFLAGS) example_all_api.cpp t3sdk/t3sdk.cpp -o $@ $(LIBS)
	@echo "编译完成: $@"

clean:
	$(RM) t3sdk_demo$(EXE) example_all_api$(EXE)
	@echo "清理完成"

run:
ifneq ($(wildcard main.cpp),)
	./t3sdk_demo$(EXE)
else
	./example_all_api$(EXE)
endif

.PHONY: all clean run
