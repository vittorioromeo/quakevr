#!/usr/bin/env python3
# make_agarmor.py -- the skins of the armed humanoids' armour shells (Quake/vr/vr_agarmor.cpp; ROUND22.md, "Armour for
# armed humanoids, P0"): quakevr/progs/vragarmor.mdl, a placeholder box carrying three skins, 0 green, 1 yellow, 2 red,
# in the player's armour colours (make_vrbody.py's ARMOR_RAMPS) and look (its armor_texel: five lames with rolled edges,
# rivets and side straps on the cuirass).
#
# The shells themselves are not made here: the engine makes them from the monster's own mesh when it loads
# "<model>#armor" (vr_agarmor.cpp: the rest pose's triangles of each piece's bones, clipped at their weights' 0.5 line,
# smoothed, pushed out along their normals, two-sided with a rim) and maps their texture coordinates into this atlas.
# The atlas's layout is shared with vr_agarmor.cpp (`atlas`): keep both alike.
#
#   cuirass (0, 0)-(160, 128): a = round the torso (0.5 the front, 0 and 1 the middle of the back), b = top (0) to
#            bottom (1): armor_texel's bu = a - 0.5, bv = 0.2 + 0.7 (1 - b)
#   helmet  (160, 0)-(256, 96): a disc seen from above, the crown in its middle, the rim at its edge, the front at
#            +a (a = 0.5 + 0.5 r cos az, b = 0.5 - 0.5 r sin az; az the angle round from the front toward the left)
#   inner   (160, 96)-(208, 128): the padding inside (dark leather)
#   rim     (208, 96)-(256, 128): the plates' edges: b 0 the outer edge (lit) .. 1 the inner (dark)
#
# Usage: python Misc/quakevr/make_agarmor.py [output progs folder]   (deterministic: the same bytes every run; needs
# numpy, which make_vrbody's mdlpolish imports: on the author's machine `py -3.13`)

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import genguard  # noqa: E402
import mdlgen  # noqa: E402
import make_vrbody as body  # noqa: E402

SKIN_W, SKIN_H = 256, 128
REGIONS = {
    "cuirass": (0, 0, 160, 128),
    "helmet": (160, 0, 256, 96),
    "inner": (160, 96, 208, 128),
    "rim": (208, 96, 256, 128),
}
INNER = [16, 17, 18, 19]  # dark olive leather padding (make_vrbody's LEATHER, its darkest)


def region_of(s, t):
    for name, (s0, t0, s1, t1) in REGIONS.items():
        if s0 <= s < s1 and t0 <= t < t1:
            return name, (s + 0.5 - s0) / (s1 - s0), (t + 0.5 - t0) / (t1 - t0)
    return "inner", 0.5, 0.5


def cuirass_texel(a, b, armor, d, s, t):
    bu = (a - 0.5) % 1.0
    bv = 0.2 + 0.7 * (1.0 - b)
    bv = min(0.899, max(0.2, bv))
    i = body.armor_texel(bu, bv, armor, d, s, t)
    return i if i is not None else body.pick(body.ARMOR_RAMPS[armor], 0.3, d)


def helmet_texel(a, b, armor, d, s, t):
    """A dome: lit at its crown, a raised crest from front to back, a ring of rivets above a rolled rim."""
    ramp = body.ARMOR_RAMPS[armor]
    x, y = (a - 0.5) * 2.0, -(b - 0.5) * 2.0
    r = math.hypot(x, y)
    az = math.atan2(y, x)  # 0 the front, +pi/2 the left
    if r > 1.0:
        return body.pick(ramp, 0.1, d)
    if r > 0.93:
        return body.pick(ramp, 0.05, d)                 # under the rolled rim
    if r > 0.86:
        return body.pick(ramp, 0.95 if r < 0.9 else 0.75, d)  # the rim, catching the light
    if r > 0.83:
        return body.pick(ramp, 0.2, d)                  # its shadow above
    # Rivets round the rim: twelve, a highlight on their upper side.
    k = az / (2.0 * math.pi) * 12.0
    du = (k - round(k)) * 2.0 * math.pi * 0.77 / 12.0 * 40.0
    dv = (r - 0.77) * 40.0
    if du * du + dv * dv < 1.0:
        return body.pick(ramp, 1.0 if dv < -0.2 else 0.05 if dv > 0.4 else 0.7, d)
    # The crest: a lit ridge with dark sides along the front-back line.
    across = abs(y)
    if across < 0.035 and r < 0.8:
        return body.pick(ramp, 0.9 if across < 0.018 else 0.5, d)
    if across < 0.06 and r < 0.8:
        return body.pick(ramp, 0.15, d)
    shade = 0.68 - 0.32 * r + 0.06 * math.cos(az)        # the dome's light from above and ahead
    if body.vnoise(s * 30, t * 30, 21) > 0.78 and body.h01(int(s * 512), int(t * 512), 22) > 0.6:
        shade += 0.3                                     # scratches and chipped paint
    shade += 0.12 * (body.vnoise(s * 50, t * 50, 23) - 0.5)
    return body.pick(ramp, shade, d)


def rim_texel(a, b, armor, d, s, t):
    ramp = body.ARMOR_RAMPS[armor]
    return body.pick(ramp, 0.9 if b < 0.35 else 0.55 if b < 0.65 else 0.15, d)


def inner_texel(a, b, armor, d, s, t):
    quilt = (a * 6.0) % 1.0
    x = 0.4 + 0.3 * math.sin(math.pi * quilt) + 0.2 * (body.vnoise(s * 60, t * 60, 24) - 0.5)
    return body.pick(INNER, x, d)


PAINTERS = {"cuirass": cuirass_texel, "helmet": helmet_texel, "rim": rim_texel, "inner": inner_texel}


def paint(armor):
    px = bytearray(SKIN_W * SKIN_H)
    for row in range(SKIN_H):
        for col in range(SKIN_W):
            name, a, b = region_of(col, row)
            d = body.h01(col, row, 1)
            px[row * SKIN_W + col] = PAINTERS[name](a, b, armor, d, col / SKIN_W, row / SKIN_H)
    return bytes(px)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "quakevr", "progs")
    skins = [paint(armor) for armor in (1, 2, 3)]
    mesh = mdlgen.Mesh(SKIN_W, SKIN_H, REGIONS)
    mesh.box((0.0, 0.0, 0.0), (1.0, 1.0, 1.0), "inner")  # (a placeholder: only the skins are used)
    path = os.path.join(out, "vragarmor.mdl")
    guard = genguard.Guard("make_agarmor.py", [path])
    mdlgen.write_mdl(path, mesh, skins, "vragarmor")
    guard.finish()
    print("vragarmor.mdl: %dx%d, %d skins -> %s" % (SKIN_W, SKIN_H, len(skins), os.path.normpath(path)))


if __name__ == "__main__":
    main()
