# 成员 4 工作说明（IR 控制流）

分支：`feature-part4`（基于 `feature-part3`）

## 1. 交付内容

| 文件 | 说明 |
| :--- | :--- |
| `src/ir/ir_ctrl.cpp` | if / if-else 基本块、比较运算、`&&`/`||` 短路求值、浮点比较与类型转换 |
| `src/ir/ir_visitor.h` | 新增成员 4 的公共辅助：`toBool()`、`convertValue()` |
| `tests/member4_ir_smoke.cpp` | 用 Mock AST 验证上述功能的冒烟测试 |

## 2. 实现说明

### 2.1 比较运算

- 关系比较 `<` `>` `<=` `>=` → `icmp slt/sgt/sle/sge`；任一操作数为 float 时改用 `fcmp olt/ogt/ole/oge`，另一侧用 `sitofp` 提升。
- 相等比较 `==` `!=` → `icmp eq/ne`（浮点用 `fcmp oeq/one`）。
- 比较结果统一为 `i1`，`lastType_ = "bool"`；布尔值参与算术比较时用 `zext` 转 `i32`。

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
- **两个分支都 return** 时不再生成 `if.end` 块，避免出现没有终结指令的空基本块；
- 嵌套 if 通过 `tempIndex_` 生成全局唯一标签。

## 3. 构建与验证

```bash
# 冒烟测试（Mock AST，不依赖 parser）
g++ -std=c++17 -Iinclude -Isrc/ir \
    src/ir/ir_basic.cpp src/ir/ir_ctrl.cpp src/ir/ir_generator.cpp \
    tests/member4_ir_smoke.cpp -o build/m4test
./build/m4test

# 完整编译器
build.bat          # Windows
make               # Linux/macOS/WSL
```

测试断言覆盖：`icmp sgt/slt/eq`、`fcmp olt`、`br i1`、`land.end`/`lor.end` 短路块、`load i1`。

## 4. 待联调

- `third_party/compiler_ir` 尚未提供，当前 IR 由文本方式发射（与成员 3 的 `ir_basic.cpp` 保持一致）；
- 拿到 `compiler_ir` 后，`llvmType()`/`emit()` 可替换为 IRBuilder 调用，控制流结构不变；
- 成员 2 的 parser 完成后，可用 `./build/cmmc --ir tests/parser_lab4/test1.sy -o out.ll` 做端到端验证。
