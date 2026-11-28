#pragma once

#include <map>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cmm::parser {

struct Production {
    std::string lhs;
    std::vector<std::string> rhs;
};

std::vector<Production> makeGrammar();

enum class ActionKind { Shift, Reduce, Accept };

struct Action {
    ActionKind kind;
    int target;
};

class SlrTable {
public:
    SlrTable();
    bool dump(std::ostream& os) const;

    const std::vector<Production> productions;
    std::vector<std::map<std::string, Action>> action;
    std::vector<std::map<std::string, int>> goTo;

private:
    using Symbols = std::set<std::string>;
    using Item = std::pair<int, std::size_t>; // 产生式编号、点的位置
    using Items = std::set<Item>;

    std::map<std::string, std::vector<int>> rules;
    std::map<std::string, Symbols> first;
    std::map<std::string, Symbols> follow;
    std::vector<Items> states;
    std::vector<std::map<std::string, int>> transitions;
    std::vector<int> elseConflicts;

    Symbols firstOf(const std::vector<std::string>& rhs, std::size_t start) const;
    void computeSets();
    Items closure(Items items) const;
    void buildStates();
    void buildTable();
    void setAction(int state, const std::string& symbol, Action entry);
};

const SlrTable& slrTable();

}  // namespace cmm::parser
