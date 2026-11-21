# CmmCompiler（2026 秋季编译原理大作业）

C-- 语言编译器前端（词法分析 → SLR 语法分析 → AST → LLVM IR 生成）。

## 快速开始

- **团队分工与接口契约**：请先阅读 [INTERFACE.md](./INTERFACE.md)
- **编译项目**：
  - Windows（MinGW / TDM-GCC）：直接运行 `build.bat`
  - Linux / macOS：`make` 或使用 `CMakeLists.txt`
- **分阶段运行**：
  ```bash
  ./build/cmmc.exe --lex tests/test1.sy
  ./build/cmmc.exe --parse tests/test1.sy
  ./build/cmmc.exe --ast tests/test1.sy
  ./build/cmmc.exe --ir tests/test1.sy -o out.ll
  ```

## 目录结构

```text
CmmCompiler/
├── include/          # 公共接口头文件（Token.h / TokenConfig.h / AST.h / Lexer.h / Parser.h / IRGenerator.h）
├── src/
│   ├── main.cpp      # 统一命令行入口
│   ├── lexer/        # 成员 1：手写自动机词法分析器
│   ├── parser/       # 成员 2：SLR 语法分析器与 AST 建树
│   └── ir/           # 成员 3 & 4：AST Visitor 与 LLVM IR 生成
├── third_party/      # 放置课程提供的 compiler_ir 中端库
├── tests/            # 测试用例（test1~3.sy 及 .ref）
├── INTERFACE.md      # 团队协作规范与模块接口说明
├── build.bat         # Windows 编译脚本
├── Makefile          # Make 构建配置
└── CMakeLists.txt    # CMake 构建配置
```
