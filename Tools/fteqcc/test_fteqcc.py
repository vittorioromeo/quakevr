"""Regression tests for the patched FTEQCC (Tools/fteqcc/README.md): tiny QC programs that once crashed it.

Usage: python Tools/fteqcc/test_fteqcc.py [--fteqcc <exe>] [--keep]
Each case is compiled in <repo>/scratch/fteqcc_tests/<case>/ and must end with the expected exit code and output line
(a clean error, never a crash). Prints one verdict line, `fteqcc tests: N passed, M failed`, and exits 1 on a failure.
"""
import argparse, os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# name: (files, expected exit code, a line the output must contain)
CASES = {
    # An #include after any compile error: QCC_PR_ParseError(0, NULL) passed the NULL format to vsnprintf (stock
    # f937b9d88 crashes too: 0xC0000005; the MSVC-CRT build: 0xC0000409). Found via QC/vr_revolver.qc using the
    # undeclared `bound(...)`: the error was printed, then the compiler died at honey_defs.qc's #include.
    "include_after_error": ({
        "progs.src": "min.dat\nmin.qc\n",
        "min.qc": "void() f = { undeclared(); };\n#include \"inc.qc\"\n",
        "inc.qc": "float g;\n",
    }, 1, 'error: Unknown value "undeclared"'),
    # The same through the "X is not a member of Y" error (QCC_PR_ParseError(ERR_BADMEMBER, NULL), qcc_pr_comp.c).
    "bad_member": ({
        "progs.src": "min.dat\nmin.qc\n",
        "min.qc": "typedef struct { float a; } s_t;\nvoid() f = { local s_t s; s.b = 1; };\n",
    }, 1, "error: b is not a member of"),
    # Control: a program that compiles.
    "compiles": ({
        "progs.src": "min.dat\nmin.qc\n",
        "min.qc": "void() f = { };\n#include \"inc.qc\"\n",
        "inc.qc": "float g;\n",
    }, 0, "Done."),
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fteqcc", default=os.path.join(ROOT, "Tools", "fteqcc", "fteqcc64.exe"))
    args = ap.parse_args()
    passed = failed = 0
    for name, (files, want_rc, want_line) in CASES.items():
        d = os.path.join(ROOT, "scratch", "fteqcc_tests", name)
        os.makedirs(d, exist_ok=True)
        for fn, body in files.items():
            with open(os.path.join(d, fn), "w", newline="\n") as f:
                f.write(body)
        try:
            p = subprocess.run([args.fteqcc], cwd=d, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
            out, rc = p.stdout.decode("latin-1"), p.returncode
        except subprocess.TimeoutExpired:
            out, rc = "", "timed out"
        if rc == want_rc and want_line in out:
            passed += 1
        else:
            failed += 1
            code = hex(rc & 0xFFFFFFFF) if isinstance(rc, int) else rc
            print("FAIL %s: exit %s (want %d), %s" % (name, code, want_rc,
                  "output has %r" % want_line if want_line in out else "output lacks %r" % want_line))
    print("fteqcc tests: %d passed, %d failed" % (passed, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
