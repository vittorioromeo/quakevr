"""Makes the shotgun's three stray stock-corner vertices follow the gun's rigid kick.

    python Misc/quakevr/fix_shot_stock_corner.py [quakevr/progs]    (in place; idempotent)

In v_shot.mdl (and vr_pumpbody_on_v_shot.mdl) three coincident vertices at 4.0 -1.8 5.7, the stock's top corner,
travel 0.4 units further back than the gun's rigid kick in shot3: vr_kickcancel.cpp left them out of the body (a moving
part), so with the kick cancelled they wobbled during firing. Each pose's kick is fitted here (a 2D rigid motion in x, z
over the gun's body, as the cancel's), and the three vertices put where it carries them (their y kept).
"""
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mdl_motion as M

CORNER = (4.04, -1.8, 5.68)


def pose_offsets(d):
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
    out = []
    for _ in range(nf):
        t, = struct.unpack_from("<i", d, off)
        off += 4
        if t == 0:
            out.append(off)
            off += 24 + 4 * nv
        else:
            n, = struct.unpack_from("<i", d, off)
            off += 12 + 4 * n
            for _ in range(n):
                out.append(off)
                off += 24 + 4 * nv
    return out


def fit2d(rest, now, ids):
    cx = sum(rest[i][0] for i in ids) / len(ids)
    cz = sum(rest[i][2] for i in ids) / len(ids)
    nx = sum(now[i][0] for i in ids) / len(ids)
    nz = sum(now[i][2] for i in ids) / len(ids)
    s = c = 0.0
    for i in ids:
        ax, az = rest[i][0] - cx, rest[i][2] - cz
        bx, bz = now[i][0] - nx, now[i][2] - nz
        c += ax * bx + az * bz
        s += ax * bz - az * bx
    th = math.atan2(s, c)
    return th, (cx, cz), (nx, nz)


def apply(th, c, n, p):
    ax, az = p[0] - c[0], p[2] - c[1]
    return (n[0] + ax * math.cos(th) - az * math.sin(th), n[1] + ax * math.sin(th) + az * math.cos(th))


def fix(path):
    names, poses = M.load(path)
    d = bytearray(open(path, "rb").read())
    scale = struct.unpack_from("<3f", d, 8)
    origin = struct.unpack_from("<3f", d, 20)
    rest = poses[0]
    nv = len(rest)
    corner = [v for v in range(nv) if math.dist(rest[v], CORNER) < 0.1]
    # the body: the gun's rear half, not the flash, not the corner itself
    body = [v for v in range(nv) if rest[v][0] < 30 and v not in corner]
    offs = pose_offsets(d)
    moved = 0.0
    for p in range(1, len(poses)):
        th, c, n = fit2d(rest, poses[p], body)
        for v in corner:
            x, z = apply(th, c, n, rest[v])
            tx = (x - origin[0]) / scale[0]
            tz = (z - origin[2]) / scale[2]
            bx, bz = round(tx), round(tz)
            assert 0 < bx < 255 and 0 < bz < 255
            o = offs[p] + 24 + 4 * v
            moved = max(moved, math.dist((poses[p][v][0], poses[p][v][2]), (bx * scale[0] + origin[0], bz * scale[2] + origin[2])))
            d[o], d[o + 2] = bx, bz
        # the pose's box
        vals = [[d[offs[p] + 24 + 4 * v + k] for v in range(nv)] for k in range(3)]
        for k in range(3):
            d[offs[p] + k], d[offs[p] + 4 + k] = min(vals[k]), max(vals[k])
    open(path, "wb").write(bytes(d))
    print("%s: %d corner vertices %s moved up to %.2f units" % (os.path.basename(path), len(corner), corner, moved))


if __name__ == "__main__":
    base = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "quakevr", "progs")
    for f in ("v_shot.mdl", "vr_pumpbody_on_v_shot.mdl"):
        fix(os.path.join(base, f))
