import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, "build", "cmmc.exe")


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
    # 1. Default build (lab3.ref non-overlapping code table)
    build = run_cmd(["cmd", "/c", "build.bat"])
    print(build.stdout.strip())

    # 2. Verify test1~test3 exact match against official .ref
    for name in ["test1", "test2", "test3"]:
        sy_path = os.path.join(ROOT, "tests", f"{name}.sy")
        ref_path = os.path.join(ROOT, "tests", f"{name}.ref")
        actual = run_cmd([EXE, "--lex", sy_path]).stdout.replace("\r\n", "\n")
        with open(ref_path, "r", encoding="utf-8") as f:
            expected = f.read().replace("\r\n", "\n")
        if actual != expected:
            print(f"[FAIL] {name}.sy mismatch!")
            a_lines = actual.splitlines()
            e_lines = expected.splitlines()
            for idx, (al, el) in enumerate(zip(a_lines, e_lines), 1):
                if al != el:
                    print(f"  First diff at line {idx}:\n    actual:   {al!r}\n    expected: {el!r}")
                    break
            print(f"  Line counts: actual={len(a_lines)}, expected={len(e_lines)}")
            sys.exit(1)
        print(f"[PASS] {name}.sy matches {name}.ref ({len(actual.splitlines())} lines)")

    # 3. Verify unary '!', '!=', symbol table, and automata JSON dump
    dump_path = os.path.join(ROOT, "build", "automata.json")
    out = run_cmd([
        EXE, "--lex", "--symbols", "--dump-automata", dump_path,
        os.path.join(ROOT, "tests", "test_unary_not.sy")
    ]).stdout
    assert "!\t<OP,29>" in out, "Missing unary '!' token"
    assert "!=\t<OP,20>" in out, "Missing '!=' token"
    assert "3.14\t<FLOAT,3.14>" in out, "Missing float token"
    assert "=== Symbol Table ===" in out, "Missing symbol table section"
    assert "\ta\t" in out and "\tb\t" in out and "\tpi\t" in out, "Missing identifiers in symbol table"
    print("[PASS] test_unary_not.sy ('!', '!=', FLOAT, Symbol Table verified)")

    with open(dump_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    nfa_n = data["nfa"]["state_count"]
    dfa_n = data["dfa"]["state_count"]
    min_n = data["min_dfa"]["state_count"]
    print(f"[PASS] Automata stats: NFA={nfa_n} states -> DFA={dfa_n} states -> MinDFA={min_n} states")
    assert min_n <= dfa_n, "MinDFA should not have more states than DFA"

    # 4. Verify --strict-lex rejects test2.sy (which has illegal chars) before parsing
    strict_proc = run_cmd([EXE, "--parse", "--strict-lex", os.path.join(ROOT, "tests", "test2.sy")], expect_ok=False)
    assert "[Lexical Warning]" in strict_proc.stderr
    print("[PASS] --strict-lex aborts on lexical errors as expected")

    # 5. Compile and run C++ interface contract unit test
    contract_exe = os.path.join(ROOT, "build", "test_contracts.exe")
    run_cmd([
        "g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Iinclude",
        "tests/test_contracts.cpp",
        "src/lexer/automata.cpp", "src/lexer/lexer.cpp",
        "src/parser/parser_stub.cpp",
        "src/ir/ir_generator.cpp", "src/ir/ir_basic.cpp", "src/ir/ir_ctrl.cpp",
        "-o", contract_exe,
    ])
    print(run_cmd([contract_exe]).stdout.strip())

    # 6. Verify PDF original code table toggle compiles and switches codes
    pdf_exe = os.path.join(ROOT, "build", "cmmc_pdf_codes.exe")
    run_cmd([
        "g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Iinclude",
        "-DCMM_USE_PDF_ORIGINAL_CODES=1",
        "src/main.cpp",
        "src/lexer/automata.cpp", "src/lexer/lexer.cpp",
        "src/parser/parser_stub.cpp",
        "src/ir/ir_generator.cpp", "src/ir/ir_basic.cpp", "src/ir/ir_ctrl.cpp",
        "-o", pdf_exe,
    ])
    pdf_out = run_cmd([pdf_exe, "--lex", os.path.join(ROOT, "tests", "test_unary_not.sy")]).stdout
    assert "(\t<SE,20>" in pdf_out and ";\t<SE,24>" in pdf_out and "!\t<OP,26>" in pdf_out
    print("[PASS] -DCMM_USE_PDF_ORIGINAL_CODES=1 toggle verified")


if __name__ == "__main__":
    main()
