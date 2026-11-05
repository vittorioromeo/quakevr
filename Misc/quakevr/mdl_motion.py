"""Reads an alias .mdl's poses (pure Python): names and every vertex's position per pose, in model units.

    python Misc/quakevr/mdl_motion.py quakevr/progs/v_shot.mdl [--moving 0.3]

lists the vertices that move across the poses (largest first) with how much their neighbours at rest move: a vertex
that moves while its neighbours stay is a stray (shotgun_pump_check.py asserts there is none near the stock).
"""
import math
import struct
import sys


def load(path):
    d = open(path, "rb").read()
    ident, ver = struct.unpack_from("<4si", d, 0)
    assert ident == b"IDPO" and ver == 6, path
    scale = struct.unpack_from("<3f", d, 8)
    origin = struct.unpack_from("<3f", d, 20)
    ns, sw, sh, nv, nt, nf = struct.unpack_from("<6i", d, 48)
    off = 84
    for _ in range(ns):
        g, = struct.unpack_from("<i", d, off)
        off += 4
        if g == 0:
            off += sw * sh
        else:
            n, = struct.unpack_from("<i", d, off)
            off += 4 + 4 * n + n * sw * sh
    off += 12 * nv + 16 * nt
    names, poses = [], []

    def pose(o):
        name = d[o + 8:o + 24].split(b"\0")[0].decode("latin-1")
        pts = []
        for i in range(nv):
            x, y, z, _n = struct.unpack_from("<4B", d, o + 24 + 4 * i)
            pts.append((x * scale[0] + origin[0], y * scale[1] + origin[1], z * scale[2] + origin[2]))
        return name, pts

    for _ in range(nf):
        t, = struct.unpack_from("<i", d, off)
        off += 4
        if t == 0:
            nm, pts = pose(off)
            names.append(nm)
            poses.append(pts)
            off += 24 + 4 * nv
        else:
            n, = struct.unpack_from("<i", d, off)
            off += 12 + 4 * n
            for _ in range(n):
                nm, pts = pose(off)
                names.append(nm)
                poses.append(pts)
                off += 24 + 4 * nv
    assert off == len(d), "trailing bytes"
    return names, poses


def max_motion(poses, ref=0):
    """Per vertex: the largest distance from its position in pose `ref` over all poses."""
    nv = len(poses[0])
    return [max(math.dist(p[v], poses[ref][v]) for p in poses) for v in range(nv)]


def neighbours(pts, v, radius):
    return [u for u in range(len(pts)) if u != v and math.dist(pts[u], pts[v]) < radius]


if __name__ == "__main__":
    names, poses = load(sys.argv[1])
    thr = float(sys.argv[sys.argv.index("--moving") + 1]) if "--moving" in sys.argv else 0.3
    print("verts", len(poses[0]), "poses", len(names))
    print(names)
    mx = max_motion(poses)
    rest = poses[0]
    order = sorted(range(len(mx)), key=lambda v: -mx[v])
    print("moving (> %.2f): %d" % (thr, sum(1 for m in mx if m > thr)))
    for v in order[:70]:
        if mx[v] <= thr:
            break
        nb = neighbours(rest, v, 1.5)
        nbm = sum(mx[u] for u in nb) / len(nb) if nb else -1
        nbmin = min((mx[u] for u in nb), default=-1)
        worst = max(range(len(poses)), key=lambda p: math.dist(poses[p][v], rest[v]))
        print("v%d max %.2f nb %d nbmean %.2f nbmin %.2f at (%.1f %.1f %.1f) pose %d %s" % (
            v, mx[v], len(nb), nbm, nbmin, *rest[v], worst, names[worst]))
