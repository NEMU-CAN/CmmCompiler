#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cmm {

// ============================================================================
// 成员 2 -> 成员 3/4 的核心数据契约（通用多叉语法树）
//
// 设计原则：
// 1. 严格贴合任务书第 3~4 页的 Visitor 伪代码风格：
//      ctx->children[i]->type / ctx->getText()
// 2. 非终结符节点的 type 统一使用附录 1 文法中的精确名称：
//      "Program", "compUnit", "decl", "constDecl", "bType", "constDef",
//      "constInitVal", "varDecl", "varDef", "initVal", "funcDef", "funcType",
//      "funcFParams", "funcFParam", "block", "blockItem", "stmt", "exp",
//      "cond", "lVal", "primaryExp", "number", "unaryExp", "unaryOp",
//      "funcRParams", "funcRParam", "mulExp", "addExp", "relExp", "eqExp",
//      "lAndExp", "lOrExp", "constExp"
// 3. 终结符节点的 type 使用归一化终结符名（与 Token::symbol 一致），
//    text 保存源码词素（如 "Ident" 节点 text="a"，"IntConst" 节点 text="10"）。
// ============================================================================
struct ASTNode {
    std::string type;  // 文法符号名（非终结符或终结符）
    std::string text;  // 终结符文本（非终结符可留空）
    int line = 0;      // 源码行号（用于语义报错定位）
    int col = 0;       // 源码列号
    std::vector<std::unique_ptr<ASTNode>> children;

    ASTNode() = default;
    ASTNode(std::string nodeType, std::string nodeText = "", int ln = 0, int cl = 0)
        : type(std::move(nodeType)), text(std::move(nodeText)), line(ln), col(cl) {}

    bool isTerminal() const { return children.empty(); }

    // 兼容任务书伪代码中的 ctx->getText()
    std::string getText() const {
        if (!text.empty() || children.empty()) {
            return text;
        }
        std::string merged;
        for (const auto& ch : children) {
            if (ch) merged += ch->getText();
        }
        return merged;
    }

    ASTNode* addChild(std::unique_ptr<ASTNode> child) {
        children.push_back(std::move(child));
        return children.back().get();
    }

    // 调试打印整棵 AST
    void dump(std::ostream& os, int indent = 0) const {
        for (int i = 0; i < indent; ++i) os << "  ";
        os << type;
        if (!text.empty()) os << " (" << text << ")";
        if (line > 0) os << " [L" << line << ":" << col << "]";
        os << "\n";
        for (const auto& ch : children) {
            if (ch) ch->dump(os, indent + 1);
        }
    }
};

}  // namespace cmm
