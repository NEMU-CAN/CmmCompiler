# CmmCompiler 团队协作与模块接口契约（v1.0）

> 本文档面向 4 位组员，旨在让所有人**无需互相等待**即可并行开发。凡改动 `include/` 下的公共头文件，必须在群里同步并由成员 1 统一合并。

---

## 1. 目录权限与 Git 分支约定（防合并冲突）

| 成员 | 负责目录（写权限） | Git 分支名 | 核心交付物 |
| :--- | :--- | :--- | :--- |
| **成员 1** | `include/`、`src/lexer/`、`src/main.cpp`、`Makefile`、`CMakeLists.txt` | `feat/lexer` | NFA→DFA→最小化 DFA 词法分析器 + 共享接口 |
| **成员 2** | `src/parser/`（替换 `parser_stub.cpp`） | `feat/parser` | 附录 1 纯 BNF 展开、SLR 表生成、归约输出与 AST 建树 |
| **成员 3** | `src/ir/`（如 `ir_basic.cpp`、`symbol_table.h`） | `feat/ir-basic` | 接入 `compiler_ir`、作用域符号表、变量/常量/函数/return/四则与单目运算 Visitor |
| **成员 4** | `src/ir/`（如 `ir_ctrl.cpp`）、`tests/` | `feat/ir-ctrl` | `if-else` 基本块、比较运算、`&&`/`\|\|` 短路、浮点处理与 `.ll` 验证 |

**协作铁律**：
1. 每个人**只改自己目录里的文件**，严禁私自改写 `include/*.h` 中已冻结的结构体字段名。
2. 每次提交前先跑 `make`（或 CMake）确保整仓编译通过。
3. 成员 3/4 拿到 `compiler_ir` 源码后放到 `third_party/compiler_ir/`，如其中自带 AST/Visitor 基类请第一时间在群里同步。

---

## 2. 编译与分阶段调试命令

```bash
# 使用 Make（Windows TDM-GCC / Linux 通用）
make

# 仅跑词法分析（成员 1 验收）
./build/cmmc.exe --lex tests/test1.sy
./build/cmmc.exe --lex --symbols tests/test1.sy

# 跑词法 + SLR 语法分析，输出归约序列（成员 2 验收）
./build/cmmc.exe --parse tests/test1.sy

# 打印语法树结构（成员 2 -> 成员 3/4 联调）
./build/cmmc.exe --ast tests/test1.sy

# 完整编译输出 LLVM IR（成员 3/4 验收）
./build/cmmc.exe --ir tests/test1.sy -o out.ll
```

---

## 3. 三大核心接口契约

### 接口 A：成员 1 → 成员 2（`include/Token.h` & `include/Lexer.h`）

成员 2 调用 `Lexer::tokenize(source)` 后，直接读取 `LexResult::parserTokens`：
- `parserTokens` 已自动**剔除词法错误**并在末尾追加 `symbol == "EOF"` 的结束符。
- 成员 2 查 SLR 分析表时，**一律使用 `tok.symbol`**（不要用 `tok.lexeme`）：

| 词法类别 | 示例源码 | `tok.lexeme` | `tok.symbol`（供 SLR 查表） | 词法输出格式 |
| :--- | :--- | :--- | :--- | :--- |
| 关键字（除 `main`） | `VoID`、`InT`、`If` | `"VoID"` | 小写原文：`"void"`、`"int"`、`"if"`、`"else"`、`"const"`、`"float"`、`"return"` | `VoID\t<KW,2>` |
| **`main` 特殊说明** | `main`、`MaIn` | `"MaIn"` | **`"Ident"`**（因为附录 1 规则 11 为 `funcDef -> funcType Ident '(' ...`） | `MaIn\t<KW,5>` |
| 标识符 | `_X1` | `"_X1"` | `"Ident"` | `_X1\t<IDN,_X1>` |
| 整数 | `42` | `"42"` | `"IntConst"` | `42\t<INT,42>` |
| 浮点数 | `3.14` | `"3.14"` | `"floatConst"` | `3.14\t<FLOAT,3.14>` |
| 运算符 / 界符 | `==`、`!`、`(` | `"=="` | 原文：`"=="`、`"!"`、`"("` 等 | `==\t<OP,17>` |
| 结束符 | （文件末尾） | `""` | `"EOF"` | 不打印 |

> **如何不等成员 1 就开工？** 成员 2 可以直接在自己单测里手工构造 `std::vector<cmm::Token>`（填好 `symbol`、`lexeme`、`line`）喂给 `Parser::parse()`！

---

### 接口 B：成员 2 → 成员 3/4（`include/AST.h` & `include/Parser.h`）

成员 2 实现 `Parser::parse(tokens, reductionOut)`，成功时返回 `std::unique_ptr<ASTNode>`（根节点 `type == "Program"`）：

```cpp
struct ASTNode {
    std::string type;  // 文法符号名（严格使用附录 1 名称，见下方约定）
    std::string text;  // 终结符源码词素（非终结符留空）
    int line, col;
    std::vector<std::unique_ptr<ASTNode>> children;

    std::string getText() const; // 兼容任务书第 3-4 页伪代码
};
```

**AST 节点命名硬性约定（2 号建树、3/4 号遍历必须完全一致）**：
1. **非终结符 `type`**：严格使用附录 1 的产生式左部名（区分大小写）：
   - `Program`, `compUnit`, `decl`, `constDecl`, `bType`, `constDef`, `constInitVal`, `varDecl`, `varDef`, `initVal`, `funcDef`, `funcType`, `funcFParams`, `funcFParam`, `block`, `blockItem`, `stmt`, `exp`, `cond`, `lVal`, `primaryExp`, `number`, `unaryExp`, `unaryOp`, `funcRParams`, `funcRParam`, `mulExp`, `addExp`, `relExp`, `eqExp`, `lAndExp`, `lOrExp`, `constExp`。
2. **EBNF 展开引入的辅助非终结符**：成员 2 展开 `*`/`?` 时引入的临时节点（如 `blockItemList`、`varDefList`），建树时建议**展平**回父节点的 `children` 中，使树形与附录 1 一致（例如 `block` 的 `children` 直接为 `{`, `blockItem`, ..., `}`）。
3. **终结符 `type` 与 `text`**：`type` 填 `tok.symbol`（如 `"Ident"`、`"IntConst"`、`"+"`、`";"`），`text` 填 `tok.lexeme`（如 `"a"`、`"10"`、`"+"`）。

> **如何不等成员 2 就开工？** 成员 3/4 可以写一个测试函数手工 `new ASTNode` 拼出 `int main() { return 0; }` 的 AST 树，直接喂给 `IRGenerator::generate(root)` 验证 `.ll` 输出！

---

### 接口 C：成员 3/4 → 主程序（`include/IRGenerator.h`）

成员 3/4 共同实现 `IRGenerator::generate(const ASTNode* root)`，返回 `IRResult { success, irText, errors }`。建议内部按职责拆成两个 `.cpp` 文件避免冲突：
- `src/ir/ir_basic.cpp`（成员 3）：符号表、全局/局部声明、函数定义与传参、`return`、单目与加减乘除。
- `src/ir/ir_ctrl.cpp`（成员 4）：关系比较、`if-else` 基本块与分支、`&&`/`||` 短路求值、浮点类型转换与运算。
