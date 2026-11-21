#pragma once

#include <string>
#include <unordered_map>

namespace cmm {

// ============================================================================
// 词法种别码配置表（集中管理，严禁在业务逻辑中硬编码数字）
//
// 默认 CMM_USE_PDF_ORIGINAL_CODES == 0：
//   采用 lab3 官方参考输出（test1.ref ~ test3.ref）的连续无重叠编号：
//     KW: 1..8, OP: 9..22 (新增 '!' 为 29), SE: 23..28
// 若助教明确要求按大作业 PDF 第 2 页原表（KW 1..8 与 OP 6..19 重叠）：
//   编译时加 -DCMM_USE_PDF_ORIGINAL_CODES=1 即可一键切换。
// ============================================================================
#ifndef CMM_USE_PDF_ORIGINAL_CODES
#define CMM_USE_PDF_ORIGINAL_CODES 0
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
#if CMM_USE_PDF_ORIGINAL_CODES
        static const std::unordered_map<std::string, int> kMap = {
            {"+",  6},  {"-",  7},  {"*",  8},  {"/",  9},  {"%",  10},
            {"=",  11}, {">",  12}, {"<",  13}, {"==", 14}, {"<=", 15},
            {">=", 16}, {"!=", 17}, {"&&", 18}, {"||", 19}, {"!",  26},
        };
#else
        static const std::unordered_map<std::string, int> kMap = {
            {"+",  9},  {"-",  10}, {"*",  11}, {"/",  12}, {"%",  13},
            {"=",  14}, {">",  15}, {"<",  16}, {"==", 17}, {"<=", 18},
            {">=", 19}, {"!=", 20}, {"&&", 21}, {"||", 22}, {"!",  29},
        };
#endif
        return kMap;
    }

    static const std::unordered_map<std::string, int>& delimiters() {
#if CMM_USE_PDF_ORIGINAL_CODES
        static const std::unordered_map<std::string, int> kMap = {
            {"(", 20}, {")", 21}, {"{", 22}, {"}", 23}, {";", 24}, {",", 25},
        };
#else
        static const std::unordered_map<std::string, int> kMap = {
            {"(", 23}, {")", 24}, {"{", 25}, {"}", 26}, {";", 27}, {",", 28},
        };
#endif
        return kMap;
    }
};

}  // namespace cmm
