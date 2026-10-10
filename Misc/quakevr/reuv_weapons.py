#!/usr/bin/env python3
# reuv_weapons.py -- the stretched skins of the super nailgun, rocket launcher, grappling hook and Mjolnir re-mapped and
# repainted from their own paint, as reuv_shot2.py did the double shotgun's fore-end (the art polishing pass of
# 2026-10-10, "For the author" 1: check_mdl_art.py found 21-42% of their surfaces stretched over 3:1, the grappling
# hook's front cap folded onto a line). The plasma gun has the thunderbolt's mesh: refine_light.py does its sides,
# this the rest.
#
# Usage: python Misc/quakevr/reuv_weapons.py [model ...]            (default: every model in SPECS, in quakevr/progs;
#                                                                    rewrites them in place)
#        python Misc/quakevr/reuv_weapons.py --report [model ...]   (what it would re-map, writes nothing)
# polish_weapons.py runs it (POST) on the models it writes, so rerunning that still gives the shipped files; after
# it, bake their normal maps again (bake_normals.py <model>). check_mdl_art.py --stretch measures before and after.
#
# What is re-mapped. The seeds: every face of real size (over MIN_AREA square units, frame 0) whose texels are
# stretched over RATIO:1 (or folded onto a line or a point) over paint that is not one colour (SPREAD: a swatch looks
# the same however it is mapped). Each seed takes in the faces reached over welded edges within GROW_ANGLE of it that
# are stretched over JOIN_RATIO too (its panel). The panels are cut into charts (within CHART_ANGLE of the chart's
# first face and NEIGHBOUR_ANGLE of the face it is reached from); charts an old strip runs between are joined, and take
# the rest of the strips they touch (see "Anchors" below); a chart that does not unwrap flat (over 2:1) is left. Each
# is unwrapped (least squares conformal: reuv_shot2.lscm) at the model's density (the area-weighted median of its
# faces under 2:1) or its old density along its sharpest direction where higher (at most 3 times the model's: a fine
# pattern resampled coarser aliases into noise), and laid in skin texels nothing else uses (the old texels of the
# re-mapped faces included), the skin growing by 8 rows whenever one does not fit.
#
# How it is painted: from the old skin. Each new texel shows a surface point; its colour is that point's through the
# old mapping, read from the skin before polish_weapons.py's edge wear (src_models/r21: the wear is painted again at
# the new density), bilinear with a sharpened blend (SHARPEN), never from the background round an old island: the
# paint, its colours and its shading stay, and gain texels. A panel whose faces were mostly folded onto a line
# (PAINT_RATIO) with the streaks across it (ALONG_ANGLE; streaks along a part are id's shading of it, and stay), or
# one a spec names ("flat_in"), takes its old texels' mean colour instead. On both: fine grain along the panel and a
# few scratches, the convex edges' first texel row lit and the next worn (mdlpolish.edge_wear's look); each texel is
# the nearest of the colours the old paint used there (the palette rows of the texels read, and their neighbours),
# ordered-dithered towards the next one of its row. A fullbright texel stays itself.
#
# What stays: every vertex position and normal (every frame's bytes), every triangle and its order, the header
# (scale, origin), the skin's width. A vertex all of whose faces are in one chart takes its new skin coordinate
# itself; one shared with other faces is copied (its bytes in every frame) for the chart's corners.
#
# Anchors: vr_anchor.cpp names a weapon's anchor vertices by their place in QuakeSpasm's old strip order of the
# triangles. Strips join faces by vertex index, so a chart's edge across a strip (its corners copied) would end the
# strip there and shift every anchor after it: the charts are joined along the old strips, and each is kept only if
# every anchor still names a vertex at the same place in every pose (a copy is at its vertex's place).

import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import mdlpolish as mp  # noqa: E402
from check_mdl_art import load_mdl, stretch, stretch_summary  # noqa: E402
from improve_weapons import BAYER4, Noise, strip_order  # noqa: E402
from refine_laserg import barycentric, tri_n, unwrap  # noqa: E402
from reuv_shot2 import ORIENT, grow, orient, raster, seg_dist  # noqa: E402

PROGS = os.path.join(HERE, "..", "..", "quakevr", "progs")
SRC = os.path.join(HERE, "src_models", "r21")  # polish_weapons.py's inputs: the skins before its edge wear
PAL = mp.palette()

RATIO = 3.0            # a seed: texels stretched over this (the singular values' ratio of the map to the skin)
SPREAD = 12.0          # ...over texels whose brightness varies by more than this (0-255): a face over one colour
                       # (polish_weapons.py's swatches: bands, bolt heads) looks the same however it is mapped
MIN_AREA = 0.5         # square model units: smaller faces are left (a few texels either way)
GROW_ANGLE = 20.0      # degrees: a seed takes in the faces this flat with it (its panel)
CHART_ANGLE = 50.0     # degrees: a chart's faces within this of its first face...
NEIGHBOUR_ANGLE = 35.0  # ...and of the face they are reached from
JOIN_RATIO = 2.0       # a panel takes in a face only if it is stretched over this too (a well-mapped one stays)
PAINT_RATIO = 15.0     # a panel mostly stretched over this (all but folded onto a line)...
ALONG_ANGLE = 25.0     # ...its streaks further than this from its length, is painted from its old mean colour
DENSITY_RANGE = (3.0, 8.0)  # texels per model unit: the auto density's bounds
SHARPEN = 2.5          # the old skin's bilinear blend, this many times steeper (1: plain bilinear)

# Per model: overrides of the above (lower case), "only" / "keep": boxes ((x0, y0, z0), (x1, y1, z1), model space,
# frame 0) a face's centre must lie in / must not lie in, "facing": (direction, least cosine) a face must face,
# "flat_in": (boxes, (direction, least cosine)): the panels centred in them and facing so are painted from their mean
# colour, all others from the old paint; "faces_before": only the faces before this one.
SPECS = {
    # After refine_light.py (its new sides, from triangle 711 on, are its own; its old ones are collapsed): the rest.
    # The ring of coil turns under the muzzle (its streaks across it are the turns) kept.
    "v_plasma.mdl": {"faces_before": 711, "keep": [((20.0, -2.5, 6.5), (36.0, 2.5, 11.0))]},
    "v_nail2.mdl": {},
    "v_rock2.mdl": {},
    # The front cap (folded onto a line: painted anew, as the hub's plates in front of it), the claws from their
    # own streaked paint.
    "v_grpple.mdl": {"flat_in": ([((20.0, -5.5, 1.5), (27.5, 5.5, 8.5))], ((1.0, 0.0, 0.0), 0.9))},
    "v_hammer.mdl": {},
}


def opt(spec, name):
    return spec.get(name.lower(), globals()[name])


# ----------------------------------------------------------------------------
# The old mapping

def old_uv(m, i):
    """Triangle i's corners in texel space as the engine reads them (st + 0.5, the back's onseam half over)."""
    fr, a, b, c = m.tris[i]
    return np.array([(m.st[v][1] + (m.sw // 2 if fr == 0 and m.st[v][0] else 0) + 0.5, m.st[v][2] + 0.5)
                     for v in (a, b, c)], float)


def streak_dir(p, uv):
    """The direction (model space, unit) along which the triangle's texels are stretched most: its streaks'."""
    e1, e2 = p[1] - p[0], p[2] - p[0]
    x = e1 / max(np.linalg.norm(e1), 1e-12)
    y = np.cross(np.cross(e1, e2), x)
    y /= max(np.linalg.norm(y), 1e-12)
    M = np.array([[e1 @ x, e2 @ x], [e1 @ y, e2 @ y]])
    J = np.array([uv[1] - uv[0], uv[2] - uv[0]]).T @ np.linalg.inv(M)
    v = np.linalg.svd(J)[2][1]
    return v[0] * x + v[1] * y


def measures(m, P):
    """Per triangle: area (frame 0), old stretch ratio (99: folded), old density (texels per unit: the mean, the
    sharpest direction's), the old streaks' direction."""
    n = len(m.tris)
    area, ratio, dens, sharp = np.zeros(n), np.zeros(n), np.zeros(n), np.zeros(n)
    streak = np.zeros((n, 3))
    for i, t in enumerate(m.tris):
        p = P[list(t[1:])]
        area[i] = 0.5 * np.linalg.norm(np.cross(p[1] - p[0], p[2] - p[0]))
        if area[i] < 1e-7:
            continue
        q = old_uv(m, i)
        e1, e2 = q[1] - q[0], q[2] - q[0]
        uvarea = 0.5 * abs(e1[0] * e2[1] - e1[1] * e2[0])
        s1, s2 = stretch(p, q)
        ratio[i] = 99.0 if s2 <= 1e-6 or uvarea < 0.05 else min(99.0, s1 / s2)
        dens[i] = math.sqrt(uvarea / area[i])
        sharp[i] = s1
        streak[i] = streak_dir(p, q)
    return area, ratio, dens, sharp, streak


def inside(c, boxes):
    return any(all(lo[k] <= c[k] <= hi[k] for k in range(3)) for lo, hi in boxes)


def spread(m, i, skin):
    """How much the brightness of the texels under triangle i varies (its corners, edges and middle, sampled)."""
    q = old_uv(m, i)
    lum = []
    for a in np.linspace(0, 1, 7):
        for b in np.linspace(0, 1 - a, 7):
            u, v = q[0] + a * (q[1] - q[0]) + b * (q[2] - q[0])
            idx = skin[min(max(int(v), 0), m.sh - 1), min(max(int(u), 0), m.sw - 1)]
            lum.append(PAL[idx] @ np.array([0.299, 0.587, 0.114]))
    return max(lum) - min(lum)


def select(m, P, spec, area, ratio):
    """The faces to re-map, in panels: each seed (the largest first) and the faces reached from it over welded edges
    within GROW_ANGLE of it, not already in a panel. Returns ([panel faces], seeds)."""
    key = lambda v: tuple(np.round(P[v], 3))
    only, keep = spec.get("only"), spec.get("keep", [])
    cen = [P[list(t[1:])].mean(0) for t in m.tris]
    facing = spec.get("facing")
    last = spec.get("faces_before", len(m.tris))
    ok = [i < last and area[i] > 1e-6 and (not only or inside(cen[i], only)) and not inside(cen[i], keep)
          and (not facing or tri_n(P, m.tris[i])[0] @ np.array(facing[0]) >= facing[1])
          for i in range(len(m.tris))]
    skin = np.frombuffer(bytes(m.skin()), np.uint8).reshape(m.sh, m.sw)
    seeds = [i for i in range(len(m.tris)) if ok[i] and area[i] > opt(spec, "MIN_AREA") and ratio[i] > opt(spec, "RATIO")
             and spread(m, i, skin) > opt(spec, "SPREAD")]
    edges = {}
    for i, t in enumerate(m.tris):
        if ok[i]:
            vs = t[1:]
            for k in range(3):
                edges.setdefault(tuple(sorted((key(vs[k]), key(vs[(k + 1) % 3])))), []).append(i)
    nrm = {i: tri_n(P, m.tris[i])[0] for i in range(len(m.tris)) if ok[i]}
    cg = math.cos(math.radians(opt(spec, "GROW_ANGLE")))
    group_of = {}
    groups = []
    for s0 in sorted(seeds, key=lambda i: -area[i]):
        if s0 in group_of:
            continue
        g = [s0]
        group_of[s0] = len(groups)
        todo = [s0]
        while todo:
            f = todo.pop()
            vs = m.tris[f][1:]
            for k in range(3):
                for h in edges[tuple(sorted((key(vs[k]), key(vs[(k + 1) % 3]))))]:
                    if h not in group_of and nrm[h] @ nrm[s0] > cg and ratio[h] > opt(spec, "JOIN_RATIO"):
                        group_of[h] = len(groups)
                        g.append(h)
                        todo.append(h)
        groups.append(sorted(g))
    return groups, seeds


def make_charts(P, m, tris, spec):
    """refine_laserg.make_charts with this model's angles."""
    key = lambda v: tuple(np.round(P[v], 3))
    info = {i: tri_n(P, m.tris[i]) for i in tris}
    edges = {}
    for i in tris:
        vs = m.tris[i][1:]
        for k in range(3):
            edges.setdefault(tuple(sorted((key(vs[k]), key(vs[(k + 1) % 3])))), []).append(i)
    left = set(tris)
    charts = []
    c1 = math.cos(math.radians(opt(spec, "CHART_ANGLE")))
    c2 = math.cos(math.radians(opt(spec, "NEIGHBOUR_ANGLE")))
    while left:
        seed = max(sorted(left), key=lambda i: info[i][1])
        chart, todo = [seed], [seed]
        left.discard(seed)
        while todo:
            f = todo.pop()
            vs = m.tris[f][1:]
            for k in range(3):
                for g in edges[tuple(sorted((key(vs[k]), key(vs[(k + 1) % 3]))))]:
                    if g in left and info[g][0] @ info[seed][0] > c1 and info[g][0] @ info[f][0] > c2:
                        left.discard(g)
                        chart.append(g)
                        todo.append(g)
        charts.append(sorted(chart))
    return charts


# ----------------------------------------------------------------------------
# Packing

def used_mask(m, skip, H):
    """The texels the faces not re-mapped read (and the engine's background texel at (0, 0)), grown by one."""
    used = np.zeros((H, m.sw), bool)
    for i in range(len(m.tris)):
        if i in skip:
            continue
        q = old_uv(m, i)
        for (tt, s) in raster([q], m.sw, H):
            used[tt, s] = True
        for s, tt in q.astype(int):
            used[min(max(tt, 0), H - 1), min(max(s, 0), m.sw - 1)] = True
    used[0:2, 0:2] = True
    return grow(used, 1)


def snap(net, density, k, f):
    keys = list(net)
    q = orient(np.array([net[x] for x in keys]) * density, k, f)
    q = np.round(q - q.min(0)).astype(int) + 1
    return dict(zip(keys, q))


def chart_mask(P, m, chart, st):
    key = lambda v: tuple(np.round(P[v], 3))
    qs = [np.array([st[key(v)] for v in m.tris[i][1:]], float) + 0.5 for i in chart]
    hi = np.max([q.max(0) for q in qs], axis=0)
    mask = np.zeros((int(hi[1]) + 3, int(hi[0]) + 3), bool)
    for (t, s) in raster(qs, mask.shape[1], mask.shape[0]):
        mask[t, s] = True
    for q in qs:  # thin triangles: their corners and edge midpoints
        for a in range(3):
            for p in (q[a], 0.5 * (q[a] + q[(a + 1) % 3])):
                mask[int(p[1]), int(p[0])] = True
    return grow(mask, 2)  # the painted margin (chart_texels)


def top_left(used, mask):
    """reuv_shot2.place's rule (top-most, then left-most) on refine_laserg.place's search."""
    H, W = used.shape
    h, w = mask.shape
    if h > H or w > W:
        return None
    F = np.fft.rfft2(used.astype(float), s=(H + h, W + w))
    G = np.fft.rfft2(mask[::-1, ::-1].astype(float), s=(H + h, W + w))
    corr = np.fft.irfft2(F * G, s=(H + h, W + w))[h - 1:H, w - 1:W]
    ok = np.argwhere(corr < 0.5)
    if not len(ok):
        return None
    t, s = ok[np.lexsort((ok[:, 1], ok[:, 0]))[0]]
    return int(t), int(s)


def solid(P, m, faces):
    """The faces with an area (those unwrapped and painted; a flat one only follows its chart's coordinates)."""
    return [i for i in faces if tri_n(P, m.tris[i])[1] > 1e-6]


def layout(m, P, charts, skip):
    """charts: [(faces, density)]. Unwraps them and packs them (largest first) in the free texels, top-most then
    left-most, the skin growing by 8 rows whenever one does not fit. Returns the skin height and
    [(faces, density, {welded key: (s, t)})]."""
    nets = [(c, d, unwrap(P, m, solid(P, m, c))[0]) for c, d in charts]
    nets.sort(key=lambda x: -sum(tri_n(P, m.tris[i])[1] for i in x[0]) * x[1] ** 2)
    H = m.sh
    used = used_mask(m, skip, H)
    placed = []
    for chart, density, net in nets:
        while True:
            best = None
            for k, f in ORIENT:
                st = snap(net, density, k, f)
                mask = chart_mask(P, m, solid(P, m, chart), st)
                at = top_left(used, mask)
                if at and (best is None or (at[0] + mask.shape[0], at[1]) < best[0]):
                    best = ((at[0] + mask.shape[0], at[1]), at, st, mask)
            if best is not None:
                break
            if H >= 1024:
                raise SystemExit("reuv_weapons: no room in the skin")
            H += 8
            used = np.vstack([used, np.zeros((8, m.sw), bool)])
        _, (t0, s0), st, mask = best
        used[t0:t0 + mask.shape[0], s0:s0 + mask.shape[1]] |= mask
        placed.append((chart, density, {x: (int(q[0]) + s0, int(q[1]) + t0) for x, q in st.items()}))
    return H, placed


# ----------------------------------------------------------------------------
# Painting

def convex_edges(P, m):
    """The model's convex corners (over 30 degrees between two faces, frame 0) as segments."""
    key = lambda v: tuple(np.round(P[v], 3))
    edges = {}
    for t in m.tris:
        n, a2 = tri_n(P, t)
        if a2 < 1e-6:
            continue
        vs = t[1:]
        for k in range(3):
            x, y, z = vs[k], vs[(k + 1) % 3], vs[(k + 2) % 3]
            edges.setdefault(tuple(sorted((key(x), key(y)))), []).append((n, P[z], P[x]))
    out = []
    for k, users in edges.items():
        if len(users) == 2:
            (n1, o1, p1), (n2, o2, p2) = users
            if n1 @ n2 < math.cos(math.radians(30)) and (o2 - p1) @ n1 < -1e-4:
                out.append((np.array(k[0]), np.array(k[1])))
    return out


class OldSkin:
    """The skin the old mapping reads: bilinear colour and the nearest texel's index."""

    def __init__(self, img, sw, sh, covered=None):
        self.idx = np.frombuffer(bytes(img), np.uint8).reshape(sh, sw)[:sh]
        self.rgb = PAL[self.idx]
        self.w, self.h = sw, sh
        self.covered = covered[:sh] if covered is not None else np.ones((sh, sw), bool)

    def nearest(self, u, v):
        return int(self.idx[min(max(int(v), 0), self.h - 1), min(max(int(u), 0), self.w - 1)])

    def colour(self, u, v):
        """Bilinear, its blend between texels sharpened (SHARPEN times as steep about the half): a well-mapped
        picture keeps its texels' edges, a stretched one runs smoothly along its stretch."""
        x, y = u - 0.5, v - 0.5
        x0, y0 = math.floor(x), math.floor(y)
        fx = min(1.0, max(0.0, (x - x0 - 0.5) * SHARPEN + 0.5))
        fy = min(1.0, max(0.0, (y - y0 - 0.5) * SHARPEN + 0.5))
        c = np.zeros(3)
        wsum = 0.0
        rows = set()
        for dy, wy in ((0, 1 - fy), (1, fy)):
            for dx, wx in ((0, 1 - fx), (1, fx)):
                yy, xx = min(max(y0 + dy, 0), self.h - 1), min(max(x0 + dx, 0), self.w - 1)
                if wx * wy > 0 and self.covered[yy, xx]:  # never the background round an old island
                    c += wx * wy * self.rgb[yy, xx]
                    wsum += wx * wy
                    rows.add(int(self.idx[yy, xx]) // 16)
        if wsum <= 0.0:
            i = self.nearest(u, v)
            return self.rgb[min(max(int(v), 0), self.h - 1), min(max(int(u), 0), self.w - 1)], frozenset([i // 16])
        return c / wsum, frozenset(rows)


def candidates(indices):
    """The colours a chart may take: those its old paint used and their palette rows' neighbours (no fullbright)."""
    out = set()
    for i in indices:
        if i >= mp.FULLBRIGHT:
            continue
        for d in (-1, 0, 1):
            j = i + d
            if 0 <= j < mp.FULLBRIGHT and j // 16 == i // 16:
                out.add(j)
    out = sorted(out)
    return np.array(out), PAL[out]


def quantize(rgb, cidx, crgb, s, t, rows=None):
    """The nearest candidate (of the palette rows given: the ramps the old paint used there), or the next nearest of
    its row on the line through it towards rgb, by a 4x4 ordered dither (no specks of another colour)."""
    if rows:
        keep = np.isin(cidx // 16, list(rows))
        if keep.any():
            cidx, crgb = cidx[keep], crgb[keep]
    d = ((crgb - rgb) ** 2).sum(1)
    i = int(np.argmin(d))
    c1 = crgb[i]
    seg = crgb - c1
    L = (seg ** 2).sum(1)
    L[(L < 1e-9) | (cidx // 16 != cidx[i] // 16)] = 1e18  # itself, the palette's repeated colours, other rows
    f = np.clip(((rgb - c1) @ seg.T) / L, 0.0, 1.0)
    # The other end: the nearest colour beyond rgb (seen from c1), on the line well enough.
    err = (((c1 + f[:, None] * seg) - rgb) ** 2).sum(1)
    err = np.where((f <= 0.0) | (f >= 1.0) | (err > 0.25 * d.min() + 4.0), 1e18, d)
    j = int(np.argmin(err))
    if err[j] < 1e17 and f[j] > (BAYER4[t % 4][s % 4] + 0.5) / 16.0:
        return int(cidx[j])
    return int(cidx[i])


def chart_texels(P, m, chart, st, sw, H):
    """{texel (t, s): (k, barycentrics)} of a chart's faces: those they cover, the ones under thin faces' corners
    and edges, and a two-texel ring round them (extrapolated from a face that touches it)."""
    key = lambda v: tuple(np.round(P[v], 3))
    qs = [np.array([st[key(v)] for v in m.tris[i][1:]], float) + 0.5 for i in chart]
    own = raster(qs, sw, H)
    for k, q in enumerate(qs):
        for a in range(3):
            for b in (0.0, 0.25, 0.5, 0.75):
                p = q[a] + b * (q[(a + 1) % 3] - q[a])
                c = (int(p[1]), int(p[0]))
                if c not in own:
                    w = [0.0, 0.0, 0.0]
                    w[a], w[(a + 1) % 3] = 1.0 - b, b
                    own[c] = (k, tuple(w))
    for _ in range(2):  # a margin two texels wide: the filtering and mip ring (check_mdl_art.py "bleed")
        ring = {}
        for (t, s), (k, _) in own.items():
            for dt in (-1, 0, 1):
                for ds in (-1, 0, 1):
                    n = (t + dt, s + ds)
                    if n not in own and n not in ring and 0 <= n[0] < H and 0 <= n[1] < sw:
                        ring[n] = (k, barycentric(qs[k], n[1] + 0.5, n[0] + 0.5))
        own.update(ring)
    return own


def paint_panel(skin, sw, H, m, P, charts, sources, edges, ratio_of, streak_of, noise, flat_in=None):
    """Paints a panel's charts [(faces, density, st)] from the old skins (sources(i, barycentrics, point): (OldSkin,
    texel coordinates)). flat_in: (boxes, (direction, least cosine)) or None (see SPECS). Returns (texels, painted
    from the mean)."""
    faces = [i for c, _, _ in charts for i in c]
    pts = np.array([P[v] for i in faces for v in m.tris[i][1:]])
    c0 = pts.mean(0)
    axis = np.linalg.svd(pts - c0)[2]  # the panel's length first
    # Painted from its mean colour when most of it was streaked across its length (or diagonally): streaks along
    # it are id's way of shading a long part, and are kept.
    area = sum(tri_n(P, m.tris[i])[1] for i in faces)
    across = sum(tri_n(P, m.tris[i])[1] for i in faces
                 if ratio_of[i] > PAINT_RATIO and abs(streak_of[i] @ axis[0]) < math.cos(math.radians(ALONG_ANGLE)))
    if flat_in is None:
        flat = across > 0.5 * area
    else:
        flat = inside(c0, flat_in[0]) and tri_n(P, m.tris[faces[0]])[0] @ np.array(flat_in[1][0]) >= flat_in[1][1]
    samples = []
    for chart, density, st in charts:
        for (t, s), (k, b) in chart_texels(P, m, chart, st, sw, H).items():
            i = chart[k]
            tri = m.tris[i][1:]
            p = sum(b[j] * P[tri[j]] for j in range(3))
            old, uv = sources(i, b, p)
            rgb, rows = old.colour(*uv)
            samples.append((t, s, p, density, rgb, old.nearest(*uv), rows))
    cidx, crgb = candidates({x[5] for x in samples})
    mean = np.mean([x[4] for x in samples if x[5] < mp.FULLBRIGHT] or [x[4] for x in samples], axis=0)
    # A panel painted from its mean colour keeps to the ramps most of its old texels were in.
    count = {}
    for x in samples:
        count[x[5] // 16] = count.get(x[5] // 16, 0) + 1
    main_rows, acc = set(), 0
    for r, n in sorted(count.items(), key=lambda kv: -kv[1]):
        if acc >= 0.8 * len(samples):
            break
        main_rows.add(r)
        acc += n
    density = max(d for _, d, _ in charts)
    lo, hi = pts.min(0) - 2.5 / density, pts.max(0) + 2.5 / density
    near = [(a, b) for a, b in edges if (np.minimum(a, b) <= hi).all() and (np.maximum(a, b) >= lo).all()]
    for t, s, p, density, rgb, near_i, rows in samples:
        if near_i >= mp.FULLBRIGHT and not flat:
            skin[t * sw + s] = near_i
            continue
        a, b_ = p @ axis[0], p @ axis[1] + p @ axis[2]
        g = 0.7 * noise.smooth(a * 0.35, b_ * 2.0) + 0.3 * noise.smooth(a * 1.2, b_ * 5.0)
        f = 1.0 + (0.24 if flat else 0.10) * (g - 0.5) + (0.04 * (noise.hash(s, t) - 0.5) if flat else 0.0)
        if noise.hash(int(a * 1.5), int(b_ * density)) > 0.985:
            f += 0.15  # a scratch
        d = min((seg_dist(p, e0, e1) for e0, e1 in near), default=99.0) * density
        if d < 1.0:
            f += 0.25
        elif d < 2.0 and noise.hash(t * 7 + 3, s * 5 + 1) > 0.55:
            f += 0.1
        skin[t * sw + s] = quantize(np.clip((mean if flat else rgb) * f, 0, 255), cidx, crgb, s, t,
                                    main_rows if flat else rows)
    return len(samples), flat


# ----------------------------------------------------------------------------
# Anchors (see the top; polish_weapons.slot_anchors: hand, muzzle, two-handed grip, ammo screen, button, magazine)

def strip_groups(tris):
    """The triangles of each strip or fan of QuakeSpasm's old order (improve_weapons.strip_order's search)."""
    n = len(tris)
    used = [0] * n

    def length(start, sv, strip):
        used[start] = 2
        last = tris[start]
        vi = last[1:]
        got = [start]
        m1, m2 = (vi[(sv + 2) % 3], vi[(sv + 1) % 3]) if strip else (vi[sv % 3], vi[(sv + 2) % 3])
        extended = True
        while extended and len(got) < 126:
            extended = False
            for j in range(start + 1, n):
                check = tris[j]
                if check[0] != last[0]:
                    continue
                cv = check[1:]
                hit = next((k for k in range(3) if cv[k] == m1 and cv[(k + 1) % 3] == m2), None)
                if hit is None:
                    continue
                if used[j]:
                    break
                nv = cv[(hit + 2) % 3]
                if strip:
                    if len(got) & 1:
                        m2 = nv
                    else:
                        m1 = nv
                else:
                    m2 = nv
                got.append(j)
                used[j] = 2
                extended = True
                break
            else:
                continue
            if not extended:
                break
        for j in range(start + 1, n):
            if used[j] == 2:
                used[j] = 0
        return got

    groups = []
    for i in range(n):
        if used[i]:
            continue
        best = None
        for strip in (False, True):
            for sv in range(3):
                t = length(i, sv, strip)
                if best is None or len(t) > len(best):
                    best = t
        for j in best:
            used[j] = 1
        groups.append(best)
    return groups


def merge_by_strips(m, P, charts):
    """Charts joined where an old strip runs from one to another, and given the rest of each strip they touch: a
    chart's edge then never cuts a strip (its corners' copies would end it there, and the strips after it shift
    every anchor). [(faces, density)]."""
    root = list(range(len(charts)))

    def find(k):
        while root[k] != k:
            root[k] = root[root[k]]
            k = root[k]
        return k

    chart_of = {i: k for k, (c, _) in enumerate(charts) for i in c}
    extra = {}
    for g in strip_groups(m.tris):
        ks = sorted({chart_of[i] for i in g if i in chart_of})
        if not ks:
            continue
        for k in ks[1:]:
            root[find(k)] = find(ks[0])
        extra.setdefault(ks[0], []).extend(i for i in g if i not in chart_of)
    out = {}
    for k, (c, d) in enumerate(charts):
        r = find(k)
        faces, wsum, asum = out.get(r, ([], 0.0, 0.0))
        a = sum(tri_n(P, m.tris[i])[1] for i in c)
        out[r] = (faces + list(c) + extra.get(k, []), wsum + d * a, asum + a)
    return [(sorted(set(f)), w / max(a, 1e-9)) for f, w, a in out.values()]  # the area-weighted density


def unwrap_ok(P, m, chart, worst=2.0):
    """The chart's faces unwrap flat with their texels at most `worst`:1 stretched (area-weighted), every corner on
    the net (a flat face's too)."""
    faces = solid(P, m, chart)
    if not faces:
        return False
    net, _ = unwrap(P, m, faces)
    key = lambda v: tuple(np.round(P[v], 3))
    if any(key(v) not in net for i in chart for v in m.tris[i][1:]):
        return False
    a_sum = r_sum = 0.0
    for i in faces:
        p = P[list(m.tris[i][1:])]
        q = np.array([net[key(v)] for v in m.tris[i][1:]])
        s1, s2 = stretch(p, q)
        if not np.isfinite(s1) or s2 <= 1e-9:
            return False
        a = tri_n(P, m.tris[i])[1]
        a_sum += a
        r_sum += min(s1 / s2, 20.0) * a
    return r_sum / a_sum <= worst


def refs(m, charts):
    """The triangles with each re-mapped corner's vertex as fix() will give it: itself when all its faces are in
    its chart, else a copy (named (vertex, chart)); and the vertex each name stands for."""
    chart_of = {i: k for k, c in enumerate(charts) for i in c}
    users = {}
    for i, t in enumerate(m.tris):
        for v in t[1:]:
            users.setdefault(v, set()).add(chart_of.get(i))
    names, src = {}, list(range(len(m.st)))
    tris = list(m.tris)
    for i, k in chart_of.items():
        fr, *vs = m.tris[i]
        out = []
        for v in vs:
            if users[v] == {k}:
                out.append(v)
            else:
                if (v, k) not in names:
                    names[(v, k)] = len(src)
                    src.append(v)
                out.append(names[(v, k)])
        tris[i] = (fr,) + tuple(out)
    return tris, src


def anchor_places(m, tris, src, anchors, poses):
    order = strip_order(tris)
    return {a: tuple(bytes(p[src[order[a]], :3]) for p in poses) if a < len(order) else None for a in anchors}


def keep_anchors(m, charts, area, anchors):
    """The charts (largest first) that can be re-mapped with every anchor in place; and those that cannot."""
    if not anchors:
        return charts, []
    poses = [np.frombuffer(bytes(m.pose_bytes(p)), np.uint8).reshape(-1, 4) for p in range(m.num_poses())]
    base = anchor_places(m, m.tris, list(range(len(m.st))), anchors, poses)
    kept, dropped = [], []
    for c, d in sorted(charts, key=lambda x: -sum(area[i] for i in x[0])):
        tris, src = refs(m, [k for k, _ in kept] + [c])
        if anchor_places(m, tris, src, anchors, poses) == base:
            kept.append((c, d))
        else:
            dropped.append(c)
    return kept, dropped


# ----------------------------------------------------------------------------

def auto_density(area, ratio, dens, chosen):
    good = [(dens[i], area[i]) for i in range(len(area)) if i not in chosen and area[i] > 1e-6 and 0 < ratio[i] < 2.0]
    good.sort()
    tot = sum(a for _, a in good)
    acc = 0.0
    for d, a in good:
        acc += a
        if acc >= tot / 2:
            return float(np.clip(d, *DENSITY_RANGE))
    return DENSITY_RANGE[0]


def group_report(label, m, P, tris, uv_of):
    a_sum = d_sum = r_sum = 0.0
    for i in tris:
        p = P[list(m.tris[i][1:])]
        a3 = 0.5 * np.linalg.norm(np.cross(p[1] - p[0], p[2] - p[0]))
        if a3 < 1e-7:
            continue
        q = uv_of(i)
        e1, e2 = q[1] - q[0], q[2] - q[0]
        uva = 0.5 * abs(e1[0] * e2[1] - e1[1] * e2[0])
        s1, s2 = stretch(p, q)
        a_sum += a3
        d_sum += math.sqrt(uva / a3) * a3
        r_sum += (20.0 if s2 <= 1e-6 or uva < 0.05 else min(20.0, s1 / s2)) * a3
    print("    %-7s %4d faces, %6.1f sq units: %.2f texels/unit, stretch %.2f:1 (area-weighted, capped at 20)" % (
        label, len(tris), a_sum, d_sum / max(a_sum, 1e-9), r_sum / max(a_sum, 1e-9)))


def fix(path, report_only=False, src_dir=SRC):
    name = os.path.basename(path)
    spec = SPECS[name]
    m = mp.Model(path)
    P = m.positions(0)
    area, ratio, dens, sharp, streak = measures(m, P)
    # The source model (polish_weapons.py's input): its faces (the later ones are polish_weapons.py's own parts).
    src_path = os.path.join(src_dir, name)
    src_model = mp.Model(src_path) if os.path.exists(src_path) else None
    if src_model and not (src_model.sw == m.sw and m.tris[:len(src_model.tris)] == src_model.tris
                          and all(m.st[v] == src_model.st[v] for v in range(len(src_model.st)))):
        src_model = None
    olds = {i: old_uv(m, i) for i in range(len(m.tris))}
    # The skins the old mapping is read from: the one before the edge wear for the source's own faces.
    cover = np.zeros((m.sh, m.sw), bool)
    for i in range(len(m.tris)):
        q = olds[i]
        for (tt, ss) in raster([q], m.sw, m.sh):
            cover[tt, ss] = True
        for ss, tt in q.astype(int):
            cover[min(max(tt, 0), m.sh - 1), min(max(ss, 0), m.sw - 1)] = True
    cur = OldSkin(bytes(m.skin()), m.sw, m.sh, cover)
    src, src_nt = cur, 0
    if src_model:
        src = OldSkin(bytes(src_model.skin()), src_model.sw, src_model.sh, cover)
        src_nt = len(src_model.tris)

    def read(i, b, p):
        bc = np.clip(np.array(b, float), 0.0, None)  # the ring's texels: read at the face's edge
        bc /= bc.sum()
        return (src if i < src_nt else cur), sum(bc[j] * olds[i][j] for j in range(3))

    sources = read
    panels, seeds = select(m, P, spec, area, ratio)
    chosen = sorted(i for p in panels for i in p)
    density = spec.get("density") or auto_density(area, ratio, dens, set(chosen))
    print("  %s re-mapped: %d seeds, %d panels, %d faces, at %.2f texels/unit" % (
        name, len(seeds), len(panels), len(chosen), density))
    group_report("before", m, P, chosen, lambda i: olds[i])
    if report_only:
        return
    edges = convex_edges(P, m)

    # Each chart at the model's density, or at its old density along its sharpest direction where that was higher
    # (no texel of the old picture lost: a fine pattern resampled coarser aliases into noise), at most 3 times it.
    charts = []
    for k, panel in enumerate(panels):
        for c in make_charts(P, m, panel, spec):
            a = sum(area[i] for i in c)
            old_d = sum(min(sharp[i], 3 * density) * area[i] for i in c) / max(a, 1e-9)
            charts.append((c, float(np.clip(old_d, density, 3.0 * density))))
    face_panel = {i: k for k, panel in enumerate(panels) for i in panel}
    charts = merge_by_strips(m, P, charts)
    good, bent = [], []
    for c, d in charts:
        (good if unwrap_ok(P, m, c) else bent).append((c, d))
    charts = good
    if bent:
        print("    %d charts (%d faces) left as they were: no flat unwrap of them (their strips joined too much)" % (
            len(bent), sum(len(c) for c, _ in bent)))
    for c, _ in charts:  # a strip's faces taken in: painted with the chart's panel
        home = next(face_panel[i] for i in c if i in face_panel)
        for i in c:
            face_panel.setdefault(i, home)
    import polish_weapons  # (here: it imports this module)
    charts, dropped = keep_anchors(m, charts, area, sorted(polish_weapons.slot_anchors().get(name, ())))
    chosen = sorted(i for c, _ in charts for i in c)
    if dropped:
        print("    %d charts (%d faces) left as they were: re-mapping them would move a weapon anchor" % (
            len(dropped), sum(len(c) for c in dropped)))
    H, placed = layout(m, P, charts, set(chosen))
    if H > m.sh:
        m.grow_skin(H - m.sh)
    skin = m.skin()
    painted = flat = 0
    noise = Noise(spec.get("seed", 31))
    for k in range(len(panels)):
        mine = [([i for i in c if face_panel[i] == k], d, st) for c, d, st in placed]
        mine = [(solid(P, m, c), d, st) for c, d, st in mine if solid(P, m, c)]
        if not mine:
            continue
        n, is_flat = paint_panel(skin, m.sw, m.sh, m, P, mine, sources,
                                 edges, ratio, streak, noise, spec.get("flat_in"))
        painted += n
        flat += is_flat

    # The corners: a vertex whose faces are all in one chart takes the new coordinate; others are copied.
    key = lambda v: tuple(np.round(P[v], 3))
    chart_of = {}
    for k, (chart, _, _) in enumerate(placed):
        for i in chart:
            chart_of[i] = k
    users = {}
    for i, t in enumerate(m.tris):
        for v in t[1:]:
            users.setdefault(v, set()).add(chart_of.get(i))
    poses = [np.frombuffer(bytes(m.pose_bytes(p)), np.uint8).reshape(-1, 4) for p in range(m.num_poses())]
    table = [np.array(a) for a in mp.anorms()]
    copies = {}
    moved = 0
    tris = list(m.tris)
    for k, (chart, _, st) in enumerate(placed):
        for i in chart:
            fr, *vs = m.tris[i]
            out = []
            for v in vs:
                s, t = st[key(v)]
                if users[v] == {k}:
                    if m.st[v] != [0, s, t]:
                        m.st[v] = [0, s, t]
                        moved += 1
                    out.append(v)
                else:
                    if (v, s, t) not in copies:
                        copies[(v, s, t)] = m.add_vertex(
                            s, t, [poses[p][v, :3].astype(np.float64) * m.scale + m.origin for p in range(len(poses))],
                            [table[poses[p][v, 3]] for p in range(len(poses))])
                    out.append(copies[(v, s, t)])
            tris[i] = (fr,) + tuple(out)
    m.tris = tris
    P2 = np.vstack([P] + [np.asarray(q[0])[None] for q in m.new_pos]) if m.new_pos else P
    group_report("after", m, P2, chosen, lambda i: old_uv(m, i))
    print("    %d charts (%d panels painted from their mean colour), %d texels painted, skin %d x %d -> %d x %d; %d vertices "
          "re-mapped, %d copied" % (len(placed), flat, painted, m.sw, m.old_sh, m.sw, m.sh, moved, len(copies)))
    m.write(path)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    paths = [a if os.path.dirname(a) else os.path.join(PROGS, a) for a in args] or \
        [os.path.join(PROGS, n) for n in SPECS]
    for path in paths:
        if "--report" not in sys.argv:
            stretch_summary(load_mdl(path), False)
        fix(path, "--report" in sys.argv)
        if "--report" not in sys.argv:
            stretch_summary(load_mdl(path), False)


if __name__ == "__main__":
    main()
