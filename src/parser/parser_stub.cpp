#include "Parser.h"

namespace cmm {

ParseResult Parser::parse(const std::vector<Token>& /*tokens*/, std::ostream* reductionOut) {
    ParseResult res;
    res.success = false;
    res.errors.push_back("Parser not implemented yet (Member 2 TODO: implement SLR parser in src/parser/).");
    if (reductionOut) {
        *reductionOut << "0\tProgram#EOF\terror\n";
    }
    return res;
}

bool Parser::dumpSLRTable(std::ostream& os) const {
    os << "# SLR table dump not implemented yet (Member 2 TODO)\n";
    return true;
}

}  // namespace cmm
