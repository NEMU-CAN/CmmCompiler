#!/usr/bin/env python3
"""成员 4（IR 控制流）验证脚本。

流程：编译 Mock AST 冒烟测试 -> 运行 -> 用 llvm-as 校验生成的 .ll 是否合法。

用法：
    python tests/verify_ir_ctrl.py

环境变量：
    CXX     指定编译器（默认 g++ / clang++）
    LLVM_AS 指定 llvm-as 可执行文件路径
"""

from __future__ import annotations

import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
SOURCES = [
    ROOT / "src" / "ir" / "ir_basic.cpp",
    ROOT / "src" / "ir" / "ir_ctrl.cpp",
    ROOT / "src" / "ir" / "ir_generator.cpp",
    ROOT / "tests" / "member4_ir_smoke.cpp",
]


def find_cxx() -> str:
    for name in (os.environ.get("CXX"), "g++", "clang++", "c++"):
        if name and shutil.which(name):
            return name
    sys.exit("[FAIL] 找不到 C++ 编译器（设置 CXX 环境变量）")


def find_llvm_as() -> list[str] | None:
    """返回执行 llvm-as 的命令前缀；Windows 下可回退到 WSL。"""
    if os.environ.get("LLVM_AS"):
        return [os.environ["LLVM_AS"]]
    exe = shutil.which("llvm-as")
    if exe:
        return [exe]
    if os.name == "nt" and shutil.which("wsl"):
        return ["wsl", "bash", "-lc"]
    return None


def main() -> int:
    BUILD.mkdir(exist_ok=True)
    exe = BUILD / ("member4_ir_smoke.exe" if os.name == "nt" else "member4_ir_smoke")

    cxx = find_cxx()
    compile_cmd = [cxx, "-std=c++17", "-I", str(ROOT / "include"), "-I", str(ROOT / "src" / "ir")]
    compile_cmd += [str(s) for s in SOURCES]
    compile_cmd += ["-o", str(exe)]
    print(f"[1/3] 编译冒烟测试: {' '.join(compile_cmd[:4])} ...")
    result = subprocess.run(compile_cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stderr)
        return 1

    print("[2/3] 运行 Mock AST 冒烟测试 ...")
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stderr)
        return 1
    ir_text = result.stdout
    if "define i32 @main" not in ir_text:
        print("[FAIL] 未生成 main 函数 IR")
        return 1

    ir_file = BUILD / "member4_ir.ll"
    ir_file.write_text(ir_text, encoding="utf-8")
    print(f"      IR 已写入: {ir_file}")

    print("[3/3] llvm-as 校验 IR ...")
    llvm_as = find_llvm_as()
    if llvm_as is None:
        print("[SKIP] 未找到 llvm-as（安装 LLVM 后可自动校验）")
        return 0

    if os.name == "nt" and llvm_as[0] == "wsl":
        win_path = str(ir_file)
        path = "/mnt/" + win_path[0].lower() + win_path[2:].replace("\\", "/")
        verify_cmd = llvm_as + [f"llvm-as '{path}' -o /dev/null && echo IR_VALID"]
        result = subprocess.run(verify_cmd, capture_output=True, text=True)
        valid = "IR_VALID" in (result.stdout or "")
    else:
        result = subprocess.run([*llvm_as, str(ir_file), "-o", os.devnull],
                                capture_output=True, text=True)
        valid = result.returncode == 0

    if not valid:
        print(result.stdout)
        print(result.stderr)
        print("[FAIL] 生成的 IR 不是合法 LLVM IR")
        return 1

    print("[PASS] 成员 4 控制流 IR 合法，冒烟测试通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
