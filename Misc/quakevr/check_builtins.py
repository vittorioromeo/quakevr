#!/usr/bin/env python3
"""check_builtins.py -- the QC's by-name builtins (`= #0`) against the engine's tables.

A QC prototype `... name = #0;` is bound at load by its name: Ironwail's pr_builtindefs (Quake/pr_cmds.c) or Quake VR's
vrBuiltins (Quake/vr/vr_builtins.cpp, bindBuiltins). A misspelt or removed name leaves the function unbound and the
first call errors at run time. This check fails when:
  - a `#0` prototype in the QC (QC/*.qc, QC/ad/*.qc) names no builtin of either table (Arcane Dimensions' optional
    ones, QC/ad/ad_builtins.qc, are allowed: it asks for them "when the engine has them");
  - a vrBuiltins entry has no `#0` prototype in the QC (a dead builtin), unless it is listed in ENGINE_ONLY below;
  - a vrBuiltins name appears twice.
Run from the repository's root (the kit's build.sh does); exit 1 on a problem.
"""
import glob
import re
import sys

# vrBuiltins entries no QC prototype names (none today: add one here with the reason when it is meant).
ENGINE_ONLY = {}

OPTIONAL_FILES = {"QC/ad/ad_builtins.qc"}


def read(path):
    with open(path, encoding="latin-1") as f:
        return f.read()


def strip_comments(s):
    s = re.sub(r"/\*.*?\*/", " ", s, flags=re.S)
    return re.sub(r"//[^\n]*", "", s)


def main():
    vr_src = read("Quake/vr/vr_builtins.cpp")
    m = re.search(r"VrBuiltin vrBuiltins\[\]\s*=\s*\{(.*?)\n\};", vr_src, re.S)
    if not m:
        print("check_builtins: vrBuiltins[] not found in Quake/vr/vr_builtins.cpp")
        return 1
    vr_names = re.findall(r'\{\s*"(\w+)"\s*,', strip_comments(m.group(1)))
    ironwail = set(re.findall(r'^\s*\{\s*"(\w+)"\s*,', read("Quake/pr_cmds.c"), re.M))

    problems = []
    seen = set()
    for n in vr_names:
        if n in seen:
            problems.append("vrBuiltins names \"%s\" twice" % n)
        seen.add(n)

    protos = {}
    for path in sorted(glob.glob("QC/*.qc") + glob.glob("QC/ad/*.qc")):
        path = path.replace("\\", "/")
        for pm in re.finditer(r"(\w+)\s*=\s*#0\s*;", strip_comments(read(path))):
            protos.setdefault(pm.group(1), []).append(path)

    for name, paths in sorted(protos.items()):
        if name in seen or name in ironwail:
            continue
        if all(p in OPTIONAL_FILES for p in paths):
            continue
        problems.append("%s: `%s = #0` names no engine builtin (vrBuiltins, pr_builtindefs)" % (paths[0], name))

    for n in vr_names:
        if n not in protos and n not in ENGINE_ONLY:
            problems.append("vrBuiltins \"%s\" has no `#0` prototype in the QC (dead, or add it to ENGINE_ONLY)" % n)

    if problems:
        for p in problems:
            print("check_builtins: " + p)
        print("check_builtins: %d problem(s)" % len(problems))
        return 1
    print("check_builtins: %d QC #0 prototypes, %d VR builtins, all bound" % (len(protos), len(vr_names)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
