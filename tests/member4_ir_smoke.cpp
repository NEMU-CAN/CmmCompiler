#include "AST.h"
#include "IRGenerator.h"

#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using cmm::ASTNode;

// ---------------------------------------------------------------- AST 构造辅助

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

// "!x"：unaryExp(unaryOp, unaryExp(primaryExp(lVal)))
static std::unique_ptr<ASTNode> notExp(const std::string& name) {
    auto lv = ASTNode::makeNode("lVal"); lv->addChild(term("Ident", name));
    auto primary = ASTNode::makeNode("primaryExp"); primary->addChild(std::move(lv));
    auto inner = ASTNode::makeNode("unaryExp"); inner->addChild(std::move(primary));
    auto outer = ASTNode::makeNode("unaryExp");
    outer->addChild(term("unaryOp", "!"));
    outer->addChild(std::move(inner));
    return outer;
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

static void addBlockItem(ASTNode* block, std::unique_ptr<ASTNode> stmt) {
    auto item = block->addChild(ASTNode::makeNode("blockItem"));
    item->addChild(std::move(stmt));
}

template <typename... Args>
static std::unique_ptr<ASTNode> blockOf(Args&&... stmts) {
    auto block = ASTNode::makeNode("block");
    block->addChild(term("{", "{"));
    (addBlockItem(block.get(), std::forward<Args>(stmts)), ...);
    block->addChild(term("}", "}"));
    return block;
}

static std::unique_ptr<ASTNode> funcDef(const std::string& name,
                                        std::vector<std::pair<std::string, std::string>> params,
                                        std::unique_ptr<ASTNode> body) {
    auto fn = ASTNode::makeNode("funcDef");
    auto ft = fn->addChild(ASTNode::makeNode("funcType"));
    ft->addChild(term("int", "int"));
    fn->addChild(term("Ident", name));
    fn->addChild(term("(", "("));
    if (!params.empty()) {
        auto formal = fn->addChild(ASTNode::makeNode("funcFParams"));
        for (const auto& p : params) {
            auto param = formal->addChild(ASTNode::makeNode("funcFParam"));
            auto bt = param->addChild(ASTNode::makeNode("bType"));
            bt->addChild(term("int", p.first));
            param->addChild(term("Ident", p.second));
        }
    }
    fn->addChild(term(")", ")"));
    fn->addChild(std::move(body));
    return fn;
}

static std::unique_ptr<ASTNode> varDeclStmt(const std::string& name) {
    auto itemDecl = ASTNode::makeNode("decl");
    auto varDecl = itemDecl->addChild(ASTNode::makeNode("varDecl"));
    auto bt = varDecl->addChild(ASTNode::makeNode("bType"));
    bt->addChild(term("int", "int"));
    auto def = varDecl->addChild(ASTNode::makeNode("varDef"));
    def->addChild(term("Ident", name));
    varDecl->addChild(term(";", ";"));
    return itemDecl;
}

int main() {
    auto program = ASTNode::makeNode("Program");
    auto unit = program->addChild(ASTNode::makeNode("compUnit"));

    // ---- main：if/else + && + || + 无 else 的 if + 嵌套 if ----
    {
        auto xDecl = varDeclStmt("x");
        auto x3 = assignStmt("x", 3);

        auto andCond = condOf(land(wrapEq(rel(lval("x"), ">", number(2))),
                                   wrapEq(rel(lval("x"), "<", number(10)))));
        auto ifAnd = ifStmt(std::move(andCond), assignStmt("x", 1), assignStmt("x", 0));

        auto orCond = condOf(lor(wrapAnd(eq(lval("x"), "==", number(1))),
                                 wrapEq(rel(floatNumber("3.5"), "<", floatNumber("4.0")))));
        auto ifOr = ifStmt(std::move(orCond), returnStmt(lval("x")), returnStmt(number(0)));

        auto body = blockOf(std::move(xDecl), std::move(x3), std::move(ifAnd), std::move(ifOr));
        unit->addChild(funcDef("main", {}, std::move(body)));
    }

    // ---- early：if 无 else，then 分支 return ----
    {
        auto cond = condOf(wrapOr(wrapAnd(wrapEq(rel(lval("y"), ">", number(0))))));
        auto ifNoElse = ifStmt(std::move(cond), returnStmt(number(1)));
        auto body = blockOf(std::move(ifNoElse), returnStmt(number(0)));
        unit->addChild(funcDef("early", {{"int", "y"}}, std::move(body)));
    }

    // ---- mixed：int 与 float 混合比较（需要 sitofp） ----
    {
        auto cond = condOf(wrapOr(wrapAnd(wrapEq(rel(lval("x"), "<", floatNumber("3.5"))))));
        auto ifMixed = ifStmt(std::move(cond), returnStmt(number(1)), returnStmt(number(0)));
        auto body = blockOf(std::move(ifMixed));
        unit->addChild(funcDef("mixed", {{"int", "x"}}, std::move(body)));
    }

    // ---- nested：嵌套 if + 条件里用 ! ----
    {
        auto inner = ifStmt(condOf(wrapOr(wrapAnd(wrapEq(rel(lval("x"), ">", number(0)))))),
                            assignStmt("x", 2), assignStmt("x", -1));
        auto outer = ifStmt(condOf(wrapOr(wrapAnd(wrapEq(eq(notExp("x"), "==", number(0)))))),
                            std::move(inner));
        auto body = blockOf(std::move(outer), returnStmt(lval("x")));
        unit->addChild(funcDef("nested", {{"int", "x"}}, std::move(body)));
    }
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

    assert(ir.find("define i32 @main()") != std::string::npos);
    assert(ir.find("define i32 @early(i32 %arg0)") != std::string::npos);
    assert(ir.find("define i32 @mixed(i32 %arg0)") != std::string::npos);
    assert(ir.find("define i32 @nested(i32 %arg0)") != std::string::npos);

    assert(ir.find("icmp sgt i32") != std::string::npos);       // x > 2
    assert(ir.find("icmp slt i32") != std::string::npos);       // x < 10
    assert(ir.find("icmp eq i32") != std::string::npos);        // x == 1
    assert(ir.find("fcmp olt float") != std::string::npos);     // 3.5 < 4.0
    assert(ir.find("sitofp i32") != std::string::npos);         // int -> float
    assert(ir.find("br i1") != std::string::npos);              // 条件跳转
    assert(ir.find("land.end.") != std::string::npos);          // && 短路块
    assert(ir.find("lor.end.") != std::string::npos);           // || 短路块
    assert(ir.find("if.end.") != std::string::npos);            // if 汇合块
    assert(ir.find("load i1") != std::string::npos);            // 短路结果回读
    assert(ir.find("ret i32") != std::string::npos);

    std::fprintf(stderr, "[member4 smoke] OK: %zu bytes IR\n", ir.size());
    return 0;
}
