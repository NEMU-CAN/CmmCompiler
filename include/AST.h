#pragma once

#include "Token.h"

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
// 2. 非终结符节点的 type 严格使用附录 1 文法中的精确名称（详见 INTERFACE.md）。
// 3. 成员 2 构造终结符节点时务必调用 ASTNode::makeTerminal(tok)，保证：
//      - type == tok.symbol（归一化终结符名）
//      - text == tok.normalizedText()（关键字已转小写，如 "InT"->"int"、"MaIn"->"main"）
//      - terminal == true
// ============================================================================
struct ASTNode {
    std::string type;       // 文法符号名（非终结符或归一化终结符）
    std::string text;       // 终结符归一化文本（非终结符留空）
    int line = 0;           // 源码行号（1-based，用于语义报错定位）
    int col = 0;            // 源码列号（1-based）
    bool terminal = false;  // 显式区分终结符与空非终结符
    std::vector<std::unique_ptr<ASTNode>> children;

    ASTNode() = default;
    ASTNode(std::string nodeType, std::string nodeText = "", int ln = 0, int cl = 0, bool isTerm = false)
        : type(std::move(nodeType)),
          text(std::move(nodeText)),
          line(ln),
          col(cl),
          terminal(isTerm) {}

    // 推荐给成员 2 的工厂函数：构造非终结符节点
    static std::unique_ptr<ASTNode> makeNode(std::string nodeType, int ln = 0, int cl = 0) {
        return std::make_unique<ASTNode>(std::move(nodeType), "", ln, cl, false);
    }

    // 推荐给成员 2 的工厂函数：由 Token 构造终结符节点（自动处理大小写归一化）
    static std::unique_ptr<ASTNode> makeTerminal(const Token& tok) {
        return std::make_unique<ASTNode>(tok.symbol, tok.normalizedText(), tok.line, tok.col, true);
    }

    bool isTerminal() const { return terminal; }

    // 兼容任务书伪代码中的 ctx->getText()
    std::string getText() const {
        if (terminal || !text.empty()) {
            return text;
        }
        std::string merged;
        for (const auto& ch : children) {
            if (ch) merged += ch->getText();
        }
        return merged;
    }

    ASTNode* addChild(std::unique_ptr<ASTNode> child) {
        if (child && line == 0 && child->line > 0) {
            line = child->line;
            col = child->col;
        }
        children.push_back(std::move(child));
        return children.back().get();
    }

    // 便捷查询接口（推荐给成员 3/4，减少脆弱的魔数下标访问）
    const ASTNode* findChild(const std::string& childType) const {
        for (const auto& ch : children) {
            if (ch && ch->type == childType) return ch.get();
        }
        return nullptr;
    }

    std::vector<const ASTNode*> findChildren(const std::string& childType) const {
        std::vector<const ASTNode*> result;
        for (const auto& ch : children) {
            if (ch && ch->type == childType) result.push_back(ch.get());
        }
        return result;
    }

    bool hasChild(const std::string& childType) const {
        return findChild(childType) != nullptr;
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
