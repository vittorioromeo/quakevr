#!/usr/bin/env python3
"""Fails on a mutable function-local `static std::...` or `thread_local` in Quake/vr (docs/vr-port/CODE_STYLE.md,
"Scratch buffers and caches"): a scratch buffer goes into its file's `mem::Scratch` set, a cache into a `mem::Cache`,
other state into a named struct at file scope. Constants (`static const`, `static constexpr`) are fine, and so is a
file-scope `thread_local` (not indented: a worker's state by design, `vr_decals.cpp`'s RNG).

A line that must stay (a worker thread's own buffer) ends with the comment `// statics-ok: <why>`.

Usage: python Misc/quakevr/check_statics.py   (from the repository's root; exit 1 and the sites when any is found)
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2] / "Quake" / "vr"
EXTS = {".cpp", ".hpp", ".h", ".inc", ".c"}

# Indented (inside a function or a class) `static`/`thread_local` of a `std::` type, not const, and a variable (its
# name followed by `;`, `=`, `{` or `[`), not a static member function (`static std::string name(`).
STATIC_STD = re.compile(
    r"^\s+(?:static\s+thread_local|thread_local\s+static|static)\s+(?!const\b|constexpr\b|inline\b)"
    r"(?:std::|::std::).*?\s[\*&]*\w+\s*(?:\[[^\]]*\]\s*)?(?:;|=|\{)"
)
# Indented `thread_local` of any type that is not a constant.
THREAD_LOCAL = re.compile(r"^\s+(?:static\s+)?thread_local\s+(?!const\b|constexpr\b)")


def main():
    found = []
    for path in sorted(ROOT.rglob("*")):
        if path.suffix not in EXTS or "external" in path.relative_to(ROOT).parts:
            continue
        for n, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            code = line.split("//", 1)[0]
            if "statics-ok" in line:
                continue
            if STATIC_STD.match(code) or THREAD_LOCAL.match(code):
                found.append(f"{path.relative_to(ROOT.parents[1]).as_posix()}:{n}: {line.strip()}")
    for f in found:
        print(f)
    if found:
        print(f"{len(found)} function-local static/thread_local state (see docs/vr-port/CODE_STYLE.md, "
              f"'Scratch buffers and caches')")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
