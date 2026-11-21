cmake_minimum_required(VERSION 3.14)
project(CmmCompiler LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE CMM_SRCS
    "src/*.cpp"
    "src/*.c"
)

add_executable(cmmc ${CMM_SRCS})

target_include_directories(cmmc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(cmmc PRIVATE /W4)
else()
    target_compile_options(cmmc PRIVATE -Wall -Wextra)
endif()
