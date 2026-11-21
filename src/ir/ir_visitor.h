#pragma once

#include "AST.h"
#include "IRGenerator.h"

#include <string>
#include <vector>

namespace cmm {
namespace ir {

// ============================================================================
// 成员 3 & 成员 4 内部协作 Visitor 骨架（避免两人修改同一文件产生 Git 冲突）
//
// 分工边界：
//   - src/ir/ir_generator.cpp : 公共入口（已接好，3/4 号通常无需改动）
//   - src/ir/ir_basic.cpp     : 成员 3 独占（符号表、全局/局部声明、函数定义/传参、return、赋值、单目与四则运算）
//   - src/ir/ir_ctrl.cpp      : 成员 4 独占（if-else 基本块、cond、关系/相等比较、&&/|| 短路、浮点运算与类型转换）
//
// 当 third_party/compiler_ir 就绪后，成员 3/4 可在此类中补充
// Module / IRBuilder / ScopeSymbolTable / Value* 等共享成员字段。
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
