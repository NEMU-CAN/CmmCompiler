#include "automata.h"
#include "TokenConfig.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <stdexcept>
#include <string>

namespace cmm {
namespace detail {

// Earlier rules win only when matched lengths are equal (maximal munch first).
// Keywords are recognized case-insensitively after a complete IDN is matched.
// FLOAT strictly follows Appendix 1 rule 36: [0-9]+'.'[0-9]+
const LexRule RULES[] = {
    {"[ \t\r\n\v\f]+",          "WS",    " "},
    {"[a-zA-Z_][a-zA-Z0-9_]*",  "IDN",   "_x9"},
    {"[0-9]+\\.[0-9]+",         "FLOAT", "3.14"},
    {"[0-9]+",                  "INT",   "42"},
    {"\\+", "OP", "+"},   {"-",   "OP", "-"},
    {"\\*", "OP", "*"},   {"/",   "OP", "/"},
    {"%",   "OP", "%"},   {"=",   "OP", "="},
    {">",   "OP", ">"},   {"<",   "OP", "<"},
    {"==",  "OP", "=="},  {"<=",  "OP", "<="},
    {">=",  "OP", ">="},  {"!=",  "OP", "!="},
    {"&&",  "OP", "&&"},  {"\\|\\|", "OP", "||"},
    {"!",   "OP", "!"},
    {"\\(", "SE", "("},   {"\\)", "SE", ")"},
    {"{",   "SE", "{"},   {"}",   "SE", "}"},
    {";",   "SE", ";"},   {",",   "SE", ","}
};

const int RULE_COUNT = static_cast<int>(sizeof(RULES) / sizeof(RULES[0]));

namespace {

struct Fragment {
    int start;
    int end;
};

struct RegexParser {
    const unsigned char* p;
    NFA* nfa;
};

int newState(NFA* nfa) {
    if (nfa->count >= MAX_NFA) {
        throw std::runtime_error("Automaton error: NFA capacity exceeded");
    }
    int index = nfa->count++;
    NFAState* state = &nfa->states[index];
    *state = NFAState{};
    return index;
}

int escapedChar(RegexParser* parser) {
    int ch = *parser->p++;
    if (!ch) throw std::runtime_error("Automaton error: unexpected end of regex");
    if (ch == '\\') {
        ch = *parser->p++;
        if (!ch) throw std::runtime_error("Automaton error: trailing regex escape");
        switch (ch) {
            case 't': return '\t';
            case 'n': return '\n';
            case 'r': return '\r';
            case 'v': return '\v';
            case 'f': return '\f';
            default: break;
        }
    }
    if (ch >= ALPHABET) throw std::runtime_error("Automaton error: regex must be ASCII");
    return ch;
}

Fragment parseUnion(RegexParser* parser);

Fragment parseAtom(RegexParser* parser) {
    NFA* nfa = parser->nfa;
    Fragment result{-1, -1};
    if (*parser->p == '(') {
        ++parser->p;
        result = parseUnion(parser);
        if (*parser->p++ != ')') {
            throw std::runtime_error("Automaton error: missing closing parenthesis");
        }
        return result;
    }
    result.start = newState(nfa);
    result.end = newState(nfa);
    nfa->states[result.start].is_chars = 1;
    nfa->states[result.start].out[0] = result.end;
    if (*parser->p == '[') {
        ++parser->p;
        if (!*parser->p || *parser->p == ']') {
            throw std::runtime_error("Automaton error: empty character class");
        }
        while (*parser->p && *parser->p != ']') {
            int first = escapedChar(parser);
            int last = first;
            if (*parser->p == '-' && parser->p[1] && parser->p[1] != ']') {
                ++parser->p;
                last = escapedChar(parser);
                if (last < first) {
                    throw std::runtime_error("Automaton error: invalid character range");
                }
            }
            for (int c = first; c <= last; ++c) {
                nfa->states[result.start].chars[c] = 1;
            }
        }
        if (*parser->p != ']') {
            throw std::runtime_error("Automaton error: missing closing bracket");
        }
        ++parser->p;
    } else {
        int ch = escapedChar(parser);
        nfa->states[result.start].chars[ch] = 1;
    }
    return result;
}

Fragment parseRepeat(RegexParser* parser) {
    Fragment body = parseAtom(parser);
    NFA* nfa = parser->nfa;
    while (*parser->p == '*' || *parser->p == '+' || *parser->p == '?') {
        int op = *parser->p++;
        Fragment wrapper{newState(nfa), newState(nfa)};
        nfa->states[wrapper.start].out[0] = body.start;
        if (op != '+') nfa->states[wrapper.start].out[1] = wrapper.end;
        nfa->states[body.end].out[0] = wrapper.end;
        if (op != '?') nfa->states[body.end].out[1] = body.start;
        body = wrapper;
    }
    return body;
}

Fragment parseConcat(RegexParser* parser) {
    Fragment result{-1, -1};
    while (*parser->p && *parser->p != '|' && *parser->p != ')') {
        Fragment next = parseRepeat(parser);
        if (result.start < 0) {
            result = next;
        } else {
            parser->nfa->states[result.end].out[0] = next.start;
            result.end = next.end;
        }
    }
    if (result.start < 0) {
        result.start = newState(parser->nfa);
        result.end = newState(parser->nfa);
        parser->nfa->states[result.start].out[0] = result.end;
    }
    return result;
}

Fragment parseUnion(RegexParser* parser) {
    Fragment left = parseConcat(parser);
    NFA* nfa = parser->nfa;
    while (*parser->p == '|') {
        ++parser->p;
        Fragment right = parseConcat(parser);
        Fragment joined{newState(nfa), newState(nfa)};
        nfa->states[joined.start].out[0] = left.start;
        nfa->states[joined.start].out[1] = right.start;
        nfa->states[left.end].out[0] = joined.end;
        nfa->states[right.end].out[0] = joined.end;
        left = joined;
    }
    return left;
}

inline int contains(const StateSet* set, int state) {
    return static_cast<int>((set->words[state / 64] >> (state % 64)) & UINT64_C(1));
}

inline void insert(StateSet* set, int state) {
    set->words[state / 64] |= UINT64_C(1) << (state % 64);
}

void epsilonClosure(const NFA* nfa, StateSet* set) {
    int stack[MAX_NFA];
    int top = 0;
    for (int i = 0; i < nfa->count; ++i) {
        if (contains(set, i)) stack[top++] = i;
    }
    while (top > 0) {
        const NFAState* state = &nfa->states[stack[--top]];
        if (!state->is_chars) {
            for (int i = 0; i < 2; ++i) {
                int target = state->out[i];
                if (target >= 0 && !contains(set, target)) {
                    insert(set, target);
                    stack[top++] = target;
                }
            }
        }
    }
}

int acceptance(const NFA* nfa, const StateSet* set) {
    int result = -1;
    for (int i = 0; i < nfa->count; ++i) {
        int label = nfa->states[i].accept;
        if (contains(set, i) && label >= 0 && (result < 0 || label < result)) {
            result = label;
        }
    }
    return result;
}

int internSubset(const NFA* nfa, DFA* dfa, const StateSet* set) {
    static const StateSet empty{};
    for (int i = 0; i < dfa->count; ++i) {
        if (std::memcmp(&dfa->subsets[i], set, sizeof(*set)) == 0) {
            return i;
        }
    }
    if (dfa->count >= MAX_DFA) {
        throw std::runtime_error("Automaton error: DFA capacity exceeded");
    }
    int idx = dfa->count++;
    dfa->subsets[idx] = *set;
    dfa->accept[idx] = acceptance(nfa, set);
    if (std::memcmp(set, &empty, sizeof(*set)) == 0) {
        dfa->dead = idx;
    }
    return idx;
}

bool sameSignature(const DFA* dfa, const int* partition, int a, int b) {
    if (partition[a] != partition[b]) return false;
    for (int c = 0; c < ALPHABET; ++c) {
        if (partition[dfa->transitions[a][c]] != partition[dfa->transitions[b][c]]) {
            return false;
        }
    }
    return true;
}

void writeJsonString(std::ostream& out, const char* text) {
    out << '"';
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
        if (*p == '\\' || *p == '"') {
            out << '\\' << static_cast<char>(*p);
        } else if (*p < 32) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", *p);
            out << buf;
        } else {
            out << static_cast<char>(*p);
        }
    }
    out << '"';
}

void dumpDfaJson(std::ostream& out, const DFA* dfa) {
    out << "{\"state_count\":" << dfa->count
        << ",\"start\":" << dfa->start
        << ",\"dead\":" << dfa->dead
        << ",\"accept\":[";
    for (int i = 0; i < dfa->count; ++i) {
        if (i) out << ',';
        out << dfa->accept[i];
    }
    out << "],\"transitions\":[\n";
    for (int i = 0; i < dfa->count; ++i) {
        out << (i ? ",[" : "[");
        for (int c = 0; c < ALPHABET; ++c) {
            if (c) out << ',';
            out << dfa->transitions[i][c];
        }
        out << "]\n";
    }
    out << "]}";
}

int lookupRuleCode(const LexRule& rule) {
    if (std::strcmp(rule.kind, "OP") == 0) {
        const auto& ops = TokenCodeTable::operators();
        auto it = ops.find(rule.example);
        return it != ops.end() ? it->second : 0;
    }
    if (std::strcmp(rule.kind, "SE") == 0) {
        const auto& ses = TokenCodeTable::delimiters();
        auto it = ses.find(rule.example);
        return it != ses.end() ? it->second : 0;
    }
    return 0;
}

}  // namespace

void buildNFA(NFA* nfa) {
    nfa->count = 0;
    nfa->start = newState(nfa);
    int hub = nfa->start;
    for (int r = 0; r < RULE_COUNT; ++r) {
        RegexParser parser{reinterpret_cast<const unsigned char*>(RULES[r].pattern), nfa};
        Fragment f = parseUnion(&parser);
        if (*parser.p) {
            throw std::runtime_error("Automaton error: unconsumed regex character");
        }
        nfa->states[f.end].accept = r;
        nfa->states[hub].out[0] = f.start;
        if (r + 1 < RULE_COUNT) {
            int next = newState(nfa);
            nfa->states[hub].out[1] = next;
            hub = next;
        }
    }
}

void determinize(const NFA* nfa, DFA* dfa) {
    std::memset(dfa->transitions, 0, sizeof(dfa->transitions));
    std::memset(dfa->accept, 0, sizeof(dfa->accept));
    dfa->count = 0;
    dfa->start = 0;
    dfa->dead = -1;
    StateSet start{};
    insert(&start, nfa->start);
    epsilonClosure(nfa, &start);
    dfa->start = internSubset(nfa, dfa, &start);

    for (int i = 0; i < dfa->count; ++i) {
        for (int c = 0; c < ALPHABET; ++c) {
            StateSet moved{};
            for (int j = 0; j < nfa->count; ++j) {
                const NFAState* state = &nfa->states[j];
                if (contains(&dfa->subsets[i], j) && state->is_chars && state->chars[c]) {
                    insert(&moved, state->out[0]);
                }
            }
            epsilonClosure(nfa, &moved);
            dfa->transitions[i][c] = internSubset(nfa, dfa, &moved);
        }
    }
}

void minimizeDFA(const DFA* dfa, DFA* minimal) {
    int partition[MAX_DFA];
    int next[MAX_DFA];
    int representative[MAX_DFA];
    std::memset(minimal->transitions, 0, sizeof(minimal->transitions));
    std::memset(minimal->accept, 0, sizeof(minimal->accept));
    minimal->count = 0;
    minimal->start = 0;
    minimal->dead = -1;

    int count = 0;
    for (int i = 0; i < dfa->count; ++i) {
        int j = 0;
        for (; j < i; ++j) {
            if (dfa->accept[i] == dfa->accept[j]) break;
        }
        partition[i] = (j < i) ? partition[j] : count++;
    }

    for (;;) {
        int newCount = 0;
        for (int i = 0; i < dfa->count; ++i) {
            int j = 0;
            for (; j < i; ++j) {
                if (sameSignature(dfa, partition, i, j)) break;
            }
            next[i] = (j < i) ? next[j] : newCount++;
        }
        std::memcpy(partition, next, static_cast<std::size_t>(dfa->count) * sizeof(*partition));
        if (newCount == count) break;
        count = newCount;
    }

    minimal->count = count;
    minimal->start = partition[dfa->start];
    minimal->dead = (dfa->dead < 0) ? -1 : partition[dfa->dead];
    for (int i = 0; i < count; ++i) representative[i] = -1;
    for (int i = 0; i < dfa->count; ++i) {
        if (representative[partition[i]] < 0) representative[partition[i]] = i;
    }
    for (int i = 0; i < count; ++i) {
        int r = representative[i];
        minimal->accept[i] = dfa->accept[r];
        for (int c = 0; c < ALPHABET; ++c) {
            minimal->transitions[i][c] = partition[dfa->transitions[r][c]];
        }
    }
}

bool dumpAutomataJson(std::ostream& out, const NFA* nfa, const DFA* dfa, const DFA* minimal) {
    out << "{\"rules\":[\n";
    for (int i = 0; i < RULE_COUNT; ++i) {
        out << (i ? ",{\"pattern\":" : "{\"pattern\":");
        writeJsonString(out, RULES[i].pattern);
        out << ",\"kind\":";
        writeJsonString(out, RULES[i].kind);
        out << ",\"code\":" << lookupRuleCode(RULES[i]) << ",\"example\":";
        writeJsonString(out, RULES[i].example);
        out << "}\n";
    }
    out << "],\"nfa\":{\"state_count\":" << nfa->count << ",\"start\":" << nfa->start << ",\"states\":[\n";
    for (int i = 0; i < nfa->count; ++i) {
        const NFAState* s = &nfa->states[i];
        out << (i ? "," : "")
            << "{\"kind\":\"" << (s->is_chars ? "chars" : "epsilon")
            << "\",\"accept\":" << s->accept
            << ",\"out\":[" << s->out[0] << "," << s->out[1] << "],\"chars\":[";
        bool first = true;
        for (int c = 0; c < ALPHABET; ++c) {
            if (s->chars[c]) {
                if (!first) out << ',';
                out << c;
                first = false;
            }
        }
        out << "]}\n";
    }
    out << "]},\"dfa\":";
    dumpDfaJson(out, dfa);
    out << ",\"min_dfa\":";
    dumpDfaJson(out, minimal);
    out << "}\n";
    return static_cast<bool>(out);
}

}  // namespace detail
}  // namespace cmm
