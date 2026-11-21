import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, "build", "cmmc.exe")


def run_cmd(args):
    proc = subprocess.run(args, cwd=ROOT, capture_output=True, text=True)
    if proc.returncode != 0:
        print("FAILED:", " ".join(args))
        print(proc.stderr)
        sys.exit(1)
    return proc.stdout


def main():
    # 1. Build
    build = subprocess.run(["cmd", "/c", "build.bat"], cwd=ROOT, capture_output=True, text=True)
    if build.returncode != 0:
        print(build.stdout)
        print(build.stderr)
        sys.exit(1)
    print(build.stdout.strip())

    # 2. Verify test1~test3 exact match against official .ref
    for name in ["test1", "test2", "test3"]:
        sy_path = os.path.join(ROOT, "tests", f"{name}.sy")
        ref_path = os.path.join(ROOT, "tests", f"{name}.ref")
        actual = run_cmd([EXE, "--lex", sy_path]).replace("\r\n", "\n")
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
    ])
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


if __name__ == "__main__":
    main()
