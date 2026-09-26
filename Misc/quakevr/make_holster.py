#!/usr/bin/env python3
# make_holster.py -- generates quakevr/progs/legholster.mdl, the holster drawn at the hips and the
# upper holsters (vr_view.cpp setupHolsters, vr_leg_holster_model_*): a leather plate that rests
# against the body, with the weapon hanging on its outer side through two leather loops, and a belt
# loop at its top. Open on the outside, it suits small and large weapons alike.
#
# Round 18 (voice note 14-35-15: "very straight and plain and flat ... a little bit of a curve that
# adapts to the body ... avoiding the floating geometry ... more grim and more dark"): the plate
# curves round the body (concave on the body's side, as a holster worn on a thigh or a chest is),
# with a raised welt round its edge; the loops run from the plate round the weapon and back into it
# (the old clips' outer bars floated apart from them), riveted at both ends; the strap that rose
# into the air is a belt loop folded over the plate's top. Dark, worn, oiled leather, blackened iron.
#
# Usage: python Misc/quakevr/make_holster.py [output progs folder]
#
# Model space (as the model it replaces, whose placement cvars stay valid): +x forward, +z up,
# +y towards the body (the left-side holsters are drawn mirrored); the holstered weapon hangs at
# the origin. The skin uses Quake palette indices (chosen from gfx/palette.lmp), so no palette is
# needed to run this. Triangles are clockwise seen from outside, as Quake's.

import math
import os
import sys

import mdlgen
from mdlgen import add, cross, dot, mul, norm, sub

SKIN = 128
REGIONS = {
    "plate": (0, 0, 64, 80),       # the plate's outer face (the weapon's side)
    "back": (64, 0, 96, 40),       # its face against the body
    "welt": (96, 0, 128, 40),      # the raised edge
    "loop": (64, 40, 128, 56),     # the weapon loops and the belt loop
    "loopend": (64, 56, 96, 64),   # their cut ends
    "iron": (96, 56, 128, 64),     # rivets, the belt loop's keeper
    "rim": (0, 80, 64, 96),        # the plate's cut edge
}

# Leather, darkest to lightest: the reddish-black browns (16..20), then the greyish browns (174..171).
LEATHER = [16, 175, 174, 17, 173, 18, 19, 172, 20, 171]
STRAP = [16, 17, 96, 18, 97, 19, 98]           # the loops: darker, redder, as oiled straps are
IRON = [0, 1, 2, 3, 4, 5, 6, 7]                # blackened iron
RUST = [96, 97, 98]

PLATE_R = 7.0      # the plate's curvature round the body (model units; the body side is concave)
PLATE_Y = 2.7      # its middle's distance from the weapon (y), at x 0
PLATE_T = 0.5      # its thickness
# Its outline, top to bottom: (z, half width along the curve). Widest under the top, a tapered toe.
PLATE_ROWS = [(6.0, 3.9), (5.2, 4.2), (0.5, 4.1), (-3.6, 3.6), (-4.7, 2.7), (-5.15, 1.6)]
COLUMNS = 8        # facets across the curve
WELT_W, WELT_H = 0.45, 0.18   # the raised edge round the outer face: its width and height
LOOPS = [(3.1, 1.0), (-2.6, 1.0)]    # the weapon loops: height (z) of their middles, their width
LOOP_T = 0.28      # their thickness
LOOP_PATH = [(-1.9, 0.0), (-1.8, -0.4), (-1.2, -0.58), (0.0, -0.62), (1.2, -0.58), (1.8, -0.4),
             (1.9, 0.0)]   # round the weapon (x, y), from the plate and back into it
# The old model's bounds' low corner. The engine scales the holster (vr_leg_holster_model_scale)
# about the model's origin, which the header puts there: the new model keeps it (everything is
# inside those bounds), so that the placement cvars keep placing it as they did.
ORIGIN = (-4.4, -1.02, -5.2)


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
    s0, t0, s1, t1 = region
    return (s0 + 0.5 + u * (s1 - s0 - 1), t0 + 0.5 + v * (t1 - t0 - 1))


def on_plate(a, z, depth):
    """The point at angle a round the plate's curve, height z, `depth` towards the body from the
    plate's middle surface (negative: out towards the weapon)."""
    r = PLATE_R - depth
    return (r * math.sin(a), PLATE_Y + PLATE_R - r * math.cos(a), z)


def plate_y(x, depth):
    """The plate's surface y at x (the weapon's side: depth -PLATE_T / 2)."""
    r = PLATE_R - depth
    return PLATE_Y + PLATE_R - math.sqrt(max(0.0, r * r - x * x))


def plate(m):
    zs = [z for z, _ in PLATE_ROWS]
    half = PLATE_T / 2

    def grid(depth, rows=PLATE_ROWS):
        return [[on_plate((c / COLUMNS * 2 - 1) * w / PLATE_R, z, depth) for c in range(COLUMNS + 1)] for z, w in rows]

    out, inn = grid(-half), grid(half)
    outward = (0.0, -1.0, 0.0)
    for i in range(len(PLATE_ROWS) - 1):
        for c in range(COLUMNS):
            v0, v1 = (zs[0] - zs[i]) / (zs[0] - zs[-1]), (zs[0] - zs[i + 1]) / (zs[0] - zs[-1])
            u0, u1 = c / COLUMNS, (c + 1) / COLUMNS
            for g, region, sign in ((out, "plate", -1.0), (inn, "back", 1.0)):
                p = [g[i][c], g[i][c + 1], g[i + 1][c + 1], g[i + 1][c]]
                centre = mul(add(p[0], p[2]), 0.5)
                # Outward: away from the curve's centre (the body side's face: towards it).
                axis = (0.0, PLATE_Y + PLATE_R, centre[2])
                n = sub(axis, centre) if sign > 0 else sub(centre, axis)
                uv = [region_uv(REGIONS[region], u, v) for u, v in ((u0, v0), (u1, v0), (u1, v1), (u0, v1))]
                m.face(p, uv, n)
    # The cut edge all round (top, sides, toe), and a welt standing out of the outer face along it.
    ring = [(0, c) for c in range(COLUMNS + 1)] + [(i, COLUMNS) for i in range(1, len(PLATE_ROWS))] + \
           [(len(PLATE_ROWS) - 1, c) for c in range(COLUMNS - 1, -1, -1)] + [(i, 0) for i in range(len(PLATE_ROWS) - 2, 0, -1)]
    centre = (0.0, PLATE_Y, (zs[0] + zs[-1]) / 2)
    n = len(ring)
    for k in range(n):
        (i0, c0), (i1, c1) = ring[k], ring[(k + 1) % n]
        a, b, c, d = out[i0][c0], out[i1][c1], inn[i1][c1], inn[i0][c0]
        mid = mul(add(a, c), 0.5)
        o = sub(mid, centre)
        o = (o[0], 0.0, o[2])
        uv = [region_uv(REGIONS["rim"], u, v) for u, v in ((k / n, 0), ((k + 1) / n, 0), ((k + 1) / n, 1), (k / n, 1))]
        m.face([a, b, c, d], uv, o)
    welt(m, out, ring, centre)


def welt(m, out, ring, centre):
    """A raised edge along the outer face's border, WELT_W wide (inwards), WELT_H proud."""
    n = len(ring)
    pts = [out[i][c] for i, c in ring]
    inner, top_o, top_i = [], [], []
    for k in range(n):
        p = pts[k]
        # Inwards along the face: towards the plate's middle line, in the face.
        to_c = sub((0.0, p[1], centre[2]), p)
        to_c = norm((to_c[0], 0.0, to_c[2]))
        normal = norm(sub(p, (0.0, PLATE_Y + PLATE_R, p[2])))  # out of the outer face
        # Keep the welt in the face as it curves: move along x/z, then back onto the surface.
        q = add(p, mul(to_c, WELT_W))
        q = (q[0], plate_y(q[0], -PLATE_T / 2), q[2])
        inner.append(add(q, mul(normal, -0.03)))
        top_o.append(add(p, mul(normal, WELT_H)))
        top_i.append(add(q, mul(normal, WELT_H)))
    for k in range(n):
        j = (k + 1) % n
        u0, u1 = k / n, (k + 1) / n
        normal = norm(sub(mul(add(pts[k], pts[j]), 0.5), (0.0, PLATE_Y + PLATE_R, pts[k][2])))
        uv = lambda v: [region_uv(REGIONS["welt"], u, vv) for u, vv in ((u0, v[0]), (u1, v[0]), (u1, v[1]), (u0, v[1]))]
        m.face([top_o[k], top_o[j], top_i[j], top_i[k]], uv((0.3, 0.7)), normal)          # the top
        inward = sub(inner[k], pts[k])
        m.face([top_i[k], top_i[j], inner[j], inner[k]], uv((0.7, 1.0)), add(inward, mul(normal, 0.2)))
        outside = sub(pts[k], inner[k])
        base_o = [add(p, mul(norm(sub(p, (0.0, PLATE_Y + PLATE_R, p[2]))), -0.03)) for p in (pts[k], pts[j])]
        m.face([base_o[0], base_o[1], top_o[j], top_o[k]], uv((0.0, 0.3)), add(outside, mul(normal, 0.2)))


def sweep(m, path, z, hw, t, region, end_region, side=None):
    """A band along a path in the plane of height z: `hw` half its width along z, `t` its thickness,
    outwards (away from the weapon at the origin) from the path, or to one side of it (`side` 1:
    the right going along it, -1: the left). Capped ends."""
    rings = []
    for i, (x, y) in enumerate(path):
        a = path[max(0, i - 1)]
        b = path[min(len(path) - 1, i + 1)]
        tx, ty = b[0] - a[0], b[1] - a[1]
        ln = math.hypot(tx, ty)
        nx, ny = ty / ln, -tx / ln          # square to the path in the plane
        if side is not None:
            nx, ny = nx * side, ny * side
        elif nx * x + ny * y < 0:
            nx, ny = -nx, -ny               # away from the weapon
        rings.append([(x, y, z - hw), (x + nx * t, y + ny * t, z - hw), (x + nx * t, y + ny * t, z + hw), (x, y, z + hw)])
    L = [0.0]
    for i in range(1, len(path)):
        L.append(L[-1] + math.dist(path[i - 1], path[i]))
    for i in range(len(rings) - 1):
        for k in range(4):
            j = (k + 1) % 4
            p = [rings[i][k], rings[i][j], rings[i + 1][j], rings[i + 1][k]]
            mid = mul(add(p[0], p[2]), 0.5)
            ctr = mul(add(mul(add(rings[i][0], rings[i][2]), 0.5), mul(add(rings[i + 1][0], rings[i + 1][2]), 0.5)), 0.5)
            u0, u1 = L[i] / L[-1], L[i + 1] / L[-1]
            v0, v1 = k / 4, (k + 1) / 4
            uv = [region_uv(region, u, v) for u, v in ((u0, v0), (u0, v1), (u1, v1), (u1, v0))]
            m.face(p, uv, sub(mid, ctr))
    for ring, i0, i1 in ((rings[0], 0, 1), (rings[-1], -1, -2)):
        c = mul(add(ring[0], ring[2]), 0.5)
        o = sub(c, mul(add(rings[i1][0], rings[i1][2]), 0.5))
        uv = [region_uv(end_region, u, v) for u, v in ((0, 0), (1, 0), (1, 1), (0, 1))]
        m.face(ring, uv, o)


def stud(m, centre, normal, r, h):
    """A domed rivet head: an octagon standing h out of a surface, a flat top."""
    normal = norm(normal)
    e1 = norm(cross(normal, (0.0, 0.0, 1.0) if abs(normal[2]) < 0.9 else (1.0, 0.0, 0.0)))
    e2 = cross(normal, e1)
    base = [add(centre, add(mul(e1, r * math.cos(k * math.pi / 4)), mul(e2, r * math.sin(k * math.pi / 4)))) for k in range(8)]
    top = [add(add(centre, mul(normal, h)), mul(sub(p, centre), 0.6)) for p in base]
    base = [add(p, mul(normal, -0.08)) for p in base]
    rg = REGIONS["iron"]
    for k in range(8):
        j = (k + 1) % 8
        mid = mul(add(base[k], top[j]), 0.5)
        uv = [region_uv(rg, u, v) for u, v in ((k / 8, 1), ((k + 1) / 8, 1), ((k + 1) / 8, 0.5), (k / 8, 0.5))]
        m.face([base[k], base[j], top[j], top[k]], uv, sub(mid, centre))
    uv = [region_uv(rg, 0.5 + 0.45 * math.cos(k * math.pi / 4), 0.25 + 0.2 * math.sin(k * math.pi / 4)) for k in range(8)]
    m.face(top, uv, normal)


def loops(m):
    """The weapon loops: from inside the plate round the weapon's outer side and back into it."""
    for z, width in LOOPS:
        y_in = plate_y(LOOP_PATH[0][0], -PLATE_T / 2) + 0.2   # the ends sink into the plate
        path = [(LOOP_PATH[0][0], y_in)] + LOOP_PATH[1:-1] + [(LOOP_PATH[-1][0], y_in)]
        sweep(m, path, z, width / 2, LOOP_T, REGIONS["loop"], REGIONS["loopend"])
        # A rivet where each end meets the plate, and one at the front of the loop.
        for x in (-2.35, 2.35):
            m_pt = (x, plate_y(x, -PLATE_T / 2), z)
            stud(m, m_pt, sub(m_pt, (0.0, PLATE_Y + PLATE_R, z)), 0.3, 0.2)
        front = (0.0, LOOP_PATH[3][1] - LOOP_T, z)
        stud(m, front, (0.0, -1.0, 0.0), 0.26, 0.1)


def belt_loop(m):
    """A strap folded over the plate's top: down its outer face, over the top, down its back (the
    belt goes through it), with an iron keeper across it."""
    ztop = PLATE_ROWS[0][0]
    half = PLATE_T / 2
    t = 0.3
    y_out = plate_y(0.0, -half) - 0.02  # the strap lies on the plate's outer face
    y_in = plate_y(0.0, half) + 1.2     # its back, off the plate's back (the belt's room)
    path = [(3.4, plate_y(0.0, -half) + 0.1), (3.4, y_out), (ztop + 1.6, y_out), (ztop + 2.3, y_out + 0.6),
            (ztop + 2.3, y_in - 0.6), (ztop + 1.6, y_in), (3.0, y_in), (3.0, plate_y(0.0, half) - 0.1)]
    # The same band builder, in the (z, y) plane at x 0: swap the axes.
    tmp = Mesh(SKIN, SKIN, REGIONS)
    sweep(tmp, [(y, z) for z, y in path], 0.0, 1.1, t, REGIONS["loop"], REGIONS["loopend"], side=-1)
    base = len(m.verts)
    for p, n_, uv in tmp.verts:
        # The band was built with x = y, y = z, z = x: (x, y, z) -> (z, x, y), a rotation (the
        # windings keep).
        m.verts.append(((p[2], p[0], p[1]), (n_[2], n_[0], n_[1]), uv))
    for a, b, c in tmp.tris:
        m.tris.append((base + a, base + b, base + c))


def paint_skin():
    px = bytearray(SKIN * SKIN)

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
        s0, t0, s1, t1 = region
        for t in range(t0, t1):
            for s in range(s0, s1):
                px[t * SKIN + s] = fn(s - s0, t - t0, s1 - s0, t1 - t0, s, t)

    def leather(k, base, stitch=False):
        def fn(u, v, w, h, s, t):
            val = base + 0.16 * (smooth(u * 0.1, v * 0.1, k) - 0.5) + 0.1 * (noise(s, t, k) - 0.5)
            val += 0.1 * (smooth(u * 0.7, v * 0.7, k + 5) - 0.5)    # grain
            if smooth(u * 0.25, v * 0.08, k + 9) > 0.78:
                val -= 0.18                                        # creases, dark
            if noise(u // 3, v, k + 3) > 0.985:
                val += 0.25                                        # a scuff
            edge = min(u + 0.5, w - u - 0.5, v + 0.5, h - v - 0.5)
            val *= 0.7 + 0.3 * min(1.0, edge / 3.0)                # darker, oiled, round the edges
            if stitch and (min(u, w - 1 - u) == 4 or min(v, h - 1 - v) == 4) and (u + v) % 3 == 0:
                return 171                                         # dull thread
            if stitch and (min(u, w - 1 - u) == 4 or min(v, h - 1 - v) == 4) and (u + v) % 3 == 1:
                return 16                                          # its hole
            return pick(LEATHER, val, s, t)
        return fn

    def strap(k):
        def fn(u, v, w, h, s, t):
            val = 0.4 + 0.25 * (smooth(u * 0.3, v * 0.5, k) - 0.5) + 0.1 * (noise(s, t, k) - 0.5)
            if v in (1, h - 2):
                val -= 0.25                                        # its edges, burnished dark
            if v in (2, h - 3) and u % 3 == 0:
                return 173                                         # stitching along them
            return pick(STRAP, val, s, t)
        return fn

    def iron(k):
        def fn(u, v, w, h, s, t):
            val = 0.35 + 0.3 * (smooth(u * 0.5, v * 0.5, k) - 0.5) + 0.15 * (noise(s, t, k) - 0.5)
            if noise(s, t, k + 1) > 0.9:
                return RUST[int(noise(s, t, k + 2) * 3) % 3]      # rust flecks
            return pick(IRON, val, s, t)
        return fn

    fill(REGIONS["plate"], leather(1, 0.48, stitch=True))
    fill(REGIONS["back"], leather(2, 0.46, stitch=True))
    fill(REGIONS["welt"], leather(3, 0.58))
    fill(REGIONS["rim"], leather(4, 0.25))
    fill(REGIONS["loop"], strap(5))
    fill(REGIONS["loopend"], leather(6, 0.2))
    fill(REGIONS["iron"], iron(7))
    for t in range(SKIN):
        for s in range(SKIN):
            assert px[t * SKIN + s] < 224, "no fullbright texels"
    return bytes(px)


def build():
    m = Mesh(SKIN, SKIN, REGIONS)
    plate(m)
    loops(m)
    belt_loop(m)
    return m


def write(path, mesh, skin):
    """mdlgen.write_mdl, with the header's origin at ORIGIN rather than at the bounds' corner."""
    assert mesh.check_winding() == 0, "counter-clockwise triangles"
    table = mdlgen.anorms()
    positions = [v[0] for v in mesh.verts]
    lo = ORIGIN
    hi = [max(p[k] for p in positions) for k in range(3)]
    assert all(p[k] >= lo[k] - 1e-6 for p in positions for k in range(3)), "inside the old bounds"
    scale = [(hi[k] - lo[k]) / 255.0 for k in range(3)]
    radius = max(math.sqrt(dot(p, p)) for p in positions)
    data = bytearray(mdlgen.HEADER.pack(b"IDPO", 6, *scale, *lo, radius, 0.0, 0.0, 0.0, 1, SKIN, SKIN,
                                        len(mesh.verts), len(mesh.tris), 1, 0, 0, 1.0))
    data += (0).to_bytes(4, "little") + skin
    for _, _, (s, t) in mesh.verts:
        data += (0).to_bytes(4, "little") + s.to_bytes(4, "little", signed=True) + t.to_bytes(4, "little", signed=True)
    for a, b, c in mesh.tris:
        for x in (1, a, b, c):
            data += x.to_bytes(4, "little", signed=True)
    data += (0).to_bytes(4, "little") + bytes((0, 0, 0, 0)) + bytes((255, 255, 255, 0)) + b"holster".ljust(16, bytes(1))
    for p, n, _ in mesh.verts:
        q = [max(0, min(255, int(round((p[k] - lo[k]) / scale[k])))) for k in range(3)]
        best = max(range(len(table)), key=lambda i: dot(table[i], n))
        data += bytes((q[0], q[1], q[2], best))
    with open(path, "wb") as f:
        f.write(data)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "progs")
    m = build()
    path = os.path.join(out, "legholster.mdl")
    write(path, m, paint_skin())
    print("legholster.mdl: %d vertices, %d triangles -> %s" % (len(m.verts), len(m.tris), os.path.normpath(path)))


if __name__ == "__main__":
    main()
