#pragma once

#include "AST.h"
#include "Token.h"

#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace cmm {

struct ParseResult {
    bool success = false;
    std::unique_ptr<ASTNode> root;     // 成功时指向 Program 根节点
    std::vector<std::string> errors;   // 语法错误信息（含行号与期望符号）
};

// 成员 2 负责实现（src/parser/）
class Parser {
public:
    Parser() = default;
    ~Parser() = default;

    // 输入：Lexer::tokenize() 产出的 parserTokens（末尾含 EOF）
    // reductionOut：若非空，按大作业规定格式输出 SLR 分析步骤：
    //   [序号]\t[栈顶符号]#[面临输入符号]\t[reduction|move|accept|error]
    ParseResult parse(const std::vector<Token>& tokens, std::ostream* reductionOut = nullptr);

    // 导出 FIRST/FOLLOW 集合、LR(0) 项目集规范族与 SLR 分析表（用于开发报告第 2 部分）
    bool dumpSLRTable(std::ostream& os) const;
};

}  // namespace cmm
