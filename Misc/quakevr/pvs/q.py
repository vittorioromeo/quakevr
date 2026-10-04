"""q.py -- ad-hoc queries over start.bsp, built on bspvis.py (its PVS reader).

    python q.py ents "<x y z>" <radius> [filter]
    python q.py leaf "<x y z>"
    python q.py diff "<x1 y1 z1>" "<x2 y2 z2>"
    python q.py box "<minx miny minz>" "<maxx maxy maxz>"
    python q.py slip                     # every slipgate face: its leaf, box and texture
    python q.py inpvsof "<x y z>" "<x2 y2 z2>"   # is the leaf at the 2nd point in the PVS of the leaf at the 1st
"""
import sys

from pathlib import Path
import bspvis

# --bsp <path> permits comparisons without overwriting the extracted original map.
argv = sys.argv[1:]
path = Path(__file__).with_name("start.bsp")
if argv[:1] == ["--bsp"]:
    path = Path(argv[1])
    argv = argv[2:]
sys.argv = [sys.argv[0], *argv]
B = bspvis.BSP(path)


def pts(a):
    return tuple(float(x) for x in a.split())


def leaf_of(p):
    return B.pointleaf(p)


def inpvs(fromp, top):
    a, c = leaf_of(fromp), leaf_of(top)
    v = B.bits(B.decompress(a))
    return a, c, c in v


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "help"
    if cmd == "slip":
        for i, name in enumerate(B.texnames):
            if "slip" in name.lower():
                print(f"tex {i} {name}")
        for f in range(len(B.faces)):
            ti = B.texinfo[B.faces[f][4]][8]
            name = B.texnames[ti] if 0 <= ti < len(B.texnames) else "?"
            if "slip" not in name.lower():
                continue
            c, tex = B.faceinfo(f)
            print(f"face {f} tex {tex:<12} centre ({c[0]:.0f} {c[1]:.0f} {c[2]:.0f})")
        return
    if cmd == "ents":
        c, r = pts(sys.argv[2]), float(sys.argv[3])
        filt = sys.argv[4].lower() if len(sys.argv) > 4 else ""
        for ent in B.ents:
            if filt not in str(ent).lower():
                continue
            o = B.entity_origin(ent)
            if o is not None and max(abs(o[i] - c[i]) for i in range(3)) <= r:
                print("  ", ent)
        return
    if cmd == "leaf":
        p = pts(sys.argv[2])
        l = leaf_of(p)
        print(f"point {p} leaf {l} {bspvis.CONTENTS.get(B.leafs[l][0])} pvs {len(B.bits(B.decompress(l)))}")
        B.show_leaf(l, 8)
        return
    if cmd == "inpvsof":
        a, c, ok = inpvs(pts(sys.argv[2]), pts(sys.argv[3]))
        print(f"leaf {a} -> leaf {c}: {'IN' if ok else 'not in'} the first's PVS")
        return
    if cmd == "diff":
        a, c = pts(sys.argv[2]), pts(sys.argv[3])
        la, lc = leaf_of(a), leaf_of(c)
        va, vc = B.bits(B.decompress(la)), B.bits(B.decompress(lc))
        sa, sc = set(va), set(vc)
        print(f"leaf {la} pvs {len(sa)}  vs  leaf {lc} pvs {len(sc)}  common {len(sa & sc)}")
        only_a, only_c = sorted(sa - sc), sorted(sc - sa)
        print(f"  only first: {len(only_a)} {only_a[:30]}")
        print(f"  only second: {len(only_c)} {only_c[:30]}")
        for leaf in only_c[:10]:
            B.show_leaf(leaf, 4)
        return
    if cmd == "box":
        lo, hi = pts(sys.argv[2]), pts(sys.argv[3])
        for leaf in range(1, B.numleafs + 1):
            lmin, lmax = B.leafbox(leaf)
            if all(lmin[i] >= lo[i] and lmax[i] <= hi[i] for i in range(3)):
                B.show_leaf(leaf, 4)
        return
    print(__doc__)


main()
