#include "IRGenerator.h"

namespace cmm {

IRResult IRGenerator::generate(const ASTNode* root) {
    IRResult res;
    if (!root) {
        res.success = false;
        res.errors.push_back("IRGenerator: AST root is null.");
        return res;
    }
    res.success = false;
    res.errors.push_back("IRGenerator not implemented yet (Member 3 & 4 TODO: implement AST Visitor in src/ir/).");
    return res;
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
