@echo off
setlocal enabledelayedexpansion
if not exist build mkdir build

set SRCS=
for /r src %%f in (*.cpp) do (
    set SRCS=!SRCS! "%%f"
)

rem Optional flag: build.bat -DCMM_USE_LAB3_CODES=1
echo Building build\cmmc.exe ...
g++ -std=c++17 -O2 -Wall -Wextra -Iinclude %* %SRCS% -o build\cmmc.exe
if errorlevel 1 (
    echo Build FAILED.
    exit /b 1
)
echo Build SUCCEEDED: build\cmmc.exe
