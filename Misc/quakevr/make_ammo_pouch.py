#!/usr/bin/env python3
# make_ammo_pouch.py -- generates quakevr/progs/vrpouch_ammo.mdl, the ammo pouch on the front of the belt (immersive
# reloading, vr_reload_mode 3: vr_view.cpp setupAmmoPouch, QC vr_reload.qc; docs/vr-port/RELOAD_PLAN.md).
#
# make_pouch.py's leather pouch (the grenade pouch at the small of the back), made wider and shallower for the front of
# the belt, with no strap over its open top (a hand dips in and out of it all through a fight), and shotgun shells
# standing in it brass up in a loose row, their heads and a hand's width of red hull out of the rim, so that it reads as
# the shotgun's ammo when you look down. Two frames: 0 full (you have ammo for the gun in your other hand), 1 empty (the
# shells gone down out of sight, the front fallen in flat); the engine picks the frame.
#
# Usage: python Misc/quakevr/make_ammo_pouch.py [output progs folder]
#
# Model space as make_pouch.py's: +x out of the body (the back against it at x 0), +y to its left, +z up, world units at
# scale 1 (vr_ammo_pouch_scale scales it as drawn). The skin is make_pouch.py's (its leather, iron and stitching), its
# grenade regions repainted as the shells' hull and brass.

import math
import os
import sys

import genguard
import make_pouch as pouch
import mdlgen

# The front pouch's shape (make_pouch.py's globals, set before building: its functions read them).
pouch.HALF_W = 4.4
pouch.DEPTH = {0: 2.5, 1: 1.3}
pouch.Z_BOTTOM, pouch.Z_TOP = -2.3, 1.5
pouch.Z_FLOOR = 0.6
pouch.BULGE_P = 3.0

# The shells standing in it: their middles (y), their lean (degrees, about x), how far their tops stand out.
SHELLS = [(-2.55, -9.0, 1.05), (-1.3, 4.0, 1.25), (0.0, -3.0, 1.15), (1.3, 7.0, 1.3), (2.55, -6.0, 1.0)]
SHELL_R = 0.52   # the hull's radius, world units (a 12-gauge shell at the shells' drawn size, a little larger to read)
RIM_R = 0.58
BRASS = 0.55     # how much of the top is brass


def shell_upright(m, x, y, lean, top, bottom, full):
    """A shell standing brass up from `bottom` to `top`, leaning `lean` degrees sideways: an octagonal hull (the
    "gren" region: red below, brass at the top) and its head (the "grentop" region: the brass base, the primer)."""
    a = math.radians(lean)
    r = SHELL_R if full else 0.12

    def at(z, rr, k):
        c = (math.cos((k + 0.5) * math.pi / 4), math.sin((k + 0.5) * math.pi / 4))
        dz = z - bottom
        return (x + rr * c[0], y + rr * c[1] * math.cos(a) - dz * math.sin(a), bottom + dz * math.cos(a) + rr * c[1] * math.sin(a))

    rings = [[at(bottom, r, k) for k in range(8)], [at(top - BRASS * 0.15, r, k) for k in range(8)],
             [at(top - BRASS * 0.15, r * RIM_R / SHELL_R, k) for k in range(8)], [at(top, r * RIM_R / SHELL_R, k) for k in range(8)]]
    vs = (1.0, 0.08, 0.04, 0.0)
    axis_top = at(top, 0.0, 0)
    for i in range(3):
        for k in range(8):
            j = (k + 1) % 8
            p = [rings[i][k], rings[i][j], rings[i + 1][j], rings[i + 1][k]]
            c = mdlgen.mul(mdlgen.add(p[0], p[2]), 0.5)
            ax = at((c[2] - bottom) / max(1e-6, math.cos(a)) + bottom, 0.0, 0)
            outward = mdlgen.sub(c, ax) if i != 1 else mdlgen.sub(axis_top, at(bottom, 0.0, 0))
            uv = [pouch.region_uv("gren", u, v) for u, v in ((k / 8, vs[i]), ((k + 1) / 8, vs[i]),
                                                               ((k + 1) / 8, vs[i + 1]), (k / 8, vs[i + 1]))]
            m.face(p, uv, outward)
    m.face(rings[3], [pouch.region_uv("grentop", 0.5 + 0.45 * math.cos((k + 0.5) * math.pi / 4),
                                      0.5 + 0.45 * math.sin((k + 0.5) * math.pi / 4)) for k in range(8)],
           mdlgen.sub(axis_top, at(bottom, 0.0, 0)))


def build(frame):
    full = frame == 0
    depth = pouch.DEPTH[frame]
    m = pouch.Mesh(pouch.SKIN_W, pouch.SKIN_H, pouch.REGIONS)
    mid = pouch.body(m, depth, full)
    sx = pouch.BACK_X + depth * 0.5
    for y, lean, out in SHELLS:
        if full:
            shell_upright(m, sx, y, lean, pouch.Z_TOP + out, pouch.Z_FLOOR - 0.2, True)
        else:
            shell_upright(m, sx, y * 0.7, 0.0, pouch.Z_FLOOR - 0.6, pouch.Z_BOTTOM + 0.6, False)
    # Rivets at the front's top corners and a pair on its middle, where a belt loop is sewn on behind.
    for y in (-pouch.HALF_W * 0.82, pouch.HALF_W * 0.82):
        px = pouch.front_x(y, depth)
        pouch.stud(m, (px, y, pouch.Z_TOP - 0.4), (px - mid[0], y, 0.0), 0.2, 0.12)
    fx = pouch.front_x(0.0, depth) * (1.03 if full else 1.0)
    for z in (-0.2, -1.4):
        pouch.stud(m, (fx, 0.0, z), (1.0, 0.0, 0.0), 0.17, 0.1)
    return m


def paint_skin():
    """make_pouch.py's skin, its grenade regions repainted: the hull's red (dark at the bottom, ribbed), the brass head
    (dull, a darker rim line) above it; the head's top brass with a grey primer in a dark ring."""
    px = bytearray(pouch.paint_skin())
    w = pouch.SKIN_W
    s0, t0, s1, t1 = pouch.REGIONS["gren"]
    for t in range(t0, t1):
        for s in range(s0, s1):
            v = (t - t0 + 0.5) / (t1 - t0)  # 0 at the top
            h = ((s * 7919 + t * 104729) % 97) / 97.0
            if v < 0.14:
                c = 30 if h < 0.5 else 31 if h < 0.8 else 199
                if 0.1 <= v:
                    c = 28
            else:
                c = 77 if h < 0.55 else 78 if h < 0.8 else 76
                if (s - s0) % 6 == 0:
                    c -= 2  # the hull's ribs
                if v > 0.8:
                    c -= 3  # down in the pouch's shadow
            px[t * w + s] = c
    s0, t0, s1, t1 = pouch.REGIONS["grentop"]
    for t in range(t0, t1):
        for s in range(s0, s1):
            u, v = (s - s0 + 0.5) / (s1 - s0) - 0.5, (t - t0 + 0.5) / (t1 - t0) - 0.5
            d = math.hypot(u, v) * 2
            h = ((s * 7919 + t * 104729) % 97) / 97.0
            px[t * w + s] = (10 if h < 0.6 else 9) if d < 0.26 else 4 if d < 0.36 else (30 if h < 0.5 else 31 if h < 0.88 else 28)
    for c in px:
        assert c < 224, "no fullbright texels"
    return bytes(px)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "progs")
    full, empty = build(0), build(1)
    assert len(full.verts) == len(empty.verts) and full.tris == empty.tris, "frames of the same mesh"
    path = os.path.join(out, "vrpouch_ammo.mdl")
    guard = genguard.Guard("make_ammo_pouch.py", [path])
    mdlgen.write_mdl(path, full, [paint_skin()], "full", frames=[empty])
    print("vrpouch_ammo.mdl: %d vertices, %d triangles, 2 frames -> %s" % (len(full.verts), len(full.tris),
                                                                          os.path.normpath(path)))
    guard.finish()


if __name__ == "__main__":
    main()
