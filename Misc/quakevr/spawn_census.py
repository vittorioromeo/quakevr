#!/usr/bin/env python3
"""The A/B spawn census (TESTING.md "The spawn census"): proof that a change spawns every map of every campaign the same.

    python Misc/quakevr/spawn_census.py maps OUTDIR     each campaign's map list (maps/*.bsp in its paks, AD's loose maps
                                                         too) into OUTDIR/maps_<campaign>.txt; spawn_census.sh calls it
    python Misc/quakevr/spawn_census.py compare A B [ignored_global ...]
                                                         every save in census folder A against B's of the same name:
                                                         the globals (minus cvarh_* and the names given) and each edict's
                                                         fields, as the save writes them (function names, string text)

Prints one line a differing save and the verdict line `census: N identical, M different, K only in one`; exit 1 when
anything differs. Reads paks and saves only (no game files are written or copied).
"""
import os
import re
import struct
import sys

STEAM = "C:/Program Files (x86)/Steam/steamapps/common/Quake"
KIT = "C:/OHWorkspace/qvr-kit"
# The campaigns in spawn_census.sh's order (its run groups), each with the paks that hold its maps.
PAKS = {
    "id1": [KIT + "/qbase/id1/pak0.pak", KIT + "/qbase/id1/pak1.pak"],
    "hipnotic": [KIT + "/qbase/hipnotic/pak0.pak"],
    "rogue": [KIT + "/qbase/rogue/pak0.pak"],
    "dopa": [STEAM + "/rerelease/dopa/pak0.pak"],
    "mg1": [STEAM + "/rerelease/mg1/pak0.pak"],
    "mg3": [STEAM + "/rerelease/mg3/pak0.pak"],
    "ad": [KIT + "/qbase/ad/pak0.pak", KIT + "/qbase/ad/pak1.pak", KIT + "/qbase/ad/pak2.pak"],
}
LOOSE = {"ad": KIT + "/qbase/ad/maps"}


def pak_maps(path):
    with open(path, "rb") as f:
        _, off, ln = struct.unpack("<4sii", f.read(12))
        f.seek(off)
        for _ in range(ln // 64):
            name = f.read(64)[:56].split(b"\0")[0].decode("latin-1").lower()
            if name.startswith("maps/") and name.endswith(".bsp") and "/" not in name[5:]:
                yield name[5:-4]


def maps(out):
    os.makedirs(out, exist_ok=True)
    total = 0
    for camp, files in PAKS.items():
        found = set()
        for p in files:
            if os.path.exists(p):
                found.update(pak_maps(p))
            else:
                print("census: missing", p)
        d = LOOSE.get(camp)
        if d and os.path.isdir(d):
            found |= {n[:-4].lower() for n in os.listdir(d) if n.lower().endswith(".bsp")}
        found = sorted(m for m in found if not m.startswith("b_"))  # (brush models, not levels)
        total += len(found)
        with open(os.path.join(out, "maps_%s.txt" % camp), "w", newline="\n") as f:
            f.write(" ".join(found) + "\n")
    print("census: %d maps listed" % total)


def parse(path):
    txt = open(path, encoding="latin-1").read()
    blocks = re.findall(r"\{([^{}]*)\}", txt)
    pairs = [re.findall(r'"([^"]*)"\s+"([^"]*)"', b) for b in blocks]
    # the first block is the globals, the rest the edicts in order
    return (dict(pairs[0]) if pairs else {}), [dict(p) for p in pairs[1:]]


def compare(a, b, ignore):
    fa = {f for f in os.listdir(a) if f.endswith(".sav")}
    fb = {f for f in os.listdir(b) if f.endswith(".sav")}
    same = diff = 0
    for f in sorted(fa ^ fb):
        print("only in", "A" if f in fa else "B", f)
    for f in sorted(fa & fb):
        ga, ea = parse(os.path.join(a, f))
        gb, eb = parse(os.path.join(b, f))
        msgs = []
        for k in sorted(set(ga) | set(gb)):
            if k not in ignore and not k.startswith("cvarh_") and ga.get(k) != gb.get(k):
                msgs.append("global %s: %s -> %s" % (k, ga.get(k), gb.get(k)))
        if len(ea) != len(eb):
            msgs.append("edicts %d -> %d" % (len(ea), len(eb)))
        for i, (x, y) in enumerate(zip(ea, eb)):
            if x != y:
                ks = sorted(k for k in set(x) | set(y) if x.get(k) != y.get(k))
                msgs.append("edict %d %s: %s" % (i, x.get("classname", y.get("classname")),
                            ", ".join("%s %s->%s" % (k, x.get(k), y.get(k)) for k in ks[:4])))
        if msgs:
            diff += 1
            print(f, len(msgs), "differences;", "; ".join(msgs[:3]))
        else:
            same += 1
    print("census: %d identical, %d different, %d only in one" % (same, diff, len(fa ^ fb)))
    return 1 if diff or fa ^ fb or not fa else 0


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "maps":
        maps(sys.argv[2])
    elif len(sys.argv) >= 4 and sys.argv[1] == "compare":
        sys.exit(compare(sys.argv[2], sys.argv[3], set(sys.argv[4:])))
    else:
        sys.exit(__doc__)
