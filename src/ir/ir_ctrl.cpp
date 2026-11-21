#include "ir_visitor.h"

namespace cmm {
namespace ir {

// ============================================================================
// 成员 4 独占实现文件：
//   - if / if-else 基本块创建与跳转
//   - cond、关系比较 (<, >, <=, >=)、相等比较 (==, !=)
//   - 逻辑与 (&&) / 逻辑或 (||) 短路求值
//   - 浮点类型转换与运算
// ============================================================================

void IRVisitor::visitIfStmt(const ASTNode* ctx) {
    addError(ctx, "visitIfStmt not implemented yet (Member 4 TODO in src/ir/ir_ctrl.cpp).");
}

void IRVisitor::visitCond(const ASTNode* /*ctx*/) {}
void IRVisitor::visitLOrExp(const ASTNode* /*ctx*/) {}
void IRVisitor::visitLAndExp(const ASTNode* /*ctx*/) {}
void IRVisitor::visitEqExp(const ASTNode* /*ctx*/) {}
void IRVisitor::visitRelExp(const ASTNode* /*ctx*/) {}

}  // namespace ir
}  // namespace cmm
