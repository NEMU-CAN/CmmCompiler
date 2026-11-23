#include "Parser.h"
#include "slr.h"

namespace cmm {
namespace {

using Node = std::unique_ptr<ASTNode>;

struct StackEntry {
    int state;
    Node node;
    const ASTNode* conditionOp = nullptr;
};

bool isList(const std::string& type) {
    return type == "topItems" || type == "constDefList" || type == "varDefList" ||
           type == "blockItems" || type == "funcFParams" || type == "funcRParams";
}

bool flatten(const std::string& type) {
    return (isList(type) && type != "funcFParams" && type != "funcRParams") ||
           type == "optFParams" || type == "optRParams";
}

Node groupExpression(Node expr, bool isCondition) {
    if (isCondition) {
        auto cond = ASTNode::makeNode("cond");
        cond->addChild(std::move(expr));
        return cond;
    }
    // No comparison or logical operators: lOrExp -> lAndExp -> eqExp -> relExp -> addExp.
    auto* addExp = &expr;
    while ((*addExp)->type != "addExp") addExp = &(*addExp)->children.front();
    auto exp = ASTNode::makeNode("exp");
    exp->addChild(std::move(*addExp));
    return exp;
}

Node reduceNode(const parser::Production& prod, std::vector<StackEntry>& stack,
                std::size_t first, const Token& lookahead) {
    const auto& lhs = prod.lhs;
    Node node;
    // Reuse the list node when reducing a left-recursive list.
    if (!prod.rhs.empty() && prod.rhs.front() == lhs && isList(lhs)) {
        node = std::move(stack[first++].node);
    } else {
        node = ASTNode::makeNode(lhs);
    }

    for (std::size_t i = first; i < stack.size(); ++i) {
        auto child = std::move(stack[i].node);
        if (child->type == "EOF") continue;
        if (flatten(child->type)) {
            for (auto& item : child->children) node->addChild(std::move(item));
            continue;
        }
        if (lhs == "funcDef" && i == first) {
            if (child->type == "bType") {
                child->type = "funcType";
            } else {
                auto type = ASTNode::makeNode("funcType");
                type->addChild(std::move(child));
                child = std::move(type);
            }
        } else if (lhs == "primaryExp" && child->type == "lOrExp") {
            child = groupExpression(std::move(child), stack[i].conditionOp != nullptr);
        }
        node->addChild(std::move(child));
    }
    if (lhs == "compUnit" && node->children.empty()) {
        node->line = lookahead.line;
        node->col = lookahead.col;
    }
    return node;
}

std::string location(int line, int col) {
    return std::to_string(line) + ":" + std::to_string(col) + ": ";
}

}  // namespace

ParseResult Parser::parse(const std::vector<Token>& tokens, std::ostream* reductionOut) {
    using parser::ActionKind;
    const auto& table = parser::slrTable();
    ParseResult result;
    std::vector<StackEntry> stack;
    stack.push_back({0, nullptr});
    std::size_t step = 0;
    auto trace = [&](const std::string& top, const std::string& input, const char* action) {
        if (reductionOut) {
            *reductionOut << ++step << '\t' << top << '#' << input << '\t' << action << '\n';
        }
    };

    if (tokens.empty() || tokens.back().symbol != "EOF") {
        int line = tokens.empty() ? 1 : tokens.back().line;
        int col = tokens.empty() ? 1 : tokens.back().col;
        result.errors.push_back(location(line, col) + "expected EOF at end of token stream");
        trace("Program", "EOF", "error");
        return result;
    }

    std::size_t cursor = 0;
    for (;;) {
        const auto& tok = tokens[cursor];
        const auto& row = table.action[stack.back().state];
        auto it = row.find(tok.symbol);
        auto error = [&](std::string message) {
            const auto& top = stack.back().node;
            trace(top ? top->type : "Program", tok.symbol, "error");
            result.errors.push_back(std::move(message));
        };

        if (it == row.end()) {
            std::string message = location(tok.line, tok.col) + "unexpected '" +
                                  (tok.lexeme.empty() ? tok.symbol : tok.lexeme) + "'; expected ";
            const char* separator = "";
            for (const auto& entry : row) {
                message += separator + ("'" + entry.first + "'");
                separator = ", ";
            }
            error(std::move(message));
            return result;
        }

        const auto& action = it->second;
        if (action.kind == ActionKind::Accept) {
            result.root = std::move(stack.back().node);
            result.success = true;
            trace("Program", "EOF", "accept");
            return result;
        }
        if (action.kind == ActionKind::Shift) {
            if (tok.symbol != "EOF") {
                trace(tok.stackDisplay(), tok.lexeme, "move");
                ++cursor;
            } else if (cursor + 1 != tokens.size()) {
                error(location(tok.line, tok.col) + "expected EOF only at end of token stream");
                return result;
            }
            // After shifting EOF, keep it as lookahead for the final reductions and accept.
            stack.push_back({action.target, ASTNode::makeTerminal(tok)});
            continue;
        }

        const auto& prod = table.productions[action.target];
        auto first = stack.size() - prod.rhs.size();
        const ASTNode* conditionOp = nullptr;
        for (std::size_t i = first; i < stack.size() && !conditionOp; ++i) {
            conditionOp = stack[i].conditionOp;
        }
        if (!conditionOp && prod.rhs.size() == 3 &&
            (prod.lhs == "relExp" || prod.lhs == "eqExp" ||
             prod.lhs == "lAndExp" || prod.lhs == "lOrExp")) {
            conditionOp = stack[first + 1].node.get();
        }
        if (conditionOp && (prod.lhs == "exp" || prod.lhs == "constExp")) {
            error(location(conditionOp->line, conditionOp->col) + "unexpected '" +
                  conditionOp->text + "'; expected an arithmetic expression");
            return result;
        }
        if (prod.lhs == "funcDef") {
            const auto& type = stack[first].node;
            if (type->getText() == "float") {
                error(location(type->line, type->col) +
                      "invalid function return type 'float'; expected 'int' or 'void'");
                return result;
            }
        }

        auto node = reduceNode(prod, stack, first, tok);
        stack.resize(first);
        int next = table.goTo[stack.back().state].at(prod.lhs);
        stack.push_back({next, std::move(node), conditionOp});
        trace(prod.lhs, tok.symbol, "reduction");
    }
}

bool Parser::dumpSLRTable(std::ostream& os) const {
    return parser::slrTable().dump(os);
}

}  // namespace cmm
