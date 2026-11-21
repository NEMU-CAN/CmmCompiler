# CmmCompiler 团队协作与模块接口契约（v1.2）

> 本文档是 4 位组员并行开发的接口基准。**词法→语法接口（第 3 节）与 AST 结构（第 4 节）已冻结**；**IR 内部接口（第 5 节）目前为草案**，待拿到 `compiler_ir` 源码后定稿。凡需改动 `include/` 下的公共头文件，必须在群里同步并由成员 1 统一合并。

---

## 1. 目录权限与 Git 分支约定

| 成员 | 负责目录 / 文件 | Git 分支名 | 核心交付物 |
| :--- | :--- | :--- | :--- |
| **成员 1** | `include/`、`src/lexer/`、`src/main.cpp`、`build.bat`、`Makefile`、`CMakeLists.txt` | `feat/lexer` | NFA→DFA→最小化 DFA 词法分析器 + 共享接口 |
| **成员 2** | `src/parser/`（替换 `parser_stub.cpp`） | `feat/parser` | 附录 1 纯 BNF 展开、FIRST/FOLLOW、LR(0)、SLR 分析表、归约输出与 AST 建树 |
| **成员 3** | `src/ir/ir_basic.cpp`（与 4 号协商定稿 `src/ir/ir_visitor.h`） | `feat/ir-basic` | 接入 `compiler_ir`、作用域符号表、变量/常量/函数/传参/赋值/return/单目与四则运算 |
| **成员 4** | `src/ir/ir_ctrl.cpp`、`tests/` | `feat/ir-ctrl` | `if-else` 基本块、比较运算、`&&`/`\|\|` 短路求值、浮点处理、全量 `.ll` 验证 |

---

## 2. 编译与分阶段调试命令

| 平台 / 工具 | 编译命令 | 可执行文件 |
| :--- | :--- | :--- |
| Windows（TDM-GCC / MinGW） | `build.bat`（附加 `-DCMM_USE_LAB3_CODES=1` 切为 lab3 编号表） | `build\cmmc.exe` |
| Linux / macOS / WSL / Git Bash | `make`（附加 `CXXFLAGS+=-DCMM_USE_LAB3_CODES=1` 可切表） | `build/cmmc` |
| CLion / VS Code CMake | 打开 `CMakeLists.txt` | `cmmc` |

分阶段运行命令：
```bash
# 1. 仅词法（成员 1 验收；默认按大作业任务书原表输出）
./build/cmmc.exe --lex tests/lexer_lab3/test1.sy
./build/cmmc.exe --lex --symbols --dump-automata build/automata.json tests/lexer_lab3/test1.sy

# 2. 词法 + SLR 语法归约输出 & 导出 SLR 分析表（成员 2 验收，参考 tests/parser_lab4/*.ref）
./build/cmmc.exe --parse --dump-slr build/slr.txt tests/parser_lab4/test1.sy

# 3. 词法 + SLR 建树并打印 AST（成员 2 -> 成员 3/4 联调）
./build/cmmc.exe --ast tests/parser_lab4/test1.sy

# 4. 完整编译输出 LLVM IR（成员 3/4 验收）
./build/cmmc.exe --ir tests/parser_lab4/test1.sy -o out.ll
```

> **词法编号表说明（待向助教确认）**：
> - 默认采用**大作业任务书原表**（`CMM_USE_LAB3_CODES == 0`）：KW `1..8`、OP `6..19`（`=` 为 `11`，暂定 `!` 为 `26`）、SE `20..25`（`;` 为 `24`），与任务书示例 `=\t<OP,11>`、`;\t<SE,24>` 一致。
> - 如需与平时实验 3 参考输出 `tests/lexer_lab3/*.ref` 逐字节比对，编译时加 `-DCMM_USE_LAB3_CODES=1`（OP `9..22`、`!` 为 `29`、SE `23..28`）。
> - 最终验收以哪张表为准以及 `!` 的编号，需向助教确认后一键切换即可。

> **关于 `test2.sy` / `test3.sy` 中的非法字符**：`Lexer::tokenize()` 保留含 `<ERROR,行,列>` 的 `rawTokens`（用于 `--lex` 输出），同时提供已跳过非法字符、末尾附带 `EOF` 的 `parserTokens` 给成员 2；在 `--parse`/`--ast`/`--ir` 模式下，`main.cpp` 会向 `stderr` 打印 `[Lexical Warning]` 但继续语法分析（与 `tests/parser_lab4/test2.ref`、`test3.ref` 行为一致）。加 `--strict-lex` 可遇词法错立即终止。

---

## 3. 接口 A：成员 1 → 成员 2（`include/Token.h` & `include/Lexer.h`，已冻结）

成员 2 直接消费 `LexResult::parserTokens`：
- **查 SLR 分析表**：**一律使用 `tok.symbol`**。
- **打印归约序列的 `move` 行**：**使用 `tok.stackDisplay() + "#" + tok.lexeme`**（不要直接用 `tok.symbol` 打印，否则 `VoID` 会印成 `void#VoID` 而与 `tests/parser_lab4/test3.ref` 不符）。
- **构造终结符 AST 节点**：**一律调用 `ASTNode::makeTerminal(tok)`**，它会自动把关键字 `text` 归一化为小写并标记 `terminal = true`。

| 词法类别 | 示例源码 | `tok.lexeme` | `tok.symbol`（SLR 查表） | `tok.stackDisplay()`（`move` 行左半） | `tok.normalizedText()`（入 AST） | `--lex` 默认输出 |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 关键字（除 `main`） | `VoID`、`InT`、`FLOAT` | `"VoID"` | `"void"`、`"int"`、`"float"` 等 | `"VoID"`（原文） | `"void"`（小写） | `VoID\t<KW,2>` |
| **`main` 特殊约定** | `main`、`MaIn` | `"MaIn"` | **`"Ident"`** | `"Ident"` | **`"main"`** | `MaIn\t<KW,5>` |
| 标识符 | `_X1`、`Pi` | `"_X1"` | `"Ident"` | `"Ident"` | `"_X1"`（保原样） | `_X1\t<IDN,_X1>` |
| 整数 | `42` | `"42"` | `"IntConst"` | `"IntConst"` | `"42"` | `42\t<INT,42>` |
| 浮点数 | `3.14` | `"3.14"` | `"floatConst"`（小写 `f`） | `"floatConst"` | `"3.14"` | `3.14\t<FLOAT,3.14>` |
| 运算符 / 界符 | `==`、`!`、`(` | `"=="` | 原文（32 个终结符全覆盖） | `"=="` | `"=="` | `==\t<OP,14>` |
| 结束符 | （文件尾） | `""` | `"EOF"` | `""` | `""` | 不打印 |

---

## 4. 接口 B：成员 2 → 成员 3/4（`include/AST.h` & `include/Parser.h`，已冻结）

### 4.1 成员 2 必读：EOF、SLR(1) 冲突与 `tests/parser_lab4/*.ref` 细节

1. **`EOF` 不要重复包裹**：附录 1 规则 2 是 `compUnit -> (decl | funcDef)* EOF`，`EOF` 本身已是文法终结符。词法器在 `parserTokens` 末尾放的这**唯一一个 `EOF` 记号**同时充当文法终结符与输入结束标志，构造增广文法时勿再额外叠加一个 `$`。
2. **两处真实 SLR(1) 冲突**：
   - **悬空 `else`（移进/归约）**：`stmt -> 'if' '(' cond ')' stmt ('else' stmt)?` 遇 `'else'` 时**移进优先**。
   - **`'int'` 后接 `Ident`（归约/归约）**：`bType -> 'int'` 与 `funcType -> 'int'` 的 `FOLLOW` 均含 `Ident`。推荐在内部 BNF 中让 `funcDef` 共用 `bType`（`funcDef -> bType Ident '(' optParams ')' block | 'void' Ident '(' optParams ')' block`，拒绝 `float` 返回值；`tests/parser_lab4/*.ref` 中出现 `bType#int` 而非 `funcType#int` 也印证了该做法），但**建出的 AST 必须始终将 `funcDef` 的第 0 个子节点规范为 `funcType`**。
3. **归约序列打印格式（参考 `tests/parser_lab4/*.ref`，建议向助教确认）**：
   - `move` 行：`[序号]\t[tok.stackDisplay()]#[tok.lexeme]\tmove`（例如 `1\tVoID#VoID\tmove`、`2\tIdent#MaIn\tmove`）。
   - `accept` 行：`[序号]\tProgram#EOF\taccept`。
   - `reduction` 行：按任务书格式 `[序号]\t[栈顶符号]#[面临输入符号]\treduction`；注意 `tests/parser_lab4/*.ref`（由 Lab 4 的 Bison 样例生成）里存在 `constDecl#SE_SEMICOLON`、`constInitVal#ConstExp`、`constDef#ConstInitVal`、`stmt#if-else`、`exp#lOrExp` 等特定标签，成员 2 实现手写 SLR 时建议向助教确认大作业评测是否要求严格对齐 Lab 4 的标签。

### 4.2 冻结的 AST 节点结构契约（全 33 个非终结符精确 `children` 布局）

> **铁律**：
> 1. 所有因展开 EBNF（`*`、`?`）引入的辅助列表节点（如 `varDefList`、`blockItemList`）**必须展平**，不得出现在最终 AST 中。
> 2. 所有标点与关键字终结符（`const`、`=`、`,`、`;`、`(`、`)`、`{`、`}`、`if`、`else`、`return` 及所有运算符）**全部保留在 `children` 中**，顺序与附录 1 产生式完全一致；`compUnit` **有意省略**末尾 `EOF` 终结符。
> 3. 成员 3/4 遍历时可配合 `node->findChild("...")` / `node->findChildren("...")` / `node->hasChild("...")`。

| 非终结符 `type` | 冻结的 `children` 顺序（严格对齐附录 1） | 关键说明（成员 3/4 查阅） |
| :--- | :--- | :--- |
| `Program` | `[compUnit]` | 唯一根节点 |
| `compUnit` | `[decl \| funcDef, ...]`（展平，**有意不含 `EOF`**） | 空文件时 `children` 为空 |
| `decl` | `[constDecl]` 或 `[varDecl]` | |
| `constDecl` | `["const", bType, constDef, ",", constDef, ..., ";"]` | 用 `findChildren("constDef")` 取全部定义 |
| `bType` | `["int"]` 或 `["float"]` | `bType->getText()` 必为小写 `"int"` 或 `"float"` |
| `constDef` | `[Ident, "=", constInitVal]` | `children[0]->text` 为常量名 |
| `constInitVal` | `[constExp]` | |
| `varDecl` | `[bType, varDef, ",", varDef, ..., ";"]` | 用 `findChildren("varDef")` 取全部定义 |
| `varDef` | `[Ident]` 或 `[Ident, "=", initVal]` | 用 `findChild("initVal")` 判断是否带初值 |
| `initVal` | `[exp]` | |
| `funcDef` | `[funcType, Ident, "(", (funcFParams)?, ")", block]` | **无参时不含 `funcFParams`**；用 `findChild("funcFParams")` 判断；`MaIn` 已归一化为 `"main"` |
| `funcType` | `["void"]` 或 `["int"]` | `funcType->getText()` 必为小写 `"void"` 或 `"int"` |
| `funcFParams` | `[funcFParam, ",", funcFParam, ...]` | 用 `findChildren("funcFParam")` 取全部形参 |
| `funcFParam` | `[bType, Ident]` | |
| `block` | `["{", blockItem, ..., "}"]` | 用 `findChildren("blockItem")` 取全部语句项 |
| `blockItem` | `[decl]` 或 `[stmt]` | |
| `stmt` | ① `[lVal, "=", exp, ";"]`<br>② `[exp, ";"]` 或 `[";"]`<br>③ `[block]`<br>④ `["if", "(", cond, ")", stmt]` 或 `["if", "(", cond, ")", stmt, "else", stmt]`<br>⑤ `["return", ";"]` 或 `["return", exp, ";"]` | 通过 `children[0]->type` 唯一区分 5 种语句；`if` 中用 `findChildren("stmt")` 长度 1 或 2 判断有无 `else` |
| `exp` | `[addExp]` | |
| `cond` | `[lOrExp]` | |
| `lVal` | `[Ident]` | `lVal->getText()` 为变量名 |
| `primaryExp` | `["(", exp, ")"]` 或 `[lVal]` 或 `[number]` | |
| `number` | `[IntConst]` 或 `[floatConst]` | 看 `children[0]->type` 区分整型/浮点字面量 |
| `unaryExp` | ① `[primaryExp]`<br>② `[Ident, "(", (funcRParams)?, ")"]`<br>③ `[unaryOp, unaryExp]` | 通过 `children[0]->type` 区分；无实参时不含 `funcRParams` |
| `unaryOp` | `["+"]` 或 `["-"]` 或 `["!"]` | `unaryOp->getText()` 为操作符 |
| `funcRParams` | `[funcRParam, ",", funcRParam, ...]` | 用 `findChildren("funcRParam")` 取全部实参 |
| `funcRParam` | `[exp]` | |
| `mulExp` | `[unaryExp]` 或 `[mulExp, "*"\|"/"\|"%", unaryExp]` | `children.size() == 1` 或 `3` |
| `addExp` | `[mulExp]` 或 `[addExp, "+"\|"-", mulExp]` | `children.size() == 1` 或 `3` |
| `relExp` | `[addExp]` 或 `[relExp, "<"\|">"\|"<="\|">=", addExp]` | `children.size() == 1` 或 `3` |
| `eqExp` | `[relExp]` 或 `[eqExp, "=="\|"!=", relExp]` | `children.size() == 1` 或 `3` |
| `lAndExp` | `[eqExp]` 或 `[lAndExp, "&&", eqExp]` | `children.size() == 1` 或 `3` |
| `lOrExp` | `[lAndExp]` 或 `[lOrExp, "||", lAndExp]` | `children.size() == 1` 或 `3` |
| `constExp` | `[addExp]` | |

---

## 5. 接口 C：成员 3 & 4 中端 IR 层（`include/IRGenerator.h` & `src/ir/`，**草案 / DRAFT**）

> **当前状态为草案**：因为尚未拿到课程仓库 `compiler_ir` 源码，目前仅冻结顶层调用入口 `IRGenerator::generate(const ASTNode*)`。`src/ir/ir_visitor.h` 中的 `visit*` 方法目前为 `void` 占位，**拿到 `compiler_ir` 后需由成员 3/4 先核对仓库真实 API（如 `Value*` 返回值、`Module::print()`、是否自带 Visitor 基类），再共同定稿 `ir_visitor.h`**，然后分别在 `ir_basic.cpp` 和 `ir_ctrl.cpp` 并行实现。

1. **`compiler_ir` 接入**：拿到源码后放至 `third_party/compiler_ir/`，在群里同步头文件路径与编译要求，成员 1 协助接入 `build.bat` / `Makefile` / `CMakeLists.txt`。
2. **SysY 运行时库预声明**：在全局符号表中预先声明任务书要求的运行时函数（`@getint()`、`@putint(i32)`、`@getch()`、`@putch(i32)`、`@getfloat()`、`@putfloat(float)` 等）。
3. **不等成员 2 即可单测 IR（Mock AST 示例）**：
   ```cpp
   using cmm::ASTNode;
   auto prog = ASTNode::makeNode("Program");
   auto cu = prog->addChild(ASTNode::makeNode("compUnit"));
   auto fn = cu->addChild(ASTNode::makeNode("funcDef"));
   auto ft = fn->addChild(ASTNode::makeNode("funcType"));
   ft->addChild(std::make_unique<ASTNode>("int", "int", 1, 1, true));
   fn->addChild(std::make_unique<ASTNode>("Ident", "main", 1, 5, true));
   // ... 按第 4.2 节表继续挂载 "(", ")", block ...
   cmm::IRGenerator gen;
   auto irRes = gen.generate(prog.get());
   ```
