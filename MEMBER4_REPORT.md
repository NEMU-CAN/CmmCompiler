# 成员 4 工作说明（IR 控制流）

分支：`feature-part4`（基于 `feature-part3`）

## 1. 交付内容

| 文件 | 说明 |
| :--- | :--- |
| `src/ir/ir_ctrl.cpp` | if / if-else 基本块、比较运算、`&&`/`||` 短路求值、浮点比较与类型转换 |
| `src/ir/ir_visitor.h` | 新增成员 4 的公共辅助：`toBool()`、`convertValue()` |
| `tests/member4_ir_smoke.cpp` | Mock AST 冒烟测试：if/else、无 else、嵌套 if、`!` 条件、int/float 混合比较 |
| `tests/verify_ir_ctrl.py` | 一键验证：编译 → 运行 → `llvm-as` 校验 IR 合法性 |

## 2. 实现说明

### 2.1 比较运算

- 关系比较 `<` `>` `<=` `>=` → `icmp slt/sgt/sle/sge`；任一操作数为 float 时改用 `fcmp olt/ogt/ole/oge`，另一侧用 `sitofp` 提升。
- 相等比较 `==` `!=` → `icmp eq/ne`（浮点用 `fcmp oeq/one`）。
- 比较结果统一为 `i1`，`lastType_ = "bool"`；布尔值参与比较时用 `zext` 转 `i32`。

### 2.2 `&&` / `||` 短路求值

用"结果槽 + 条件跳转"实现，不依赖前驱基本块标签（避免 phi 需要前驱信息）：

```llvm
%slot = alloca i1
store i1 false, i1* %slot        ; && 初值 false；|| 初值为 true
br i1 %lhs, label %land.rhs.N, label %land.end.N
land.rhs.N:
  ... 求值 rhs ...
  store i1 %rhs, i1* %slot
  br label %land.end.N
land.end.N:
  %res = load i1, i1* %slot
```

`||` 把跳转方向与初值反过来（lhs 为真直接到 end）。

### 2.3 if / if-else

```llvm
br i1 %cond, label %if.then.N, label %if.else.N   ; 无 else 时直接跳 if.end.N
if.then.N:
  ...
  br label %if.end.N
if.else.N:
  ...
  br label %if.end.N
if.end.N:
```

- 分支以 `return` 结束（`terminated_ == true`）时不再补 `br`；
- **有 else 且两个分支都 return** 时不生成 `if.end` 块（所有路径已终结，避免空基本块）；
- **无 else** 时 false 分支会落到 if 之后，因此即使 then 分支 return 也一定生成 `if.end`；
- 嵌套 if 通过 `tempIndex_` 生成全局唯一标签。

### 2.4 兼容性

各 `visit*` 入口先按节点真实类型分发（`relExp`/`eqExp`/`addExp`/`mulExp`/`unaryExp`/`primaryExp`），
因此语法分析器给出扁平化或过度嵌套的 AST 时也能正确求值。

## 3. 构建与验证

```bash
# 一键验证（编译 + 运行 + llvm-as 校验）
python tests/verify_ir_ctrl.py

# 或手动
g++ -std=c++17 -Iinclude -Isrc/ir \
    src/ir/ir_basic.cpp src/ir/ir_ctrl.cpp src/ir/ir_generator.cpp \
    tests/member4_ir_smoke.cpp -o build/m4test
./build/m4test
llvm-as build/member4_ir.ll -o /dev/null
```

测试覆盖：

| 用例 | 覆盖点 |
| :--- | :--- |
| `main` | if/else、`&&`、`\|\|`、float 常量比较 |
| `early` | if 无 else + then 分支 return |
| `mixed` | `int < float` 混合比较（`sitofp`） |
| `nested` | 嵌套 if、`!` 条件 |

验证环境：LLVM 18.1.3 `llvm-as`，生成的 4 个函数 IR **全部合法**。

## 4. 浮点支持的收尾修复（fix/ir-float 分支）

端到端联调时发现 `src/ir/ir_basic.cpp` 有两处会导致浮点程序生成非法 IR，已一并修复：

- 四则运算按操作数类型选择 `fadd/fsub/fmul/fdiv`，int/float 混用时插入 `sitofp`
- 浮点常量用 `%.17g` 打印精确值（LLVM 要求 float 常量精确可表示，
  `3.14` 需输出为 `3.1400001049041748`）

修复后 `tests/` 下全部 8 个 `.sy` 用例（含 `custom/unary_not.sy`）生成的 IR 均通过 `llvm-as` 校验。
- `third_party/compiler_ir` 尚未提供，当前 IR 由文本方式发射；
  拿到库之后 `emit()` / `llvmType()` 可替换为 IRBuilder 调用，控制流结构不变。
- 端到端 `--ir` 验证依赖成员 2 的 parser（当前仓库仍是 `parser_stub.cpp`）。
