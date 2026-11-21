@echo off
setlocal enabledelayedexpansion
if not exist build mkdir build

set SRCS=src\main.cpp
for %%f in (src\lexer\*.cpp) do set SRCS=!SRCS! %%f
for %%f in (src\parser\*.cpp) do set SRCS=!SRCS! %%f
for %%f in (src\ir\*.cpp) do set SRCS=!SRCS! %%f

echo Building build\cmmc.exe ...
g++ -std=c++17 -O2 -Wall -Wextra -Iinclude %SRCS% -o build\cmmc.exe
if errorlevel 1 (
    echo Build FAILED.
    exit /b 1
)
echo Build SUCCEEDED: build\cmmc.exe
