#include "Lexer.h"

namespace cmm {

Lexer::Lexer() = default;
Lexer::~Lexer() = default;

LexResult Lexer::tokenize(const std::string& /*source*/) const {
    LexResult res;
    Token eofTok;
    eofTok.lexeme = "";
    eofTok.symbol = "EOF";
    eofTok.category = TokenCategory::END;
    eofTok.attr = "";
    res.parserTokens.push_back(eofTok);
    return res;
}

void Lexer::printTokens(const LexResult& result, std::ostream& os) {
    for (const auto& tok : result.rawTokens) {
        os << tok.formatLine() << "\n";
    }
}

void Lexer::printSymbolTable(const LexResult& result, std::ostream& os) {
    os << "ID\tName\tFirstLine\tFirstCol\tCount\n";
    for (const auto& sym : result.symbols) {
        os << sym.id << "\t" << sym.name << "\t" << sym.firstLine << "\t"
           << sym.firstCol << "\t" << sym.count << "\n";
    }
}

bool Lexer::dumpAutomata(std::ostream& os) const {
    os << "{ \"status\": \"stub\" }\n";
    return true;
}

}  // namespace cmm
