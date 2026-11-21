#pragma once

#include "AST.h"
#include "IRGenerator.h"

#include <string>
#include <vector>

namespace cmm {
namespace ir {

// ============================================================================
// [草案 / DRAFT] 成员 3 & 成员 4 内部协作 Visitor 骨架
//
// 注意：目前尚未拿到 third_party/compiler_ir 源码，以下 visit* 返回值暂为 void。
// 拿到 compiler_ir 后，表达式类 visit*（visitExp/visitAddExp/visitMulExp 等）
// 需按 compiler_ir 真实类型改为返回 Value* 等，并补充 Module / IRBuilder /
// 作用域符号表等共享成员；若 compiler_ir 自带 Visitor 基类则优先采用自带定义。
//
// 建议分工边界：
//   - src/ir/ir_generator.cpp : 公共入口（已接好）
//   - src/ir/ir_basic.cpp     : 成员 3（符号表、声明、函数、return、赋值、单目与四则）
//   - src/ir/ir_ctrl.cpp      : 成员 4（if-else、cond、比较、&&/|| 短路、浮点转换）
// ============================================================================
class IRVisitor {
public:
    IRVisitor() = default;
    ~IRVisitor() = default;

    // 统一驱动入口（由 ir_generator.cpp 调用，实现位于 ir_basic.cpp）
    IRResult run(const ASTNode* root);

protected:
    // --- 成员 3 负责（实现于 src/ir/ir_basic.cpp） ---
    void visitProgram(const ASTNode* ctx);
    void visitCompUnit(const ASTNode* ctx);
    void visitDecl(const ASTNode* ctx);
    void visitConstDecl(const ASTNode* ctx);
    void visitVarDecl(const ASTNode* ctx);
    void visitConstDef(const ASTNode* ctx);
    void visitVarDef(const ASTNode* ctx);
    void visitFuncDef(const ASTNode* ctx);
    void visitBlock(const ASTNode* ctx);
    void visitStmt(const ASTNode* ctx);  // 遇到 if 语句时转发给成员 4 的 visitIfStmt
    void visitExp(const ASTNode* ctx);
    void visitAddExp(const ASTNode* ctx);
    void visitMulExp(const ASTNode* ctx);
    void visitUnaryExp(const ASTNode* ctx);
    void visitPrimaryExp(const ASTNode* ctx);
    void visitLVal(const ASTNode* ctx);
    void visitNumber(const ASTNode* ctx);
    void visitConstExp(const ASTNode* ctx);

    // --- 成员 4 负责（实现于 src/ir/ir_ctrl.cpp） ---
    void visitIfStmt(const ASTNode* ctx);
    void visitCond(const ASTNode* ctx);
    void visitLOrExp(const ASTNode* ctx);
    void visitLAndExp(const ASTNode* ctx);
    void visitEqExp(const ASTNode* ctx);
    void visitRelExp(const ASTNode* ctx);

    // 公共报错收集
    void addError(const ASTNode* ctx, const std::string& msg) {
        std::string prefix;
        if (ctx && ctx->line > 0) {
            prefix = "Line " + std::to_string(ctx->line) + ", Col " + std::to_string(ctx->col) + ": ";
        }
        errors_.push_back(prefix + msg);
    }

    std::vector<std::string> errors_;
};

}  // namespace ir
}  // namespace cmm
