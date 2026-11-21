#pragma once

#include <string>

namespace cmm {

enum class TokenCategory {
    KW,     // 关键字（大小写不敏感）
    OP,     // 运算符
    SE,     // 界符
    IDN,    // 标识符
    INT,    // 整数字面量
    FLOAT,  // 浮点数字面量
    END,    // 输入结束符 EOF（供语法分析器查 SLR 表使用）
    ERROR   // 词法错误（不进入正常语法流，仅记录与输出）
};

inline std::string categoryToString(TokenCategory cat) {
    switch (cat) {
        case TokenCategory::KW:    return "KW";
        case TokenCategory::OP:    return "OP";
        case TokenCategory::SE:    return "SE";
        case TokenCategory::IDN:   return "IDN";
        case TokenCategory::INT:   return "INT";
        case TokenCategory::FLOAT: return "FLOAT";
        case TokenCategory::END:   return "EOF";
        case TokenCategory::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

// 成员 1 -> 成员 2 的核心数据契约
struct Token {
    std::string lexeme;     // 源码原始词素，如 "VoID"、"3.14"、"_X1"
    std::string symbol;     // 归一化后的附录 1 终结符名（成员 2 直接用此字段查 SLR 表）：
                            //   关键字 -> 小写原文（"int","void","if","else","return","const","float","main"）
                            //   运算符/界符 -> 原文（"+","==","&&","(",")",";",...）
                            //   标识符 -> "Ident"（注：main 按任务书同时为 KW,5，见下方约定）
                            //   整数   -> "IntConst"
                            //   浮点数 -> "floatConst"
                            //   结束符 -> "EOF"
    TokenCategory category = TokenCategory::ERROR;
    std::string attr;       // 输出属性：KW/OP/SE 为编号字符串（如 "1"），IDN/INT/FLOAT 为原文，ERROR 为 "line,col"
    int line = 1;           // 1-based 行号
    int col = 1;            // 1-based 列号

    // 按大作业规定格式格式化单行输出：[词素]\t<[种别],[内容]>
    std::string formatLine() const {
        return lexeme + "\t<" + categoryToString(category) + "," + attr + ">";
    }
};

// 词法阶段收集的标识符符号表项
struct LexSymbolEntry {
    int id = 0;
    std::string name;
    int firstLine = 1;
    int firstCol = 1;
    int count = 0;
};

}  // namespace cmm
