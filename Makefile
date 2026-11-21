# POSIX Makefile (Linux / macOS / WSL / Git Bash)
# Windows cmd / PowerShell 请直接运行 build.bat

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude

TARGET   := build/cmmc
SRCS     := $(shell find src -name '*.cpp')

# 引入 third_party/compiler_ir 后可取消下方注释并按实际路径微调：
# CXXFLAGS += -Ithird_party/compiler_ir/include
# SRCS     += $(shell find third_party/compiler_ir/src -name '*.cpp')

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

clean:
	@rm -rf build
