#!/usr/bin/env python3
# make_brains.py -- generates the brain chunks a head bursts into (Quake VR's small gibs of brain matter: QC
# vr_smallgibs.qc, "Brain chunks"; docs/vr-port/ROUND21.md, "Brain chunks"):
#   quakevr/progs/gib_brain1..3.mdl   torn lumps of brain, 5 to 7 cm: a rounded, lumpy piece of the cortex (its folds,
#                                     the gyri and the narrow sulci between them, in the relief and the colour; the
#                                     shape's own lumps in the mesh) and the rough, paler face it was torn off along
#                                     (the white matter, bloodier); two skins each (0 pinkish, 1 greyish), smeared
#                                     with blood as the gibs are
#   quakevr/progs/<model>_<skin>.png  each skin in full colour at HIRES (2) times the size (256 x 256): what the engine
#                                     draws; the model's own 8-bit skins are the same in Quake's palette
#
# Usage: python Misc/quakevr/make_brains.py [output game folder] [--preview out.png]
# Then bake the normal map (python Misc/quakevr/bake_normals.py gib_brain1.mdl gib_brain2.mdl gib_brain3.mdl): its
# relief is this file's (relief(), normaltiles.py's "relief" recipe), the folds' creases and the torn face's grain.
#
# Model space: Quake units at vr_world_scale 1 (1 m = 1 / 0.0381 units), as make_debris.py: QC scales each chunk to the
# small gibs' size (vr_smallgibs_size_min/max of a rock's) times vr_brains_size. The origin in the middle of its box.
# The mesh: a subdivided icosahedron (1280 triangles), smooth, pulled into the chunk's ellipsoid and lumped, cut by a
# rough plane (the torn face: a hard edge round it). The physics' hull (Box3D: the convex hull of the drawn model) is
# its outline, the torn face a side to rest on.
#
# Skins: 128 x 128, the cortex box-projected onto six islands (by each triangle's facing) and the torn face its own,
# shelf-packed at one texel density, two texels of bleed round each. Painted at RELIEF (4) times the size from 3D
# noise (seamless over the islands' edges): the folds are the contour lines of a warped value noise (each line a
# sulcus, rounded gyri between), dark and bloody in the sulci, fine vessels over the gyri, blood smeared over it
# (thicker towards the torn face), the colour darker and duller than the palette's pinks as the gibs' dark meat is.
#
# Deterministic: fixed seeds; the same files each run. genguard.py keeps it from overwriting models edited since.

import math
import os
import sys

import numpy as np

import genguard
import make_debris as md
import mdlgen

UNITS = md.UNITS
SKIN = 128
BLEED = 2
HIRES = 2
RELIEF = 4
M = 0.01  # a centimetre, in metres

BRAINS = [  # (file, seed, size cm x y z, the torn face's normal, the share of the depth along it torn off)
    ("gib_brain1.mdl", 11, (6.4, 4.6, 3.6), (0.25, -0.3, -1.0), 0.28),
    ("gib_brain2.mdl", 23, (5.2, 4.4, 3.4), (-0.9, 0.15, -0.5), 0.32),
    ("gib_brain3.mdl", 37, (7.0, 3.8, 3.0), (0.1, 0.8, -0.6), 0.26),
]

# The skins: (name, a gyrus' top, a sulcus' floor, the torn face (white matter)), sRGB 0..255, before DARKEN.
SKINS = [
    ("pinkish", (176, 130, 134), (84, 46, 54), (190, 158, 152)),
    ("greyish", (146, 134, 138), (64, 52, 58), (170, 160, 160)),
]
BLOOD = np.array((96, 8, 10)) / 255.0  # fresh (the gibs' reds: palette 64-79)
CLOT = np.array((52, 2, 4)) / 255.0    # dark, thick
VESSEL = np.array((150, 30, 40)) / 255.0
DARKEN = 0.6  # as dim as the gibs' meat beside them
FOLDS = 6.5    # the folds' noise, cycles a unit (more: closer folds)
DEPTH = 0.10   # units: a sulcus' depth in the relief (about 4 mm at vr_world_scale 1)
PALETTE_IDX = list(range(128, 144)) + list(range(144, 160)) + list(range(160, 176)) + list(range(64, 80)) + \
    list(range(1, 15)) + list(range(96, 102))  # mauve greys, pinks, pinkish beige, reds, greys, dark browns


# ---------------------------------------------------------------------------------------------------------------------
# The shape


def icosphere(n):
    """A unit icosphere subdivided `n` times: (vertices (V, 3), triangles (T, 3)), counter-clockwise from outside."""
    t = (1 + 5 ** 0.5) / 2
    v = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
         (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
    v = [np.array(p, dtype=np.float64) / np.linalg.norm(p) for p in v]
    f = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6),
         (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7),
         (9, 8, 1)]
    for _ in range(n):
        mid = {}

        def m(a, b):
            k = (min(a, b), max(a, b))
            if k not in mid:
                p = v[a] + v[b]
                v.append(p / np.linalg.norm(p))
                mid[k] = len(v) - 1
            return mid[k]

        nf = []
        for a, b, c in f:
            ab, bc, ca = m(a, b), m(b, c), m(c, a)
            nf += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        f = nf
    return np.array(v), np.array(f)


def brain_shape(seed, size, torn_n, torn_share):
    """The chunk: (vertices (V, 3) units, triangles (T, 3) counter-clockwise from outside, each vertex torn or not,
    the torn face's normal)."""
    D, T = icosphere(3)
    h = np.array(size) * 0.5 * M * UNITS
    lump = (md.fbm(D * 1.6 + 3.0, seed, 3) - 0.5) * 2  # -1..1: the lobes' swell
    bump = (md.fbm(D * 4.0 + 7.0, seed + 5, 2) - 0.5) * 2  # the folds' bulges, as far as the mesh holds them
    n = np.array(torn_n, dtype=np.float64)
    n /= np.linalg.norm(n)
    # The plane, placed on the smooth ellipsoid; the lumps fade out round where it cuts it, so the cut is convex there
    # (a lump's dip under the rim would fold the triangles across it).
    s0 = (D * h) @ n
    d0 = s0.max() - torn_share * (s0.max() - s0.min())
    near = np.clip(np.abs(s0 - d0) / (0.2 * (s0.max() - s0.min())), 0, 1)
    P = D * h * (1 + (0.24 * lump + 0.09 * bump) * near * near * (3 - 2 * near))[:, None]
    s = P @ n
    torn = s > d0 - 1e-9
    assert d0 > 0, "the torn face through the middle"
    # Onto the plane along each vertex's ray from the middle (not straight across: the lumps would fold the face's
    # triangles over each other); inside the outline it had, so its rim meets the cortex.
    R = P[torn] / np.linalg.norm(P[torn], axis=1, keepdims=True)
    P[torn] = R * (d0 / (R @ n))[:, None]
    # The torn face: rough, swelling a little out (soft matter torn, not cut), none of it at its rim (w 0 there, by the
    # distance to its rim: else a lip would overhang the cortex).
    rim_v = np.unique(T[torn[T].any(axis=1) & ~torn[T].all(axis=1)])
    rim = P[rim_v[torn[rim_v]]]
    dist = np.min(np.linalg.norm(P[torn][:, None, :] - rim[None, :, :], axis=2), axis=1)
    w = np.clip(dist / max(1e-6, 0.6 * dist.max()), 0, 1)
    w = w * w * (3 - 2 * w)
    rough = (md.fbm(P[torn] * 2.2, seed + 9, 3) - 0.5) * 2
    P[torn] += np.outer((0.06 * rough + 0.12) * h.min() * w, n)
    mid = (P.min(axis=0) + P.max(axis=0)) * 0.5
    return P - mid, T, torn, n


def tri_normals(P, T):
    n = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
    return n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)


def charts(P, T, torn, torn_n):
    """Each triangle's island (0-5: the cortex by its facing, +x -x +y -y +z -z; 6: the torn face) and each island's
    projection (u, v axes)."""
    fn = tri_normals(P, T)
    istorn = torn[T].all(axis=1)
    a = np.argmax(np.abs(fn), axis=1)
    sgn = np.take_along_axis(fn, a[:, None], 1)[:, 0] < 0
    chart = np.where(istorn, 6, a * 2 + sgn)
    axes = []
    for c in range(6):
        nrm = np.zeros(3)
        nrm[c // 2] = -1 if c % 2 else 1
        u = np.zeros(3)
        u[(c // 2 + 1) % 3] = 1
        axes.append((u, np.cross(nrm, u)))
    u = np.cross(torn_n, (0, 0, 1) if abs(torn_n[2]) < 0.9 else (1, 0, 0))
    u /= np.linalg.norm(u)
    axes.append((u, np.cross(torn_n, u)))
    return chart, axes


def build_mesh(P, T, torn, torn_n):
    """The MDL mesh: a vertex for each (vertex, island) it lies on, the islands packed (as make_debris.py's atlas);
    smooth normals within the cortex and within the torn face, a hard edge between. Also each triangle's corners'
    (s, t) and 3D points and its island (the painting's)."""
    chart, axes = charts(P, T, torn, torn_n)
    fn = tri_normals(P, T)
    area = np.linalg.norm(np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]]), axis=1)
    # Normals: the cortex's and the torn face's apart.
    vn = {}
    for ti, tri in enumerate(T):
        side = chart[ti] == 6
        for vi in tri:
            vn[(vi, side)] = vn.get((vi, side), 0) + fn[ti] * area[ti]
    flat = []
    for c in range(7):
        ids = np.nonzero(chart == c)[0]
        if len(ids) == 0:
            flat.append(None)
            continue
        vs = np.unique(T[ids])
        u, v = axes[c]
        q = {int(i): (float(P[i] @ u), float(P[i] @ v)) for i in vs}
        x0 = min(p[0] for p in q.values())
        y0 = min(p[1] for p in q.values())
        flat.append((ids, {k: (p[0] - x0, p[1] - y0) for k, p in q.items()}))

    def pack(density):
        order = sorted((c for c in range(7) if flat[c]), key=lambda c: -max(p[1] for p in flat[c][1].values()))
        x = y = shelf = 0
        place = {}
        for c in order:
            w = int(math.ceil(max(p[0] for p in flat[c][1].values()) * density)) + 1 + 2 * BLEED
            hh = int(math.ceil(max(p[1] for p in flat[c][1].values()) * density)) + 1 + 2 * BLEED
            if x + w > SKIN:
                x, y, shelf = 0, y + shelf, 0
            if y + hh > SKIN or w > SKIN:
                return None
            place[c] = (x + BLEED, y + BLEED)
            x += w
            shelf = max(shelf, hh)
        return place

    lo, hi = 1.0, 400.0
    for _ in range(40):
        mid = (lo + hi) * 0.5
        (lo, hi) = (mid, hi) if pack(mid) else (lo, mid)
    place = pack(lo)
    mesh = mdlgen.Mesh(SKIN, SKIN, {})
    index = {}
    tris_st = []
    for c in range(7):
        if not flat[c]:
            continue
        ids, q = flat[c]
        ox, oy = place[c]
        for ti in ids:
            corners = []
            for vi in T[ti]:
                key = (int(vi), c)
                if key not in index:
                    nrm = vn[(int(vi), c == 6)]
                    nrm = nrm / np.linalg.norm(nrm)
                    st = (int(round(ox + q[int(vi)][0] * lo)), int(round(oy + q[int(vi)][1] * lo)))
                    index[key] = len(mesh.verts)
                    mesh.verts.append((tuple(float(x) for x in P[vi]), tuple(float(x) for x in nrm), st))
                corners.append(index[key])
            a, b, cc = corners
            mesh.tris.append((a, cc, b))  # clockwise seen from outside (Quake's)
            tris_st.append(([mesh.verts[k][2] for k in corners], P[T[ti]], c))
    return mesh, tris_st, lo


def texel_points(tris_st, k):
    """For each texel at `k` times the skin's size: the model-space point it shows and its island (-1: none, then
    filled from a neighbour so the islands bleed)."""
    size = SKIN * k
    Pt = np.zeros((size, size, 3))
    C = np.full((size, size), -1, dtype=np.int32)
    for st, pts, c in tris_st:
        st = np.array(st, dtype=np.float64)
        a, b, cc = st
        det = (b[0] - a[0]) * (cc[1] - a[1]) - (cc[0] - a[0]) * (b[1] - a[1])
        if abs(det) < 1e-9:
            continue
        # The texels whose centres (in skin texels: (i + 0.5) / k - 0.5) may fall in it.
        x0 = max(0, int(math.floor((st[:, 0].min() + 0.5) * k)) - 1)
        x1 = min(size, int(math.ceil((st[:, 0].max() + 0.5) * k)) + 1)
        y0 = max(0, int(math.floor((st[:, 1].min() + 0.5) * k)) - 1)
        y1 = min(size, int(math.ceil((st[:, 1].max() + 0.5) * k)) + 1)
        ys, xs = (np.mgrid[y0:y1, x0:x1] + 0.5) / k - 0.5
        l1 = ((xs - a[0]) * (cc[1] - a[1]) - (cc[0] - a[0]) * (ys - a[1])) / det
        l2 = ((b[0] - a[0]) * (ys - a[1]) - (xs - a[0]) * (b[1] - a[1])) / det
        l0 = 1 - l1 - l2
        m = (l0 >= -1e-6) & (l1 >= -1e-6) & (l2 >= -1e-6)
        sub = Pt[y0:y1, x0:x1]
        sub[m] = l0[m, None] * pts[0] + l1[m, None] * pts[1] + l2[m, None] * pts[2]
        C[y0:y1, x0:x1][m] = c
    for _ in range((2 * BLEED + 2) * k):
        empty = C < 0
        if not empty.any():
            break
        for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0), (1, 1), (-1, -1), (1, -1), (-1, 1)):
            src = np.roll(np.roll(C, dy, 0), dx, 1)
            take = (C < 0) & (src >= 0)
            C[take] = src[take]
            Pt[take] = np.roll(np.roll(Pt, dy, 0), dx, 1)[take]
    return Pt, C


# ---------------------------------------------------------------------------------------------------------------------
# The paint


def brain_paint(P, C, seed, torn_n, torn_d):
    """The skins ((h, w, 3) sRGB 0..1) and the relief ((h, w) units) at texels P (their islands C)."""
    torn = C == 6
    # The folds: contour lines of a warped noise, each a sulcus; between them a gyrus, rounded (a half circle's
    # profile: the sulcus a narrow crease, the gyrus' top broad).
    warp = np.stack([md.fbm(P * 1.1 + o, seed + 40 + j, 2) - 0.5 for j, o in enumerate((0.0, 5.3, 9.7))], -1)
    # (Where two noises of the folds' size cross their middles: a labyrinth of meandering lines, not the nested rings
    # of one noise's contours.)
    n1 = md.vnoise(P * FOLDS + warp * 2.5, seed + 50) - 0.5
    n2 = md.vnoise(P * FOLDS * 1.37 + warp * 2.5 + 4.1, seed + 51) - 0.5
    d = np.minimum(np.abs(n1), np.abs(n2) * 0.8)
    d = np.clip(d / 0.2, 0, 1) * 0.5  # 0 in a sulcus .. 0.5 on a gyrus' crest
    gyrus = np.sqrt(np.clip(1 - (1 - 2 * d) ** 2, 0, 1))
    # Some sulci deeper (the fissures), some shallow.
    deep = md.smoothstep(md.vnoise(P * 0.9 + 2.0, seed + 60), 0.35, 0.75)
    fine = md.fbm(P * 14.0, seed + 70, 2) - 0.5
    mottle = md.fbm(P * 2.6, seed + 80, 3) - 0.5
    # Vessels over the gyri: thin lines where a noise crosses its middle, broken into pieces.
    vn = md.fbm(P * 3.2, seed + 90, 2)
    vessel = (1 - md.smoothstep(np.abs(vn - 0.5), 0.0, 0.022)) * md.smoothstep(md.vnoise(P * 1.3, seed + 91), 0.45, 0.6)
    vessel *= md.smoothstep(gyrus, 0.3, 0.7)
    # Blood: smears, thicker towards the torn face (s: how near it, along its normal), pooled in the sulci.
    near = md.smoothstep(P @ torn_n, torn_d - 0.9, torn_d)
    bl = md.fbm(P * 1.5 + 11.0, seed + 100, 3) + 0.22 * near
    smear = md.smoothstep(bl, 0.70, 0.78)
    clot = md.smoothstep(md.fbm(P * 4.0, seed + 110, 2) + 0.15 * near, 0.62, 0.72)
    pooled = (1 - md.smoothstep(d, 0.0, 0.22)) * deep  # the deep sulci: blood in them
    # The torn face: rough, pitted, blood-soaked.
    trough = md.fbm(P * 5.0, seed + 120, 3) - 0.5
    tpits = md.smoothstep(md.vnoise(P * 9.0, seed + 121), 0.68, 0.85)
    tblood = md.smoothstep(md.fbm(P * 2.6, seed + 122, 3) + 0.25 * (md.fbm(P * 7.0, seed + 123, 2) - 0.5), 0.5, 0.6)

    h_cortex = DEPTH * ((gyrus - 1) * (0.6 + 0.6 * deep) + 0.06 * fine + 0.25 * mottle)
    h_torn = DEPTH * (0.7 * trough - 0.35 * tpits + 0.05 * fine)
    h = np.where(torn, h_torn, h_cortex)

    out = []
    for _, top, floor, white in SKINS:
        top = np.array(top) / 255.0
        floor = np.array(floor) / 255.0
        white = np.array(white) / 255.0
        g = gyrus ** 0.8
        c = md.mix(floor, top, g) * (1 + 0.22 * mottle + 0.05 * fine)[..., None]
        c = md.mix(c, VESSEL, 0.35 * vessel)
        c = md.mix(c, BLOOD, 0.6 * pooled)
        c = md.mix(c, BLOOD, 0.8 * smear)
        c = md.mix(c, CLOT, 0.7 * smear * clot)
        w = white * (1 + 0.25 * trough + 0.06 * fine)[..., None]
        w = md.mix(w, floor, 0.5 * tpits)
        w = md.mix(w, BLOOD, 0.8 * np.maximum(tblood, 0.7 * tpits))
        w = md.mix(w, CLOT, 0.6 * tblood * clot)
        c = np.where(torn[..., None], w, c)
        out.append(np.clip(c * DARKEN, 0, 1))
    return out, h


def build(spec):
    """A model: (name, mesh, its 8-bit skins (bytes), the full-colour skins, the relief ((R, R) units), texels a unit,
    vertices, triangles)."""
    name, seed, size, torn_n, share = spec
    P, T, torn, n = brain_shape(seed, size, torn_n, share)
    mesh, tris_st, density = build_mesh(P, T, torn, n)
    Pt, C = texel_points(tris_st, RELIEF)
    torn_d = float((P[torn] @ n).mean())
    skins, h = brain_paint(Pt, C, seed, n, torn_d)
    full = [np.round(md.down(s, RELIEF // HIRES) * 255).astype(np.uint8) for s in skins]
    own = [md.to_palette(s, SKIN, PALETTE_IDX).tobytes() for s in skins]
    return name, mesh, own, full, h, density, P, T


_relief = {}


def relief(name):
    """The relief of model `name` (gib_brain1.mdl...) for its normal map (normaltiles.py): (heights (R, R) in units,
    its own skin's texels a unit, RELIEF)."""
    if name not in _relief:
        m = build(next(s for s in BRAINS if s[0] == name))
        _relief[name] = (m[4], m[5], RELIEF)
    return _relief[name]


def preview(models, path):
    """A contact sheet: each model's skins, and its shape lit from above-left (its triangles, flat)."""
    from PIL import Image, ImageDraw
    W = 2 * (2 * SKIN + 4) + 2 * 260
    img = Image.new("RGB", (W, len(models) * (2 * SKIN + 18)), (32, 32, 32))
    d = ImageDraw.Draw(img)
    light = np.array((0.3, -0.5, 0.8))
    light /= np.linalg.norm(light)
    for r, (name, mesh, own, full, h, density, P, T) in enumerate(models):
        y = r * (2 * SKIN + 18)
        d.text((2, y + 2), name, fill=(255, 255, 0))
        for k, s in enumerate(full):
            img.paste(Image.fromarray(s), (k * (2 * SKIN + 4), y + 14))
        fn = tri_normals(P, T)
        scale = 110 / np.abs(P).max()
        for view, ox in ((0, 2 * (2 * SKIN + 4)), (1, 2 * (2 * SKIN + 4) + 260)):
            ax = 2 if view == 0 else 1
            order = np.argsort(P[T].mean(axis=1)[:, ax] * (1 if view == 0 else -1))
            for ti in order:
                if (fn[ti, 2] <= 0) if view == 0 else (fn[ti, 1] >= 0):
                    continue
                pts = [(ox + 130 + p[0] * scale, y + 14 + 128 - (p[1] if view == 0 else p[2]) * scale)
                       for p in P[T[ti]]]
                shade = int(60 + 170 * max(0.0, float(fn[ti] @ light)))
                d.polygon(pts, fill=(shade, int(shade * 0.75), int(shade * 0.78)))
    img.save(path)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    args = sys.argv[1:]
    prev = None
    if "--preview" in args:
        i = args.index("--preview")
        prev = args[i + 1]
        del args[i:i + 2]
    game = args[0] if args else os.path.join(here, "..", "..", "quakevr")
    models = [build(s) for s in BRAINS]
    paths = [os.path.join(game, "progs", m[0]) for m in models]
    extra = [os.path.join(game, "progs", "%s_%d.png" % (m[0], k)) for m in models for k in range(len(m[3]))]
    guard = genguard.Guard("make_brains.py", paths + extra)
    from PIL import Image
    for (name, mesh, own, full, h, density, P, T), path in zip(models, paths):
        if path not in guard.kept:
            mdlgen.write_mdl(path, mesh, own, name.split(".")[0])
        for k, s in enumerate(full):
            out = "%s_%d.png" % (path, k)
            if out not in guard.kept:
                Image.fromarray(s).save(out, optimize=True)
        size = (P.max(axis=0) - P.min(axis=0)) / UNITS * 100
        print("%s: %d vertices, %d triangles; %.1f x %.1f x %.1f cm; %.1f texels/unit (%d x %d; full colour %d x %d)"
              % (
            name, len(mesh.verts), len(mesh.tris), size[0], size[1], size[2], density, SKIN, SKIN, SKIN * HIRES,
            SKIN * HIRES))
    guard.finish()
    if prev:
        preview(models, prev)
        print("preview -> " + prev)


if __name__ == "__main__":
    main()
