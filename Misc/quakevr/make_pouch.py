#!/usr/bin/env python3
# make_pouch.py -- generates quakevr/progs/vrpouch.mdl, the grenade pouch on the belt at the small of the back (hand
# grenades, vr_handgrenade: vr_view.cpp setupPouch; docs/vr-port/ROUND21.md, "Hand grenades from the back pouch").
#
# A deep leather pouch, open at the top, its front rounded out by what is in it, with a strap over the top down to an
# iron buckle on the front. Two grenades' iron heads (id's grenade: dark iron with a red band) stand out of it between
# the strap's sides, so that the pouch reads as grenades from across a room and, from the eyes, in a mirror.
# Two frames: 0 full (you have rockets: the grenade launcher's ammo, which hand grenades use), 1 empty (the heads gone
# down into it, the front fallen in flat); the engine picks the frame from your rockets.
#
# Usage: python Misc/quakevr/make_pouch.py [output progs folder]
#
# Model space: +x out of the body (the pouch's back lies against the body at x 0), +y to its left, +z up; the origin,
# the middle of its back, is where the engine puts the body's surface under the pouch (the hand reaches a little out
# of it: vr_grenade_pouch_*). World units (the model is drawn at scale 1). The skin uses Quake palette indices (chosen
# from gfx/palette.lmp), so no palette is needed to run this. Triangles are clockwise seen from outside, as Quake's.

import math
import os
import sys

import genguard
import mdlgen
from mdlgen import add, cross, dot, mul, norm, sub

TEXEL = 2          # skin texels to a layout unit below: the skin is 256 x 128 (a texel 0.1 units on the walls)
SKIN_W, SKIN_H = 128 * TEXEL, 64 * TEXEL
REGIONS = {name: tuple(c * TEXEL for c in r) for name, r in {
    "side": (0, 0, 96, 40),        # the pouch's walls, wrapped round it (the back too)
    "bottom": (96, 0, 128, 16),    # its bottom
    "inside": (96, 16, 128, 32),   # its inside, in shadow
    "rim": (96, 32, 128, 40),      # the top's rolled edge
    "strap": (0, 40, 64, 48),      # the strap over the top
    "strapend": (64, 40, 80, 48),  # its cut end and edges
    "iron": (80, 40, 96, 48),      # the buckle, the rivets
    "gren": (0, 48, 64, 64),       # the grenades' iron heads, round them
    "grentop": (64, 48, 96, 64),   # their tops
    "spare": (96, 40, 128, 64),
}.items()}

# Leather, darkest to lightest (make_holster.py's): the reddish-black browns (16..20), the greyish browns (174..171).
LEATHER = [16, 175, 174, 17, 173, 18, 19, 172, 20, 171]
STRAP = [16, 17, 96, 18, 97, 19, 98]           # darker, redder, as oiled straps are
IRON = [0, 1, 2, 3, 4, 5, 6, 7]                # blackened iron
RUST = [96, 97, 98]
RED = [66, 67, 68, 69, 70, 71]                 # the grenades' painted band (dark brick reds, not fullbright)

HALF_W = 3.8        # half its width (y)
DEPTH = {0: 3.4, 1: 1.7}   # how far its front stands out of the back at its middle (x), full and empty
BACK_X = 0.1        # its back, just off the body
BULGE_P = 2.6       # the front's roundness (a superellipse's power: 2 an ellipse, more a squarer box)
Z_BOTTOM, Z_TOP = -2.8, 2.0
INNER = 0.88        # the rim's inner edge, as a share of the outline (the leather's thickness)
Z_FLOOR = 1.2       # the inside's floor (what shows of the inside, looking in)
RING = 14           # points round the outline
GREN_Y = 1.75       # the heads' middles, either side of the strap
GREN_R = 1.2        # their radius
GREN_TOP = 2.75     # their tops (full): 0.75 out of the pouch
STRAP_HW = 0.5      # the strap's half width
STRAP_T = 0.16      # its thickness


class Mesh(mdlgen.Mesh):
    def face(self, pts, uvs, outward):
        """A convex polygon with its own texture coordinates, facing `outward`."""
        n = norm(cross(sub(pts[1], pts[0]), sub(pts[2], pts[0])))
        if dot(n, outward) < 0:
            pts, uvs = pts[::-1], uvs[::-1]
            n = mul(n, -1.0)
        base = len(self.verts)
        for p, uv in zip(pts, uvs):
            self.verts.append((p, n, (int(round(uv[0])), int(round(uv[1])))))
        for k in range(1, len(pts) - 1):
            self.tris.append((base, base + k + 1, base + k))  # clockwise from outside


def region_uv(region, u, v):
    s0, t0, s1, t1 = REGIONS[region]
    return (s0 + 0.5 + u * (s1 - s0 - 1), t0 + 0.5 + v * (t1 - t0 - 1))


def front_x(y, depth):
    """The front's distance out of the back at `y` (a superellipse: rounded at the sides, full in the middle)."""
    a = min(1.0, abs(y) / HALF_W)
    return BACK_X + depth * (1.0 - a ** BULGE_P) ** (1.0 / BULGE_P)


def outline(depth, scale=1.0, shrink=0.0):
    """The pouch's outline at a height: RING points round it, from the back's left corner round the front to its right
    corner (the back is the straight side), `scale` towards its middle, `shrink` units in from every side."""
    pts = []
    n_front = RING - 2
    for k in range(n_front):
        t = k / (n_front - 1)
        y = HALF_W * math.cos(math.pi * t)            # left (+y) round to right (-y), bunched at the sides
        x = front_x(y, depth)
        pts.append((x, y))
    # The back's two corners, rounded off a little.
    pts.append((BACK_X, -HALF_W * 0.93))
    pts.append((BACK_X, HALF_W * 0.93))
    mid = (BACK_X + depth * 0.45, 0.0)
    out = []
    for x, y in pts:
        dx, dy = x - mid[0], y - mid[1]
        ln = math.hypot(dx, dy) or 1.0
        s = scale * max(0.0, 1.0 - shrink / ln)
        out.append((mid[0] + dx * s, mid[1] + dy * s))
    return out, mid


def body(m, depth, full):
    """The walls (levels up from the bottom), the bottom, the rolled rim, the inside and its floor."""
    bulge = 1.0 if not full else 1.04                  # the middle rounded out a little more by the grenades
    levels = [(Z_BOTTOM, 0.84), (Z_BOTTOM + 0.35, 0.96), (-1.6, 1.0), (0.0, bulge), (1.4, 1.0), (Z_TOP, 1.0)]
    rings = []
    mid = None
    for z, sc in levels:
        o, mid = outline(depth, sc)
        rings.append([(x, y, z) for x, y in o])
    # Perimeter lengths for the wrap (the full pouch's: both frames share their skin coordinates).
    per = [0.0]
    base_ring = [(x, y, 0.0) for x, y in outline(DEPTH[0])[0]]
    for k in range(RING):
        per.append(per[-1] + math.dist(base_ring[k], base_ring[(k + 1) % RING]))
    zs = [z for z, _ in levels]
    for i in range(len(rings) - 1):
        for k in range(RING):
            j = (k + 1) % RING
            p = [rings[i][k], rings[i][j], rings[i + 1][j], rings[i + 1][k]]
            c = mul(add(p[0], p[2]), 0.5)
            outward = sub(c, (mid[0], mid[1], c[2]))
            if i == 0:
                outward = add(outward, (0.0, 0.0, -1.5))   # the bottom's rounded edge faces down too
            u0, u1 = per[k] / per[-1], per[k + 1] / per[-1]
            v0 = (Z_TOP - zs[i]) / (Z_TOP - Z_BOTTOM)
            v1 = (Z_TOP - zs[i + 1]) / (Z_TOP - Z_BOTTOM)
            uv = [region_uv("side", u, v) for u, v in ((u0, v0), (u1, v0), (u1, v1), (u0, v1))]
            m.face(p, uv, outward)
    # The bottom: a fan.
    bottom = rings[0]
    centre = (mid[0], mid[1], Z_BOTTOM)
    bo, bmid = outline(DEPTH[0], 1.0)  # (the full pouch's, for the skin)
    for k in range(RING):
        j = (k + 1) % RING
        pts = [centre, bottom[k], bottom[j]]
        uvs = [region_uv("bottom", 0.5, 0.5)] + [region_uv("bottom", 0.5 + 0.48 * (bo[q][1] / HALF_W),
                                                            0.5 + 0.48 * ((bo[q][0] - bmid[0]) / (DEPTH[0] * 0.6)))
                                                 for q in (k, j)]
        m.face(pts, uvs, (0.0, 0.0, -1.0))
    # The rim: over the top edge from the outside in, then the inside wall down to the floor.
    top = rings[-1]
    roll, _ = outline(depth, (1.0 + INNER) * 0.5)
    roll = [(x, y, Z_TOP + 0.18) for x, y in roll]
    inner, _ = outline(depth, INNER)
    inner_top = [(x, y, Z_TOP) for x, y in inner]
    inner_floor = [(x, y, Z_FLOOR) for x, y in inner]
    for k in range(RING):
        j = (k + 1) % RING
        u0, u1 = k / RING, (k + 1) / RING
        for a, b, region, v in ((top, roll, "rim", (0.0, 0.5)), (roll, inner_top, "rim", (0.5, 1.0))):
            p = [a[k], a[j], b[j], b[k]]
            c = mul(add(p[0], p[2]), 0.5)
            outward = add(sub(c, (mid[0], mid[1], c[2])), (0.0, 0.0, 1.2))
            if a is roll:
                outward = add(sub((mid[0], mid[1], c[2]), c), (0.0, 0.0, 1.2))
            uv = [region_uv(region, u, vv) for u, vv in ((u0, v[0]), (u1, v[0]), (u1, v[1]), (u0, v[1]))]
            m.face(p, uv, outward)
        p = [inner_top[k], inner_top[j], inner_floor[j], inner_floor[k]]
        c = mul(add(p[0], p[2]), 0.5)
        uv = [region_uv("inside", u, v) for u, v in ((u0, 0.0), (u1, 0.0), (u1, 0.6), (u0, 0.6))]
        m.face(p, uv, sub((mid[0], mid[1], c[2]), c))    # the inside wall faces in
    fc = (mid[0], mid[1], Z_FLOOR)
    for k in range(RING):
        j = (k + 1) % RING
        m.face([fc, inner_floor[k], inner_floor[j]],
               [region_uv("inside", 0.5, 0.8), region_uv("inside", k / RING, 1.0), region_uv("inside", (k + 1) / RING, 1.0)],
               (0.0, 0.0, 1.0))
    return mid


def grenade_head(m, centre_xy, r, z0, z1):
    """A grenade's iron head: an octagonal body from z0 up to z1, then a low dome and a flat top (a fuse's stub)."""
    cx, cy = centre_xy
    oct_ = [(math.cos((k + 0.5) * math.pi / 4), math.sin((k + 0.5) * math.pi / 4)) for k in range(8)]
    rings = [[(cx + r * a, cy + r * b, z0) for a, b in oct_],
             [(cx + r * a, cy + r * b, z1) for a, b in oct_],
             [(cx + r * 0.72 * a, cy + r * 0.72 * b, z1 + r * 0.3) for a, b in oct_],
             [(cx + r * 0.3 * a, cy + r * 0.3 * b, z1 + r * 0.42) for a, b in oct_]]
    vs = (0.0, 0.62, 0.86, 1.0)
    for i in range(3):
        for k in range(8):
            j = (k + 1) % 8
            p = [rings[i][k], rings[i][j], rings[i + 1][j], rings[i + 1][k]]
            c = mul(add(p[0], p[2]), 0.5)
            outward = sub(c, (cx, cy, c[2] - (0.0 if i == 0 else 1.0)))
            region = "gren" if i == 0 else "grentop"
            if i == 0:
                uv = [region_uv(region, u, v) for u, v in ((k / 8, 1.0), ((k + 1) / 8, 1.0), ((k + 1) / 8, 0.0), (k / 8, 0.0))]
            else:
                rr = (0.48, 0.32, 0.12)
                uv = [region_uv(region, 0.5 + rr[i - 1 + (1 if q >= 2 else 0)] * math.cos((k + (1 if q in (1, 2) else 0) + 0.5) * math.pi / 4),
                                0.5 + rr[i - 1 + (1 if q >= 2 else 0)] * math.sin((k + (1 if q in (1, 2) else 0) + 0.5) * math.pi / 4))
                      for q in range(4)]
            m.face(p, uv, outward)
    m.face(rings[3], [region_uv("grentop", 0.5 + 0.12 * math.cos((k + 0.5) * math.pi / 4),
                                0.5 + 0.12 * math.sin((k + 0.5) * math.pi / 4)) for k in range(8)], (0.0, 0.0, 1.0))


def sweep_xz(m, path, hw, t):
    """A strap along a path in the x-z plane (at y 0), `hw` its half width along y, `t` its thickness outwards (the
    path's left going along it, which is out of the pouch for a path over it from the back to the front)."""
    rings = []
    for i, (x, z) in enumerate(path):
        a = path[max(0, i - 1)]
        b = path[min(len(path) - 1, i + 1)]
        tx, tz = b[0] - a[0], b[1] - a[1]
        ln = math.hypot(tx, tz)
        nx, nz = -tz / ln, tx / ln                     # square to the path, its left: out of the pouch (the path
                                                        # goes clockwise round it seen from +y)
        rings.append([(x, -hw, z), (x + nx * t, -hw, z + nz * t), (x + nx * t, hw, z + nz * t), (x, hw, z)])
    for i in range(len(rings) - 1):
        for k in range(4):
            j = (k + 1) % 4
            p = [rings[i][k], rings[i][j], rings[i + 1][j], rings[i + 1][k]]
            mid = mul(add(p[0], p[2]), 0.5)
            ctr = mul(add(mul(add(rings[i][0], rings[i][2]), 0.5), mul(add(rings[i + 1][0], rings[i + 1][2]), 0.5)), 0.5)
            u0, u1 = i / (len(rings) - 1), (i + 1) / (len(rings) - 1)  # (by segment: the same in both frames)
            region = "strap" if k in (1, 3) else "strapend"
            v0, v1 = (0.0, 1.0) if k in (1, 3) else (0.0, 1.0)
            uv = [region_uv(region, u, v) for u, v in ((u0, v0), (u0, v1), (u1, v1), (u1, v0))]
            m.face(p, uv, sub(mid, ctr))
    for ring, i1 in ((rings[0], 1), (rings[-1], -2)):
        c = mul(add(ring[0], ring[2]), 0.5)
        o = sub(c, mul(add(rings[i1][0], rings[i1][2]), 0.5))
        m.face(ring, [region_uv("strapend", u, v) for u, v in ((0, 0), (1, 0), (1, 1), (0, 1))], o)
    return rings


def stud(m, centre, normal, r, h):
    """A domed rivet head (make_holster.py's): an octagon standing h out of a surface, a flat top."""
    normal = norm(normal)
    e1 = norm(cross(normal, (0.0, 0.0, 1.0) if abs(normal[2]) < 0.9 else (1.0, 0.0, 0.0)))
    e2 = cross(normal, e1)
    base = [add(centre, add(mul(e1, r * math.cos(k * math.pi / 4)), mul(e2, r * math.sin(k * math.pi / 4)))) for k in range(8)]
    top = [add(add(centre, mul(normal, h)), mul(sub(p, centre), 0.6)) for p in base]
    base = [add(p, mul(normal, -0.08)) for p in base]
    for k in range(8):
        j = (k + 1) % 8
        mid = mul(add(base[k], top[j]), 0.5)
        uv = [region_uv("iron", u, v) for u, v in ((k / 8, 1), ((k + 1) / 8, 1), ((k + 1) / 8, 0.5), (k / 8, 0.5))]
        m.face([base[k], base[j], top[j], top[k]], uv, sub(mid, centre))
    uv = [region_uv("iron", 0.5 + 0.45 * math.cos(k * math.pi / 4), 0.25 + 0.2 * math.sin(k * math.pi / 4)) for k in range(8)]
    m.face(top, uv, normal)


def buckle(m, x, z):
    """An iron buckle on the front, where the strap ends: a frame standing out of the leather, its tongue across."""
    hy, hz, t = 0.75, 0.55, 0.14
    bars = [((x + t, -hy, z + hz - 0.14), (x + t, hy, z + hz)),       # top bar
            ((x + t, -hy, z - hz), (x + t, hy, z - hz + 0.14)),       # bottom bar
            ((x + t, -hy, z - hz), (x + t, -hy + 0.16, z + hz)),      # left, right
            ((x + t, hy - 0.16, z - hz), (x + t, hy, z + hz)),
            ((x + t + 0.04, -0.07, z - hz + 0.1), (x + t + 0.04, 0.07, z + hz - 0.1))]  # the tongue
    for lo, hi in bars:
        cx, cy, cz = (lo[0] + hi[0]) / 2 - t / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2
        m.box((cx, cy, cz), (t / 2 + 0.06, (hi[1] - lo[1]) / 2, (hi[2] - lo[2]) / 2), "iron")


def build(frame):
    """Frame 0 full, 1 empty: the same vertices, moved."""
    full = frame == 0
    depth = DEPTH[frame]
    m = Mesh(SKIN_W, SKIN_H, REGIONS)
    mid = body(m, depth, full)
    # The grenades' heads: out of the top, full; empty, gone down into it (hidden under the inside's floor, and thin).
    gx = BACK_X + depth * 0.5
    for y in (GREN_Y, -GREN_Y):
        if full:
            grenade_head(m, (gx, y), GREN_R, Z_FLOOR - 0.1, GREN_TOP)
        else:
            grenade_head(m, (gx, y * 0.8), 0.25, Z_BOTTOM + 0.5, Z_FLOOR - 0.8)
    # The strap: from the back over the top between the heads, down the front to the buckle.
    fx = front_x(0.0, depth) * (1.04 if full else 1.0)
    path = [(BACK_X + 0.3, Z_TOP - 0.3), (BACK_X + 0.3, Z_TOP + 0.3), (BACK_X + depth * 0.5, Z_TOP + 0.55),
            (fx - 0.15, Z_TOP + 0.2), (fx + 0.02, Z_TOP - 0.5), (fx + 0.02, 0.35)]
    sweep_xz(m, path, STRAP_HW, STRAP_T)
    buckle(m, fx + STRAP_T, 0.0)
    # Rivets where the strap is sewn to the back, over the rim.
    for y in (-STRAP_HW * 0.55, STRAP_HW * 0.55):
        stud(m, (BACK_X + 0.3 + STRAP_T, y, Z_TOP - 0.1), (1.0, 0.0, 0.0), 0.16, 0.1)
    # A rivet at each of the front's top corners (the gusset's), where the walls meet the rim.
    for y in (-HALF_W * 0.8, HALF_W * 0.8):
        px = front_x(y, depth)
        stud(m, (px, y, Z_TOP - 0.45), (px - mid[0], y, 0.0), 0.2, 0.12)
    return m


def paint_skin():
    px = bytearray(SKIN_W * SKIN_H)

    def noise(s, t, k):
        h = (s * 374761393 + t * 668265263 + k * 2147483647) & 0xFFFFFFFF
        h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
        return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0

    def smooth(x, y, k):
        xi, yi = math.floor(x), math.floor(y)
        fx, fy = x - xi, y - yi
        fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
        a, b, c, d = noise(xi, yi, k), noise(xi + 1, yi, k), noise(xi, yi + 1, k), noise(xi + 1, yi + 1, k)
        return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy

    bayer = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]

    def pick(ramp, v, s, t):
        x = max(0.0, min(1.0, v)) * (len(ramp) - 1)
        i = int(x)
        if x - i > (bayer[t % 4][s % 4] + 0.5) / 16.0:
            i += 1
        return ramp[min(i, len(ramp) - 1)]

    def fill(region, fn):
        # The painters work in layout units (TEXEL texels): their patterns keep their size, the texels are finer.
        s0, t0, s1, t1 = REGIONS[region]
        for t in range(t0, t1):
            for s in range(s0, s1):
                px[t * SKIN_W + s] = fn((s - s0) / TEXEL, (t - t0) / TEXEL, (s1 - s0) // TEXEL, (t1 - t0) // TEXEL, s, t)

    def leather(k, base, seams=()):
        def fn(u, v, w, h, s, t):
            val = base + 0.16 * (smooth(u * 0.1, v * 0.1, k) - 0.5) + 0.1 * (noise(s, t, k) - 0.5)
            val += 0.1 * (smooth(u * 0.7, v * 0.7, k + 5) - 0.5)    # grain
            if smooth(u * 0.25, v * 0.08, k + 9) > 0.8:
                val -= 0.16                                        # creases, dark
            if noise(int(u) // 3, int(v), k + 3) > 0.985:
                val += 0.25                                        # a scuff
            edge = min(v + 0.5, h - v - 0.5)
            val *= 0.72 + 0.28 * min(1.0, edge / 3.0)              # darker, oiled, at the top and bottom
            for su in seams:                                      # the side gussets' seams, stitched
                d = abs(u - su * w)
                if d < 0.5:
                    return 16 if int(v) % 3 else 171
                if d < 2.0:
                    val -= 0.1
            if int(v) == 3 and int(u) % 3 != 2:
                return 171 if int(u) % 3 == 0 else 16                   # the stitching under the rim
            return pick(LEATHER, val, s, t)
        return fn

    def strap(k):
        def fn(u, v, w, h, s, t):
            val = 0.42 + 0.25 * (smooth(u * 0.3, v * 0.5, k) - 0.5) + 0.1 * (noise(s, t, k) - 0.5)
            if int(v) in (0, h - 1):
                val -= 0.25                                        # its edges, burnished dark
            if int(v) in (1, h - 2) and int(u) % 3 == 0:
                return 173                                         # stitching along them
            return pick(STRAP, val, s, t)
        return fn

    def iron(k):
        def fn(u, v, w, h, s, t):
            val = 0.38 + 0.3 * (smooth(u * 0.5, v * 0.5, k) - 0.5) + 0.15 * (noise(s, t, k) - 0.5)
            if noise(s, t, k + 1) > 0.92:
                return RUST[int(noise(s, t, k + 2) * 3) % 3]      # rust flecks
            return pick(IRON, val, s, t)
        return fn

    def grenade(k):
        def fn(u, v, w, h, s, t):
            # Dark iron, a painted red band round its top (v 0 is the top), and the casting's seams down it.
            if 2 <= v <= 5:
                val = 0.5 + 0.3 * (smooth(u * 0.4, v * 0.4, k) - 0.5) + 0.1 * (noise(s, t, k) - 0.5)
                if noise(s, t, k + 4) > 0.9:
                    return IRON[3]                                 # chipped paint
                return pick(RED, val, s, t)
            val = 0.42 + 0.25 * (smooth(u * 0.4, v * 0.4, k) - 0.5) + 0.12 * (noise(s, t, k) - 0.5)
            if int(u) % (w // 4) == 0:
                val -= 0.2                                         # the segments' grooves
            return pick(IRON, val, s, t)
        return fn

    def grenade_top(k):
        def fn(u, v, w, h, s, t):
            r = math.hypot(u - w / 2 + 0.5, v - h / 2 + 0.5) / (w / 2)
            val = 0.45 + 0.25 * (smooth(u * 0.4, v * 0.4, k) - 0.5) + 0.1 * (noise(s, t, k) - 0.5)
            if 0.62 < r < 0.8:
                return pick(RED, val, s, t)                        # the band's edge over the shoulder
            if r < 0.2:
                return pick(IRON, val - 0.2, s, t)                 # the fuse's stub, dark
            return pick(IRON, val + (0.1 if r < 0.45 else 0.0), s, t)
        return fn

    def inside(k):
        def fn(u, v, w, h, s, t):
            val = 0.12 + 0.1 * (smooth(u * 0.2, v * 0.2, k) - 0.5) + 0.06 * (noise(s, t, k) - 0.5)
            return pick(LEATHER, val, s, t)
        return fn

    fill("side", leather(1, 0.5, seams=(0.03, 0.97, 0.5)))
    fill("bottom", leather(2, 0.3))
    fill("inside", inside(3))
    fill("rim", leather(4, 0.6))
    fill("strap", strap(5))
    fill("strapend", leather(6, 0.22))
    fill("iron", iron(7))
    fill("gren", grenade(8))
    fill("grentop", grenade_top(9))
    fill("spare", leather(10, 0.4))
    for t in range(SKIN_H):
        for s in range(SKIN_W):
            assert px[t * SKIN_W + s] < 224, "no fullbright texels"
    return bytes(px)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "progs")
    full, empty = build(0), build(1)
    assert len(full.verts) == len(empty.verts) and full.tris == empty.tris, "frames of the same mesh"
    for f, g in zip(full.verts, empty.verts):
        assert f[2] == g[2], "the same skin coordinates in both frames"
    path = os.path.join(out, "vrpouch.mdl")
    # The files edited by hand since this wrote them are not overwritten (genguard.py: --keep-edited, --force).
    guard = genguard.Guard("make_pouch.py", [path])
    mdlgen.write_mdl(path, full, [paint_skin()], "full", frames=[empty])
    print("vrpouch.mdl: %d vertices, %d triangles, 2 frames -> %s" % (len(full.verts), len(full.tris),
                                                                      os.path.normpath(path)))
    guard.finish()


if __name__ == "__main__":
    main()
