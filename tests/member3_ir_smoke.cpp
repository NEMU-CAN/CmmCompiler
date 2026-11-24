#include "AST.h"
#include "IRGenerator.h"

#include <cassert>
#include <memory>
#include <string>

using cmm::ASTNode;

static std::unique_ptr<ASTNode> term(const std::string& type, const std::string& text) {
    return std::make_unique<ASTNode>(type, text, 1, 1, true);
}

static std::unique_ptr<ASTNode> number(int value) {
    auto n = ASTNode::makeNode("number");
    n->addChild(term("IntConst", std::to_string(value)));
    auto p = ASTNode::makeNode("primaryExp"); p->addChild(std::move(n));
    auto u = ASTNode::makeNode("unaryExp"); u->addChild(std::move(p));
    auto m = ASTNode::makeNode("mulExp"); m->addChild(std::move(u));
    auto a = ASTNode::makeNode("addExp"); a->addChild(std::move(m));
    auto e = ASTNode::makeNode("exp"); e->addChild(std::move(a));
    return e;
}

static std::unique_ptr<ASTNode> lvalExp(const std::string& name) {
    auto lv = ASTNode::makeNode("lVal"); lv->addChild(term("Ident", name));
    auto p = ASTNode::makeNode("primaryExp"); p->addChild(std::move(lv));
    auto u = ASTNode::makeNode("unaryExp"); u->addChild(std::move(p));
    auto m = ASTNode::makeNode("mulExp"); m->addChild(std::move(u));
    auto a = ASTNode::makeNode("addExp"); a->addChild(std::move(m));
    auto e = ASTNode::makeNode("exp"); e->addChild(std::move(a));
    return e;
}

static std::unique_ptr<ASTNode> callExp(const std::string& name, std::unique_ptr<ASTNode> argument) {
    auto u = ASTNode::makeNode("unaryExp"); u->addChild(term("Ident", name)); u->addChild(term("(", "("));
    auto params = u->addChild(ASTNode::makeNode("funcRParams"));
    auto param = params->addChild(ASTNode::makeNode("funcRParam")); param->addChild(std::move(argument));
    u->addChild(term(")", ")"));
    auto m = ASTNode::makeNode("mulExp"); m->addChild(std::move(u));
    auto a = ASTNode::makeNode("addExp"); a->addChild(std::move(m));
    auto e = ASTNode::makeNode("exp"); e->addChild(std::move(a));
    return e;
}

int main() {
    auto program = ASTNode::makeNode("Program");
    auto unit = program->addChild(ASTNode::makeNode("compUnit"));
    auto identity = unit->addChild(ASTNode::makeNode("funcDef"));
    auto identityType = identity->addChild(ASTNode::makeNode("funcType")); identityType->addChild(term("int", "int"));
    identity->addChild(term("Ident", "identity")); identity->addChild(term("(", "("));
    auto formalParams = identity->addChild(ASTNode::makeNode("funcFParams"));
    auto formal = formalParams->addChild(ASTNode::makeNode("funcFParam"));
    auto formalType = formal->addChild(ASTNode::makeNode("bType")); formalType->addChild(term("int", "int"));
    formal->addChild(term("Ident", "value")); identity->addChild(term(")", ")"));
    auto identityBlock = identity->addChild(ASTNode::makeNode("block")); identityBlock->addChild(term("{", "{"));
    auto identityItem = identityBlock->addChild(ASTNode::makeNode("blockItem"));
    auto identityReturn = identityItem->addChild(ASTNode::makeNode("stmt"));
    identityReturn->addChild(term("return", "return")); identityReturn->addChild(lvalExp("value")); identityReturn->addChild(term(";", ";"));
    identityBlock->addChild(term("}", "}"));

    auto fn = unit->addChild(ASTNode::makeNode("funcDef"));
    auto ft = fn->addChild(ASTNode::makeNode("funcType")); ft->addChild(term("int", "int"));
    fn->addChild(term("Ident", "main")); fn->addChild(term("(", "(")); fn->addChild(term(")", ")"));
    auto block = fn->addChild(ASTNode::makeNode("block")); block->addChild(term("{", "{"));

    auto item = block->addChild(ASTNode::makeNode("blockItem"));
    auto decl = item->addChild(ASTNode::makeNode("decl"));
    auto varDecl = decl->addChild(ASTNode::makeNode("varDecl"));
    auto bt = varDecl->addChild(ASTNode::makeNode("bType")); bt->addChild(term("int", "int"));
    auto def = varDecl->addChild(ASTNode::makeNode("varDef")); def->addChild(term("Ident", "x")); def->addChild(term("=", "="));
    auto init = def->addChild(ASTNode::makeNode("initVal")); init->addChild(callExp("identity", number(7)));
    varDecl->addChild(term(";", ";"));

    item = block->addChild(ASTNode::makeNode("blockItem"));
    auto stmt = item->addChild(ASTNode::makeNode("stmt"));
    auto lv = stmt->addChild(ASTNode::makeNode("lVal")); lv->addChild(term("Ident", "x"));
    stmt->addChild(term("=", "=")); stmt->addChild(number(9)); stmt->addChild(term(";", ";"));

    item = block->addChild(ASTNode::makeNode("blockItem")); stmt = item->addChild(ASTNode::makeNode("stmt"));
    stmt->addChild(term("return", "return")); stmt->addChild(lvalExp("x")); stmt->addChild(term(";", ";"));
    block->addChild(term("}", "}"));

    cmm::IRGenerator generator;
    const cmm::IRResult result = generator.generate(program.get());
    assert(result.success);
    assert(result.irText.find("define i32 @identity(i32 %arg0)") != std::string::npos);
    assert(result.irText.find("call i32 @identity(i32 7)") != std::string::npos);
    assert(result.irText.find("define i32 @main()") != std::string::npos);
    assert(result.irText.find("alloca i32") != std::string::npos);
    assert(result.irText.find("store i32 9") != std::string::npos);
    assert(result.irText.find("ret i32") != std::string::npos);
}
