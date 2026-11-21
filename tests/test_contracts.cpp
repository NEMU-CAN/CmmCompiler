#include "AST.h"
#include "IRGenerator.h"
#include "Lexer.h"
#include "Parser.h"
#include "Token.h"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

int main() {
    // 1. Test Lexer move semantics & mixed-case + error handling
    cmm::Lexer lex1;
    cmm::Lexer lex2(std::move(lex1));
    std::string src = "VoID MaIn() { @ CoNsT InT _X1 = !0; FLOAT Pi = 3.14; }";
    cmm::LexResult res = lex2.tokenize(src);

    // Lexical error recorded in rawTokens & errors, stripped from parserTokens
    assert(res.errors.size() == 1);
    assert(!res.parserTokens.empty());
    assert(res.parserTokens.back().symbol == "EOF");
    assert(res.parserTokens.back().category == cmm::TokenCategory::END);
    for (const auto& t : res.parserTokens) {
        assert(t.category != cmm::TokenCategory::ERROR);
    }

    // Verify exact symbol, stackDisplay & normalizedText contract for Member 2 and Member 3/4
    // Tokens: VoID MaIn ( ) { CoNsT InT _X1 = ! 0 ; FLOAT Pi = 3.14 ; } EOF
    const auto& pt = res.parserTokens;
    assert(pt[0].lexeme == "VoID" && pt[0].symbol == "void" && pt[0].stackDisplay() == "VoID" && pt[0].normalizedText() == "void");
    assert(pt[1].lexeme == "MaIn" && pt[1].symbol == "Ident" && pt[1].stackDisplay() == "Ident" && pt[1].normalizedText() == "main");
    assert(pt[5].lexeme == "CoNsT" && pt[5].symbol == "const" && pt[5].stackDisplay() == "CoNsT" && pt[5].normalizedText() == "const");
    assert(pt[6].lexeme == "InT" && pt[6].symbol == "int" && pt[6].stackDisplay() == "InT" && pt[6].normalizedText() == "int");
    assert(pt[7].lexeme == "_X1" && pt[7].symbol == "Ident" && pt[7].stackDisplay() == "Ident" && pt[7].normalizedText() == "_X1");
    assert(pt[8].lexeme == "=" && pt[8].symbol == "=" && pt[8].stackDisplay() == "=" && pt[8].normalizedText() == "=");
    assert(pt[9].lexeme == "!" && pt[9].symbol == "!" && pt[9].stackDisplay() == "!" && pt[9].normalizedText() == "!");
    assert(pt[10].lexeme == "0" && pt[10].symbol == "IntConst" && pt[10].stackDisplay() == "IntConst" && pt[10].normalizedText() == "0");
    assert(pt[12].lexeme == "FLOAT" && pt[12].symbol == "float" && pt[12].stackDisplay() == "FLOAT" && pt[12].normalizedText() == "float");
    assert(pt[13].lexeme == "Pi" && pt[13].symbol == "Ident" && pt[13].stackDisplay() == "Ident" && pt[13].normalizedText() == "Pi");
    assert(pt[15].lexeme == "3.14" && pt[15].symbol == "floatConst" && pt[15].stackDisplay() == "floatConst" && pt[15].normalizedText() == "3.14");

    // 2. Test ASTNode factory, isTerminal() on empty non-terminal, and query helpers
    auto emptyCompUnit = cmm::ASTNode::makeNode("compUnit");
    assert(!emptyCompUnit->isTerminal());

    auto funcDef = cmm::ASTNode::makeNode("funcDef");
    auto funcType = funcDef->addChild(cmm::ASTNode::makeNode("funcType"));
    funcType->addChild(cmm::ASTNode::makeTerminal(pt[0]));  // VoID -> type="void", text="void"
    auto identNode = funcDef->addChild(cmm::ASTNode::makeTerminal(pt[1]));  // MaIn -> type="Ident", text="main"
    assert(funcType->getText() == "void");
    assert(identNode->isTerminal());
    assert(identNode->type == "Ident" && identNode->text == "main");
    assert(funcDef->hasChild("funcType"));
    assert(funcDef->findChild("Ident") == identNode);
    assert(funcDef->findChildren("funcType").size() == 1);
    assert(funcDef->line == 1);  // propagated from first child

    // 3. Test Parser::dumpSLRTable stub & IRGenerator null guard
    cmm::Parser parser;
    std::ostringstream slrOut;
    assert(parser.dumpSLRTable(slrOut));
    assert(!slrOut.str().empty());

    cmm::IRGenerator irGen;
    auto nullRes = irGen.generate(nullptr);
    assert(!nullRes.success && !nullRes.errors.empty());

    std::cout << "[PASS] All C++ interface contracts verified!\n";
    return 0;
}
