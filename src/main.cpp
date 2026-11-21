#include "IRGenerator.h"
#include "Lexer.h"
#include "Parser.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static void printUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--lex | --parse | --ast | --ir] [--symbols] [--dump-automata <json>] <input.sy> [-o <output>]\n"
              << "  --lex                   Run lexer only and print token sequence\n"
              << "  --parse                 Run lexer + SLR parser and print reduction sequence\n"
              << "  --ast                   Run lexer + SLR parser and dump AST\n"
              << "  --ir                    Run full pipeline and emit LLVM IR (.ll) [default]\n"
              << "  --symbols               Print lexical symbol table (with --lex)\n"
              << "  --dump-automata <json>  Dump NFA/DFA/MinDFA tables to JSON\n";
}

int main(int argc, char* argv[]) {
    std::string mode = "ir";
    bool showSymbols = false;
    std::string dumpAutomataPath;
    std::string inputPath;
    std::string outputPath;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--lex") mode = "lex";
        else if (arg == "--parse") mode = "parse";
        else if (arg == "--ast") mode = "ast";
        else if (arg == "--ir") mode = "ir";
        else if (arg == "--symbols") showSymbols = true;
        else if (arg == "--dump-automata" && i + 1 < argc) dumpAutomataPath = argv[++i];
        else if (arg == "-o" && i + 1 < argc) outputPath = argv[++i];
        else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (inputPath.empty()) {
            inputPath = arg;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    if (inputPath.empty()) {
        printUsage(argv[0]);
        return 1;
    }

    std::ifstream ifs(inputPath, std::ios::binary);
    if (!ifs) {
        std::cerr << "Failed to open input file: " << inputPath << "\n";
        return 1;
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    std::string source = ss.str();

    std::ofstream ofs;
    std::ostream* out = &std::cout;
    if (!outputPath.empty()) {
        ofs.open(outputPath);
        if (!ofs) {
            std::cerr << "Failed to open output file: " << outputPath << "\n";
            return 1;
        }
        out = &ofs;
    }

    cmm::Lexer lexer;
    if (!dumpAutomataPath.empty()) {
        std::ofstream dumpOfs(dumpAutomataPath);
        if (!dumpOfs || !lexer.dumpAutomata(dumpOfs)) {
            std::cerr << "Failed to write automata JSON: " << dumpAutomataPath << "\n";
            return 1;
        }
    }
    cmm::LexResult lexRes = lexer.tokenize(source);

    if (mode == "lex") {
        cmm::Lexer::printTokens(lexRes, *out);
        if (showSymbols) {
            *out << "\n=== Symbol Table ===\n";
            cmm::Lexer::printSymbolTable(lexRes, *out);
        }
        return 0;
    }

    cmm::Parser parser;
    cmm::ParseResult parseRes = parser.parse(lexRes.parserTokens, mode == "parse" ? out : nullptr);
    if (!parseRes.success) {
        for (const auto& err : parseRes.errors) {
            std::cerr << "[Syntax Error] " << err << "\n";
        }
        return 1;
    }

    if (mode == "parse") {
        return 0;
    }
    if (mode == "ast") {
        if (parseRes.root) {
            parseRes.root->dump(*out);
        }
        return 0;
    }

    cmm::IRGenerator irGen;
    std::vector<std::string> irErrors;
    if (!irGen.emitToStream(parseRes.root.get(), *out, &irErrors)) {
        for (const auto& err : irErrors) {
            std::cerr << "[Semantic/IR Error] " << err << "\n";
        }
        return 1;
    }
    return 0;
}
