#pragma once

#include "Token.h"

#include <ostream>
#include <string>
#include <vector>

namespace cmm {

struct LexResult {
    std::vector<Token> rawTokens;      // 全部词法输出（含 ERROR，用于严格按格式打印）
    std::vector<Token> parserTokens;   // 供成员 2 语法分析器使用的纯净 Token 流（已剔除 ERROR，末尾带 EOF）
    std::vector<LexSymbolEntry> symbols;
    std::vector<std::string> errors;
};

class Lexer {
public:
    Lexer();
    ~Lexer();

    // 对 C-- 源码执行 NFA -> DFA -> 最小化 DFA 扫描
    LexResult tokenize(const std::string& source) const;

    // 按大作业规定格式输出词法序列：[词素]\t<[种别],[内容]>
    static void printTokens(const LexResult& result, std::ostream& os);

    // 输出词法阶段收集的标识符符号表
    static void printSymbolTable(const LexResult& result, std::ostream& os);

    // 导出自动机三阶段信息（用于开发报告第 1 部分）
    bool dumpAutomata(std::ostream& os) const;
};

}  // namespace cmm
