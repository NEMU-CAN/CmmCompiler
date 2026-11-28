# 成员 1 开发报告：工程架构、模块接口与自动机词法分析器

> 本文档对应《2026 秋季编译原理大作业》**开发报告第 1 部分（词法分析器与整体工程设计）**，可直接合入小组最终开发报告。

---

## 1. 任务范围与职责

成员 1 负责整个 C-- 编译器前端的**工程底盘搭建、模块间接口契约设计**以及**任务 1：手写自动机词法分析器**，具体包括：

1. **项目工程与构建体系**：建立按成员职责隔离的目录结构与 Git 分支协作规范，编写跨平台构建脚本（Windows `build.bat`、Linux/macOS `Makefile`、`CMakeLists.txt`）与统一命令行驱动 `src/main.cpp`。
2. **模块接口契约**：编写公共头文件 `include/TokenConfig.h`、`include/Token.h`、`include/AST.h`、`include/Lexer.h`、`include/Parser.h`、`include/IRGenerator.h` 以及团队接口文档 `INTERFACE.md`，保障 4 名成员并行开发零合并冲突。
3. **自动机词法分析器**：在不借助 Lex/Flex 等生成工具的前提下，使用 C++17 完整手写实现 **正则解析 → Thompson NFA → 子集构造 DFA → 分区细化最小化 DFA**，覆盖大作业附录 1 全部 32 个终结符，维护词法符号表并按规定格式输出。
4. **自动化回归与契约测试**：编写跨平台验证脚本 `tests/verify_lexer.py` 与 C++ 接口契约单测 `tests/test_contracts.cpp`。

---

## 2. 整体工程架构与接口设计

### 2.1 编译器三段式流水线

```text
C-- 源码 (.sy)
   │
   ▼  [成员 1] Lexer::tokenize()      (src/lexer/)
LexResult { rawTokens, parserTokens, symbols, errors }
   │
   ▼  [成员 2] Parser::parse()        (src/parser/)
ParseResult { success, ASTNode* root, errors } + SLR 归约序列
   │
   ▼  [成员 3 & 4] IRGenerator::generate()  (src/ir/)
IRResult { success, irText (.ll), errors }
```

### 2.2 核心接口设计要点

- **配置与逻辑解耦（`include/TokenConfig.h`）**：将关键字、运算符、界符的种别码表集中管理，默认采用大作业任务书原表（KW `1..8`、OP `6..19`、SE `20..25`，单目 `!` 暂编 `26`），并支持编译宏 `-DCMM_USE_LAB3_CODES=1` 一键切换为平时实验 3 的连续编号表（KW `1..8`、OP `9..22`、SE `23..28`，`!` 为 `29`）。
- **面向语法分析的 Token 标准化（`include/Token.h`）**：
  - `lexeme`：保留源码原始词素（如 `"VoID"`、`"MaIn"`、`"3.14"`），用于词法格式输出与移进打印；
  - `symbol`：归一化后的附录 1 终结符名（关键字转小写，`main`/`MaIn` 按附录 1 规则 11 映射为 `"Ident"`，整数为 `"IntConst"`，浮点数为 `"floatConst"`，结尾附带 `"EOF"`），供成员 2 直接查 SLR 表；
  - `stackDisplay()`：供成员 2 打印归约序列的 `move` 行；
  - `normalizedText()`：关键字统一转小写（如 `"InT"` → `"int"`、`"MaIn"` → `"main"`），标识符保留原大小写，避免成员 3/4 在语义分析阶段受大小写混写干扰。
- **双通道 Token 输出（`include/Lexer.h`）**：`rawTokens` 保留词法错误 `<ERROR,行,列>` 用于 `--lex` 严格格式输出；`parserTokens` 自动剔除非法字符并追加唯一 `EOF` 记号，直接送入 `Parser::parse()`。
- **通用多叉语法树（`include/AST.h`）**：采用贴合任务书 Visitor 伪代码的 `ASTNode { type, text, line, col, terminal, children }`，提供 `makeNode()`、`makeTerminal(tok)`、`getText()`、`findChild()`、`findChildren()`、`hasChild()`，并在 `INTERFACE.md` 中冻结全部 33 个非终结符的子节点布局。

---

## 3. 词法分析器算法描述

词法分析器核心位于 `src/lexer/automata.h`、`src/lexer/automata.cpp` 和 `src/lexer/lexer.cpp`，采用 pImpl 模式将自动机结构封装在堆上（避免 Windows 默认 1 MB 栈空间溢出），每次初始化时实时执行三阶段自动机构造，扫描阶段仅驱动最小化 DFA。

### 3.1 词法规则表（共 25 条基础正则规则）

| 类别 | 正则表达式 / 规则说明 | 对应附录 1 终结符 |
| :--- | :--- | :--- |
| 空白符（跳过） | `[ \t\r\n\v\f]+` | — |
| 标识符 / 关键字 | `[a-zA-Z_][a-zA-Z0-9_]*`（匹配后按 ASCII 小写查关键字表） | `Ident` / `int` `void` `return` `const` `float` `if` `else` |
| 浮点数 | `[0-9]+\.[0-9]+`（严格对齐附录 1 规则 36） | `floatConst` |
| 整数 | `[0-9]+`（对齐附录 1 规则 34） | `IntConst` |
| 单/双目运算符（15 条） | `+` `-` `*` `/` `%` `=` `>` `<` `==` `<=` `>=` `!=` `&&` `\|\|` `!` | 对应原文符号 |
| 界符（6 条） | `(` `)` `{` `}` `;` `,` | 对应原文符号 |

> **优先级约定**：运行时优先遵循**最长匹配（Maximal Munch）**原则（例如 `!=` 优先于 `!`，`==` 优先于 `=`，`3.14` 优先于 `3`）；长度相同时，规则表位置靠前的优先。

### 3.2 阶段一：正则解析与 Thompson NFA 构造（`buildNFA`）

编写手写递归下降正则表达式解析器（`parseUnion → parseConcat → parseRepeat → parseAtom`），支持并（`|`）、连接、闭包（`*`、`+`、`?`）、括号分组、字符类（`[a-z]` 等范围与转义）：
1. 为每条规则构造独立的 Thompson NFA 片段 `(start, end)`，在终点状态 `end` 标记接受规则编号 `accept = r`；
2. 建立统一的总起始状态链，通过 ε-转移并联全部 25 条规则片段；
3. 本工程最终构造出 **103 个 NFA 状态**（起始状态为 `0`）。

### 3.3 阶段二：子集构造法 NFA → DFA 确定化（`determinize`）

使用 1024 位位图 `StateSet`（16 个 `uint64_t`）表示 NFA 状态子集，以队列式工作表展开全部可达子集：

```text
算法 determinize(NFA):
  startSet ← epsilonClosure({ NFA.start })
  DFA.start ← internSubset(startSet)          // 记录子集并计算接受标签（最小规则号优先）
  for i = 0 to DFA.count - 1:
    for c ∈ ASCII(0..127):
      moved ← ∅
      for each s ∈ DFA.subsets[i]:
        if s 是字符转移状态 且 s.chars[c] == 1:
          moved ← moved ∪ { s.out[0] }
      nextSet ← epsilonClosure(moved)
      DFA.transitions[i][c] ← internSubset(nextSet)   // 空集自动映射为显式死状态 dead
```

本工程确定化后生成 **31 个 DFA 状态**（起始状态 `0`，死状态 `1`）。

### 3.4 阶段三：带规则标签的分区细化 DFA 最小化（`minimizeDFA`）

普通单语言最小化只区分“接受/非接受”，但词法分析器必须保留不同词法规则的区分，因此初始划分按 `accept[s]` 精确分组：

```text
算法 minimizeDFA(DFA):
  1. 初始划分：将具有相同 accept[s] 标签的状态归入同一等价类（死状态、非接受中间态与 25 类接受态各自独立）。
  2. 迭代细化：
     repeat:
       若两个状态 a, b 当前属于同一等价类，且对所有 c ∈ ASCII(0..127) 都有
       partition[trans[a][c]] == partition[trans[b][c]]，则下一轮仍归为同类；
       否则拆分到新等价类。
     until 等价类总数不再增加
  3. 取每个等价类的代表状态，重建最小化 DFA 的转移表、起始状态与死状态。
```

本工程最小化结果：**NFA 103 状态 → DFA 31 状态 → 最小化 DFA 30 状态**（起始状态 `0`，死状态 `1`，25 个接受状态分别对应 25 条词法规则，另有 3 个非接受中间状态用于识别 `&&`、`||` 及浮点数小数点过渡）。

### 3.5 阶段四：扫描、符号表与错误恢复（`Lexer::tokenize`）

1. **换行归一化**：扫描前将 `\r\n` 和孤立 `\r` 统一归一化为 `\n`，精确维护 1-based 行号与列号。
2. **最长匹配扫描**：从当前位置 `pos` 出发沿最小化 DFA 推进，遇死状态 `dead` 或非 ASCII 字节停止；回退到最后一次经过接受态的位置 `lastEnd`。
3. **关键字与符号表填写**：
   - 若匹配 `IDN`，将其转为 ASCII 小写查 `TokenCodeTable::keywords()`；命中则输出 `<KW,编号>`（`main` 的语法 `symbol` 归一化为 `"Ident"`），否则输出 `<IDN,原词素>` 并写入词法符号表 `LexResult::symbols`（记录唯一 `ID`、名称、首次出现行列号及出现次数）。
4. **词法错误恢复**：若未能命中任何接受状态（如 `@`、`#`、`$`、`^`、孤立 `&`/`|`、`:`、`?`），输出 `[字符]\t<ERROR,行,列>` 到 `rawTokens` 并记录错误信息到 `errors`，然后前进 1 字节继续扫描，不中断后续词法分析，也不污染给语法分析器的 `parserTokens`。

---

## 4. 输出格式说明

### 4.1 词法单词序列输出格式

严格遵循大作业规定格式：
```text
[待测代码中的单词符号]\t<[单词符号种别],[单词符号内容]>
```

默认配置（大作业任务书原表）输出示例：
- 关键字：`int\t<KW,1>`、`VoID\t<KW,2>`、`MaIn\t<KW,5>`、`FLOAT\t<KW,6>`
- 运算符：`+\t<OP,6>`、`=\t<OP,11>`、`!=\t<OP,17>`、`&&\t<OP,18>`、`!\t<OP,26>`
- 界符：`(\t<SE,20>`、`)\t<SE,21>`、`{\t<SE,22>`、`}\t<SE,23>`、`;\t<SE,24>`、`,\t<SE,25>`
- 标识符 / 整数 / 浮点数：`a\t<IDN,a>`、`10\t<INT,10>`、`3.14\t<FLOAT,3.14>`
- 非法字符：`@\t<ERROR,1,28>`

### 4.2 词法符号表输出格式（`--symbols`）

```text
ID	Name	FirstLine	FirstCol	Count
1	a	2	9	3
2	b	3	9	2
3	pi	4	11	1
```

---

## 5. 源程序编译与运行步骤

### 5.1 编译环境与命令

- **语言标准**：C++17（测试通过环境：Windows TDM-GCC `g++ 9.2.0`，编译选项 `-std=c++17 -O2 -Wall -Wextra`，**零警告零错误**）。
- **Windows 编译**：
  ```bat
  build.bat
  ```
  如需切换为平时实验 3 的连续编号表进行比对：
  ```bat
  build.bat -DCMM_USE_LAB3_CODES=1
  ```
- **Linux / macOS / WSL 编译**：
  ```bash
  make
  ```
- **CMake 编译**：
  ```bash
  cmake -B build && cmake --build build
  ```

### 5.2 运行命令

```bash
# 1. 输出规定格式的词法单词序列
./build/cmmc.exe --lex tests/custom/unary_not.sy

# 2. 同时输出词法符号表，并导出 NFA / DFA / 最小化 DFA 完整 JSON 数据
./build/cmmc.exe --lex --symbols --dump-automata build/automata.json tests/custom/unary_not.sy

# 3. 导出结果到指定文件
./build/cmmc.exe --lex tests/lexer_lab3/test1.sy -o build/test1.lex

# 4. 运行自动化回归与接口契约测试（跨平台）
python tests/verify_lexer.py
```

---

## 6. 测试与验证结果

### 6.1 自动化回归验证汇总（`python tests/verify_lexer.py`）

| 测试项 | 覆盖内容 | 验证结果 |
| :--- | :--- | :--- |
| 默认编译（`build.bat` / `make`） | 全仓 C++17 严格告警编译（`-Wall -Wextra`） | **PASS（0 警告 0 错误）** |
| 默认大作业原表验证（`unary_not.sy`） | `=`(`<OP,11>`)、`;`(`<SE,24>`)、`!`(`<OP,26>`)、`!=`(`<OP,17>`)、浮点、符号表 | **PASS** |
| 自动机状态压缩验证 | `--dump-automata` 导出校验 | **PASS（NFA 103 → DFA 31 → MinDFA 30）** |
| `--strict-lex` 严格模式 | 遇到 `test2.sy` 非法字符中止后续解析 | **PASS** |
| Lab 3 参考表逐字节比对（`-DCMM_USE_LAB3_CODES=1`） | `test1.sy`（50 行）、`test2.sy`（129 行）、`test3.sy`（149 行，含大小写混写与 6 处非法字符） | **全部 PASS（逐字节一致）** |
| C++ 接口契约单测（`test_contracts.cpp`） | `Token::symbol` / `stackDisplay()` / `normalizedText()`、`ASTNode::makeTerminal()`、移动语义及 SLR 联调 | **PASS** |

### 6.2 典型测试输出示例（`tests/custom/unary_not.sy`）

输入源码：
```c
int main() {
    int a = !0;
    int b = 3;
    float pi = 3.14;
    if (!(a != b) && !a) {
        return 1;
    }
    return 0;
}
```

实际运行 `./build/cmmc.exe --lex --symbols tests/custom/unary_not.sy` 节选输出：
```text
int	<KW,1>
main	<KW,5>
(	<SE,20>
)	<SE,21>
{	<SE,22>
int	<KW,1>
a	<IDN,a>
=	<OP,11>
!	<OP,26>
0	<INT,0>
;	<SE,24>
...
float	<KW,6>
pi	<IDN,pi>
=	<OP,11>
3.14	<FLOAT,3.14>
;	<SE,24>
if	<KW,7>
(	<SE,20>
!	<OP,26>
(	<SE,20>
a	<IDN,a>
!=	<OP,17>
b	<IDN,b>
)	<SE,21>
&&	<OP,18>
!	<OP,26>
a	<IDN,a>
)	<SE,21>
...
=== Symbol Table ===
ID	Name	FirstLine	FirstCol	Count
1	a	2	9	3
2	b	3	9	2
3	pi	4	11	1
```

---

## 7. 交付文件清单

- 公共接口：`include/TokenConfig.h`、`include/Token.h`、`include/AST.h`、`include/Lexer.h`、`include/Parser.h`、`include/IRGenerator.h`
- 词法核心：`src/lexer/automata.h`、`src/lexer/automata.cpp`、`src/lexer/lexer.cpp`
- 工程驱动与构建：`src/main.cpp`、`build.bat`、`Makefile`、`CMakeLists.txt`、`.gitattributes`
- 接口文档与验证：`INTERFACE.md`、`tests/verify_lexer.py`、`tests/test_contracts.cpp`、`tests/custom/unary_not.sy`、`MEMBER1_REPORT.md`
