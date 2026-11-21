#pragma once

#include <cstdint>
#include <ostream>

namespace cmm {
namespace detail {

constexpr int ALPHABET = 128;
constexpr int MAX_NFA = 1024;
constexpr int MAX_DFA = 2048;
constexpr int SET_WORDS = (MAX_NFA + 63) / 64;

struct LexRule {
    const char* pattern;
    const char* kind;     // "WS", "IDN", "FLOAT", "INT", "OP", "SE"
    const char* example;
};

struct NFAState {
    int is_chars = 0;
    unsigned char chars[ALPHABET]{};
    int out[2]{-1, -1};
    int accept = -1;      // rule index, -1 means non-accepting
};

struct NFA {
    NFAState states[MAX_NFA];
    int count = 0;
    int start = 0;
};

struct StateSet {
    std::uint64_t words[SET_WORDS]{};
};

struct DFA {
    int transitions[MAX_DFA][ALPHABET]{};
    int accept[MAX_DFA]{};
    StateSet subsets[MAX_DFA]{};
    int count = 0;
    int start = 0;
    int dead = -1;
};

extern const LexRule RULES[];
extern const int RULE_COUNT;

void buildNFA(NFA* nfa);
void determinize(const NFA* nfa, DFA* dfa);
void minimizeDFA(const DFA* dfa, DFA* minimal);
bool dumpAutomataJson(std::ostream& out, const NFA* nfa, const DFA* dfa, const DFA* minimal);

}  // namespace detail
}  // namespace cmm
