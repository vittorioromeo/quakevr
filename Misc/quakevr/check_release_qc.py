#!/usr/bin/env python3
"""Fails unless the release flavour's QC compiles clean: no error, no warning, the 32-bit progs format.

A release's progs.dat leaves the tests out (fteqcc -DQVR_NO_TESTS: Windows/package-quakevr.ps1, QC/vr_macros.qc), a
flavour no development build compiles, so a test file without its guard or a game file calling into a test file
unguarded only shows on release day. This compiles it on every build; and every QC/*_test.qc must be wholly inside
the `#ifdef QVR_TESTS` guard (one with no test helper in it would compile either way, and ship).

Usage: python Misc/quakevr/check_release_qc.py [--fteqcc PATH]   (from anywhere; exit 1 and the reasons when it fails)
The compiler: --fteqcc, else $FTEQCC, else Tools/fteqcc/fteqcc64.exe, else fteqcc64/fteqcc on PATH (none: skipped).
Prints one line: the globals, statements and functions of the release progs. Its work files go to build/qcrelease/
(git-ignored), overwritten each run; QC/ and quakevr/progs.dat are not touched (checked).
"""
import argparse
import hashlib
import importlib.util
import os
import pathlib
import re
import shutil
import subprocess
import sys

TREE = pathlib.Path(__file__).resolve().parents[2]
QC = TREE / "QC"
WORK = TREE / "build" / "qcrelease"
DEV_PROGS = TREE / "quakevr" / "progs.dat"
# QC/build.sh's flags (the kit's, Visual Studio's, package-quakevr.ps1's), plus the flavour.
FLAGS = ["-DQVR_NO_TESTS", "-O3", "-Fautoproto", "-Olo", "-Fiffloat", "-Fifvector", "-Fvectorlogic", "-Flo", "-Fsubscope",
         "-Wall", "-Wextra", "-Wno-F209", "-Wno-F208"]

GUARD = "#ifdef QVR_TESTS"
# fteqcc's diagnostics: `file:line: error: ...` / `warning F123: ...`, and a final `... 3 warnings`-style summary.
PROBLEM = re.compile(r"\b(?:error|warning)\b[ :]", re.I)


def find_fteqcc(arg):
    own = TREE / "Tools" / "fteqcc" / "fteqcc64.exe"
    for c in (arg, os.environ.get("FTEQCC"), own if os.name == "nt" else None, "fteqcc64", "fteqcc"):
        found = c and (c if pathlib.Path(c).is_file() else shutil.which(str(c)))
        if found:
            return os.path.abspath(str(found))  # (it runs from WORK; QC/build.sh's is relative)
    return None


def unguarded_tests():
    """Every QC/**/*_test.qc whose code, comments aside, isn't wholly inside the QVR_TESTS guard."""
    bad = []
    for path in sorted(QC.rglob("*_test.qc")):
        code = [l.strip() for l in path.read_text(errors="replace").splitlines()]
        code = [l for l in code if l and not l.startswith("//")]
        if not code or code[0] != GUARD or not code[-1].startswith("#endif"):
            bad.append(path.relative_to(TREE).as_posix())
    return bad


def digest(path):
    """A file's hash and modified time, or None without it."""
    if not path.is_file():
        return None
    return hashlib.sha1(path.read_bytes()).hexdigest(), path.stat().st_mtime_ns


def compile_release(fteqcc):
    """Compiles QC/progs.src into WORK (as check_qc_precedence.py does); returns (exit code, compiler's output)."""
    WORK.mkdir(parents=True, exist_ok=True)
    entries = [l.split("//")[0].strip() for l in (QC / "progs.src").read_text().splitlines()]
    entries = [e for e in entries if e][1:]  # the first entry is the output file
    # Relative paths: fteqcc's progs.src reader stops at a drive's "C:".
    src = ["progs.dat"] + [pathlib.Path(os.path.relpath(QC / e, WORK)).as_posix() for e in entries]
    (WORK / "progs.src").write_text("\n".join(src) + "\n")
    if (QC / "fteqcc.ini").is_file():
        shutil.copyfile(QC / "fteqcc.ini", WORK / "fteqcc.ini")
    r = subprocess.run([fteqcc] + FLAGS + ["-srcfile", "progs.src"], cwd=WORK,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
    return r.returncode, r.stdout


def progs_format_module():
    spec = importlib.util.spec_from_file_location("check_progs_format", TREE / "Misc" / "quakevr" / "check_progs_format.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fteqcc")
    fteqcc = find_fteqcc(ap.parse_args().fteqcc)
    if not fteqcc:
        print("check_release_qc: no fteqcc found; skipped")
        return 0
    fail = False
    bad = unguarded_tests()
    if bad:
        fail = True
        print("check_release_qc: test files not wholly inside the guard (comments aside, first line `%s`, last "
              "`#endif`):" % GUARD)
        for b in bad:
            print("  " + b)
    before = digest(DEV_PROGS)
    out_progs = WORK / "progs.dat"
    if out_progs.is_file():
        out_progs.write_bytes(b"")  # (a failed compile must not pass on the last run's output)
    rc, out = compile_release(fteqcc)
    problems = [l for l in out.splitlines() if PROBLEM.search(l)]
    if rc != 0 or problems:
        fail = True
        print("check_release_qc: the release QC (-DQVR_NO_TESTS) doesn't compile clean: exit %d, %d lines of errors and "
              "warnings (a game file calling into a test file wraps the call in #ifdef QVR_TESTS: QC/vr_macros.qc):"
              % (rc, len(problems)))
        for l in problems[:30]:
            print("  " + l)
        if rc != 0 and not problems:
            print("\n".join(out.splitlines()[-10:]))
    if digest(DEV_PROGS) != before:
        fail = True
        print("check_release_qc: quakevr/progs.dat changed: this check must not touch it")
    if rc == 0:
        pf = progs_format_module()
        fmt, h = pf.progs_format(out_progs.read_bytes())
        if h is None or fmt != "fte32":
            fail = True
            print("check_release_qc: the release progs isn't the 32-bit format: %s" % fmt)
        else:
            print("check_release_qc: %s: %d globals, %d statements, %d functions, %d bytes" % (
                "FAILED" if fail else "ok", h[13], h[3], h[9], out_progs.stat().st_size))
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())
