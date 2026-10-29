#!/usr/bin/env python3
# relightamb_stats.py <map.bsp> [<map.bsp> ...]: style-0 lightmap brightness of a relit BSP29/BSP2 map (its .lit beside
# it, else the .bsp's own light). Prints the darkest percentiles of the luxels (mean of r, g, b; 128 is full light).
# Last two maps given: "relightamb: PASS/FAIL" when the second's darkest luxels are brighter than the first's.
import math
import os
import struct
import sys


def lump(d, i):
    return struct.unpack_from("<ii", d, 4 + 8 * i)


def luxels(path):
    d = open(path, "rb").read()
    ver = struct.unpack_from("<i", d, 0)[0]
    bsp2 = d[:4] == b"BSP2"
    vo, vl = lump(d, 3)
    verts = [struct.unpack_from("<fff", d, vo + 12 * i) for i in range(vl // 12)]
    to, tl = lump(d, 6)
    texinfo = [struct.unpack_from("<8fii", d, to + 40 * i) for i in range(tl // 40)]
    eo, el = lump(d, 12)
    esz = 8 if bsp2 else 4
    efmt = "<II" if bsp2 else "<HH"
    edges = [struct.unpack_from(efmt, d, eo + esz * i) for i in range(el // esz)]
    so, sl = lump(d, 13)
    surfedges = struct.unpack_from("<%di" % (sl // 4), d, so)
    fo, fl = lump(d, 7)
    ffmt, fsz = ("<iiiii4Bi", 28) if bsp2 else ("<hhihh4Bi", 20)
    lo, ll = lump(d, 8)
    lit = os.path.splitext(path)[0] + ".lit"
    rgb = open(lit, "rb").read()[8:] if os.path.exists(lit) else None
    out = []
    for i in range(fl // fsz):
        f = struct.unpack_from(ffmt, d, fo + fsz * i)
        _, _, first, num, ti, s0, _, _, _, ofs = f
        if ofs < 0 or s0 != 0:  # (style 0 first: the light that is on from the start)
            continue
        t = texinfo[ti]
        if t[9] & 1:  # TEX_SPECIAL (sky, liquids)
            continue
        st = [[], []]
        for k in range(num):
            e = surfedges[first + k]
            v = verts[edges[e][0]] if e >= 0 else verts[edges[-e][1]]
            for a in range(2):
                st[a].append(v[0] * t[4 * a] + v[1] * t[4 * a + 1] + v[2] * t[4 * a + 2] + t[4 * a + 3])
        w, h = [int(math.ceil(max(x) / 16.0)) - int(math.floor(min(x) / 16.0)) + 1 for x in st]
        n = w * h
        if rgb is not None:
            for j in range(n):
                p = 3 * (ofs + j)
                out.append((rgb[p] + rgb[p + 1] + rgb[p + 2]) / 3.0)
        else:
            out.extend(d[lo + ofs: lo + ofs + n])
    return sorted(out)


def pct(v, q):
    return v[min(len(v) - 1, int(q * len(v)))]


results = []
for path in sys.argv[1:]:
    v = luxels(path)
    r = {"min": v[0], "p1": pct(v, 0.01), "p5": pct(v, 0.05), "p50": pct(v, 0.5),
         "dark": 100.0 * sum(1 for x in v if x < 16) / len(v)}
    results.append(r)
    print("%s: %d luxels, min %.1f p1 %.1f p5 %.1f median %.1f, under 16: %.2f%%" % (
        path, len(v), r["min"], r["p1"], r["p5"], r["p50"], r["dark"]))
if len(results) >= 2:
    a, b = results[-2], results[-1]
    # (the darkest percentiles and the share under 16, not the very darkest luxel: a lightmap's corner luxel whose sample
    # falls inside a brush it touches -- vrarena's crates and trim against a wall -- stays 0 at any level)
    ok = b["p1"] > a["p1"] and b["p5"] >= a["p5"] and b["dark"] < a["dark"] and b["min"] >= a["min"]
    print("relightamb: %s (darkest: min %.1f -> %.1f, p1 %.1f -> %.1f, p5 %.1f -> %.1f)" % (
        "PASS" if ok else "FAIL", a["min"], b["min"], a["p1"], b["p1"], a["p5"], b["p5"]))
