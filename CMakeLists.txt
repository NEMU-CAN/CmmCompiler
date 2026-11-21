cmake_minimum_required(VERSION 3.14)
project(CmmCompiler LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB LEXER_SRCS  "src/lexer/*.c" "src/lexer/*.cpp")
file(GLOB PARSER_SRCS "src/parser/*.cpp")
file(GLOB IR_SRCS     "src/ir/*.cpp")

add_executable(cmmc
    src/main.cpp
    ${LEXER_SRCS}
    ${PARSER_SRCS}
    ${IR_SRCS}
)

target_include_directories(cmmc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
