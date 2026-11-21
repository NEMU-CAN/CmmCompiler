CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude

TARGET   := build/cmmc.exe
SRCS     := src/main.cpp \
            $(wildcard src/lexer/*.cpp) \
            $(wildcard src/parser/*.cpp) \
            $(wildcard src/ir/*.cpp)

# 若后续引入了 third_party/compiler_ir，可取消下方注释并将对应头文件目录加入 CXXFLAGS
# CXXFLAGS += -Ithird_party/compiler_ir/include
# SRCS     += $(wildcard third_party/compiler_ir/src/*.cpp)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	@if not exist build mkdir build
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

clean:
	@if exist build rmdir /s /q build
