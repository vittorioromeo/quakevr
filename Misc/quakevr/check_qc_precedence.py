#!/usr/bin/env python3
"""Fails when a QC expression means something else to fteqcc than to a C reader (docs/vr-port/CODE_STYLE.md, QuakeC).

fteqcc's default ("QC") operator priorities are not C's. With our flags (QC/build.sh), fteqcc 64:
  - `x = a && b` is `(x = a) && b`: an assignment statement (`=`, `+=`, a field's `self.f = ...`) takes only the
    first operand of `&&` / `||`; the rest is computed and thrown away. (A declaration's initializer,
    `float x = a && b;`, a `return` and a call's argument are whole expressions: fine.)
  - `a && b ? c : d` is `a && (b ? c : d)`: `?:` binds tighter than `&&` / `||`, everywhere.
  - `a || b && c` is `(a || b) && c`: `&&` and `||` have the same priority, left to right.
  - `!a == b` is `!(a == b)` (fteqcc warns, F304); `a & b == c` is `(a & b) == c`.
Comparisons against arithmetic (`time - last < 0.2`) are as in C.

The check compiles the progs twice, with QC priorities (the build's) and with C priorities (-Fcpriority), and
compares each function's instructions. A function that differs holds an expression that depends on the priorities:
parenthesise it (or use a temporary) so it reads the same both ways.

Usage: python Misc/quakevr/check_qc_precedence.py [--fteqcc PATH]   (from anywhere; exit 1 and the sites when any)
The compiler: --fteqcc, else $FTEQCC, else C:/OHWorkspace/quakevr/QC/fteqcc64.exe, else fteqcc64/fteqcc on PATH.
Work files go to build/qcprec/ (git-ignored), overwritten each run; QC/ and quakevr/progs.dat are not touched.
"""
import argparse
import concurrent.futures
import os
import pathlib
import re
import shutil
import subprocess
import sys

TREE = pathlib.Path(__file__).resolve().parents[2]
QC = TREE / "QC"
WORK = TREE / "build" / "qcprec"
# QC/build.sh's flags, plus the asm listing.
FLAGS = ["-O3", "-Fautoproto", "-Olo", "-Fiffloat", "-Fifvector", "-Fvectorlogic", "-Flo", "-Fsubscope",
         "-Wall", "-Wextra", "-Wno-F209", "-Wno-F208", "-Fwasm"]

LINE_NO = re.compile(r"\s*/\*(\d+)\*/\s*$")
# Temporaries are numbered per function and may be allocated differently: compare them by role, not number.
TEMP = re.compile(r"\b(?:temp|locked)_\d+\b")


def find_fteqcc(arg):
    for c in (arg, os.environ.get("FTEQCC"), "C:/OHWorkspace/quakevr/QC/fteqcc64.exe", "fteqcc64", "fteqcc"):
        found = c and (c if pathlib.Path(c).is_file() else shutil.which(c))
        if found:
            return found
    return None


def compile_asm(fteqcc, sub, extra):
    """Compiles QC/progs.src into WORK/sub; returns {function: [(line, instruction)]}."""
    out = WORK / sub
    out.mkdir(parents=True, exist_ok=True)
    entries = [l.split("//")[0].strip() for l in (QC / "progs.src").read_text().splitlines()]
    entries = [e for e in entries if e][1:]  # the first entry is the output file
    # Relative paths: fteqcc's progs.src reader stops at a drive's "C:".
    src = ["progs.dat"] + [pathlib.Path(os.path.relpath(QC / e, out)).as_posix() for e in entries]
    (out / "progs.src").write_text("\n".join(src) + "\n")
    if (QC / "fteqcc.ini").is_file():
        shutil.copyfile(QC / "fteqcc.ini", out / "fteqcc.ini")
    r = subprocess.run([fteqcc] + FLAGS + extra + ["-srcfile", "progs.src"], cwd=out,
                       capture_output=True, text=True, errors="replace")
    asm = out / "qc.asm"
    if r.returncode != 0 or "ERROR" in r.stdout or not asm.is_file():
        sys.exit("check_qc_precedence: the compile failed (%s):\n%s" % (sub, "\n".join(r.stdout.splitlines()[-10:])))
    # qc.asm is large (each function lists every local: -Flo shares one section): streamed, the locals skipped.
    funcs = {}
    name, body = None, None
    with open(asm, errors="replace") as f:
        for l in f:
            if l[0] == "\t" and body is not None:
                n = LINE_NO.search(l)
                body.append((int(n.group(1)) if n else 0, TEMP.sub("tmp", LINE_NO.sub("", l.strip()))))
            elif l.endswith(" = asm\n"):
                name, body = l[:-7], []
            elif l[0] == "}" and body is not None:
                funcs[name] = body
                name, body = None, None
    return funcs


def source_file(func, line):
    """The QC file that defines `func` (its name as qc.asm prints it: `float(entity) Name`) and has line `line`."""
    name = re.escape(func.split(")")[-1].strip())
    define = re.compile(r"^[^/\n]*\)\s*" + name + r"\s*=", re.M)
    for path in sorted(QC.rglob("*.qc")):
        text = path.read_text(errors="replace")
        if define.search(text) and text.count("\n") >= line - 1:
            return path.relative_to(TREE).as_posix()
    return "?"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fteqcc")
    fteqcc = find_fteqcc(ap.parse_args().fteqcc)
    if not fteqcc:
        print("check_qc_precedence: no fteqcc found; skipped")
        return 0
    with concurrent.futures.ThreadPoolExecutor(2) as pool:  # (each its own folder: they run side by side)
        qc_job = pool.submit(compile_asm, fteqcc, "qc", [])
        c_job = pool.submit(compile_asm, fteqcc, "c", ["-Fcpriority"])
        qc_prio, c_prio = qc_job.result(), c_job.result()
    found = []
    for name, body in qc_prio.items():
        other = c_prio.get(name)
        if other is None or [i for _, i in body] == [i for _, i in other]:
            continue
        # The first differing instruction's source line.
        k = next((j for j, (a, b) in enumerate(zip(body, other)) if a[1] != b[1]), min(len(body), len(other)) - 1)
        found.append((name, body[k][0] if body else 0))
    if found:
        print("QC expressions that read differently under C priorities (parenthesise them; "
              "docs/vr-port/CODE_STYLE.md, QuakeC):")
        for name, line in sorted(found, key=lambda f: f[0]):
            print("  %s: %s:%d" % (name, source_file(name, line), line))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
