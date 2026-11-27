#include "AST.h"
#include "IRGenerator.h"

#include <cassert>
#include <cstdio>
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
    return a;
}

static std::unique_ptr<ASTNode> floatNumber(const std::string& value) {
    auto n = ASTNode::makeNode("number");
    n->addChild(term("floatConst", value));
    auto p = ASTNode::makeNode("primaryExp"); p->addChild(std::move(n));
    auto u = ASTNode::makeNode("unaryExp"); u->addChild(std::move(p));
    auto m = ASTNode::makeNode("mulExp"); m->addChild(std::move(u));
    auto a = ASTNode::makeNode("addExp"); a->addChild(std::move(m));
    return a;
}

static std::unique_ptr<ASTNode> lval(const std::string& name) {
    auto lv = ASTNode::makeNode("lVal"); lv->addChild(term("Ident", name));
    auto p = ASTNode::makeNode("primaryExp"); p->addChild(std::move(lv));
    auto u = ASTNode::makeNode("unaryExp"); u->addChild(std::move(p));
    auto m = ASTNode::makeNode("mulExp"); m->addChild(std::move(u));
    auto a = ASTNode::makeNode("addExp"); a->addChild(std::move(m));
    return a;
}

static std::unique_ptr<ASTNode> wrapRel(std::unique_ptr<ASTNode> node) {
    auto r = ASTNode::makeNode("relExp"); r->addChild(std::move(node)); return r;
}

static std::unique_ptr<ASTNode> wrapEq(std::unique_ptr<ASTNode> node) {
    auto e = ASTNode::makeNode("eqExp"); e->addChild(std::move(node)); return e;
}

static std::unique_ptr<ASTNode> wrapAnd(std::unique_ptr<ASTNode> node) {
    auto a = ASTNode::makeNode("lAndExp"); a->addChild(std::move(node)); return a;
}

static std::unique_ptr<ASTNode> wrapOr(std::unique_ptr<ASTNode> node) {
    auto o = ASTNode::makeNode("lOrExp"); o->addChild(std::move(node)); return o;
}

// relExp: lhs op rhs
static std::unique_ptr<ASTNode> rel(std::unique_ptr<ASTNode> lhs, const std::string& op,
                                    std::unique_ptr<ASTNode> rhs) {
    auto e = ASTNode::makeNode("relExp");
    e->addChild(wrapRel(std::move(lhs)));
    e->addChild(term(op, op));
    e->addChild(std::move(rhs));
    return e;
}

static std::unique_ptr<ASTNode> eq(std::unique_ptr<ASTNode> lhs, const std::string& op,
                                   std::unique_ptr<ASTNode> rhs) {
    auto e = ASTNode::makeNode("eqExp");
    e->addChild(wrapEq(std::move(lhs)));
    e->addChild(term(op, op));
    e->addChild(std::move(rhs));
    return e;
}

static std::unique_ptr<ASTNode> land(std::unique_ptr<ASTNode> lhs, std::unique_ptr<ASTNode> rhs) {
    auto e = ASTNode::makeNode("lAndExp");
    e->addChild(wrapAnd(std::move(lhs)));
    e->addChild(term("&&", "&&"));
    e->addChild(std::move(rhs));
    return e;
}

static std::unique_ptr<ASTNode> lor(std::unique_ptr<ASTNode> lhs, std::unique_ptr<ASTNode> rhs) {
    auto e = ASTNode::makeNode("lOrExp");
    e->addChild(wrapOr(std::move(lhs)));
    e->addChild(term("||", "||"));
    e->addChild(std::move(rhs));
    return e;
}

static std::unique_ptr<ASTNode> condOf(std::unique_ptr<ASTNode> lOr) {
    auto c = ASTNode::makeNode("cond"); c->addChild(std::move(lOr)); return c;
}

static std::unique_ptr<ASTNode> assignStmt(const std::string& name, int value) {
    auto s = ASTNode::makeNode("stmt");
    auto lv = ASTNode::makeNode("lVal"); lv->addChild(term("Ident", name));
    s->addChild(std::move(lv));
    s->addChild(term("=", "="));
    auto e = ASTNode::makeNode("exp"); e->addChild(number(value));
    s->addChild(std::move(e));
    s->addChild(term(";", ";"));
    return s;
}

static std::unique_ptr<ASTNode> returnStmt(std::unique_ptr<ASTNode> exp) {
    auto s = ASTNode::makeNode("stmt");
    s->addChild(term("return", "return"));
    auto e = ASTNode::makeNode("exp"); e->addChild(std::move(exp));
    s->addChild(std::move(e));
    s->addChild(term(";", ";"));
    return s;
}

static std::unique_ptr<ASTNode> ifStmt(std::unique_ptr<ASTNode> cond,
                                       std::unique_ptr<ASTNode> thenStmt,
                                       std::unique_ptr<ASTNode> elseStmt = nullptr) {
    auto s = ASTNode::makeNode("stmt");
    s->addChild(term("if", "if"));
    s->addChild(term("(", "("));
    s->addChild(std::move(cond));
    s->addChild(term(")", ")"));
    s->addChild(std::move(thenStmt));
    if (elseStmt) {
        s->addChild(term("else", "else"));
        s->addChild(std::move(elseStmt));
    }
    return s;
}

int main() {
    auto program = ASTNode::makeNode("Program");
    auto unit = program->addChild(ASTNode::makeNode("compUnit"));
    auto fn = unit->addChild(ASTNode::makeNode("funcDef"));
    auto ft = fn->addChild(ASTNode::makeNode("funcType")); ft->addChild(term("int", "int"));
    fn->addChild(term("Ident", "main")); fn->addChild(term("(", "(")); fn->addChild(term(")", ")"));
    auto block = fn->addChild(ASTNode::makeNode("block")); block->addChild(term("{", "{"));

    // int x;
    auto item = block->addChild(ASTNode::makeNode("blockItem"));
    auto decl = item->addChild(ASTNode::makeNode("decl"));
    auto varDecl = decl->addChild(ASTNode::makeNode("varDecl"));
    auto bt = varDecl->addChild(ASTNode::makeNode("bType")); bt->addChild(term("int", "int"));
    auto def = varDecl->addChild(ASTNode::makeNode("varDef")); def->addChild(term("Ident", "x"));
    varDecl->addChild(term(";", ";"));
    block->addChild(term(";", ";"));

    // x = 3;
    item = block->addChild(ASTNode::makeNode("blockItem"));
    item->addChild(assignStmt("x", 3));

    // if (x > 2 && x < 10) { x = 1; } else { x = 0; }
    auto andCond = condOf(land(wrapEq(rel(lval("x"), ">", number(2))),
                               wrapEq(rel(lval("x"), "<", number(10)))));
    auto thenStmt = assignStmt("x", 1);
    auto elseStmt = assignStmt("x", 0);
    item = block->addChild(ASTNode::makeNode("blockItem"));
    item->addChild(ifStmt(std::move(andCond), std::move(thenStmt), std::move(elseStmt)));

    // if (x == 1 || 3.5 < 4.0) { return x; } else { return 0; }
    auto orCond = condOf(lor(wrapAnd(eq(lval("x"), "==", number(1))),
                             wrapEq(rel(floatNumber("3.5"), "<", floatNumber("4.0")))));
    item = block->addChild(ASTNode::makeNode("blockItem"));
    item->addChild(ifStmt(std::move(orCond),
                          returnStmt(lval("x")),
                          returnStmt(number(0))));
    block->addChild(term("}", "}"));

    cmm::IRGenerator generator;
    const cmm::IRResult result = generator.generate(program.get());
    if (!result.success) {
        for (const auto& e : result.errors) {
            std::fprintf(stderr, "[IR ERROR] %s\n", e.c_str());
        }
        return 1;
    }

    const std::string& ir = result.irText;
    std::printf("%s\n", ir.c_str());
    assert(ir.find("icmp sgt i32") != std::string::npos);       // x > 2
    assert(ir.find("icmp slt i32") != std::string::npos);       // x < 10
    assert(ir.find("br i1") != std::string::npos);              // 条件跳转
    assert(ir.find("land.end.") != std::string::npos);          // && 短路块
    assert(ir.find("lor.end.") != std::string::npos);           // || 短路块
    assert(ir.find("load i1") != std::string::npos);            // 短路结果回读
    assert(ir.find("fcmp olt float") != std::string::npos);     // 浮点比较
    assert(ir.find("icmp eq i32") != std::string::npos);        // == 比较
    assert(ir.find("ret i32") != std::string::npos);

    return 0;
}
