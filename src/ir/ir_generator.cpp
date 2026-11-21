#include "IRGenerator.h"
#include "ir_visitor.h"

namespace cmm {

IRResult IRGenerator::generate(const ASTNode* root) {
    if (!root) {
        IRResult res;
        res.success = false;
        res.errors.push_back("IRGenerator: AST root is null.");
        return res;
    }
    ir::IRVisitor visitor;
    return visitor.run(root);
}

bool IRGenerator::emitToStream(const ASTNode* root, std::ostream& os, std::vector<std::string>* errorsOut) {
    IRResult res = generate(root);
    if (errorsOut) {
        *errorsOut = res.errors;
    }
    if (!res.success) {
        return false;
    }
    os << res.irText;
    return true;
}

}  // namespace cmm
