#include "Lexer.h"
#include "TokenConfig.h"
#include "automata.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <unordered_map>

namespace cmm {

struct Lexer::Impl {
    detail::NFA nfa;
    detail::DFA dfa;
    detail::DFA minimal;

    Impl() {
        detail::buildNFA(&nfa);
        detail::determinize(&nfa, &dfa);
        detail::minimizeDFA(&dfa, &minimal);
    }
};

namespace {

std::string normalizeNewlines(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\r') {
            if (i + 1 < raw.size() && raw[i + 1] == '\n') ++i;
            out.push_back('\n');
        } else {
            out.push_back(raw[i]);
        }
    }
    return out;
}

std::string toLowerAscii(const std::string& s) {
    std::string out = s;
    for (char& ch : out) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return out;
}

}  // namespace

Lexer::Lexer() : impl_(std::make_unique<Impl>()) {}
Lexer::~Lexer() = default;
Lexer::Lexer(Lexer&&) noexcept = default;
Lexer& Lexer::operator=(Lexer&&) noexcept = default;

LexResult Lexer::tokenize(const std::string& rawSource) const {
    LexResult res;
    std::string source = normalizeNewlines(rawSource);
    const detail::DFA& minimal = impl_->minimal;
    const auto& kwMap = TokenCodeTable::keywords();
    const auto& opMap = TokenCodeTable::operators();
    const auto& seMap = TokenCodeTable::delimiters();

    std::unordered_map<std::string, std::size_t> symIndex;

    std::size_t pos = 0;
    int line = 1;
    int col = 1;
    const std::size_t len = source.size();

    while (pos < len) {
        std::size_t cursor = pos;
        std::size_t lastEnd = pos;
        int state = minimal.start;
        int label = -1;

        while (cursor < len && static_cast<unsigned char>(source[cursor]) < detail::ALPHABET) {
            int target = minimal.transitions[state][static_cast<unsigned char>(source[cursor])];
            if (target == minimal.dead) break;
            state = target;
            ++cursor;
            if (minimal.accept[state] >= 0) {
                label = minimal.accept[state];
                lastEnd = cursor;
            }
        }

        if (label < 0) {
            Token errTok;
            errTok.lexeme = source.substr(pos, 1);
            errTok.symbol = "ERROR";
            errTok.category = TokenCategory::ERROR;
            errTok.attr = std::to_string(line) + "," + std::to_string(col);
            errTok.line = line;
            errTok.col = col;
            res.rawTokens.push_back(errTok);
            res.errors.push_back("Line " + std::to_string(line) + ", Col " + std::to_string(col) +
                                 ": illegal character '" + errTok.lexeme + "'");
            lastEnd = pos + 1;
        } else {
            const detail::LexRule& rule = detail::RULES[label];
            if (std::strcmp(rule.kind, "WS") != 0) {
                std::string lexeme = source.substr(pos, lastEnd - pos);
                Token tok;
                tok.lexeme = lexeme;
                tok.line = line;
                tok.col = col;

                if (std::strcmp(rule.kind, "IDN") == 0) {
                    std::string lower = toLowerAscii(lexeme);
                    auto it = kwMap.find(lower);
                    if (it != kwMap.end()) {
                        tok.category = TokenCategory::KW;
                        tok.attr = std::to_string(it->second);
                        // 附录 1 规则 11: funcDef -> funcType Ident '(' ...
                        // main 在词法上输出 <KW,5>，但在语法终结符层面归一化为 Ident
                        tok.symbol = (lower == "main") ? "Ident" : lower;
                    } else {
                        tok.category = TokenCategory::IDN;
                        tok.attr = lexeme;
                        tok.symbol = "Ident";

                        auto sit = symIndex.find(lexeme);
                        if (sit == symIndex.end()) {
                            LexSymbolEntry entry;
                            entry.id = static_cast<int>(res.symbols.size()) + 1;
                            entry.name = lexeme;
                            entry.firstLine = line;
                            entry.firstCol = col;
                            entry.count = 1;
                            symIndex[lexeme] = res.symbols.size();
                            res.symbols.push_back(std::move(entry));
                        } else {
                            res.symbols[sit->second].count += 1;
                        }
                    }
                } else if (std::strcmp(rule.kind, "INT") == 0) {
                    tok.category = TokenCategory::INT;
                    tok.attr = lexeme;
                    tok.symbol = "IntConst";
                } else if (std::strcmp(rule.kind, "FLOAT") == 0) {
                    tok.category = TokenCategory::FLOAT;
                    tok.attr = lexeme;
                    tok.symbol = "floatConst";
                } else if (std::strcmp(rule.kind, "OP") == 0) {
                    tok.category = TokenCategory::OP;
                    auto it = opMap.find(lexeme);
                    tok.attr = std::to_string(it != opMap.end() ? it->second : 0);
                    tok.symbol = lexeme;
                } else if (std::strcmp(rule.kind, "SE") == 0) {
                    tok.category = TokenCategory::SE;
                    auto it = seMap.find(lexeme);
                    tok.attr = std::to_string(it != seMap.end() ? it->second : 0);
                    tok.symbol = lexeme;
                }

                res.rawTokens.push_back(tok);
                res.parserTokens.push_back(std::move(tok));
            }
        }

        for (std::size_t i = pos; i < lastEnd; ++i) {
            if (source[i] == '\n') {
                ++line;
                col = 1;
            } else {
                ++col;
            }
        }
        pos = lastEnd;
    }

    Token eofTok;
    eofTok.lexeme = "";
    eofTok.symbol = "EOF";
    eofTok.category = TokenCategory::END;
    eofTok.attr = "";
    eofTok.line = line;
    eofTok.col = col;
    res.parserTokens.push_back(std::move(eofTok));

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
    return detail::dumpAutomataJson(os, &impl_->nfa, &impl_->dfa, &impl_->minimal);
}

}  // namespace cmm
