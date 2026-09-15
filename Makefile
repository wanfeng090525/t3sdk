# T3 SDK - Shared Library Makefile
# 编译生成 libt3sdk.so 动态链接库

CC      ?= gcc
AR      ?= ar
CFLAGS  := -Wall -Wextra -fPIC -O2 -Iinclude
LDFLAGS := -shared
LIBS    := -lcurl

# 目录结构
SRCDIR   := src
INCDIR   := include
TESTDIR  := test
BUILDDIR := build
OBJDIR   := $(BUILDDIR)/obj

# 源文件
SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))

# 目标库
TARGET_SO := $(BUILDDIR)/libt3sdk.so
TARGET_A  := $(BUILDDIR)/libt3sdk.a

# 测试程序
TEST_SRC := $(TESTDIR)/test.c
TEST_BIN := $(BUILDDIR)/test_t3sdk

.PHONY: all clean test install

all: $(TARGET_SO) $(TARGET_A)

# 创建目录
$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

# 编译目标文件
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 生成动态库
$(TARGET_SO): $(OBJS) | $(BUILDDIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
	@echo ">>> 动态库已生成: $@"

# 生成静态库
$(TARGET_A): $(OBJS) | $(BUILDDIR)
	$(AR) rcs $@ $(OBJS)
	@echo ">>> 静态库已生成: $@"

# 编译测试程序
test: $(TARGET_SO) $(TEST_SRC) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $(TEST_BIN) $(TEST_SRC) -L$(BUILDDIR) -lt3sdk $(LIBS) -Wl,-rpath,$(BUILDDIR)
	@echo ">>> 测试程序已生成: $(TEST_BIN)"
	@echo ">>> 运行: LD_LIBRARY_PATH=$(BUILDDIR) $(TEST_BIN)"

clean:
	rm -rf $(BUILDDIR)

install: $(TARGET_SO)
	cp $(TARGET_SO) /usr/local/lib/
	cp $(INCDIR)/t3sdk.h /usr/local/include/
	ldconfig
	@echo ">>> 安装完成"
