#include "ir_visitor.h"

#include <string>

namespace cmm {
namespace ir {

// ============================================================================
// 成员 4 独占实现文件：
//   - if / if-else 基本块创建与跳转
//   - cond、关系比较 (<, >, <=, >=)、相等比较 (==, !=)
//   - 逻辑与 (&&) / 逻辑或 (||) 短路求值
//   - 浮点类型转换与比较
//
// 约定：
//   * 比较/逻辑运算结果统一为 i1，lastType_ 记为 "bool"；
//   * 布尔值参与算术比较时按 0/1 的 i32 处理（zext）；
//   * 短路求值用 alloca i1 保存中间结果，避免依赖前驱基本块（phi 需要前驱标签）；
//   * 每个 visit* 先按节点真实类型分发，兼容语法分析器可能给出的扁平/嵌套 AST。
// ============================================================================

namespace {
const char* const BOOL_TYPE = "bool";
}  // namespace

std::string IRVisitor::toBool(const std::string& value, const std::string& type) {
    if (type == BOOL_TYPE) {
        return value;
    }
    std::string out = newTemp();
    if (type == "float") {
        emit(out + " = fcmp one float " + value + ", 0.000000e+00");
    } else {
        emit(out + " = icmp ne i32 " + value + ", 0");
    }
    return out;
}

std::string IRVisitor::convertValue(const std::string& value, const std::string& from,
                                    const std::string& to, const ASTNode* ctx) {
    if (from == to) {
        return value;
    }
    if (to == BOOL_TYPE) {
        return toBool(value, from);
    }
    if (from == BOOL_TYPE) {
        std::string asInt = newTemp();
        emit(asInt + " = zext i1 " + value + " to i32");
        if (to == "float") {
            std::string asFloat = newTemp();
            emit(asFloat + " = sitofp i32 " + asInt + " to float");
            return asFloat;
        }
        return asInt;
    }
    if (from == "float" && to == "int") {
        std::string out = newTemp();
        emit(out + " = fptosi float " + value + " to i32");
        return out;
    }
    if (from == "int" && to == "float") {
        std::string out = newTemp();
        emit(out + " = sitofp i32 " + value + " to float");
        return out;
    }
    addError(ctx, "cannot convert '" + from + "' to '" + to + "'.");
    return value;
}

// ---------------------------------------------------------------- 比较运算

void IRVisitor::visitRelExp(const ASTNode* ctx) {
    if (!ctx || ctx->children.empty()) {
        return;
    }
    // 允许其他表达式节点直接进入（分发到对应 visit*）
    if (ctx->type == "unaryExp") { visitUnaryExp(ctx); return; }
    if (ctx->type == "mulExp") { visitMulExp(ctx); return; }
    if (ctx->type == "addExp") { visitAddExp(ctx); return; }
    if (ctx->type == "primaryExp") { visitPrimaryExp(ctx); return; }
    if (ctx->type == "eqExp") { visitEqExp(ctx); return; }

    if (ctx->children.size() != 3) {
        const ASTNode* child = ctx->children[0].get();
        if (!child) {
            return;
        }
        if (child->type == "relExp") {
            visitRelExp(child);
        } else if (child->type == "mulExp") {
            visitMulExp(child);
        } else if (child->type == "unaryExp") {
            visitUnaryExp(child);
        } else if (child->type == "primaryExp") {
            visitPrimaryExp(child);
        } else {
            visitAddExp(child);
        }
        return;
    }
    visitRelExp(ctx->children[0].get());
    std::string lhs = lastValue_;
    std::string lhsType = lastType_;
    const std::string op = ctx->children[1]->getText();
    visitAddExp(ctx->children[2].get());
    std::string rhs = lastValue_;
    std::string rhsType = lastType_;

    if (!(op == "<" || op == ">" || op == "<=" || op == ">=")) {
        addError(ctx, "unknown relational operator '" + op + "'.");
        return;
    }
    const bool useFloat = (lhsType == "float" || rhsType == "float");
    const std::string target = useFloat ? "float" : "int";
    lhs = convertValue(lhs, lhsType, target, ctx);
    rhs = convertValue(rhs, rhsType, target, ctx);

    std::string pred;
    if (useFloat) {
        pred = op == "<" ? "olt" : op == ">" ? "ogt" : op == "<=" ? "ole" : "oge";
    } else {
        pred = op == "<" ? "slt" : op == ">" ? "sgt" : op == "<=" ? "sle" : "sge";
    }
    std::string out = newTemp();
    emit(out + " = " + (useFloat ? "fcmp " : "icmp ") + pred + " " + llvmType(target)
         + " " + lhs + ", " + rhs);
    lastValue_ = out;
    lastType_ = BOOL_TYPE;
}

void IRVisitor::visitEqExp(const ASTNode* ctx) {
    if (!ctx || ctx->children.empty()) {
        return;
    }
    if (ctx->type == "relExp") { visitRelExp(ctx); return; }
    if (ctx->type == "unaryExp") { visitUnaryExp(ctx); return; }
    if (ctx->type == "addExp") { visitAddExp(ctx); return; }
    if (ctx->type == "mulExp") { visitMulExp(ctx); return; }
    if (ctx->type == "primaryExp") { visitPrimaryExp(ctx); return; }

    if (ctx->children.size() != 3) {
        const ASTNode* child = ctx->children[0].get();
        if (!child) {
            return;
        }
        if (child->type == "eqExp") {
            visitEqExp(child);
        } else if (child->type == "relExp") {
            visitRelExp(child);
        } else if (child->type == "addExp") {
            visitAddExp(child);
        } else {
            visitRelExp(child);
        }
        return;
    }
    visitEqExp(ctx->children[0].get());
    std::string lhs = lastValue_;
    std::string lhsType = lastType_;
    const std::string op = ctx->children[1]->getText();
    visitRelExp(ctx->children[2].get());
    std::string rhs = lastValue_;
    std::string rhsType = lastType_;

    if (op != "==" && op != "!=") {
        addError(ctx, "unknown equality operator '" + op + "'.");
        return;
    }
    const bool useFloat = (lhsType == "float" || rhsType == "float");
    if (useFloat) {
        lhs = convertValue(lhs, lhsType, "float", ctx);
        rhs = convertValue(rhs, rhsType, "float", ctx);
        std::string out = newTemp();
        emit(out + " = fcmp " + (op == "==" ? "oeq" : "one") + " float " + lhs + ", " + rhs);
        lastValue_ = out;
    } else {
        lhs = convertValue(lhs, lhsType, "int", ctx);
        rhs = convertValue(rhs, rhsType, "int", ctx);
        std::string out = newTemp();
        emit(out + " = icmp " + (op == "==" ? "eq" : "ne") + " i32 " + lhs + ", " + rhs);
        lastValue_ = out;
    }
    lastType_ = BOOL_TYPE;
}

// ---------------------------------------------------------------- 短路求值

void IRVisitor::visitLAndExp(const ASTNode* ctx) {
    if (!ctx || ctx->children.empty()) {
        return;
    }
    if (ctx->type == "eqExp") { visitEqExp(ctx); return; }
    if (ctx->type == "relExp") { visitRelExp(ctx); return; }
    if (ctx->type == "unaryExp") { visitUnaryExp(ctx); return; }
    if (ctx->type == "addExp") { visitAddExp(ctx); return; }

    if (ctx->children.size() != 3) {
        const ASTNode* child = ctx->children[0].get();
        if (!child) {
            return;
        }
        if (child->type == "lAndExp") {
            visitLAndExp(child);
        } else if (child->type == "eqExp") {
            visitEqExp(child);
        } else if (child->type == "relExp") {
            visitRelExp(child);
        } else {
            visitEqExp(child);
        }
        if (lastType_ != BOOL_TYPE) {
            lastValue_ = toBool(lastValue_, lastType_);
            lastType_ = BOOL_TYPE;
        }
        return;
    }
    visitLAndExp(ctx->children[0].get());
    const std::string lhs = toBool(lastValue_, lastType_);

    // %slot = alloca i1; store false; br lhs ? rhs : end; rhs: 求值后 store; br end; end: load
    std::string slot = newTemp();
    emit(slot + " = alloca i1");
    emit("store i1 false, i1* " + slot);

    const std::string rhsLabel = "land.rhs." + std::to_string(tempIndex_++);
    const std::string endLabel = "land.end." + std::to_string(tempIndex_++);
    emit("br i1 " + lhs + ", label %" + rhsLabel + ", label %" + endLabel);
    terminated_ = true;

    ir_ += rhsLabel + ":\n";
    terminated_ = false;
    visitEqExp(ctx->children[2].get());
    const std::string rhs = toBool(lastValue_, lastType_);
    emit("store i1 " + rhs + ", i1* " + slot);
    emit("br label %" + endLabel);
    terminated_ = true;

    ir_ += endLabel + ":\n";
    terminated_ = false;
    std::string out = newTemp();
    emit(out + " = load i1, i1* " + slot);
    lastValue_ = out;
    lastType_ = BOOL_TYPE;
}

void IRVisitor::visitLOrExp(const ASTNode* ctx) {
    if (!ctx || ctx->children.empty()) {
        return;
    }
    if (ctx->type == "lAndExp") { visitLAndExp(ctx); return; }
    if (ctx->type == "eqExp") { visitEqExp(ctx); return; }
    if (ctx->type == "relExp") { visitRelExp(ctx); return; }
    if (ctx->type == "unaryExp") { visitUnaryExp(ctx); return; }

    if (ctx->children.size() != 3) {
        const ASTNode* child = ctx->children[0].get();
        if (!child) {
            return;
        }
        if (child->type == "lOrExp") {
            visitLOrExp(child);
        } else if (child->type == "lAndExp") {
            visitLAndExp(child);
        } else if (child->type == "eqExp") {
            visitEqExp(child);
        } else {
            visitLAndExp(child);
        }
        return;
    }
    visitLOrExp(ctx->children[0].get());
    const std::string lhs = toBool(lastValue_, lastType_);

    std::string slot = newTemp();
    emit(slot + " = alloca i1");
    emit("store i1 true, i1* " + slot);

    const std::string rhsLabel = "lor.rhs." + std::to_string(tempIndex_++);
    const std::string endLabel = "lor.end." + std::to_string(tempIndex_++);
    emit("br i1 " + lhs + ", label %" + endLabel + ", label %" + rhsLabel);
    terminated_ = true;

    ir_ += rhsLabel + ":\n";
    terminated_ = false;
    visitLAndExp(ctx->children[2].get());
    const std::string rhs = toBool(lastValue_, lastType_);
    emit("store i1 " + rhs + ", i1* " + slot);
    emit("br label %" + endLabel);
    terminated_ = true;

    ir_ += endLabel + ":\n";
    terminated_ = false;
    std::string out = newTemp();
    emit(out + " = load i1, i1* " + slot);
    lastValue_ = out;
    lastType_ = BOOL_TYPE;
}

void IRVisitor::visitCond(const ASTNode* ctx) {
    if (!ctx || ctx->children.empty()) {
        return;
    }
    const ASTNode* lOr = ctx->findChild("lOrExp");
    if (lOr) {
        visitLOrExp(lOr);
    } else {
        const ASTNode* child = ctx->children[0].get();
        if (!child) {
            return;
        }
        if (child->type == "lOrExp") {
            visitLOrExp(child);
        } else if (child->type == "lAndExp") {
            visitLAndExp(child);
        } else if (child->type == "eqExp") {
            visitEqExp(child);
        } else if (child->type == "relExp") {
            visitRelExp(child);
        } else if (child->type == "unaryExp") {
            visitUnaryExp(child);
        } else {
            visitAddExp(child);
        }
    }
    if (lastType_ != BOOL_TYPE) {
        lastValue_ = toBool(lastValue_, lastType_);
        lastType_ = BOOL_TYPE;
    }
}

// ---------------------------------------------------------------- if 语句

void IRVisitor::visitIfStmt(const ASTNode* ctx) {
    if (!ctx) {
        return;
    }
    const ASTNode* cond = ctx->findChild("cond");
    const std::vector<const ASTNode*> branches = ctx->findChildren("stmt");
    if (!cond || branches.empty()) {
        addError(ctx, "malformed if statement.");
        return;
    }

    visitCond(cond);
    const std::string condValue = toBool(lastValue_, lastType_);
    const bool hasElse = branches.size() >= 2;

    const std::string thenLabel = "if.then." + std::to_string(tempIndex_++);
    const std::string elseLabel = "if.else." + std::to_string(tempIndex_++);
    const std::string endLabel = "if.end." + std::to_string(tempIndex_++);
    emit("br i1 " + condValue + ", label %" + thenLabel + ", label %"
         + (hasElse ? elseLabel : endLabel));
    terminated_ = true;

    // then 分支
    ir_ += thenLabel + ":\n";
    terminated_ = false;
    visitStmt(branches[0]);
    const bool thenTerminated = terminated_;
    if (!thenTerminated) {
        emit("br label %" + endLabel);
        terminated_ = true;
    }

    // 没有 else 时 false 分支会落到 if 之后，属于"未终结"，必须生成 merge 块
    bool elseTerminated = false;
    if (hasElse) {
        ir_ += elseLabel + ":\n";
        terminated_ = false;
        visitStmt(branches[1]);
        elseTerminated = terminated_;
        if (!elseTerminated) {
            emit("br label %" + endLabel);
            terminated_ = true;
        }
    }

    // 两个分支都 return 时后续代码不可达，不再生成 merge 块
    if (thenTerminated && elseTerminated) {
        terminated_ = true;
        return;
    }
    ir_ += endLabel + ":\n";
    terminated_ = false;
}

}  // namespace ir
}  // namespace cmm
