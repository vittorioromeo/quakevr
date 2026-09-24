#!/usr/bin/env python3
# make_holster.py -- generates quakevr/progs/legholster.mdl, the holster drawn at the hip and upper
# holsters (vr_view.cpp, vr_leg_holster_model_*): a leather plate that rests against the body,
# with the weapon hanging on its outer side under two leather clips, and a strap up to the belt.
# Open on the outside, it suits small and large weapons alike.
#
# Usage: python Misc/quakevr/make_holster.py [output progs folder]
#
# Model space (as the model it replaces, whose placement cvars stay valid): +x forward, +z up,
# +y towards the body (the left-side holster is drawn mirrored); the holstered weapon hangs at
# the origin. The skin uses Quake palette indices (chosen from gfx/palette.lmp: leathers 166..173,
# reddish straps 97..100, metal 7..11), so no palette is needed to run this.

import math
import os
import struct
import sys

# ----------------------------------------------------------------------------
# Geometry: bevelled boxes, flat-shaded (each face has its own vertices)

verts = []  # (position, normal, (s, t) in skin pixels)
tris = []   # (a, b, c), clockwise seen from outside


def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, k): return (a[0] * k, a[1] * k, a[2] * k)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def norm(a):
    l = math.sqrt(dot(a, a))
    return mul(a, 1.0 / l)


SKIN_W, SKIN_H = 64, 64
# Skin regions (s0, t0, s1, t1) in pixels.
REGIONS = {"plate": (0, 0, 32, 32), "strap": (32, 0, 64, 32), "metal": (0, 32, 32, 64), "edge": (32, 32, 64, 64)}


def quad(p0, p1, p2, p3, region):
    """A quad p0 p1 p2 p3, counter-clockwise seen from outside (the MDL wants clockwise)."""
    n = norm(cross(sub(p1, p0), sub(p3, p0)))
    s0, t0, s1, t1 = REGIONS[region]
    st = [(s0 + 1, t1 - 1), (s1 - 1, t1 - 1), (s1 - 1, t0 + 1), (s0 + 1, t0 + 1)]
    base = len(verts)
    for p, uv in zip((p0, p1, p2, p3), st):
        verts.append((p, n, uv))
    tris.append((base, base + 2, base + 1))
    tris.append((base, base + 3, base + 2))


def box(centre, half, region, bevel=0.0):
    """An axis-aligned box; with a bevel, its vertical edges are cut at 45 degrees."""
    cx, cy, cz = centre
    hx, hy, hz = half
    b = min(bevel, hx * 0.5, hy * 0.5)
    # Outline in x/y (counter-clockwise from above), eight corners with the bevel.
    ring = [(hx - b, -hy), (hx, -hy + b), (hx, hy - b), (hx - b, hy), (-hx + b, hy), (-hx, hy - b), (-hx, -hy + b), (-hx + b, -hy)]
    if b == 0.0:
        ring = [(hx, -hy), (hx, hy), (-hx, hy), (-hx, -hy)]
    lo = [(cx + x, cy + y, cz - hz) for x, y in ring]
    hi = [(cx + x, cy + y, cz + hz) for x, y in ring]
    n = len(ring)
    for i in range(n):
        j = (i + 1) % n
        quad(lo[i], lo[j], hi[j], hi[i], region)
    # Top and bottom as triangle fans.
    for cap, pts, up in ((1, hi, True), (0, lo, False)):
        c = (cx, cy, cz + (hz if up else -hz))
        normal = (0.0, 0.0, 1.0 if up else -1.0)
        s0, t0, s1, t1 = REGIONS["edge"]
        base = len(verts)
        verts.append((c, normal, ((s0 + s1) // 2, (t0 + t1) // 2)))
        for p in pts:
            verts.append((p, normal, ((s0 + s1) // 2 + 4, (t0 + t1) // 2)))
        for i in range(n):
            a, bb = base + 1 + i, base + 1 + (i + 1) % n
            tris.append((base, bb, a) if up else (base, a, bb))


# The plate against the body (y 2.2 .. 3.2), tall and a little tapered at the bottom.
box((0.0, 2.7, 0.5), (4.2, 0.5, 5.5), "plate", bevel=1.0)
# Its stitched rim, a hair proud of it on the outer side.
box((0.0, 2.1, 0.5), (4.4, 0.12, 5.7), "edge", bevel=1.1)
# Two clips: from the plate's outer face around the weapon's side (open on the outside).
for z in (3.3, -2.7):
    box((0.0, 1.2, z), (1.6, 1.0, 0.55), "strap")          # out from the plate
    box((0.0, -0.6, z), (1.6, 0.35, 0.55), "strap")        # across the weapon
    box((0.0, -0.6, z), (0.55, 0.42, 0.65), "metal")       # a rivet on it
# The strap up to the belt, with a buckle.
box((0.0, 2.9, 7.6), (1.2, 0.35, 2.4), "strap")
box((0.0, 2.5, 6.4), (1.5, 0.2, 0.7), "metal")

# ----------------------------------------------------------------------------
# Skin (palette indices)

PLATE = [168, 169, 170, 171]
STRAP = [98, 99, 100, 101]
METAL = [7, 8, 9, 10, 11]
EDGE = [172, 173, 174]


def skin():
    rng = 4242
    px = bytearray()
    for t in range(SKIN_H):
        for s in range(SKIN_W):
            region = next(r for r, (s0, t0, s1, t1) in REGIONS.items() if s0 <= s < s1 and t0 <= t < t1)
            ramp = {"plate": PLATE, "strap": STRAP, "metal": METAL, "edge": EDGE}[region]
            rng = (rng * 1103515245 + 12345) & 0x7FFFFFFF
            k = (rng >> 16) % 10
            i = len(ramp) // 2 + (-1 if k < 3 else 1 if k > 7 else 0)
            if region == "plate" and (s % 32 in (2, 29) or t % 32 in (2, 29)) and (s + t) % 3 == 0:
                px.append(EDGE[0])  # stitching
                continue
            px.append(ramp[max(0, min(len(ramp) - 1, i))])
    return bytes(px)

# ----------------------------------------------------------------------------
# MDL


def anorms():
    here = os.path.dirname(os.path.abspath(__file__))
    text = open(os.path.join(here, "..", "..", "Quake", "anorms.h")).read()
    import re
    return [tuple(float(x) for x in m.groups()) for m in
            re.finditer(r"\{\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\}", text)]


def write_mdl(path):
    table = anorms()
    positions = [v[0] for v in verts]
    lo = [min(p[k] for p in positions) for k in range(3)]
    hi = [max(p[k] for p in positions) for k in range(3)]
    scale = [(hi[k] - lo[k]) / 255.0 or 1.0 for k in range(3)]
    radius = max(math.sqrt(dot(p, p)) for p in positions)

    data = bytearray(struct.pack("<4si3f3f f3f 8i f", b"IDPO", 6, *scale, *lo, radius, 0.0, 0.0, 0.0,
                                 1, SKIN_W, SKIN_H, len(verts), len(tris), 1, 0, 0, 1.0))
    data += struct.pack("<i", 0) + skin()
    for _, _, (s, t) in verts:
        data += struct.pack("<3i", 0, s, t)
    for a, b, c in tris:
        data += struct.pack("<4i", 1, a, b, c)
    data += struct.pack("<i", 0)
    data += bytes((0, 0, 0, 0)) + bytes((255, 255, 255, 0))
    data += b"holster".ljust(16, b"\0")
    for p, n, _ in verts:
        q = [max(0, min(255, int(round((p[k] - lo[k]) / scale[k])))) for k in range(3)]
        best = max(range(len(table)), key=lambda i: dot(table[i], n))
        data += bytes((q[0], q[1], q[2], best))
    with open(path, "wb") as f:
        f.write(data)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "progs")
    path = os.path.join(out, "legholster.mdl")
    write_mdl(path)
    print("legholster.mdl: %d vertices, %d triangles -> %s" % (len(verts), len(tris), os.path.normpath(path)))


if __name__ == "__main__":
    main()
