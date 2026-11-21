#include "ir_visitor.h"

namespace cmm {
namespace ir {

// ============================================================================
// 成员 3 独占实现文件：
//   - 引入 third_party/compiler_ir 并初始化 Module / SysY 运行时库声明
//   - 多层作用域符号表
//   - 变量/常量声明、函数定义与传参、代码块、赋值、return、单目与四则运算
// ============================================================================

IRResult IRVisitor::run(const ASTNode* root) {
    IRResult res;
    errors_.clear();
    visitProgram(root);
    res.errors = errors_;
    res.success = false;
    res.errors.push_back("IRVisitor::run not implemented yet (Member 3 TODO in src/ir/ir_basic.cpp).");
    return res;
}

void IRVisitor::visitProgram(const ASTNode* /*ctx*/) {}
void IRVisitor::visitCompUnit(const ASTNode* /*ctx*/) {}
void IRVisitor::visitDecl(const ASTNode* /*ctx*/) {}
void IRVisitor::visitConstDecl(const ASTNode* /*ctx*/) {}
void IRVisitor::visitVarDecl(const ASTNode* /*ctx*/) {}
void IRVisitor::visitConstDef(const ASTNode* /*ctx*/) {}
void IRVisitor::visitVarDef(const ASTNode* /*ctx*/) {}
void IRVisitor::visitFuncDef(const ASTNode* /*ctx*/) {}
void IRVisitor::visitBlock(const ASTNode* /*ctx*/) {}
void IRVisitor::visitStmt(const ASTNode* ctx) {
    if (ctx && ctx->hasChild("if")) {
        visitIfStmt(ctx);
    }
}
void IRVisitor::visitExp(const ASTNode* /*ctx*/) {}
void IRVisitor::visitAddExp(const ASTNode* /*ctx*/) {}
void IRVisitor::visitMulExp(const ASTNode* /*ctx*/) {}
void IRVisitor::visitUnaryExp(const ASTNode* /*ctx*/) {}
void IRVisitor::visitPrimaryExp(const ASTNode* /*ctx*/) {}
void IRVisitor::visitLVal(const ASTNode* /*ctx*/) {}
void IRVisitor::visitNumber(const ASTNode* /*ctx*/) {}
void IRVisitor::visitConstExp(const ASTNode* /*ctx*/) {}

}  // namespace ir
}  // namespace cmm
