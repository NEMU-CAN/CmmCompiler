#pragma once

#include <string>
#include <unordered_map>

namespace cmm {

// ============================================================================
// 词法种别码配置表（集中管理，严禁在业务逻辑中硬编码数字）
//
// 默认（CMM_USE_LAB3_CODES == 0）：大作业任务书原表。
//   KW 1..8，OP 6..19，SE 20..25（KW 与 OP 编号有重叠，靠种别区分）。
//   任务书示例 `=\t<OP,11>`、`;\t<SE,24>` 与此表一致。
//
// 可选（编译时加 -DCMM_USE_LAB3_CODES=1）：平时实验 3 参考输出的连续编号。
//   KW 1..8，OP 9..22，SE 23..28。仅用于和 lab3 的 .ref 逐字节比对。
//
// 待助教确认：
//   1. 最终评测采用哪张表；
//   2. 单目运算符 '!' 两张表都没有给编号，这里暂定为各表 OP 的下一个空号
//      （原表 26，lab3 表 29）。
// ============================================================================
#ifndef CMM_USE_LAB3_CODES
#define CMM_USE_LAB3_CODES 0
#endif

struct TokenCodeTable {
    static const std::unordered_map<std::string, int>& keywords() {
        static const std::unordered_map<std::string, int> kMap = {
            {"int",    1},
            {"void",   2},
            {"return", 3},
            {"const",  4},
            {"main",   5},
            {"float",  6},
            {"if",     7},
            {"else",   8},
        };
        return kMap;
    }

    static const std::unordered_map<std::string, int>& operators() {
#if CMM_USE_LAB3_CODES
        static const std::unordered_map<std::string, int> kMap = {
            {"+",  9},  {"-",  10}, {"*",  11}, {"/",  12}, {"%",  13},
            {"=",  14}, {">",  15}, {"<",  16}, {"==", 17}, {"<=", 18},
            {">=", 19}, {"!=", 20}, {"&&", 21}, {"||", 22}, {"!",  29},
        };
#else
        static const std::unordered_map<std::string, int> kMap = {
            {"+",  6},  {"-",  7},  {"*",  8},  {"/",  9},  {"%",  10},
            {"=",  11}, {">",  12}, {"<",  13}, {"==", 14}, {"<=", 15},
            {">=", 16}, {"!=", 17}, {"&&", 18}, {"||", 19}, {"!",  26},
        };
#endif
        return kMap;
    }

    static const std::unordered_map<std::string, int>& delimiters() {
#if CMM_USE_LAB3_CODES
        static const std::unordered_map<std::string, int> kMap = {
            {"(", 23}, {")", 24}, {"{", 25}, {"}", 26}, {";", 27}, {",", 28},
        };
#else
        static const std::unordered_map<std::string, int> kMap = {
            {"(", 20}, {")", 21}, {"{", 22}, {"}", 23}, {";", 24}, {",", 25},
        };
#endif
        return kMap;
    }
};

}  // namespace cmm
