#!/usr/bin/env python3
# relight_probe.py -- how bright a relit map's lightmap is round its light fixtures, for tuning relight_textures.cfg.
#
# For each texture named (or every glowing one relight_maps.py --list-glows lists), the mean of the luxels (style 0,
# the light that is on from the start) on the faces within --radius units of each fixture made of it (its faces
# grouped into things as relight_maps.py does; the fixture's own faces left out), then the mean over its fixtures.
# Values are the lightmap's (0-255; 128 is full light with Quake's overbright), the mean of red, green and blue.
#
#   python Misc/quakevr/relight_probe.py quakevr/relit/hipnotic/maps/hip1m1.bsp --textures tlight02 tlight01
#   python Misc/quakevr/relight_probe.py a.bsp b.bsp --textures tlight11       (before and after, side by side)
#
# The .lit next to the .bsp is read when there is one (else the .bsp's own white light).

import argparse
import math
import os
import struct
import sys

import relight_maps as rm


def luxels(data, lit):
    """[(face index, (x, y, z), brightness)] of every luxel of style 0 of the map (BSP29)."""
    vofs, vlen = rm.lump(data, 3)
    tofs, tlen = rm.lump(data, 6)
    fofs, flen = rm.lump(data, 7)
    eofs, elen = rm.lump(data, 12)
    sofs, slen = rm.lump(data, 13)
    pofs, plen = rm.lump(data, 1)
    lofs, llen = rm.lump(data, 8)
    verts = [struct.unpack_from("<3f", data, vofs + i * 12) for i in range(vlen // 12)]
    edges = [struct.unpack_from("<2H", data, eofs + i * 4) for i in range(elen // 4)]
    surfedges = struct.unpack_from("<%di" % (slen // 4), data, sofs)
    planes = [struct.unpack_from("<4f", data, pofs + i * 20) for i in range(plen // 20)]
    texinfo = [struct.unpack_from("<8fii", data, tofs + i * 40) for i in range(tlen // 40)]
    out = []
    for i in range(flen // 20):
        planenum, side, first, count, ti = struct.unpack_from("<hhihh", data, fofs + i * 20)
        styles = struct.unpack_from("<4B", data, fofs + i * 20 + 12)
        (lightofs,) = struct.unpack_from("<i", data, fofs + i * 20 + 16)
        vecs = texinfo[ti]
        if lightofs < 0 or vecs[9] & 1 or styles[0] != 0:
            continue
        pts = []
        for k in range(count):
            e = surfedges[first + k]
            pts.append(verts[edges[e][0]] if e >= 0 else verts[edges[-e][1]])
        lo, size = [], []
        for j in range(2):
            vals = [p[0] * vecs[j * 4] + p[1] * vecs[j * 4 + 1] + p[2] * vecs[j * 4 + 2] + vecs[j * 4 + 3] for p in pts]
            lo.append(math.floor(min(vals) / 16))
            size.append(int(math.ceil(max(vals) / 16) - lo[j]) + 1)
        nx, ny, nz, dist = planes[planenum]
        s_axis, t_axis = vecs[0:3], vecs[4:7]
        # The point on the face's plane with texture coordinates (s, t): solve [S; T; N] p = [s - s0, t - t0, dist].
        m = (s_axis, t_axis, (nx, ny, nz))
        det = (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
               + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]))
        if abs(det) < 1e-9:
            continue
        inv = [[0.0] * 3 for _ in range(3)]  # the inverse of m (its adjugate over det)
        for r in range(3):
            for c in range(3):
                a1, a2 = m[(c + 1) % 3], m[(c + 2) % 3]
                inv[r][c] = (a1[(r + 1) % 3] * a2[(r + 2) % 3] - a1[(r + 2) % 3] * a2[(r + 1) % 3]) / det
        for t in range(size[1]):
            for s in range(size[0]):
                rhs = ((lo[0] + s) * 16 - vecs[3], (lo[1] + t) * 16 - vecs[7], dist)
                p = [inv[r][0] * rhs[0] + inv[r][1] * rhs[1] + inv[r][2] * rhs[2] for r in range(3)]
                k = lightofs + t * size[0] + s
                if lit:
                    v = sum(lit[8 + k * 3 : 8 + k * 3 + 3]) / 3.0
                else:
                    v = data[lofs + k] if k < llen else 0
                out.append((i, tuple(p), v))
    return out


def fixture_things(data, names):
    """{texture: [(face indices, centre)]} of the textures named (lower case), grouped as relight_maps.things."""
    fofs, flen = rm.lump(data, 7)
    tofs, _ = rm.lump(data, 6)
    offset, _ = rm.lump(data, 2)
    (count,) = struct.unpack_from("<i", data, offset)
    mipname = {}
    for i in range(count):
        (mip,) = struct.unpack_from("<i", data, offset + 4 + i * 4)
        if mip >= 0:
            mipname[i] = data[offset + mip : offset + mip + 16].split(b"\0")[0].decode("latin-1").lower()
    faces = rm.texture_faces(data)
    # texture_faces drops nothing but faces without points: number them as the face lump does.
    index = {}
    for i in range(flen // 20):
        (ti,) = struct.unpack_from("<h", data, fofs + i * 20 + 10)
        (m,) = struct.unpack_from("<i", data, tofs + ti * 40 + 32)
        index.setdefault(m, []).append(i)
    out = {}
    for m, name in mipname.items():
        if names and name not in names:
            continue
        own = list(zip(faces.get(m, []), index.get(m, [])))
        groups = []
        for f, fi in own:
            joined = [g for g in groups if any(rm.near(f[0], h[0][0], rm.GLOW_OBJECT) for h in g)]
            merged = [(f, fi)]
            for g in joined:
                groups.remove(g)
                merged += g
            groups.append(merged)
        if groups:
            out[name] = [({fi for _, fi in g}, tuple(sum(f[0][j] for f, _ in g) / len(g) for j in range(3)))
                         for g in groups]
    return out


def probe(path, names, radius):
    with open(path, "rb") as f:
        data = f.read()
    lit_path = os.path.splitext(path)[0] + ".lit"
    lit = open(lit_path, "rb").read() if os.path.isfile(lit_path) else None
    grid = {}
    for fi, p, v in luxels(data, lit):
        grid.setdefault(tuple(int(math.floor(x / radius)) for x in p), []).append((fi, p, v))
    result = {}
    for name, things in sorted(fixture_things(data, names).items()):
        means = []
        for own, centre in things:
            cell = [int(math.floor(x / radius)) for x in centre]
            near = [e for dx in (-1, 0, 1) for dy in (-1, 0, 1) for dz in (-1, 0, 1)
                    for e in grid.get((cell[0] + dx, cell[1] + dy, cell[2] + dz), ())]
            vals = [v for fi, p, v in near if fi not in own and rm.near(p, centre, radius)]
            if vals:
                means.append(sum(vals) / len(vals))
        if means:
            result[name] = (len(things), sum(means) / len(means), min(means), max(means))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("bsp", nargs="+", help="relit maps (a .bsp; its .lit beside it is read)")
    parser.add_argument("--textures", nargs="*", default=[], help="the fixtures' textures (default: all)")
    parser.add_argument("--radius", type=float, default=128.0, help="how far round each fixture (default 128)")
    args = parser.parse_args()
    names = {t.lower() for t in args.textures}
    results = [probe(p, names, args.radius) for p in args.bsp]
    print("%-16s %6s  %s" % ("texture", "things", "  ".join("%-22s" % os.path.basename(p)[:22] for p in args.bsp)))
    for name in sorted(set().union(*results)):
        cells = []
        for r in results:
            n, mean, lo, hi = r.get(name, (0, 0, 0, 0))
            cells.append("%5.1f (%5.1f..%5.1f)" % (mean, lo, hi) if n else "%-22s" % "-")
        print("%-16s %6d  %s" % (name, max(r.get(name, (0,))[0] for r in results), "  ".join(cells)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
