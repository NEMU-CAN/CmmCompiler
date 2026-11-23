import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IS_WIN = sys.platform == "win32"
EXE_EXT = ".exe" if IS_WIN else ""
DEFAULT_EXE = os.path.join(ROOT, "build", "cmmc" + EXE_EXT)

CORE_SRCS = [
    "src/lexer/automata.cpp",
    "src/lexer/lexer.cpp",
    "src/parser/grammar.cpp",
    "src/parser/parser.cpp",
    "src/parser/slr.cpp",
    "src/ir/ir_generator.cpp",
    "src/ir/ir_basic.cpp",
    "src/ir/ir_ctrl.cpp",
]


def run_cmd(args, expect_ok=True):
    proc = subprocess.run(args, cwd=ROOT, capture_output=True, text=True)
    if expect_ok and proc.returncode != 0:
        print("FAILED:", " ".join(args))
        print(proc.stdout)
        print(proc.stderr)
        sys.exit(1)
    if not expect_ok and proc.returncode == 0:
        print("EXPECTED FAILURE BUT SUCCEEDED:", " ".join(args))
        sys.exit(1)
    return proc


def main():
    os.makedirs(os.path.join(ROOT, "build"), exist_ok=True)

    # 1. Default build (Big Assignment PDF original code table)
    if IS_WIN:
        build = run_cmd(["cmd", "/c", "build.bat"])
    else:
        build = run_cmd(["make"])
    if build.stdout.strip():
        print(build.stdout.strip())

    # 2. Verify default build matches Big Assignment PDF table (= <OP,11>, ; <SE,24>, ! <OP,26>)
    dump_path = os.path.join(ROOT, "build", "automata.json")
    unary_sy = os.path.join(ROOT, "tests", "custom", "unary_not.sy")
    out = run_cmd([DEFAULT_EXE, "--lex", "--symbols", "--dump-automata", dump_path, unary_sy]).stdout
    assert "=\t<OP,11>" in out, "Default PDF table: expected '=' -> <OP,11>"
    assert ";\t<SE,24>" in out, "Default PDF table: expected ';' -> <SE,24>"
    assert "(\t<SE,20>" in out, "Default PDF table: expected '(' -> <SE,20>"
    assert "!\t<OP,26>" in out, "Default PDF table: expected '!' -> <OP,26>"
    assert "!=\t<OP,17>" in out, "Default PDF table: expected '!=' -> <OP,17>"
    assert "3.14\t<FLOAT,3.14>" in out, "Missing float token"
    assert "=== Symbol Table ===" in out, "Missing symbol table section"
    assert "\ta\t" in out and "\tb\t" in out and "\tpi\t" in out, "Missing identifiers in symbol table"
    print("[PASS] Default build (PDF original code table) + unary '!', FLOAT, Symbol Table verified")

    with open(dump_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    nfa_n = data["nfa"]["state_count"]
    dfa_n = data["dfa"]["state_count"]
    min_n = data["min_dfa"]["state_count"]
    print(f"[PASS] Automata stats: NFA={nfa_n} states -> DFA={dfa_n} states -> MinDFA={min_n} states")
    assert min_n <= dfa_n, "MinDFA should not have more states than DFA"

    # 3. Verify --strict-lex aborts on lexical errors in test2.sy
    strict_proc = run_cmd(
        [DEFAULT_EXE, "--parse", "--strict-lex", os.path.join(ROOT, "tests", "lexer_lab3", "test2.sy")],
        expect_ok=False,
    )
    assert "[Lexical Warning]" in strict_proc.stderr
    print("[PASS] --strict-lex aborts on lexical errors as expected")

    # 4. Build with -DCMM_USE_LAB3_CODES=1 and verify byte-for-byte match against lab3 test1~3.ref
    lab3_exe = os.path.join(ROOT, "build", "cmmc_lab3" + EXE_EXT)
    run_cmd([
        "g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Iinclude",
        "-DCMM_USE_LAB3_CODES=1", "src/main.cpp", *CORE_SRCS, "-o", lab3_exe,
    ])
    for name in ["test1", "test2", "test3"]:
        sy_path = os.path.join(ROOT, "tests", "lexer_lab3", f"{name}.sy")
        ref_path = os.path.join(ROOT, "tests", "lexer_lab3", f"{name}.ref")
        actual = run_cmd([lab3_exe, "--lex", sy_path]).stdout.replace("\r\n", "\n")
        with open(ref_path, "r", encoding="utf-8") as f:
            expected = f.read().replace("\r\n", "\n")
        if actual != expected:
            print(f"[FAIL] {name}.sy mismatch under -DCMM_USE_LAB3_CODES=1!")
            for idx, (al, el) in enumerate(zip(actual.splitlines(), expected.splitlines()), 1):
                if al != el:
                    print(f"  First diff at line {idx}:\n    actual:   {al!r}\n    expected: {el!r}")
                    break
            sys.exit(1)
        print(f"[PASS] lexer_lab3/{name}.sy matches {name}.ref ({len(actual.splitlines())} lines)")

    # 5. Compile and run C++ interface contract unit test
    contract_exe = os.path.join(ROOT, "build", "test_contracts" + EXE_EXT)
    run_cmd([
        "g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Iinclude",
        "tests/test_contracts.cpp", *CORE_SRCS, "-o", contract_exe,
    ])
    print(run_cmd([contract_exe]).stdout.strip())


if __name__ == "__main__":
    main()
