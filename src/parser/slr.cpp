#include "slr.h"

#include <stdexcept>

namespace cmm::parser {
namespace {

const std::string epsilon = "";

}  // namespace

SlrTable::SlrTable() : productions(makeGrammar()) {
    for (int i = 0; i < static_cast<int>(productions.size()); ++i) {
        rules[productions[i].lhs].push_back(i);
    }
    computeSets();
    buildStates();
    buildTable();
}

SlrTable::Symbols SlrTable::firstOf(const std::vector<std::string>& rhs,
                                   std::size_t start) const {
    Symbols result;
    for (std::size_t i = start; i < rhs.size(); ++i) {
        const auto& symbols = first.at(rhs[i]);
        for (const auto& symbol : symbols) {
            if (symbol != epsilon) result.insert(symbol);
        }
        if (!symbols.count(epsilon)) return result;
    }
    result.insert(epsilon);
    return result;
}

void SlrTable::computeSets() {
    for (const auto& rule : rules) {
        first[rule.first];
        follow[rule.first];
    }
    for (const auto& prod : productions) {
        for (const auto& symbol : prod.rhs) {
            if (!rules.count(symbol)) first[symbol].insert(symbol);
        }
    }

    bool changed;
    do {
        changed = false;
        for (const auto& prod : productions) {
            auto symbols = firstOf(prod.rhs, 0);
            auto& dest = first[prod.lhs];
            auto before = dest.size();
            dest.insert(symbols.begin(), symbols.end());
            if (dest.size() != before) changed = true;
        }
    } while (changed);

    follow[productions.front().lhs].insert("EOF");
    do {
        changed = false;
        for (const auto& prod : productions) {
            for (std::size_t i = 0; i < prod.rhs.size(); ++i) {
                if (!rules.count(prod.rhs[i])) continue;
                auto symbols = firstOf(prod.rhs, i + 1);
                if (symbols.erase(epsilon)) {
                    const auto& parent = follow.at(prod.lhs);
                    symbols.insert(parent.begin(), parent.end());
                }
                auto& dest = follow[prod.rhs[i]];
                auto before = dest.size();
                dest.insert(symbols.begin(), symbols.end());
                if (dest.size() != before) changed = true;
            }
        }
    } while (changed);
}

SlrTable::Items SlrTable::closure(Items items) const {
    std::vector<Item> pending(items.begin(), items.end());
    for (std::size_t i = 0; i < pending.size(); ++i) {
        auto [rule, dot] = pending[i];
        const auto& rhs = productions[rule].rhs;
        if (dot == rhs.size()) continue;
        auto it = rules.find(rhs[dot]);
        if (it == rules.end()) continue;
        for (int next : it->second) {
            Item item{next, 0};
            if (items.insert(item).second) pending.push_back(item);
        }
    }
    return items;
}

void SlrTable::buildStates() {
    states.push_back(closure({{0, 0}}));
    std::map<Items, int> numbers{{states.front(), 0}};
    for (std::size_t i = 0; i < states.size(); ++i) {
        std::map<std::string, Items> kernels;
        for (auto [rule, dot] : states[i]) {
            const auto& rhs = productions[rule].rhs;
            if (dot < rhs.size()) kernels[rhs[dot]].emplace(rule, dot + 1);
        }
        transitions.emplace_back();
        for (auto& [symbol, kernel] : kernels) {
            auto next = closure(std::move(kernel));
            auto [it, added] = numbers.emplace(next, static_cast<int>(states.size()));
            if (added) states.push_back(std::move(next));
            transitions[i][symbol] = it->second;
        }
    }
}

void SlrTable::setAction(int state, const std::string& symbol, Action entry) {
    auto [it, added] = action[state].emplace(symbol, entry);
    if (added) return;
    // 移进项先填；只有无 else 的 if 规则可以让位。
    if (symbol == "else" && it->second.kind == ActionKind::Shift &&
        entry.kind == ActionKind::Reduce) {
        const auto& prod = productions[entry.target];
        if (prod.lhs == "stmt" && prod.rhs.size() == 5 && prod.rhs.front() == "if") {
            elseConflicts.push_back(state);
            return;
        }
    }
    throw std::logic_error("SLR conflict in state " + std::to_string(state) +
                           " on '" + symbol + "'");
}

void SlrTable::buildTable() {
    action.resize(states.size());
    goTo.resize(states.size());
    for (int i = 0; i < static_cast<int>(states.size()); ++i) {
        for (const auto& [symbol, target] : transitions[i]) {
            if (rules.count(symbol)) goTo[i][symbol] = target;
            else setAction(i, symbol, {ActionKind::Shift, target});
        }
        for (auto [rule, dot] : states[i]) {
            const auto& prod = productions[rule];
            if (dot != prod.rhs.size()) continue;
            if (rule == 0) setAction(i, "EOF", {ActionKind::Accept, 0});
            else {
                for (const auto& symbol : follow.at(prod.lhs)) {
                    setAction(i, symbol, {ActionKind::Reduce, rule});
                }
            }
        }
    }
}

bool SlrTable::dump(std::ostream& os) const {
    os << "[Productions]\n";
    for (std::size_t i = 0; i < productions.size(); ++i) {
        const auto& prod = productions[i];
        os << i << '\t' << prod.lhs << " ->";
        if (prod.rhs.empty()) os << " epsilon";
        for (const auto& symbol : prod.rhs) os << ' ' << symbol;
        os << '\n';
    }

    auto printSets = [&](const char* title, const auto& sets) {
        os << "\n[" << title << "]\n";
        for (const auto& [symbol, members] : sets) {
            os << symbol << ':';
            for (const auto& member : members) os << ' ' << (member.empty() ? "epsilon" : member);
            os << '\n';
        }
    };
    printSets("FIRST", first);
    printSets("FOLLOW", follow);

    os << "\n[LR(0) items]\n";
    for (std::size_t i = 0; i < states.size(); ++i) {
        os << 'I' << i << ":\n";
        for (auto [rule, dot] : states[i]) {
            const auto& prod = productions[rule];
            os << "  " << prod.lhs << " ->";
            for (std::size_t j = 0; j <= prod.rhs.size(); ++j) {
                if (j == dot) os << " .";
                if (j < prod.rhs.size()) os << ' ' << prod.rhs[j];
            }
            os << '\n';
        }
    }

    os << "\n[ACTION]\nstate\tsymbol\taction\n";
    for (std::size_t i = 0; i < action.size(); ++i) {
        for (const auto& [symbol, entry] : action[i]) {
            os << i << '\t' << symbol << '\t';
            switch (entry.kind) {
                case ActionKind::Shift: os << 's' << entry.target; break;
                case ActionKind::Reduce: os << 'r' << entry.target; break;
                case ActionKind::Accept: os << "acc"; break;
            }
            os << '\n';
        }
    }
    os << "\n[GOTO]\nstate\tsymbol\ttarget\n";
    for (std::size_t i = 0; i < goTo.size(); ++i) {
        for (const auto& [symbol, target] : goTo[i]) {
            os << i << '\t' << symbol << '\t' << target << '\n';
        }
    }
    os << "\n[Resolved conflicts]\n";
    for (int state : elseConflicts) os << "I" << state << " on else: shift\n";
    return static_cast<bool>(os);
}

const SlrTable& slrTable() {
    static const SlrTable table;
    return table;
}

}  // namespace cmm::parser
