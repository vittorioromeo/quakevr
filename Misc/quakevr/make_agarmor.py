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
#   helmet  (160, 0)-(256, 64): the helmet's walls (vr_agarmor.cpp `helmetOf`: the Ranger's helmet), mirrored left
#            and right: a from the front's middle (0) round the side to the back's (1), b down its rows; the columns'
#            and rows' places are HELM_U and HELM_V (vr_agarmor.cpp's helmU and helmV: keep them alike)
#   cap     (160, 64)-(192, 96): the helmet's flat top seen from above, the front at +a
#   lining  (192, 64)-(224, 96): the helmet's open edges (round the face, under the guards): b 0 outer .. 1 inner
#   bracket (224, 64)-(256, 96): the small block on each side of the helmet: its outer face the whole block
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
    "helmet": (160, 0, 256, 64),
    "cap": (160, 64, 192, 96),
    "lining": (192, 64, 224, 96),
    "bracket": (224, 64, 256, 96),
    "inner": (160, 96, 208, 128),
    "rim": (208, 96, 256, 128),
}
INNER = [16, 17, 18, 19]  # dark olive leather padding (make_vrbody's LEATHER, its darkest)
LINING = [96, 97, 98, 99, 100, 101, 102]  # the helmet's lining: dark orange-brown leather
# The helmet's columns (the front's middle, the front chamfer's ends, the back chamfer's ends, the back's middle) and
# rows (the top's edge, below its chamfer, the bucket's brow, the ledge under it, the cheekbones, the jaw and nape).
HELM_U = [0.0, 0.16, 0.3, 0.7, 0.84, 1.0]
HELM_V = [0.0, 0.08, 0.38, 0.46, 0.7, 1.0]


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


def wear(ramp, shade, d, s, t, seed):
    """Worn paint: scratches and chips (lighter, the metal showing), grime (darker)."""
    if body.vnoise(s * 34, t * 34, seed) > 0.8 and body.h01(int(s * 512), int(t * 512), seed + 1) > 0.8:
        shade += 0.3
    grime = body.vnoise(s * 12, t * 12, seed + 2)
    shade -= 0.18 * max(0.0, grime - 0.55) / 0.45
    shade += 0.06 * (body.vnoise(s * 60, t * 60, seed + 3) - 0.5)
    # long thin scratches: a few lines across
    k = (s * 37.0 + t * 11.0) % 1.0
    if k < 0.012 and body.vnoise(s * 9, t * 9, seed + 4) > 0.62:
        shade += 0.35
    return body.pick(ramp, shade, d)


def helmet_texel(a, b, armor, d, s, t):
    """The walls: a flat-faced bucket over the crown (its top chamfer lit, its lower edge a lit lip over a dark
    ledge), the cheek guards under it (lit edges at the face and the jaw, three slanted vents low on the side)."""
    ramp = body.ARMOR_RAMPS[armor]
    U, V = HELM_U, HELM_V
    du = 1.0 / 96.0
    dv = 1.0 / 64.0
    # the face this texel is on: its light from above and ahead (the front lit most, the back least)
    facet = [0.06, 0.02, -0.02, -0.08, -0.12][min(4, sum(1 for x in U[1:-1] if a >= x))]
    if b < V[1]:
        shade = 0.88 + facet                                  # the top's chamfer: in the light
        if b > V[1] - 1.5 * dv:
            shade = 1.0                                       # its lower edge
    elif b < V[2]:
        shade = 0.62 + facet + 0.1 * (V[2] - b) / (V[2] - V[1])  # the bucket's wall
        if b < V[1] + 1.2 * dv:
            shade = 0.45
        if b > V[2] - 2.2 * dv:
            shade = 0.95 if b > V[2] - 1.2 * dv else 0.8      # the lip
    elif b < V[3]:
        shade = 0.12                                          # under the ledge, in shadow
    else:
        shade = 0.52 + facet - 0.12 * (b - V[3]) / (1.0 - V[3])  # the guards
        if b < V[3] + 1.5 * dv:
            shade = 0.2                                       # the shadow the ledge throws
        if b > 1.0 - 1.6 * dv:
            shade = 0.85                                      # the jaw's edge
        elif b > 1.0 - 2.8 * dv:
            shade = 0.3
        if U[1] <= a < U[1] + 1.3 * du:
            shade = 0.9                                       # the guard's edge round the face
        # three slanted vents low on the side, near the front
        x = (a - 0.33) / 0.17
        y = (b - 0.74) / 0.2
        if 0.0 <= x <= 1.0 and 0.0 <= y <= 1.0:
            k = (x * 3.0 + y * 1.2) % 1.0
            band = int(x * 3.0 + y * 1.2)
            if band < 3 and 0.12 < y < 0.92:
                if k < 0.42:
                    shade = 0.02                              # the slot
                elif k < 0.6:
                    shade = 0.85                              # its lit lower lip
    # the chamfers' edges catch the light (the crown and the guards)
    if b >= V[1] + 1.2 * dv and not (V[2] - 2.2 * dv < b < V[3] + 1.5 * dv) and b < 1.0 - 2.8 * dv:
        for x in U[1:-1]:
            if abs(a - x) < 0.6 * du and not (x == U[1] and b > V[3]):
                shade = max(shade, 0.78 + facet)
    return wear(ramp, shade, d, s, t, 21)


def cap_texel(a, b, armor, d, s, t):
    """The flat top: in the light, darker to the back, a dent and scratches."""
    ramp = body.ARMOR_RAMPS[armor]
    x, y = (a - 0.5) * 2.0, (b - 0.5) * 2.0
    shade = 0.84 + 0.08 * x - 0.06 * math.hypot(x, y)
    if math.hypot(x + 0.3, y - 0.25) < 0.16:
        shade -= 0.2                                          # a dent
    return wear(ramp, shade, d, s, t, 31)


def lining_texel(a, b, armor, d, s, t):
    x = 0.75 if b < 0.22 else 0.3 + 0.15 * body.vnoise(s * 70, t * 70, 41)
    return body.pick(LINING, x, d)


def bracket_texel(a, b, armor, d, s, t):
    """A square frame with a dark slot through it."""
    ramp = body.ARMOR_RAMPS[armor]
    e = min(a, b, 1.0 - a, 1.0 - b)
    if e < 0.08:
        shade = 0.3 if (a > 0.92 or b > 0.92) else 0.95       # its edges: lit above and ahead, dark below
    elif 0.3 < a < 0.7 and 0.28 < b < 0.72:
        shade = 0.02 if b > 0.36 else 0.5                     # the slot
    else:
        shade = 0.7
    return wear(ramp, shade, d, s, t, 51)


def rim_texel(a, b, armor, d, s, t):
    ramp = body.ARMOR_RAMPS[armor]
    return body.pick(ramp, 0.9 if b < 0.35 else 0.55 if b < 0.65 else 0.15, d)


def inner_texel(a, b, armor, d, s, t):
    quilt = (a * 6.0) % 1.0
    x = 0.4 + 0.3 * math.sin(math.pi * quilt) + 0.2 * (body.vnoise(s * 60, t * 60, 24) - 0.5)
    return body.pick(INNER, x, d)


PAINTERS = {"cuirass": cuirass_texel, "helmet": helmet_texel, "cap": cap_texel, "lining": lining_texel,
            "bracket": bracket_texel, "rim": rim_texel, "inner": inner_texel}


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
