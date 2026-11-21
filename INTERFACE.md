# CmmCompiler 团队协作与模块接口契约（v1.1）

> 本文档是 4 位组员并行开发的**唯一接口基准**。请务必先完整阅读后再写代码；凡需改动 `include/` 下的公共头文件，必须在群里同步并由成员 1 统一合并。

---

## 1. 目录权限与 Git 分支约定（零合并冲突设计）

| 成员 | 独占修改文件（写权限） | Git 分支名 | 核心交付物 |
| :--- | :--- | :--- | :--- |
| **成员 1** | `include/`、`src/lexer/`、`src/main.cpp`、`build.bat`、`Makefile`、`CMakeLists.txt` | `feat/lexer` | NFA→DFA→最小化 DFA 词法分析器 + 共享接口 |
| **成员 2** | `src/parser/`（替换 `parser_stub.cpp`） | `feat/parser` | 附录 1 纯 BNF 展开、FIRST/FOLLOW、LR(0)、SLR 分析表、归约输出与 AST 建树 |
| **成员 3** | `src/ir/ir_basic.cpp`（可与 4 号共同扩展 `src/ir/ir_visitor.h`） | `feat/ir-basic` | 接入 `compiler_ir`、作用域符号表、变量/常量/函数/传参/赋值/return/单目与四则运算 |
| **成员 4** | `src/ir/ir_ctrl.cpp`、`tests/` | `feat/ir-ctrl` | `if-else` 基本块、比较运算、`&&`/`\|\|` 短路求值、浮点处理、全量 `.ll` 验证 |

> `src/ir/ir_generator.cpp` 已将 `IRGenerator::generate()` 接到 `ir::IRVisitor::run()`，3/4 号**无需修改此文件**，分别在 `ir_basic.cpp` 和 `ir_ctrl.cpp` 中开发即可彻底避免合并冲突。

---

## 2. 编译与分阶段调试命令

| 平台 / 工具 | 编译命令 | 可执行文件 |
| :--- | :--- | :--- |
| Windows（TDM-GCC / MinGW） | `build.bat`（支持附加宏，如 `build.bat -DCMM_USE_PDF_ORIGINAL_CODES=1`） | `build\cmmc.exe` |
| Linux / macOS / WSL / Git Bash | `make` | `build/cmmc` |
| CLion / VS Code CMake | 打开 `CMakeLists.txt` | `cmmc` |

分阶段运行命令：
```bash
# 1. 仅词法（成员 1 验收）
./build/cmmc.exe --lex tests/test1.sy
./build/cmmc.exe --lex --symbols --dump-automata build/automata.json tests/test1.sy

# 2. 词法 + SLR 语法归约输出 & 导出 SLR 分析表（成员 2 验收）
./build/cmmc.exe --parse --dump-slr build/slr.txt tests/test1.sy

# 3. 词法 + SLR 建树并打印 AST（成员 2 -> 成员 3/4 联调）
./build/cmmc.exe --ast tests/test1.sy

# 4. 完整编译输出 LLVM IR（成员 3/4 验收）
./build/cmmc.exe --ir tests/test1.sy -o out.ll
```

> **关于 `test2.sy` / `test3.sy` 中的非法字符**：`Lexer::tokenize()` 会保留含 `<ERROR,行,列>` 的 `rawTokens`（用于 `--lex` 严格输出），同时提供已跳过非法字符、末尾附带 `EOF` 的 `parserTokens` 给成员 2；在 `--parse`/`--ast`/`--ir` 模式下，`main.cpp` 会向 `stderr` 打印 `[Lexical Warning]` 但继续语法分析（与平时 Lab 4 行为一致）。若需遇到词法错误直接中断，加 `--strict-lex`。

---

## 3. 接口 A：成员 1 → 成员 2（`include/Token.h` & `include/Lexer.h`）

成员 2 直接消费 `LexResult::parserTokens`：
- **查 SLR 分析表**：**一律使用 `tok.symbol`**（绝不要用 `tok.lexeme`）。
- **构造终结符 AST 节点**：**一律调用 `ASTNode::makeTerminal(tok)`**（不要手动拼 `ASTNode`），它会自动处理大小写归一化并标记 `terminal = true`。

| 词法类别 | 示例源码 | `tok.lexeme` | `tok.symbol`（SLR 查表） | `tok.normalizedText()`（入 AST） | `--lex` 输出格式 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| 关键字（除 `main`） | `VoID`、`InT`、`FLOAT` | `"VoID"` | `"void"`、`"int"`、`"float"`、`"const"`、`"if"`、`"else"`、`"return"` | 同小写 | `VoID\t<KW,2>` |
| **`main` 特殊约定** | `main`、`MaIn` | `"MaIn"` | **`"Ident"`** | **`"main"`** | `MaIn\t<KW,5>` |
| 标识符 | `_X1`、`Pi` | `"_X1"` | `"Ident"` | `"_X1"`（保留原大小写） | `_X1\t<IDN,_X1>` |
| 整数 | `42` | `"42"` | `"IntConst"` | `"42"` | `42\t<INT,42>` |
| 浮点数 | `3.14` | `"3.14"` | `"floatConst"`（小写 `f`） | `"3.14"` | `3.14\t<FLOAT,3.14>` |
| 运算符 / 界符 | `==`、`!`、`(` | `"=="` | 原文（32 个终结符全覆盖） | 原文 | `==\t<OP,17>` |
| 结束符 | （文件尾） | `""` | `"EOF"` | `""` | 不打印 |

---

## 4. 接口 B：成员 2 → 成员 3/4（`include/AST.h` & `include/Parser.h`）

### 4.1 成员 2 必读：附录 1 在 SLR(1) 中的 2 处真实冲突与消解要求

1. **移进/归约冲突（悬空 `else`）**：
   - `stmt -> 'if' '(' cond ')' stmt ('else' stmt)?` 遇到 `'else'` 时按标准规则**移进优先**（与最近 `if` 匹配）。
2. **归约/归约冲突（`'int'` 后接 `Ident`）**：
   - `varDecl -> bType varDef ...`（`bType -> 'int'`）与 `funcDef -> funcType Ident '(' ...`（`funcType -> 'int'`）在看到 `'int'` 且面临 `Ident` 时无法用 1 字符向前看区分。
   - **推荐解法**：在内部纯 BNF 中将 `funcDef` 改写为共用 `bType` 或左提公因子（参考平时实验 4 做法：`funcDef -> bType Ident '(' optParams ')' block | 'void' Ident '(' optParams ')' block`，若函数返回类型为 `float` 则报语法错），但**建出的 AST 必须始终将 `funcDef` 的第 0 个子节点规范为 `funcType` 节点**（保证与下方冻结表 100% 一致，成员 3/4 无需关心 SLR 内部变形！）。

### 4.2 冻结的 AST 节点结构契约（全 33 个非终结符精确 `children` 布局）

> **铁律**：
> 1. 所有因展开 EBNF（`*`、`?`）引入的辅助列表节点（如 `varDefList`、`blockItemList`）**必须展平**，不得出现在最终 AST 中。
> 2. 所有标点与关键字终结符（`const`、`=`、`,`、`;`、`(`、`)`、`{`、`}`、`if`、`else`、`return` 及所有运算符）**全部保留在 `children` 中**，顺序与附录 1 产生式完全一致。
> 3. 成员 3/4 遍历时可配合 `node->findChild("...")` / `node->findChildren("...")` / `node->hasChild("...")` 编写健壮代码。

| 非终结符 `type` | 冻结的 `children` 顺序（严格对齐附录 1） | 关键说明（成员 3/4 查阅） |
| :--- | :--- | :--- |
| `Program` | `[compUnit]` | 唯一根节点 |
| `compUnit` | `[decl \| funcDef, ...]`（展平，不含 `EOF`） | 空文件时 `children` 为空 |
| `decl` | `[constDecl]` 或 `[varDecl]` | |
| `constDecl` | `["const", bType, constDef, ",", constDef, ..., ";"]` | 用 `findChildren("constDef")` 取全部定义 |
| `bType` | `["int"]` 或 `["float"]` | `bType->getText()` 必为小写 `"int"` 或 `"float"` |
| `constDef` | `[Ident, "=", constInitVal]` | `children[0]->text` 为常量名 |
| `constInitVal` | `[constExp]` | |
| `varDecl` | `[bType, varDef, ",", varDef, ..., ";"]` | 用 `findChildren("varDef")` 取全部定义 |
| `varDef` | `[Ident]` 或 `[Ident, "=", initVal]` | 用 `findChild("initVal")` 判断是否带初值 |
| `initVal` | `[exp]` | |
| `funcDef` | `[funcType, Ident, "(", (funcFParams)?, ")", block]` | **无参时不含 `funcFParams`**；用 `findChild("funcFParams")` 判断；`children[1]->text` 对 `MaIn` 已归一化为 `"main"` |
| `funcType` | `["void"]` 或 `["int"]` | `funcType->getText()` 必为小写 `"void"` 或 `"int"` |
| `funcFParams` | `[funcFParam, ",", funcFParam, ...]` | 用 `findChildren("funcFParam")` 取全部形参 |
| `funcFParam` | `[bType, Ident]` | |
| `block` | `["{", blockItem, ..., "}"]` | 用 `findChildren("blockItem")` 取全部语句项 |
| `blockItem` | `[decl]` 或 `[stmt]` | |
| `stmt` | ① `[lVal, "=", exp, ";"]`<br>② `[exp, ";"]` 或 `[";"]`<br>③ `[block]`<br>④ `["if", "(", cond, ")", stmt]` 或 `["if", "(", cond, ")", stmt, "else", stmt]`<br>⑤ `["return", ";"]` 或 `["return", exp, ";"]` | 通过 `children[0]->type` 即可唯一区分 5 种语句；`if` 中用 `findChildren("stmt")` 长度 1 或 2 判断有无 `else` |
| `exp` | `[addExp]` | |
| `cond` | `[lOrExp]` | |
| `lVal` | `[Ident]` | `lVal->getText()` 为变量名 |
| `primaryExp` | `["(", exp, ")"]` 或 `[lVal]` 或 `[number]` | |
| `number` | `[IntConst]` 或 `[floatConst]` | 看 `children[0]->type` 区分整型/浮点字面量 |
| `unaryExp` | ① `[primaryExp]`<br>② `[Ident, "(", (funcRParams)?, ")"]`<br>③ `[unaryOp, unaryExp]` | 通过 `children[0]->type` 区分（`"primaryExp"` / `"Ident"` / `"unaryOp"`）；无实参时不含 `funcRParams` |
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

## 5. 接口 C：成员 3 & 4 协作说明（`include/IRGenerator.h` & `src/ir/`）

1. **`compiler_ir` 接入**：拿到课程提供的 `compiler_ir` 后放至 `third_party/compiler_ir/`，在群里同步其真实头文件路径和类名，成员 1 协助将其接入 `build.bat` / `Makefile` / `CMakeLists.txt`。
2. **SysY 运行时库预声明**：别忘了在全局符号表中预先声明任务书要求的运行时函数（如 `@getint()`、`@putint(i32)`、`@getch()`、`@putch(i32)`、`@getfloat()`、`@putfloat(float)`）。
3. **不等成员 2 即可单测 IR（Mock AST 示例）**：
   ```cpp
   // 在成员 3/4 自己的测试文件里手工拼一棵 `int main() { return 0; }` 的 AST 即可直接测 IRGenerator：
   using cmm::ASTNode;
   auto prog = ASTNode::makeNode("Program");
   auto cu = prog->addChild(ASTNode::makeNode("compUnit"));
   auto fn = cu->addChild(ASTNode::makeNode("funcDef"));
   auto ft = fn->addChild(ASTNode::makeNode("funcType"));
   ft->addChild(std::make_unique<ASTNode>("int", "int", 1, 1, true));
   fn->addChild(std::make_unique<ASTNode>("Ident", "main", 1, 5, true));
   fn->addChild(std::make_unique<ASTNode>("(", "(", 1, 9, true));
   fn->addChild(std::make_unique<ASTNode>(")", ")", 1, 10, true));
   auto blk = fn->addChild(ASTNode::makeNode("block"));
   blk->addChild(std::make_unique<ASTNode>("{", "{", 1, 12, true));
   auto item = blk->addChild(ASTNode::makeNode("blockItem"));
   auto stmt = item->addChild(ASTNode::makeNode("stmt"));
   stmt->addChild(std::make_unique<ASTNode>("return", "return", 1, 14, true));
   // ... 挂载 exp(0) 和 ";" 与 "}" ...
   cmm::IRGenerator gen;
   auto irRes = gen.generate(prog.get());
   ```
