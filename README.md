# CmmCompiler（2026 秋季编译原理大作业）

C-- 语言编译器前端（手写自动机词法分析 → SLR 语法分析 → AST → LLVM IR 生成）。

## 快速开始

- **团队分工与接口契约**：请务必先阅读 [INTERFACE.md](./INTERFACE.md)
- **编译项目**：
  - Windows（MinGW / TDM-GCC）：运行 `build.bat`（生成 `build\cmmc.exe`；加 `-DCMM_USE_LAB3_CODES=1` 可切为 lab3 连续编号表）
  - Linux / macOS / WSL：运行 `make`（生成 `build/cmmc`）
  - CLion / VS Code CMake：打开 `CMakeLists.txt`
- **自动化验证（跨平台，验证默认原表 + lab3 参考表 + C++ 契约）**：
  ```bash
  python tests/verify_lexer.py
  ```
- **分阶段运行**：
  ```bash
  ./build/cmmc.exe --lex tests/lexer_lab3/test1.sy
  ./build/cmmc.exe --lex --symbols --dump-automata build/automata.json tests/lexer_lab3/test1.sy
  ./build/cmmc.exe --parse --dump-slr build/slr.txt tests/parser_lab4/test1.sy
  ./build/cmmc.exe --ast tests/parser_lab4/test1.sy
  ./build/cmmc.exe --ir tests/parser_lab4/test1.sy -o out.ll
  ```

## 目录结构

```text
CmmCompiler/
├── include/            # 公共接口头文件（Token.h / TokenConfig.h / AST.h / Lexer.h / Parser.h / IRGenerator.h）
├── src/
│   ├── main.cpp        # 统一命令行入口
│   ├── lexer/          # 成员 1：手写 NFA->DFA->最小化DFA 词法分析器（已完成并验证）
│   ├── parser/         # 成员 2：SLR 语法分析器与 AST 建树
│   └── ir/             # 成员 3 & 4：AST Visitor 与 LLVM IR 生成（草案，待接入 compiler_ir 定稿）
├── third_party/        # 放置课程提供的 compiler_ir 中端库
├── tests/
│   ├── lexer_lab3/     # 平时实验 3 词法用例与参考输出（test1~3.sy/.ref）
│   ├── parser_lab4/    # 平时实验 4 语法用例与参考归约输出（example + test1~3.sy/.ref）
│   ├── custom/         # 扩展用例（含单目 '!' 等）
│   ├── test_contracts.cpp
│   └── verify_lexer.py
├── INTERFACE.md        # 团队协作规范与模块接口契约
├── build.bat           # Windows 编译脚本
├── Makefile            # Linux/macOS/WSL 构建配置
└── CMakeLists.txt      # CMake 构建配置
```
