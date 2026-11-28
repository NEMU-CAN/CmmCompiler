# SLR 语法分析

在项目根目录运行：

```sh
make
./build/cmmc --parse --dump-slr build/slr.txt tests/parser_lab4/test1.sy
./build/cmmc --ast tests/parser_lab4/test1.sy
python3 tests/verify_lexer.py
```

`slr.txt` 中的 `sN` 表示移进到状态 N，`rN` 表示用第 N 条产生式归约，`acc` 表示接受。分析时用同一个 EOF 完成移进和最后的查表，移进 EOF 不打印，也不保留在 AST 中。

归约行记录归约后的栈顶符号和当前输入符号。Lab 4 的 `.ref` 记录的是 Bison 样例的标签和输出顺序，不能与这里的 SLR 过程逐行对照。Lab 4 的 `test2.sy` 在第 14 行写了 `x = x > y;`，本分析器按 `exp -> addExp` 将它报为语法错误。

普通算术括号仍生成 `primaryExp` 的 `[(, exp, )]`。为支持 `if (!(a != b))` 一类的条件，括号内含比较或逻辑运算时，中间的子节点为 `cond`，其下仍为 `lOrExp`。这种括号条件只允许用在条件中，不能用作赋值右侧、返回值或实参。
