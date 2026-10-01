#!/usr/bin/env python3
"""Vendors Zancle into Quake/vr/external/zancle: the headers the VR code includes, every header they reach and each
reached header's source file (Quake/vr/external/zancle/README.md).

usage:
  zancle_vendor.py <upstream checkout> [Zancle/X/Y.hpp ...]   copies what is missing (the VR code's includes when no
                                                               header is named); files already there are kept (their
                                                               local changes too)
  zancle_vendor.py --list-unused                               the vendored files nothing reaches (to delete)
"""
import os
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
VR = ROOT / "Quake" / "vr"
DST = VR / "external" / "zancle"
INC = re.compile(r'^\s*#\s*include\s*[<"]((?:Zancle|moodycamel)/[^">]+|[a-z]*concurrentqueue\.h|lightweightsemaphore\.h)[">]', re.M)


def locate(root, name):
    for sub in ("include", "src"):
        if (root / sub / name).exists():
            return os.path.normpath(os.path.join(sub, name))
    if name.endswith(".h"):
        for sub in ("extlibs/moodycamel", "extlibs/headers/moodycamel"):
            if (root / sub / os.path.basename(name)).exists():
                return os.path.normpath(os.path.join(sub, os.path.basename(name)))
    return None


def closure(root, seeds):
    todo, seen = list(seeds), set()
    while todo:
        rel = os.path.normpath(todo.pop())
        if rel in seen:
            continue
        seen.add(rel)
        for m in INC.finditer((root / rel).read_text(encoding="utf-8", errors="replace")):
            r = locate(root, m.group(1))
            if r:
                todo.append(r)
        if rel.startswith("include") and rel.endswith(".hpp"):
            cpp = os.path.join("src", rel[len("include") + 1:-4] + ".cpp")
            if (root / cpp).exists():
                todo.append(cpp)
    return seen


def vr_includes(root):
    out = set()
    for f in VR.iterdir():
        if f.suffix in (".cpp", ".hpp", ".h", ".inc"):
            for m in INC.finditer(f.read_text(encoding="utf-8", errors="replace")):
                r = locate(root, m.group(1))
                if r:
                    out.add(r)
    return out


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 2
    if args[0] == "--list-unused":
        seeds = vr_includes(DST) | {os.path.relpath(p, DST) for p in (DST / "src").rglob("*") if p.is_file()}
        used = closure(DST, seeds)
        for p in sorted(DST.rglob("*")):
            rel = os.path.normpath(os.path.relpath(p, DST))
            if p.is_file() and rel.startswith(("include", "src", "extlibs")) and rel not in used:
                print("unused:", rel)
        return 0
    up = pathlib.Path(args[0])
    seeds = [locate(up, a) for a in args[1:]] if len(args) > 1 else sorted(vr_includes(up))
    need = closure(up, [s for s in seeds if s])
    added = 0
    for rel in sorted(need):
        d = DST / rel.replace("headers" + os.sep, "") if rel.startswith("extlibs") else DST / rel
        if d.exists():
            continue
        added += 1
        print("add", rel)
        d.parent.mkdir(parents=True, exist_ok=True)
        d.write_bytes((up / rel).read_bytes())
    print(f"{len(need)} files reached, {added} added")
    return 0


if __name__ == "__main__":
    sys.exit(main())
