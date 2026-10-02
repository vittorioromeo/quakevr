#!/usr/bin/env python3
# make_debris.py -- generates the rocks and bricks that lie about the maps (Quake VR's debris: engine vr_debris.cpp,
# QC vr_debris.qc; docs/vr-port/ROUND21.md, "Rocks and bricks"):
#   quakevr/progs/vr_rock1..5.mdl    loose stones, 8 to 15 cm: chunky low-poly fractured shapes (a box cut by a dozen
#                                    planes, each a flat facet), a flat side to rest on; six skins each (grey, beige,
#                                    brown, mossy, red-brown, dark), painted from 3D noise (seamless over the facets)
#   quakevr/progs/vr_brick1..4.mdl   a whole brick (19 x 9 x 6 cm), a chipped one, a half brick, a broken piece: worn
#                                    edges, chips and a rough fracture face, mortar left on its beds and ends; six
#                                    skins (red, brown, yellow, blue-grey, sooty, pale)
#   quakevr/progs/<model>_<skin>.png each skin in full colour at HIRES (2) times the size (256 x 256): what the engine
#                                    draws (an external skin); the model's own 8-bit skins are the same, in Quake's
#                                    palette, for a renderer without them
#
# Usage: python Misc/quakevr/make_debris.py [output game folder] [--preview out.png]
# Then bake the normal maps (python Misc/quakevr/bake_normals.py vr_rock1.mdl ... vr_brick4.mdl): their relief is this
# file's (relief(), normaltiles.py's "relief" recipe).
#   --preview: a contact sheet of every model's skins, and the shapes seen from above and the side (numpy, PIL).
#
# Model space: Quake units at vr_world_scale 1 (1 m = 1 / 0.0381 units), as make_shell.py: the engine scales each
# piece by vr_world_scale times its random size (vr_debris.cpp). x along the longest side, z up, the origin in the
# middle of its box; each model rests on its flat base (z = its lowest). Every shape is convex, so the physics' hull
# (Box3D: the convex hull of the drawn model) is exactly what is drawn: a stone rests on its drawn facets.
#
# Skins: 128 x 128, every facet its own island of the skin (a shelf-packed atlas at one texel density, two texels
# of bleed round each), so the baked normal map (bake_normals.py; normaltiles.py's "relief" recipe: this file's
# relief, the facets' edges rounded) has a place for each. Painted at RELIEF (4) times the size with the relief
# (lumps, crystalline ridges, grit, pits, cracks, chipped edges, strata; a brick's sand-struck clay, drag creases,
# pores, mortar lumps, a rough fracture), the colour following it (the pits and cracks dark and grimy), as the texture
# packs' walls look, whose bumps the engine makes from their shading; the relief as deep as those bumps (measured:
# ROCK_DEPTH, BRICK_DEPTH). The engine picks a skin by the colour of the wall or floor a piece lies by (vr_debris.cpp
# reads the 8-bit skins' average colours from the files: repainting them works).
#
# Deterministic: fixed seeds; the same files each run. genguard.py keeps it from overwriting models edited since.

import math
import os
import sys

import numpy as np

import genguard
import mdlgen

UNITS = 1.0 / 0.0381
SKIN = 128
BLEED = 2  # texels round each island
HIRES = 2  # the full-colour skins' texels per texel of the model's own 8-bit skin (256 x 256)
RELIEF = 4  # the relief's (the baked normal map's): painted at this, the colour then averaged down to HIRES

# ---------------------------------------------------------------------------------------------------------------------
# Convex shapes: a box cut by planes


def v3(x):
    return np.asarray(x, dtype=np.float64)


class Face:
    def __init__(self, pts, n, tag):
        self.pts = [v3(p) for p in pts]
        self.n = v3(n) / np.linalg.norm(n)
        self.tag = tag


def box(hx, hy, hz):
    """A box's six faces, each counter-clockwise seen from outside."""
    c = [v3((sx * hx, sy * hy, sz * hz)) for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]

    def P(sx, sy, sz):
        return c[((sx > 0) * 4) + ((sy > 0) * 2) + (sz > 0)]

    return [
        Face([P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), P(1, -1, 1)], (1, 0, 0), "end"),
        Face([P(-1, 1, -1), P(-1, -1, -1), P(-1, -1, 1), P(-1, 1, 1)], (-1, 0, 0), "end"),
        Face([P(1, 1, -1), P(-1, 1, -1), P(-1, 1, 1), P(1, 1, 1)], (0, 1, 0), "side"),
        Face([P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1)], (0, -1, 0), "side"),
        Face([P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1)], (0, 0, 1), "bed"),
        Face([P(-1, 1, -1), P(1, 1, -1), P(1, -1, -1), P(-1, -1, -1)], (0, 0, -1), "bed"),
    ]


def order_ccw(pts, n):
    """Points of a convex polygon on a plane of normal n, counter-clockwise seen from where n points."""
    c = sum(pts) / len(pts)
    u = pts[0] - c
    u /= np.linalg.norm(u)
    v = np.cross(n, u)
    return sorted(pts, key=lambda p: math.atan2(np.dot(p - c, v), np.dot(p - c, u)))


def dedupe(pts, eps=1e-6):
    out = []
    for p in pts:
        if all(np.linalg.norm(p - q) > eps for q in out):
            out.append(p)
    return out


def clip(faces, n, d, tag):
    """The shape cut by the plane n.p = d, keeping n.p <= d; the cut is a new face of `tag`."""
    n = v3(n) / np.linalg.norm(n)
    eps = 1e-7
    out, cut = [], []
    for f in faces:
        pts, kept = f.pts, []
        for i in range(len(pts)):
            a, b = pts[i], pts[(i + 1) % len(pts)]
            da, db = np.dot(n, a) - d, np.dot(n, b) - d
            if da <= eps:
                kept.append(a)
            if abs(da) <= eps:
                cut.append(a)
            if (da < -eps and db > eps) or (da > eps and db < -eps):
                p = a + (b - a) * (da / (da - db))
                kept.append(p)
                cut.append(p)
        kept = dedupe(kept)
        if len(kept) >= 3 and polygon_area(kept, f.n) > 1e-6:
            out.append(Face(kept, f.n, f.tag))
    cut = dedupe(cut, 1e-5)
    if len(cut) >= 3:
        out.append(Face(order_ccw(cut, n), n, tag))
    return out


def polygon_area(pts, n):
    s = np.zeros(3)
    for i in range(len(pts)):
        s += np.cross(pts[i], pts[(i + 1) % len(pts)])
    return abs(np.dot(s, n)) * 0.5


def support(faces, n):
    return max(np.dot(p, n) for f in faces for p in f.pts)


def unit(rng):
    while True:
        p = rng.uniform(-1, 1, 3)
        l = np.linalg.norm(p)
        if 0.2 < l <= 1:
            return p / l


def rock_shape(seed, size, cuts, flat):
    """A fractured stone: a box of `size` (metres, x y z) cut by `cuts` planes, each a random facet reaching into the
    ellipsoid it bounds, and a flat base `flat` of its height up from the bottom (a side to rest on)."""
    rng = np.random.default_rng(seed)
    h = v3(size) * 0.5 * UNITS
    faces = box(*h)
    for f in faces:
        f.tag = "facet"
    # The box's corners go first (a box is no stone): planes across each, then random facets.
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                n = v3((sx / h[0], sy / h[1], sz / h[2]))
                n = n / np.linalg.norm(n) + rng.normal(0, 0.18, 3)
                n /= np.linalg.norm(n)
                reach = math.sqrt(sum((n[k] * h[k]) ** 2 for k in range(3)))  # the ellipsoid's support
                faces = clip(faces, n, reach * rng.uniform(0.80, 0.93), "facet")
    for _ in range(cuts):
        n = unit(rng)
        n[2] *= 0.8
        n /= np.linalg.norm(n)
        reach = math.sqrt(sum((n[k] * h[k]) ** 2 for k in range(3)))
        faces = clip(faces, n, min(support(faces, n), reach) * rng.uniform(0.86, 0.97), "facet")
    # The base: flat, a little tilted off the box's axes (not machined).
    lo = min(p[2] for f in faces for p in f.pts)
    hi = max(p[2] for f in faces for p in f.pts)
    n = v3((rng.normal(0, 0.04), rng.normal(0, 0.04), -1.0))
    n /= np.linalg.norm(n)
    faces = clip(faces, n, -(lo + flat * (hi - lo)), "base")
    return centred(faces)


def brick_shape(seed, length, chamfer, chips=(), breaks=None, width=0.09, height=0.06):
    """A brick `length` metres long (a whole one 0.19), `width` by `height`: its twelve edges worn (`chamfer` metres,
    uneven), its corners knocked, `chips` (a corner or an edge: (sx, sy, sz, depth metres), 0 for an edge's free axis)
    cut off, and a broken end at +x (`breaks`: that many planes, a rough fracture face)."""
    rng = np.random.default_rng(seed)
    h = v3((length, width, height)) * 0.5 * UNITS
    faces = box(*h)
    c = chamfer * UNITS

    def cut(n, depth, tag):
        nonlocal faces
        n = v3(n)
        n /= np.linalg.norm(n)
        faces = clip(faces, n, support(faces, n) - depth, tag)

    # A broken end first (the worn edges then run round what is left).
    if breaks:
        x = h[0]
        for k in range(breaks):
            n = v3((1.0, rng.normal(0, 0.35), rng.normal(0, 0.35)))
            cut(n, rng.uniform(0.0, 0.35) * UNITS * 0.01 + (0.004 * UNITS if k else 0.0), "break")
    # Worn edges: each of the twelve, unevenly.
    for a in range(3):
        b, e = [k for k in range(3) if k != a]
        for sb in (-1, 1):
            for se in (-1, 1):
                n = np.zeros(3)
                n[b], n[e] = sb, se
                n += rng.normal(0, 0.12, 3) * (np.arange(3) != a)
                cut(n, c * rng.uniform(0.5, 1.4), "chamfer")
    # Knocked corners.
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                cut((sx, sy, sz) + rng.normal(0, 0.15, 3), c * rng.uniform(0.8, 2.2), "chamfer")
    for sx, sy, sz, depth in chips:
        n = v3((sx, sy, sz)) + rng.normal(0, 0.2, 3)
        cut(n, depth * UNITS, "break")
    return centred(faces)


def centred(faces):
    """Moved so that its box's middle is the origin."""
    pts = [p for f in faces for p in f.pts]
    lo, hi = np.min(pts, axis=0), np.max(pts, axis=0)
    mid = (lo + hi) * 0.5
    for f in faces:
        f.pts = [p - mid for p in f.pts]
    return faces


# ---------------------------------------------------------------------------------------------------------------------
# The skin's atlas: each facet flat on its own island


def atlas(faces):
    """Each face's (s, t) per corner (whole texels, as MDL keeps them), packed in shelves into SKIN x SKIN at the largest
    texel density that fits."""
    flat = []
    for f in faces:
        o = f.pts[0]
        # Along the face's longest extent: the islands pack tighter.
        best = None
        for i in range(len(f.pts)):
            u = f.pts[(i + 1) % len(f.pts)] - f.pts[i]
            u /= np.linalg.norm(u)
            v = np.cross(f.n, u)
            q = [(np.dot(p - o, u), np.dot(p - o, v)) for p in f.pts]
            w = max(x for x, _ in q) - min(x for x, _ in q)
            hh = max(y for _, y in q) - min(y for _, y in q)
            if best is None or w * hh < best[0]:
                best = (w * hh, q)
        q = best[1]
        x0, y0 = min(x for x, _ in q), min(y for _, y in q)
        flat.append([(x - x0, y - y0) for x, y in q])

    def pack(density):
        order = sorted(range(len(flat)), key=lambda i: -max(y for _, y in flat[i]))
        x = y = shelf = 0
        place = {}
        for i in order:
            w = int(math.ceil(max(p[0] for p in flat[i]) * density)) + 1 + 2 * BLEED
            hh = int(math.ceil(max(p[1] for p in flat[i]) * density)) + 1 + 2 * BLEED
            if x + w > SKIN:
                x, y, shelf = 0, y + shelf, 0
            if y + hh > SKIN or w > SKIN:
                return None
            place[i] = (x + BLEED, y + BLEED)
            x += w
            shelf = max(shelf, hh)
        return place

    lo, hi = 1.0, 400.0
    for _ in range(40):
        mid = (lo + hi) * 0.5
        (lo, hi) = (mid, hi) if pack(mid) else (lo, mid)
    place = pack(lo)
    uvs = []
    for i, q in enumerate(flat):
        ox, oy = place[i]
        uvs.append([(int(round(ox + x * lo)), int(round(oy + y * lo))) for x, y in q])
    return uvs, lo


def mesh_of(faces, uvs):
    """The MDL mesh: each face's corners its own vertices (flat-shaded facets, each on its island), fanned."""
    mesh = mdlgen.Mesh(SKIN, SKIN, {})
    for f, st in zip(faces, uvs):
        base = len(mesh.verts)
        for p, uv in zip(f.pts, st):
            mesh.verts.append((tuple(float(x) for x in p), tuple(float(x) for x in f.n), uv))
        for i in range(1, len(f.pts) - 1):
            mesh.tris.append((base, base + i + 1, base + i))  # clockwise seen from outside (Quake's)
    return mesh


def texel_points(faces, uvs, k=1):
    """For each texel of the skin at `k` times its size (the same atlas): the model-space point it shows, its face's
    number (-1: none, filled from a neighbour so the islands bleed), and how far it is from its face's outline
    (units)."""
    size = SKIN * k
    P = np.zeros((size, size, 3))
    F = np.full((size, size), -1, dtype=np.int32)
    E = np.zeros((size, size))
    ys, xs = (np.mgrid[0:size, 0:size] + 0.5) / k - 0.5  # texel centres, in the skin's texels
    for fi, (f, st) in enumerate(zip(faces, uvs)):
        st = [v3(s) for s in st]
        edges3 = [(f.pts[i], f.pts[(i + 1) % len(f.pts)]) for i in range(len(f.pts))]
        for i in range(1, len(st) - 1):
            a, b, c = st[0], st[i], st[i + 1]
            A, B, C = f.pts[0], f.pts[i], f.pts[i + 1]
            det = (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1])
            if abs(det) < 1e-9:
                continue
            l1 = ((xs - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (ys - a[1])) / det
            l2 = ((b[0] - a[0]) * (ys - a[1]) - (xs - a[0]) * (b[1] - a[1])) / det
            l0 = 1 - l1 - l2
            m = (l0 >= -1e-6) & (l1 >= -1e-6) & (l2 >= -1e-6)
            pts = l0[m, None] * A + l1[m, None] * B + l2[m, None] * C
            P[m] = pts
            F[m] = fi
            E[m] = np.min(np.stack([seg_dist(pts, e0, e1) for e0, e1 in edges3]), axis=0)
    # Bleed: empty texels take their nearest island's (a few passes of the 8 neighbours).
    for _ in range((2 * BLEED + 2) * k):
        empty = F < 0
        if not empty.any():
            break
        for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0), (1, 1), (-1, -1), (1, -1), (-1, 1)):
            src_f = np.roll(np.roll(F, dy, 0), dx, 1)
            take = empty & (src_f >= 0) & (F < 0)
            F[take] = src_f[take]
            P[take] = np.roll(np.roll(P, dy, 0), dx, 1)[take]
            E[take] = np.roll(np.roll(E, dy, 0), dx, 1)[take]
    return P, F, E


def seg_dist(p, a, b):
    ab = b - a
    t = np.clip(((p - a) @ ab) / max(np.dot(ab, ab), 1e-12), 0, 1)
    return np.linalg.norm(p - (a + t[:, None] * ab), axis=1)


# ---------------------------------------------------------------------------------------------------------------------
# Painting: 3D value noise, so a pattern runs on over a facet's edge


def hash3(ix, iy, iz, seed):
    h = (ix.astype(np.uint32) * np.uint32(73856093)) ^ (iy.astype(np.uint32) * np.uint32(19349663)) ^ \
        (iz.astype(np.uint32) * np.uint32(83492791)) ^ np.uint32(seed * 2654435761 & 0xFFFFFFFF)
    h ^= h >> np.uint32(13)
    h *= np.uint32(1274126177)
    h ^= h >> np.uint32(16)
    return (h & np.uint32(0xFFFFFF)).astype(np.float64) / float(0xFFFFFF)


def vnoise(p, seed):
    i = np.floor(p)
    f = p - i
    u = f * f * (3 - 2 * f)
    i = i.astype(np.int64)
    out = 0.0
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                w = (u[..., 0] if dx else 1 - u[..., 0]) * (u[..., 1] if dy else 1 - u[..., 1]) * \
                    (u[..., 2] if dz else 1 - u[..., 2])
                out = out + w * hash3(i[..., 0] + dx, i[..., 1] + dy, i[..., 2] + dz, seed)
    return out


def fbm(p, seed, octaves=3):
    total, amp, norm = 0.0, 1.0, 0.0
    for o in range(octaves):
        total = total + amp * vnoise(p * (2.0 ** o), seed + 17 * o)
        norm += amp
        amp *= 0.5
    return total / norm


def smoothstep(x, a, b):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0 - 0.47


def ramp_rgb(ramp, light):
    """Lightness 0..1 along a palette ramp (dark to light) as full colour (sRGB 0..1): its colours blended."""
    pal = np.array(palette(), dtype=np.float64)[np.asarray(ramp)] / 255.0
    x = np.clip(light, 0, 1) * (len(ramp) - 1)
    i = np.clip(np.floor(x).astype(np.int64), 0, len(ramp) - 2)
    f = (x - i)[..., None]
    return pal[i] * (1 - f) + pal[i + 1] * f


def to_palette(img, size, idx):
    """A full-colour skin ((h, w, 3) 0..1) as a model's own 8-bit one: its texels averaged down to `size` wide, then
    the nearest of the palette's colours `idx` (no fullbrights), with ordered dithering."""
    small = down(img, img.shape[1] // size)
    idx = np.asarray(sorted(set(idx)))
    pal = np.array(palette(), dtype=np.float64)[idx] / 255.0
    ys, xs = np.mgrid[0:small.shape[0], 0:small.shape[1]]
    c = small + BAYER[ys % 4, xs % 4][..., None] * 0.035
    d = ((c[..., None, :] - pal[None, None]) ** 2).sum(-1)
    return idx[np.argmin(d, -1)].astype(np.uint8)


def down(img, k):
    """An image averaged down k times (k x k texels to one)."""
    h, w = img.shape[:2]
    return img.reshape(h // k, k, w // k, k, -1).mean((1, 3)).reshape((h // k, w // k) + img.shape[2:])


def mix(a, b, t):
    t = np.asarray(t, dtype=np.float64)
    return a + (b - a) * (t[..., None] if t.ndim else t)


def ridged(p, seed, octaves=3):
    """Ridged noise 0..1: sharp crests where value noise crosses its middle (a crystalline break's)."""
    total, amp, norm = 0.0, 1.0, 0.0
    for o in range(octaves):
        n = 1.0 - np.abs(2.0 * vnoise(p * (2.0 ** o), seed + 17 * o) - 1.0)
        total = total + amp * n * n
        norm += amp
        amp *= 0.5
    return total / norm


def cracks(p, seed, width, keep):
    """Hairline cracks 0..1 where a noise crosses its middle (within `width` of it), broken into pieces (`keep`: the
    share of their length kept)."""
    n = fbm(p, seed, 3)
    on = smoothstep(vnoise(p * 0.5 + 9.0, seed + 1), 1.0 - keep - 0.08, 1.0 - keep + 0.08)
    return (1.0 - smoothstep(np.abs(n - 0.5), 0.0, width)) * on


# Palette ramps, dark to light (gfx/palette.lmp's rows: greys 0-15, browns 16-31, blue-greys 32-47, reds and oranges
# 96-111, tans 112-127, beige stone 160-175 (light first), grey-greens 176-191 (light first)).
GREY = list(range(1, 10))
BEIGE = list(range(174, 164, -1))
BROWN = list(range(17, 28))
YELLOWBROWN = list(range(20, 31))
MOSSGREY = list(range(190, 180, -1))
MOSS = [48 + k for k in range(3, 11)]  # olive greens
TAN = list(range(113, 122))
DARK = list(range(1, 8))
BLUEGREY = list(range(32, 41))
BRICKRED = list(range(96, 105))
SOOT = list(range(96, 102))
MORTAR = list(range(4, 9))
PALEMORTAR = list(range(171, 166, -1))

ROCK_SKINS = [  # (name, ramp, fleck ramp, moss)
    ("grey", GREY, BEIGE, False),
    ("beige", BEIGE, GREY, False),
    ("brown", BROWN, TAN, False),
    ("mossy", list(range(1, 9)), MOSSGREY, True),
    ("redbrown", TAN, BROWN, False),
    ("dark", DARK, BLUEGREY, False),
]
BRICK_SKINS = [  # (name, clay, fired patches (the kiln's flashing, darker), mortar)
    ("red", BRICKRED, SOOT, PALEMORTAR),
    ("brown", BROWN, list(range(16, 24)), PALEMORTAR),
    ("yellow", YELLOWBROWN, TAN, PALEMORTAR),
    ("bluegrey", BLUEGREY, DARK, MORTAR),
    ("sooty", SOOT, list(range(16, 21)), PALEMORTAR),
    ("pale", BEIGE, BROWN, MORTAR),
]
GRIME = np.array((0.075, 0.06, 0.045))  # dirt packed into the pits and cracks
DARKEN = 0.86  # the skins a little darker and duller than the ramps: weathered, dirty, as the texture packs' stone

# Relief depth (units) of the rocks' and the bricks' detail, at vr_normalmap_authored 1: matched by measurement to
# the bumps the engine makes from the texture pack's walls (QRP's bricka2_1, rock1_2, rock3_8 at his
# vr_normalmap_strength 1.5: mean tilt 0.6-1.1, the 90th percentile 1.2-2.1; ROUND21.md, "Grittier debris, crates
# and crowbar").
ROCK_DEPTH = 1.7
BRICK_DEPTH = 1.9


def rock_paint(faces, P, F, E, seed, strata, res):
    """A rock's six skins ((h, w, 3) sRGB 0..1) and its relief ((h, w) units) at texels P, F, E (`res` texels a
    unit). The relief: lumps over each facet, crystalline ridges, grit, pits, hairline cracks, edges chipped, the
    strata's ledges; the colour follows it (the crests lighter, the pits and cracks dark and packed with grime), as
    the texture packs' stone does, whose bumps the engine makes from their shading."""
    rng = np.random.default_rng(seed)
    face_shade = rng.uniform(-0.05, 0.05, len(faces) + 1)
    normals = np.array([f.n for f in faces] + [(0, 0, 1)])
    Fi = np.where(F < 0, len(faces), F)
    N = normals[Fi]
    q = P * (1.0 / 1.1)
    fine = min(18.0, 0.22 * res)  # the finest grain the relief holds (a few texels across)
    lump = fbm(P * 0.9, seed + 101, 3) - 0.5
    crys = ridged(P * 3.2, seed + 103, 3) - 0.45
    grit = fbm(P * fine, seed + 105, 2) - 0.5
    pits = smoothstep(vnoise(P * 6.5, seed + 9), 0.70, 0.9) + 0.6 * smoothstep(vnoise(P * 15.0, seed + 11), 0.78, 0.92)
    crack = cracks(P * 1.4, seed + 107, 0.018, 0.55)
    chipn = vnoise(P * 2.2, seed + 111)
    chip = (1 - smoothstep(E, 0.0, 0.08 + 0.18 * chipn)) * smoothstep(chipn, 0.35, 0.65)
    detail = 0.07 * crys + 0.016 * grit - 0.045 * pits - 0.06 * crack - 0.07 * chip
    h = 0.10 * lump + detail
    ledge = np.zeros_like(lump)
    if strata is not None:
        phase = (P @ v3(strata)) * 5.5 / (2 * math.pi) + 0.5 * fbm(q, seed + 3, 2)
        ledge = smoothstep(phase - np.floor(phase), 0.0, 0.18) - 0.5  # a step at each layer, then a slope
        h = h + 0.035 * ledge
    h = h * ROCK_DEPTH
    hd = detail / 0.07  # the detail, about -1.5..0.6: the colour's
    light = 0.45 + 0.30 * (fbm(q, seed, 3) - 0.5) * 2
    light += 0.10 * (vnoise(P * 7.0, seed + 5) - 0.5)
    light += 0.22 * hd
    light += 0.18 * (vnoise(P * fine * 1.7, seed + 13) - 0.5)  # salt-and-pepper grains
    light += 0.08 * ledge
    light += face_shade[Fi]
    light += 0.10 * (1 - smoothstep(E, 0.0, 0.09)) * (1 - chip)  # worn edges catch the light
    light -= 0.10 * (N[..., 2] < -0.5)  # the underside, darker (it lay in the dirt)
    light = np.clip(light, 0, 1)
    fleck = smoothstep(vnoise(P * 11.0, seed + 21), 0.80, 0.88)
    vein = smoothstep(fbm(P * 0.6, seed + 23, 3), 0.55, 0.75)
    dirt = smoothstep(-hd, 0.3, 1.1)
    moss_mask = smoothstep(N[..., 2], -0.1, 0.5) * smoothstep(fbm(P * 0.9, seed + 31, 3), 0.42, 0.56)
    moss_mask = moss_mask * (0.6 + 0.4 * smoothstep(vnoise(P * fine, seed + 33), 0.3, 0.7))
    out = []
    for _, ramp, fleck_ramp, moss in ROCK_SKINS:
        c = ramp_rgb(ramp, light)
        c = mix(c, ramp_rgb(fleck_ramp, light), 0.22 * vein)
        c = mix(c, ramp_rgb(fleck_ramp, light + 0.1), 0.8 * fleck)
        if moss:
            c = mix(c, ramp_rgb(MOSS, light * 0.8 + 0.1), moss_mask)
        c = mix(c, GRIME, 0.55 * dirt)
        out.append(np.clip(c * DARKEN, 0, 1))
    return out, h


def brick_paint(faces, P, F, E, seed, res):
    """A brick's six skins and its relief, as rock_paint's: the clay's sand-struck skin (a shallow undulation, the
    mould's drag creases along it, pores, pits, grit), hairline cracks, chipped edges, the fracture rough and deep,
    mortar left in lumps on its beds and ends."""
    rng = np.random.default_rng(seed)
    face_shade = rng.uniform(-0.04, 0.04, len(faces) + 1)
    tags = [f.tag for f in faces] + ["side"]
    Fi = np.where(F < 0, len(faces), F)
    tag = np.array(tags)[Fi]
    inner = tag == "break"
    fine = min(18.0, 0.22 * res)
    und = fbm(P * 0.9, seed + 201, 3) - 0.5
    crease = 1 - smoothstep(np.abs(fbm(P * v3((0.5, 2.4, 2.4)), seed + 203, 3) - 0.5), 0.0, 0.03)
    grit = fbm(P * fine, seed + 205, 2) - 0.5
    pores = smoothstep(vnoise(P * 16.0, seed + 9), 0.74, 0.9)
    pits = smoothstep(vnoise(P * 5.5, seed + 207), 0.78, 0.93)
    crack = cracks(P * 1.1, seed + 209, 0.015, 0.4)
    chipn = vnoise(P * 2.6, seed + 211)
    chip = (1 - smoothstep(E, 0.0, 0.06 + 0.16 * chipn)) * smoothstep(chipn, 0.4, 0.7) * ~inner
    rough = fbm(P * 3.0, seed + 213, 4) - 0.5
    detail = -0.03 * crease + 0.014 * grit - 0.04 * pores - 0.05 * pits - 0.05 * crack - 0.07 * chip
    detail = np.where(inner, 0.12 * rough + 0.025 * grit - 0.04 * pores, detail)
    h = np.where(inner, 0.0, 0.05 * und) + detail
    # Mortar left on the beds and the ends (where it was laid), in lumps; none on the breaks or the worn edges.
    mlump = fbm(P * 4.5, seed + 215, 3)
    mo = ((tag == "bed") | (tag == "end")) * smoothstep(fbm(P * 2.2, seed + 41, 3), 0.62, 0.7) * smoothstep(E, 0.05, 0.12)
    h = h + mo * (0.04 + 0.07 * (mlump - 0.3) + 0.012 * grit)
    h = h * BRICK_DEPTH
    hd = detail / 0.06
    light = 0.42 + 0.22 * (fbm(P * 0.8, seed, 3) - 0.5) * 2
    light += 0.10 * (vnoise(P * 9.0, seed + 5) - 0.5)
    light += 0.25 * hd
    light += 0.16 * (vnoise(P * fine * 1.5, seed + 13) - 0.5)  # grains of sand and grog
    light += face_shade[Fi]
    light += 0.08 * (1 - smoothstep(E, 0.0, 0.06)) * (1 - chip)
    light = np.where(inner, light + 0.10, light)  # the fracture: fresher, lighter clay
    light = np.clip(light, 0, 1)
    fired = smoothstep(fbm(P * 1.6, seed + 13, 2), 0.6, 0.74) * ~inner  # darker fired patches, the kiln's flash
    salt = smoothstep(fbm(P * 1.2, seed + 17, 3), 0.62, 0.8) * smoothstep(vnoise(P * fine, seed + 19), 0.4, 0.8)
    dirt = smoothstep(-hd, 0.3, 1.1)
    mlight = np.clip(0.42 + 0.3 * (mlump - 0.5) + 0.12 * (vnoise(P * fine * 1.5, seed + 23) - 0.5), 0, 1)
    out = []
    for _, clay, spots, mortar_ramp in BRICK_SKINS:
        c = ramp_rgb(clay, light)
        c = mix(c, ramp_rgb(spots, light * 0.8), 0.85 * fired)
        c = mix(c, ramp_rgb(clay, light + 0.18), chip * 0.7)  # fresh clay where it chipped
        c = mix(c, np.array((0.55, 0.52, 0.47)), 0.18 * salt)  # efflorescence: salts dried out of it
        c = mix(c, GRIME, 0.5 * dirt)
        c = mix(c, ramp_rgb(mortar_ramp, mlight), np.clip(mo * 1.4, 0, 1))
        out.append(np.clip(c * DARKEN, 0, 1))
    return out, h


# ---------------------------------------------------------------------------------------------------------------------

M = 0.01  # a centimetre, in metres

ROCKS = [  # (file, seed, size x y z, cuts, flat base, strata axis)
    ("vr_rock1.mdl", 101, (12 * M, 9 * M, 7.5 * M), 10, 0.14, None),
    ("vr_rock2.mdl", 202, (9 * M, 8 * M, 6.5 * M), 9, 0.18, (0.3, 0.2, 1.0)),
    ("vr_rock3.mdl", 303, (15 * M, 9 * M, 7 * M), 12, 0.12, None),
    ("vr_rock4.mdl", 404, (11 * M, 10 * M, 8.5 * M), 11, 0.16, (1.0, 0.1, 0.5)),
    ("vr_rock5.mdl", 505, (13 * M, 8.5 * M, 5.5 * M), 10, 0.2, None),
]
BRICKS = [  # (file, seed, length, chamfer, chips, break planes)
    ("vr_brick1.mdl", 11, 19 * M, 0.35 * M, (), None),
    ("vr_brick2.mdl", 22, 19 * M, 0.45 * M, ((1, -1, 1, 1.6 * M), (-1, 0, -1, 1.0 * M)), None),
    ("vr_brick3.mdl", 33, 9.5 * M, 0.4 * M, (), 3),
    ("vr_brick4.mdl", 44, 13 * M, 0.5 * M, ((-1, 1, 1, 1.4 * M),), 4),
]


def build(kind, spec):
    """A model: (name, faces, mesh, its 8-bit skins (bytes), the full-colour skins ((S, S, 3) uint8, S = HIRES times
    the skin's size), the relief ((R, R) units, R = RELIEF times it), texels a unit of its own skin)."""
    global SKIN
    SKIN = 128  # (make_crates.py sets its own)
    if kind == "rock":
        name, seed, size, cuts, flat, strata = spec
        faces = rock_shape(seed, size, cuts, flat)
    else:
        name, seed, length, chamfer, chips, breaks = spec
        faces = brick_shape(seed, length, chamfer, chips, breaks)
    uvs, density = atlas(faces)
    mesh = mesh_of(faces, uvs)
    P, F, E = texel_points(faces, uvs, RELIEF)
    if kind == "rock":
        skins, h = rock_paint(faces, P, F, E, seed, strata, density * RELIEF)
        idx = [r + f + (MOSS if m else []) for _, r, f, m in ROCK_SKINS]
    else:
        skins, h = brick_paint(faces, P, F, E, seed, density * RELIEF)
        idx = [c + s + m for _, c, s, m in BRICK_SKINS]
    full = [np.round(down(s, RELIEF // HIRES) * 255).astype(np.uint8) for s in skins]
    own = [to_palette(s, SKIN, i).tobytes() for s, i in zip(skins, idx)]
    return name, faces, mesh, own, full, h, density


_relief = {}


def relief(name):
    """The relief of model `name` (vr_rock1.mdl...) for its normal map (normaltiles.py): (heights (R, R) in units, its
    own skin's texels a unit, RELIEF)."""
    if name not in _relief:
        kind, spec = next((k, s) for k, specs in (("rock", ROCKS), ("brick", BRICKS)) for s in specs if s[0] == name)
        m = build(kind, spec)
        _relief[name] = (m[5], m[6], RELIEF)
    return _relief[name]


def volume(faces):
    """The shape's volume (units^3): tetrahedra from the origin."""
    v = 0.0
    for f in faces:
        for i in range(1, len(f.pts) - 1):
            v += np.dot(f.pts[0], np.cross(f.pts[i], f.pts[i + 1])) / 6.0
    return abs(v)


def preview(models, path):
    from PIL import Image, ImageDraw
    rows = []
    for name, faces, mesh, own, full, h, _ in models:
        row = [Image.fromarray(s).resize((SKIN, SKIN), Image.LANCZOS) for s in full]
        rows.append((name, row, faces))
    W = 6 * (SKIN + 4) + 2 * 164
    img = Image.new("RGB", (W, len(rows) * (SKIN + 18)), (32, 32, 32))
    d = ImageDraw.Draw(img)
    for r, (name, row, faces) in enumerate(rows):
        y = r * (SKIN + 18)
        d.text((2, y + 2), name, fill=(255, 255, 0))
        for k, im in enumerate(row):
            img.paste(im, (k * (SKIN + 4), y + 14))
        for view, ox in ((0, 6 * (SKIN + 4)), (1, 6 * (SKIN + 4) + 164)):
            for f in sorted(faces, key=lambda f: -f.n[2] if view == 0 else f.n[1]):
                if (view == 0 and f.n[2] <= 0) or (view == 1 and f.n[1] >= 0):
                    continue
                pts = [(ox + 80 + p[0] * 14, y + 14 + 64 - (p[1] if view == 0 else p[2]) * 14) for p in f.pts]
                shade = int(90 + 140 * max(0.0, np.dot(f.n, v3((0.3, -0.5, 0.8)) / np.linalg.norm((0.3, -0.5, 0.8)))))
                d.polygon(pts, fill=(shade, shade, shade), outline=(20, 20, 20))
    img.save(path)


def palette():
    here = os.path.dirname(os.path.abspath(__file__))
    sys.path.insert(0, os.path.join(here, "blender", "addons", "quakevr_models"))
    import qpal
    return qpal.PALETTE


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    args = sys.argv[1:]
    prev = None
    if "--preview" in args:
        i = args.index("--preview")
        prev = args[i + 1]
        del args[i:i + 2]
    game = args[0] if args else os.path.join(here, "..", "..", "quakevr")
    models = [build("rock", s) for s in ROCKS] + [build("brick", s) for s in BRICKS]
    paths = [os.path.join(game, "progs", m[0]) for m in models]
    extra = [os.path.join(game, "progs", "%s_%d.png" % (m[0], k)) for m in models for k in range(len(m[4]))]
    guard = genguard.Guard("make_debris.py", paths + extra)
    from PIL import Image
    for (name, faces, mesh, own, full, h, density), path in zip(models, paths):
        if path not in guard.kept:
            mdlgen.write_mdl(path, mesh, own, name.split(".")[0])
        for k, s in enumerate(full):
            out = "%s_%d.png" % (path, k)
            if out not in guard.kept:
                Image.fromarray(s).save(out, optimize=True)
        pts = np.array([p for f in faces for p in f.pts])
        size = (pts.max(axis=0) - pts.min(axis=0)) / UNITS * 100
        vol = volume(faces) / UNITS ** 3 * 1e3  # litres
        print("%s: %d faces, %d vertices, %d triangles; %.1f x %.1f x %.1f cm, %.2f l (%.2f kg of stone at 2600 kg/m^3, "
              "%.2f of brick at 1900); %.1f texels/unit (%d x %d; full colour %d x %d)" % (
                  name, len(faces), len(mesh.verts), len(mesh.tris), size[0], size[1], size[2], vol, vol * 2.6,
                  vol * 1.9, density, SKIN, SKIN, SKIN * HIRES, SKIN * HIRES))
    guard.finish()
    if prev:
        preview(models, prev)
        print("preview -> " + prev)


if __name__ == "__main__":
    main()
