#!/usr/bin/env python3
# refine_laserg.py -- the laser cannon (v_laserg.mdl): its stretched skin re-mapped and its painted vents carved
# (NOTES.md vrfiringrange_2026-10-01_22-38-05: "the UV mapping on the bottom ... the handle ... a little bit too
# stretched"; "the grooves and darker parts on the main body ... indented in the shape of the weapon itself", as
# make_enemyguns.py carved the enemy guns).
#
# Usage: python Misc/quakevr/refine_laserg.py [model]   (default: quakevr/progs/v_laserg.mdl; rewrites it in place)
#        python Misc/quakevr/refine_laserg.py --report [model]   (the parts' stretch only, writes nothing)
# polish_weapons.py runs it (POST) on the v_laserg.mdl it writes, so rerunning that still gives the shipped file.
#
# The stretch (texels per model unit and the map's anisotropy, area-weighted; --report): the keel under the body
# (id's: its sides folded onto 8-texel strips, up to 14:1), the body's bottom (folded onto a line), and round 17's
# spade grip (improve_weapons3.py's lofts: the head 5:1, the neck 4:1, the trigger guard 20:1, the trigger 8:1).
# Each is cut into charts (faces within CHART_ANGLE of the chart's first, joined by edges), unwrapped flat (least
# squares conformal: reuv_shot2.lscm) at one density, laid in skin space nothing uses (id's old hand picture, the
# keel's old strips) and painted there from the 3D surface (painters below: its own colours, the convex edges' first
# texel row lit and the next worn, as mdlpolish.edge_wear and reuv_shot2.paint do): no streaks whatever the angle.
# The body's -y side was mapped 9 texels off its +y side (its vents sat lower, across the side's bend): it now takes
# the +y side's mapping mirrored, on its own copy of the panel (the skin's, 153 texels right and 13 up).
#
# The carving: the four red vent windows on each side of the body (the panel's dark windows, 0.7 deep) and the dark
# grooves between them (0.35 deep) are cut into the body (Blender's exact boolean, headless: blender/carve_mesh.py, as
# make_enemyguns.py's carve). Inside each cut the surface sinks to the body moved in by the depth (its floor keeps
# the painted window: the red lamps now sit inside it); the walls take the window's darkest texel. Positions read off
# the +y panel (luminance runs; CUTS, skin texels) and mirrored to -y across the body's middle plane.
#
# What stays: every old vertex's bytes and index, the triangles before the body (vr_anchor.cpp's strip order: the
# hand 4, muzzle 22 and ammo counter 226 anchors are vertices of the first 170 triangles; polish_weapons.py checks
# them), the header (scale, origin: the weapon offsets, hotspots and sights are model space). Re-mapped corners get
# copies of their vertex (every pose's bytes, a new skin coordinate); the cut's vertices are new (the body never
# moves: every pose the same). New triangles go after the old ones.

import json
import math
import os
import subprocess
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import make_enemyguns as eg  # noqa: E402
import mdlpolish as mp  # noqa: E402
from improve_weapons import BAYER4, Noise  # noqa: E402
from reuv_shot2 import ORIENT, feature_edges, grow, lscm, orient, raster, seg_dist  # noqa: E402

DEFAULT = os.path.join(HERE, "..", "..", "quakevr", "progs", "v_laserg.mdl")

CHART_ANGLE = 50.0   # degrees: a face joins a chart within this of its first face (and 35 of its neighbour)
# The grip's lofts (improve_weapons3.build_laser's skin regions, from row 229: s0, t0, s1, t1).
GRIP_ROW = 229
HANDLE = {"head": (96, 0, 224, 32), "neck": (256, 0, 352, 32), "guard": (352, 0, 448, 12),
          "trigger": (352, 12, 400, 24)}
# Palette ramps, darkest first.
KEEL_RAMP = [0, 49, 16, 174, 17, 173, 18, 172, 171]          # the keel's dark browns (id's: 17, 16, 0, 49, 174)
LASER = [16, 174, 17, 173, 18, 19, 172, 20]                    # improve_weapons3's: the grip's browns
LASER_DARK = [49, 16, 32, 174, 33, 17, 34]
STEEL = [0, 1, 2, 3, 4, 5, 6, 7, 8]
# part: (density in texels per model unit, ramp, base value, noise spread, grain along x)
PAINT = {
    "keel": (3.2, KEEL_RAMP, 0.36, 0.16, True),
    "bottom": (3.2, KEEL_RAMP, 0.30, 0.12, True),
    "head": (6.0, LASER, 0.50, 0.14, False),
    "neck": (6.0, LASER, 0.50, 0.14, False),
    "guard": (9.0, LASER_DARK, 0.50, 0.10, False),
    "trigger": (9.0, STEEL, 0.50, 0.10, False),
}

# The body's -y side mapped as its +y side mirrored, onto the -y copy of the panel (skin offset).
PANEL_SHIFT = (153, -13)
# The cuts (skin texels on the +y panel: u0, u1, v0, v1; depth in model units). The windows: their dark runs (each
# 16 x 9-10 texels, the red lamps in the middle); the grooves between them: 14 x 3 texels.
WINDOW_DEPTH, GROOVE_DEPTH = 0.7, 0.35
CUTS = ([((585, 602, v0, v0 + 10), WINDOW_DEPTH) for v0 in (67, 89, 111, 133)]
        + [((586, 600, v0, v0 + 3), GROOVE_DEPTH) for v0 in (81, 103, 125)])


# ----------------------------------------------------------------------------
# The model's pieces

def tri_n(P, t):
    """Outward normal (Quake's front faces are clockwise seen from outside) and area."""
    _, a, b, c = t
    n = np.cross(P[c] - P[a], P[b] - P[a])
    a2 = np.linalg.norm(n)
    return (n / a2 if a2 > 1e-9 else n), a2 / 2


def welded_components(m, P):
    nv = len(m.st)
    root = list(range(nv))

    def find(v):
        while root[v] != v:
            root[v] = root[root[v]]
            v = root[v]
        return v

    def join(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            root[rb] = ra

    for _, a, b, c in m.tris:
        join(a, b)
        join(a, c)
    seen = {}
    for v in range(nv):
        k = tuple(np.round(P[v], 3))
        if k in seen:
            join(seen[k], v)
        else:
            seen[k] = v
    return find


def parts(m, P):
    """{part: [triangle indices]}: the keel, the body (its bottom too), the grip's head, neck, guard and trigger."""
    find = welded_components(m, P)

    def near(q):
        return min(range(len(P)), key=lambda v: np.linalg.norm(P[v] - np.array(q)))

    keel_root = find(near((54.8, -2.9, -24.1)))
    body_root = find(near((46.6, 4.8, -10.9)))
    out = {"keel": [], "body": []}
    for i, t in enumerate(m.tris):
        r = find(t[1])
        if r == keel_root:
            out["keel"].append(i)
        elif r == body_root:
            out["body"].append(i)
    for name, (s0, t0, s1, t1) in HANDLE.items():
        out[name] = [i for i, t in enumerate(m.tris)
                     if s0 <= sum(m.st[v][1] for v in t[1:]) / 3 < s1
                     and GRIP_ROW + t0 <= sum(m.st[v][2] for v in t[1:]) / 3 < GRIP_ROW + t1]
    expect = {"keel": 20, "body": 38, "head": 48, "neck": 78, "guard": 48, "trigger": 32}
    for k, n in expect.items():
        assert len(out[k]) == n, "%s: %d triangles, expected %d (the input model changed?)" % (k, len(out[k]), n)
    return out


# ----------------------------------------------------------------------------
# Measures

def stretch_report(m, P, tris_by_part, corners):
    for name, tris in tris_by_part.items():
        a_sum = d_sum = an_sum = 0.0
        for i in tris:
            t = m.tris[i]
            A = P[list(t[1:])]
            U = np.array([(s, tt) for _, s, tt in corners[i]], float)
            e1, e2 = A[1] - A[0], A[2] - A[0]
            n = np.cross(e1, e2)
            a3 = np.linalg.norm(n) / 2
            if a3 < 1e-6:
                continue
            x = e1 / np.linalg.norm(e1)
            y = np.cross(n / (2 * a3), x)
            M3 = np.array([[e1 @ x, e2 @ x], [e1 @ y, e2 @ y]])
            f1, f2 = U[1] - U[0], U[2] - U[0]
            if abs(f1[0] * f2[1] - f1[1] * f2[0]) < 1e-9:
                dens, an = 0.0, 50.0
            else:
                J = np.array([f1, f2]).T @ np.linalg.inv(M3)
                s = np.linalg.svd(J, compute_uv=False)
                dens, an = math.sqrt(abs(np.linalg.det(J))), min(50.0, s[0] / s[1])
            a_sum += a3
            d_sum += dens * a3
            an_sum += an * a3
        print("  %-8s %3d triangles, %6.1f units^2: %.2f texels/unit, anisotropy %.2f" % (
            name, len(tris), a_sum, d_sum / a_sum, an_sum / a_sum))


# ----------------------------------------------------------------------------
# Charts: unwrapping, packing, painting

def make_charts(P, tris, m):
    """The part's faces (non-degenerate) in charts: grown from the largest face over shared edges (welded), a face
    joining within CHART_ANGLE of the chart's first face and 35 degrees of the face it is reached from."""
    key = lambda v: tuple(np.round(P[v], 3))
    info = {i: tri_n(P, m.tris[i]) for i in tris}
    faces = [i for i in tris if info[i][1] > 1e-6]
    edges = {}
    for i in faces:
        vs = m.tris[i][1:]
        for k in range(3):
            e = tuple(sorted((key(vs[k]), key(vs[(k + 1) % 3]))))
            edges.setdefault(e, []).append(i)
    left = set(faces)
    charts = []
    c1, c2 = math.cos(math.radians(CHART_ANGLE)), math.cos(math.radians(35.0))
    while left:
        seed = max(sorted(left), key=lambda i: info[i][1])
        chart, todo = [seed], [seed]
        left.discard(seed)
        while todo:
            f = todo.pop()
            vs = m.tris[f][1:]
            for k in range(3):
                e = tuple(sorted((key(vs[k]), key(vs[(k + 1) % 3]))))
                for g in edges[e]:
                    if g in left and info[g][0] @ info[seed][0] > c1 and info[g][0] @ info[f][0] > c2:
                        left.discard(g)
                        chart.append(g)
                        todo.append(g)
        charts.append(sorted(chart))
    return charts


def unwrap(P, m, chart):
    """{welded key: (u, v)} in model units (least squares conformal), x along u."""
    key = lambda v: tuple(np.round(P[v], 3))
    ids = {}
    pts = []
    tri_ids = []
    for i in chart:
        row = []
        for v in m.tris[i][1:]:
            k = key(v)
            if k not in ids:
                ids[k] = len(pts)
                pts.append(P[v])
            row.append(ids[k])
        tri_ids.append(tuple(row))
    Pw = np.array(pts)
    net = lscm(Pw, tri_ids)
    return {k: net[j] for k, j in ids.items()}, tri_ids


def snap(net, density, k, f):
    keys = list(net)
    q = orient(np.array([net[x] for x in keys]) * density, k, f)
    q = np.round(q - q.min(0)).astype(int) + 1
    return dict(zip(keys, q))


def chart_mask(P, m, chart, st):
    key = lambda v: tuple(np.round(P[v], 3))
    qs = [np.array([st[key(v)] for v in m.tris[i][1:]], float) + 0.5 for i in chart]
    hi = np.max([q.max(0) for q in qs], axis=0)
    W, H = int(hi[0]) + 3, int(hi[1]) + 3
    mask = np.zeros((H, W), bool)
    for (t, s) in raster(qs, W, H):
        mask[t, s] = True
    for q in qs:  # thin triangles: their corners and edge midpoints
        for a in range(3):
            for p in (q[a], 0.5 * (q[a] + q[(a + 1) % 3])):
                mask[int(p[1]), int(p[0])] = True
    return grow(mask, 1)


def place(used, shape_mask):
    """Left-most, then top-most offset (t, s) where the mask fits on free texels (reuv_shot2.place's search); None if
    none. Left first: the charts fill id's old hand picture (the skin's left quarter) before anything else."""
    H, W = used.shape
    h, w = shape_mask.shape
    if h > H or w > W:
        return None
    F = np.fft.rfft2(used.astype(float), s=(H + h, W + w))
    G = np.fft.rfft2(shape_mask[::-1, ::-1].astype(float), s=(H + h, W + w))
    corr = np.fft.irfft2(F * G, s=(H + h, W + w))[h - 1:H, w - 1:W]
    ok = np.argwhere(corr < 0.5)
    if not len(ok):
        return None
    t, s = ok[np.lexsort((ok[:, 0], ok[:, 1]))[0]]
    return int(t), int(s)


def used_mask(m, corners, skip, H):
    used = np.zeros((H, m.sw), bool)
    for i, t in enumerate(m.tris):
        if i in skip:
            continue
        q = np.array([(s, tt) for _, s, tt in corners[i]], float) + 0.5
        for (tt, s) in raster([q], m.sw, H):
            used[tt, s] = True
        for s, tt in q.astype(int):
            used[min(tt, H - 1), min(s, m.sw - 1)] = True
    used[0:2, 0:2] = True
    return grow(used, 1)


def layout(m, P, corners, jobs):
    """jobs: [(part, chart)]. Unwraps every chart, packs them (largest first: lowest, then leftmost) in the skin's
    free texels (growing it by the fewest rows), and sets their corners' skin coordinates. Returns the skin height
    and [(part, chart triangles, {key: st})]."""
    skip = {i for _, c in jobs for i in c}
    nets = []
    for part, chart in jobs:
        net, _ = unwrap(P, m, chart)
        nets.append((part, chart, net))
    nets.sort(key=lambda x: -sum(tri_n(P, m.tris[i])[1] for i in x[1]) * PAINT[x[0]][0] ** 2)
    for extra in range(0, 512, 8):
        H = m.sh + extra
        used = used_mask(m, corners, skip, H)
        placed = []
        for part, chart, net in nets:
            best = None
            for k, f in ORIENT:
                st = snap(net, PAINT[part][0], k, f)
                mask = chart_mask(P, m, chart, st)
                at = place(used, mask)
                if at and (best is None or (at[1] + mask.shape[1], at[0]) < best[0]):
                    best = ((at[1] + mask.shape[1], at[0]), at, st, mask)
            if best is None:
                break
            _, (t0, s0), st, mask = best
            used[t0:t0 + mask.shape[0], s0:s0 + mask.shape[1]] |= mask
            placed.append((part, chart, {x: (int(q[0]) + s0, int(q[1]) + t0) for x, q in st.items()}))
        else:
            return H, placed
    raise SystemExit("refine_laserg: no room in the skin")


def painter(part, edges, seed):
    density, ramp, base, spread, grain = PAINT[part]
    noise = Noise(seed)

    def fn(p, s, t):
        d = min(seg_dist(p, a, e) for a, e in edges) * density if edges else 99.0  # texels from a convex edge
        if grain:  # grain along the gun (x), as id's paint
            g = 0.7 * noise.smooth(p[0] * 0.35, (p[1] + p[2]) * 2.0) + 0.3 * noise.smooth(p[0] * 1.2, (p[1] + p[2]) * 5.0)
            v = base + spread * 2 * (g - 0.5)
        else:      # mottled worn metal (improve_weapons.metal)
            v = base + spread * 2 * (noise.smooth((p[0] + p[2]) * 1.2, (p[1] - p[2]) * 1.2) - 0.5)
        v += 0.05 * (noise.hash(s, t) - 0.5)
        if noise.hash(int(p[0] * 1.5), int((p[1] - p[2]) * density)) > 0.985:
            v += 0.18  # a scratch
        if d < 1.0:
            v += 0.3
        elif d < 2.0 and noise.hash(t * 7 + 3, s * 5 + 1) > 0.55:
            v += 0.12
        i = max(0.0, min(1.0, v)) * (len(ramp) - 1)
        j = int(i)
        if i - j > (BAYER4[t % 4][s % 4] + 0.5) / 16.0:
            j += 1
        return ramp[min(j, len(ramp) - 1)]
    return fn


def paint_charts(skin, sw, H, m, P, placed):
    """Each chart's texels (and a one-texel margin round it) painted from the surface point they show."""
    key = lambda v: tuple(np.round(P[v], 3))
    seeds = {}
    for part, chart, st in placed:
        seeds[part] = seeds.get(part, 40 + len(seeds))
        tris = [tuple(m.tris[i][1:]) for i in chart]
        edges = feature_edges(P, tris)
        fn = painter(part, edges, seeds[part])
        qs = [np.array([st[key(v)] for v in t], float) + 0.5 for t in tris]
        own = raster(qs, sw, H)
        for k, q in enumerate(qs):  # thin triangles: the texels under their corners and edges
            for a in range(3):
                for b in (0.0, 0.25, 0.5, 0.75):
                    p = q[a] + b * (q[(a + 1) % 3] - q[a])
                    c = (int(p[1]), int(p[0]))
                    if c not in own:
                        w = [0.0, 0.0, 0.0]
                        w[a], w[(a + 1) % 3] = 1.0 - b, b
                        own[c] = (k, tuple(w))
        ring = {}
        for (t, s), (k, _) in own.items():
            for dt in (-1, 0, 1):
                for ds in (-1, 0, 1):
                    n = (t + dt, s + ds)
                    if n not in own and n not in ring and 0 <= n[0] < H and 0 <= n[1] < sw:
                        ring[n] = (k, None)
        for (t, s), (k, b) in list(own.items()) + list(ring.items()):
            tri = tris[k]
            if b is None:
                b = barycentric(qs[k], s + 0.5, t + 0.5)
            p = sum(b[j] * P[tri[j]] for j in range(3))
            skin[t * sw + s] = fn(p, s, t)


def barycentric(q, x, y):
    (x0, y0), (x1, y1), (x2, y2) = q
    det = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
    if abs(det) < 1e-12:
        return (1 / 3, 1 / 3, 1 / 3)
    b0 = ((y1 - y2) * (x - x2) + (x2 - x1) * (y - y2)) / det
    b1 = ((y2 - y0) * (x - x2) + (x0 - x2) * (y - y2)) / det
    return b0, b1, 1 - b0 - b1


# ----------------------------------------------------------------------------
# The body: its -y side's mapping, the cuts

def uv_point(P, m, tris, corners, u, v):
    """The surface point the skin texel coordinate (u, v) shows on these triangles (None: none of them)."""
    for i in tris:
        t = m.tris[i]
        q = np.array([(s, tt) for _, s, tt in corners[i]], float)
        b = barycentric(q, u, v)
        if min(b) >= -1e-6 and abs(np.linalg.det(np.array([q[1] - q[0], q[2] - q[0]]))) > 1e-9:
            return sum(b[j] * P[t[1 + j]] for j in range(3))
    return None


def mirror_plane(P, m, body):
    """The body's middle plane (y): the mean of its widest points' y."""
    ys = [P[v][1] for i in body for v in m.tris[i][1:]]
    return 0.5 * (min(ys) + max(ys))


def body_sides(P, m, body, c):
    """The body's side faces: +y and -y (their normals mostly sideways)."""
    plus, minus = [], []
    for i in body:
        n, a = tri_n(P, m.tris[i])
        if a < 1e-6:
            continue
        xs = [P[v][0] for v in m.tris[i][1:]]
        if max(xs) - min(xs) < 20.0:  # the back's small faces (round the folded blade) stay as they are
            continue
        if n[1] > 0.6:
            plus.append(i)
        elif n[1] < -0.6:
            minus.append(i)
    return plus, minus


def remap_minus_side(P, m, corners, plus, minus, c):
    """The -y side's corners take the +y side's skin coordinates at their mirror point, on the -y panel."""
    for i in minus:
        new = []
        for (src, _, _) in corners[i]:
            p = P[src].copy()
            p[1] = 2 * c - p[1]
            uv = surface_uv(P, m, plus, corners, p)
            new.append((src, int(round(uv[0] + PANEL_SHIFT[0])), int(round(uv[1] + PANEL_SHIFT[1]))))
        corners[i] = new


def surface_uv(P, m, tris, corners, p):
    """The skin coordinates at the point of these triangles nearest p (extrapolated off the nearest)."""
    best = None
    for i in tris:
        t = m.tris[i]
        A = P[list(t[1:])]
        n = np.cross(A[1] - A[0], A[2] - A[0])
        nn = n @ n
        r = p - A[0]
        w1 = np.cross(r, A[2] - A[0]) @ n / nn
        w2 = np.cross(A[1] - A[0], r) @ n / nn
        w = np.array((1 - w1 - w2, w1, w2))
        score = abs(r @ n) / math.sqrt(nn) + max(0.0, -w.min())
        if best is None or score < best[0]:
            q = np.array([(s, tt) for _, s, tt in corners[i]], float)
            best = (score, w @ q)
    return best[1]


def snap_axis(x, o, s):
    return o + round((x - o) / s) * s


def cut_boxes(m, P, plus, corners, c):
    """The cuts as boxes in model space (+y side; each mirrored to -y), their walls on the file's grid."""
    out = []
    for (u0, u1, v0, v1), depth in CUTS:
        um, vm = 0.5 * (u0 + u1), 0.5 * (v0 + v1)
        pa, pb = uv_point(P, m, plus, corners, um, v0), uv_point(P, m, plus, corners, um, v1)
        pc, pd = uv_point(P, m, plus, corners, u0, vm), uv_point(P, m, plus, corners, u1, vm)
        assert all(q is not None for q in (pa, pb, pc, pd)), "a cut off the +y side: %s" % ((u0, u1, v0, v1),)
        x0, x1 = sorted((pa[0], pb[0]))
        z0, z1 = sorted((pc[2], pd[2]))
        x0, x1 = (snap_axis(x, m.origin[0], m.scale[0]) for x in (x0, x1))
        z0, z1 = (snap_axis(z, m.origin[2], m.scale[2]) for z in (z0, z1))
        mid = 0.5 * (pa + pb)
        for side in (1, -1):
            box = (x0, x1, c, c + 20.0) if side > 0 else (x0, x1, c - 20.0, c)
            probe = np.array((mid[0], c + side * 20.0, 0.5 * (z0 + z1)))
            out.append(dict(box=box + (z0, z1), depth=depth, probe=(probe, np.array((0.0, -side, 0.0)))))
    return out


def carve(m, P, body, corners, cuts):
    """The cuts taken out of the body (Blender, headless: blender/carve_mesh.py). Returns the body's new triangles:
    [(corner positions, corner skin coordinates, recess)]."""
    faces_q = [i for i in body if tri_n(P, m.tris[i])[1] > 1e-6]
    weld, W, wi = {}, [], {}
    for i in faces_q:
        for v in m.tris[i][1:]:
            k = tuple(np.round(P[v], 4))
            if k not in weld:
                weld[k] = len(W)
                W.append(P[v])
            wi[v] = weld[k]
    W = np.array(W)
    faces = [(wi[a], wi[c], wi[b]) for _, a, b, c in (m.tris[i] for i in faces_q)]  # clockwise -> counter-clockwise
    uvs = []
    for i in faces_q:
        u = [(float(s), float(t)) for _, s, t in corners[i]]
        uvs.append([u[0], u[2], u[1]])
    lum = mp.palette() @ np.array((0.3, 0.59, 0.11))
    skin = m.skin()
    job = {"verts": W.tolist(), "faces": faces, "uvs": uvs, "cuts": [], "dissolve": True}
    for ct in cuts:
        u, v = (int(np.floor(x)) for x in eg.ray_uv(W, faces, uvs, *ct["probe"]))
        u, v = min(((a, b) for a in range(u - 2, u + 3) for b in range(v - 2, v + 3)),
                   key=lambda q: (lum[skin[q[1] * m.sw + q[0]]], abs(q[0] - u) + abs(q[1] - v)))
        job["cuts"].append({"box": [float(x) for x in ct["box"]], "inner": eg.inner_mesh(W, faces, ct["depth"]).tolist(),
                            "wall_uv": [u + 0.5, v + 0.5]})
    script = os.path.join(HERE, "blender", "carve_mesh.py")
    with tempfile.TemporaryDirectory() as tmp:
        src, dst = os.path.join(tmp, "in.json"), os.path.join(tmp, "out.json")
        with open(src, "w") as f:
            json.dump(job, f)
        r = subprocess.run([eg.BLENDER, "-b", "--factory-startup", "--python-exit-code", "1", "-P", script, "--", src,
                            dst], capture_output=True, text=True)
        assert r.returncode == 0 and os.path.exists(dst), "Blender failed:\n%s" % (r.stdout + r.stderr)[-3000:]
        with open(dst) as f:
            res = json.load(f)
    V = np.array(res["verts"], np.float64)
    eg.split_slivers(V, res["faces"], res["uvs"], res["cut"])
    out = []
    for f, uv, is_cut in zip(res["faces"], res["uvs"], res["cut"]):
        f = (f[0], f[2], f[1])  # back to clockwise
        uv = (uv[0], uv[2], uv[1])
        out.append(([V[k] for k in f], [(int(math.floor(a + 1e-3)), int(math.floor(b + 1e-3))) for a, b in uv],
                    bool(is_cut)))
    return out, faces_q


# ----------------------------------------------------------------------------
# Writing

def pose_arrays(m):
    out = []
    for p in range(m.num_poses()):
        b = np.frombuffer(bytes(m.pose_bytes(p)), np.uint8).reshape(-1, 4)
        out.append(b)
    return out


def fix(path, report_only=False):
    m = mp.Model(path)
    P = m.positions(0)
    pp = parts(m, P)
    corners = [[(v, m.st[v][1], m.st[v][2]) for v in t[1:]] for t in m.tris]
    body = pp["body"]
    c = mirror_plane(P, m, body)
    plus, minus = body_sides(P, m, body, c)
    bottom = [i for i in body if tri_n(P, m.tris[i])[1] > 1e-6 and tri_n(P, m.tris[i])[0][2] < -0.9]
    report = {k: pp[k] for k in ("keel", "head", "neck", "guard", "trigger")}
    report.update({"bottom": bottom, "body -y": minus})
    print("v_laserg.mdl, before:")
    stretch_report(m, P, report, corners)
    if report_only:
        return
    poses = pose_arrays(m)
    for name in ("keel", "head", "neck", "guard", "trigger"):
        for i in pp[name]:
            assert all((poses[p][m.tris[i][1:], :3] == poses[0][m.tris[i][1:], :3]).all() or name != "keel"
                       for p in range(len(poses)))
    for p in range(len(poses)):
        assert (poses[p][[v for i in body for v in m.tris[i][1:]]] == poses[0][[v for i in body for v in m.tris[i][1:]]]).all(), \
            "the body moves"

    # 1. The stretched parts re-mapped and painted.
    jobs = [(name, chart) for name in ("keel", "head", "neck", "guard", "trigger")
            for chart in make_charts(P, pp[name], m)]
    jobs += [("bottom", chart) for chart in make_charts(P, bottom, m)]
    H, placed = layout(m, P, corners, jobs)
    if H > m.sh:
        m.grow_skin(H - m.sh)
    key = lambda v: tuple(np.round(P[v], 3))
    for part, chart, st in placed:
        for i in chart:
            corners[i] = [(v, st[key(v)][0], st[key(v)][1]) for v in m.tris[i][1:]]
    skin = m.skin()
    paint_charts(skin, m.sw, m.sh, m, P, placed)

    # 3. The cuts.
    cuts = cut_boxes(m, P, plus, corners, c)
    carved, faces_q = carve(m, P, body, corners, cuts)

    # 4. The triangles: every old one but the body's (its degenerate ones kept), then the carved body.
    old_pos = {}
    for v in sorted({v for i in body for v in m.tris[i][1:]}):
        old_pos.setdefault(tuple(np.round(P[v], 3)), v)
    old_n = np.array(eg.smooth_vectors([tuple(p) for p in P], [m.tris[i] for i in faces_q]))
    body_tris = [m.tris[i] for i in faces_q]
    from mdlgen import anorms
    table = [np.array(a) for a in anorms()]
    vid = {}
    for v in range(len(m.st)):
        vid[(v, m.st[v][1], m.st[v][2])] = v

    def old_vertex(src, s, t):
        k = (src, s, t)
        if k not in vid:
            per_pose = [poses[p][src, :3].astype(np.float64) * m.scale + m.origin for p in range(len(poses))]
            nrm = [table[poses[p][src, 3]] for p in range(len(poses))]
            vid[k] = m.add_vertex(s, t, per_pose, nrm)
        return vid[k]

    def new_vertex(p, s, t, n):
        q = tuple(np.round((np.asarray(p) - m.origin) / m.scale).astype(int))  # the place as the file keeps it
        k = ("p", q, s, t, int(np.argmax(np.array(table) @ np.asarray(n))))  # the normal as the file keeps it
        if k not in vid:
            hit = old_pos.get(tuple(np.round(p, 3)))
            if hit is not None and (m.st[hit][1], m.st[hit][2]) == (s, t):
                vid[k] = hit
            elif hit is not None:
                vid[k] = old_vertex(hit, s, t)
            else:
                vid[k] = m.add_vertex(s, t, [np.array(p)] * len(poses), [n] * len(poses))
        return vid[k]

    body_set = set(body)
    tris = []
    for i, t in enumerate(m.tris):
        if i in body_set and i in faces_q:
            continue
        tris.append((t[0],) + tuple(old_vertex(src, s, tt) for src, s, tt in corners[i]))
    for pos, sts, is_cut in carved:
        if is_cut:
            n = np.cross(pos[2] - pos[0], pos[1] - pos[0])
            n = n / np.linalg.norm(n)
            ns = [n] * 3
        else:
            ns = [eg.interpolated_normal(np.array(p), P, body_tris, old_n) for p in pos]
        tris.append((1,) + tuple(new_vertex(p, s, t, nn) for p, (s, t), nn in zip(pos, sts, ns)))
    m.tris = tris
    print("v_laserg.mdl, after: %d -> %d triangles, %d -> %d vertices, skin %dx%d" % (
        len(corners), len(tris), m.old_nv, len(m.st), m.sw, m.sh))
    m.write(path)


def main():
    args = [a for a in sys.argv[1:] if a != "--report"]
    fix(args[0] if args else DEFAULT, "--report" in sys.argv)


if __name__ == "__main__":
    main()
