#!/usr/bin/env python3
# gunlooks.py -- round 24's looks pass on the shotgun and the nailgun (the author's mockups: textures that fit the
# geometry, no flat faces, believable wood grain, subtle rust, a few low rivets; the colour family, the silhouette and
# the glowing sights as they are).
#
# Usage (Python with numpy): python Misc/quakevr/gunlooks.py [--force] [model ...]   (default: every model in SPECS)
#        python Misc/quakevr/gunlooks.py --capture                                   (once: see "Inputs")
# then   python Misc/quakevr/bake_normals.py v_shot.mdl v_nail.mdl v_lava.mdl        (the relief, from this script's heights)
# and    python Misc/quakevr/gunlooks_check.py                                       (the verdict line)
# Rerun after polish_weapons.py or anything else that rewrites these models (--capture first, on what they rewrote).
#
# Inputs (src_models/gunlooks/): each model as polish_weapons.py and fix_shot_stock_corner.py left it, paint and all: its
# old skin is the reference (round 24's second pass, the author's notes: its materials, rings, grates, holes and dark
# interiors are the gun's look); its .json holds which triangles stay as they are, each old triangle's mean colour and,
# for the shotgun, which triangles are the pump's. Rerunning gives the same files.
#
# What stays: the header (scale, origin), every old vertex's bytes in every pose at its index, every old triangle in its
# order, on the same points (a corner may be a coincident copy appended at the end, as reuv_weapons.py's), the frames,
# every fullbright texel where it was, and the triangles that use them, on the same texels (FB_KEEP). New vertices and
# triangles only at the end; rivets carried rigidly by the piece they sit on (mdlpolish.Polisher).
#
# What changes:
# - Rivets (recipes below): low hexagonal heads on side panels and the receiver's flanks, outside the sight line and
#   clear of where the hands hold the gun; never on the pump's rings, so its part files only gain them in the body.
# - Skin coordinates: every face but those re-mapped into charts of one facing (CHART_ANGLE) at one density (spec "D",
#   texels per model unit), laid in free texels: nothing stretched, every texel chunky and the same size.
# - The paint, per texel: the old skin at the surface point it shows (its colour, so its materials and painted detail; a
#   face the old map stretched beyond OLD_RATIO takes its mean colour instead), kept off pure black (DARK_LIFT), then on top,
#   from that surface point in model space (continuous over seams): slight tone, brushing and grit, worn convex edges, rust patches
#   (spec "rust": the fraction of the surface) and stains under rivets, and last a baked ambient occlusion (bake_ao: rays
#   from each texel against the model's own triangles, AO_STRENGTH at most). Wood: rings and fibres along the part's
#   length, its tone the old paint's. Palette only, never a fullbright index.
# - Corners left with a vertex normal pointing away from every face they serve: a copy with those faces' normal.
# - The barrel masks in weapons.txt: their triangle count, the new triangles not the barrel's.
# - The relief: this script's heights (src_models/gunlooks/<model>.relief.npz), read by the bake (normaltiles.py).

import hashlib
import json
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import mdlpolish as mp  # noqa: E402

PROGS = os.path.join(HERE, "..", "..", "quakevr", "progs")
SRC = os.path.join(HERE, "src_models", "gunlooks")
PAL = mp.palette()

FB_KEEP = 0.5          # a triangle stays as it is when at least this much of what it covers is fullbright...
FB_SMALL = 0.2         # ...or it is smaller than this (square units) and covers any
CHART_ANGLE = 38.0     # degrees: a chart's faces within this of its first face
NEIGHBOUR_ANGLE = 30.0  # and of the face they are reached from
CHART_MARGIN = 1       # texels of gutter round a chart (filled from it)
# The palette the paint may use: the greys, browns, blue-greys, the old paint's dark olives and reds, the rusts and tans.
CANDIDATES = np.array(list(range(1, 72)) + list(range(80, 128)) + list(range(160, 192)))
RIVET_BASE = np.array([48.0, 47.0, 46.0])
CREASE_COS = 0.82      # faces meeting flatter than this (35 degrees) have no worn edge between them
TUBE_LIFT = 1.25
SLIVER_TEXELS = 2.0    # a face narrower than this many texels is a sliver
OLD_RATIO = 30.0       # a face the old skin was stretched over more than this takes its mean colour, not the streaks
DARK_LIFT = 4.0        # the old paint's colours all lifted by this (0-255): off pure black, its dark detail (grates, slots) kept
AO_STRENGTH = 0.6      # the baked occlusion darkens by this much at most
AO_RANGE = 3.0         # model units: what is further off occludes nothing
AO_RAYS = 20
WARM = np.array([1.04, 1.0, 0.95])  # the metal a little warmer than the old paint's blue-black
FLAT_TEXELS = 6        # a face covering this many texels or more must show more than one palette index


# ----------------------------------------------------------------------------
# Rivets (model space: x forward, y left, z up): few, in mirrored pairs, where a fastener would be: joining two parts, at
# a panel's corners and seams, on the gun's own lines. Nothing on the tops (the sights' side), the pump's rings or under
# the main hand's thumb (behind x 8).

def rivets_shot(p):
    # A pair up the receiver's front edge, where it meets the fore-end; and one more in the row of pins along its lower
    # edge (polish_weapons.py's at x 13.2 and 16.6, z 1.7), on their spacing.
    out = []
    for s in (1, -1):
        for x, z in ((17.7, 3.9), (17.7, 2.5), (9.8, 1.7)):
            out.append(p.stud((x, s * 12.0, z), (0, -s, 0), radius=0.4, height=0.19, material="steel"))
    return out


def rivets_nail(p):
    # The receiver plate's two front corners, at the seam ahead of its two pins (polish_weapons.py's, on its middle line).
    out = []
    for s in (1, -1):
        for x, z in ((10.4, 1.05), (10.4, -1.05)):
            out.append(p.stud((x, s * 12.0, z), (0, -s, 0), radius=0.4, height=0.19, material="steel"))
    return out


# The nailgun's tubes draw much darker than the plate beside them under flat light, whatever their normals (near black
# in the old skin too), though not under the map's light: the tubes alone a little lighter (not the rings at their ends).
def detail_nail(P, N, metal):
    mult = np.ones(len(P))
    for yc in (3.8, -3.8):
        r = np.hypot(P[:, 1] - yc, P[:, 2] - 3.8)
        mult[metal & (r < 2.35) & (P[:, 0] > 6.2) & (P[:, 0] < 14.6)] *= TUBE_LIFT
    return mult


# Wood: boxes in model space (x0 x1 y0 y1 z0 z1), each with the part's length (the grain) and its centre line's point.
GRIP_SHOT = dict(box=(0.0, 7.6, -9, 9, -7, 2.3), along=(0.26, 0.0, -0.97), centre=(4.4, 0.0, -2.0))
FORE_SHOT = dict(box=(18.4, 31.3, -9, 9, -7, 6.6), along=(1.0, 0.0, 0.0), centre=(25.0, 0.0, 2.5))
GRIP_NAIL = dict(box=(-5.9, 3.3, -9, 9, -7, 2.05), along=(0.26, 0.0, -0.97), centre=(0.0, 0.0, -2.0))

SPECS = {
    "v_shot.mdl": dict(D=6.0, width=512, rivets=rivets_shot, wood=[GRIP_SHOT, FORE_SHOT], wood_chroma=2.0, rust=0.12,
                       relief=True, parts=("vr_pump_on_v_shot.mdl", "vr_pumpbody_on_v_shot.mdl"), seed=11),
    "v_nail.mdl": dict(D=6.0, width=512, rivets=rivets_nail, wood=[GRIP_NAIL], wood_chroma=2.0, rust=0.12, relief=True,
                       detail=detail_nail, seed=23),
    "v_lava.mdl": dict(D=6.0, width=512, rivets=rivets_nail, wood=[GRIP_NAIL], wood_chroma=2.0, rust=0.10, relief=True,
                       detail=detail_nail, glow_transfer=True, seed=23),
    # The pickups: the same paint, no rivets (they lie on the floor), no baked relief (the engine's from the skin).
    "g_shot0.mdl": dict(D=4.0, width=256, wood=[], wood_chroma=10.0, rust=0.12, seed=11),
    "g_nail.mdl": dict(D=4.0, width=256, wood=[], wood_chroma=10.0, rust=0.12, seed=23),
}


# ----------------------------------------------------------------------------
# Noise (value noise on a hashed lattice: the same on every machine)

MASK = 0xFFFFFFFF


def hash3(ix, iy, iz, seed):
    h = (ix.astype(np.int64) * 73856093) ^ (iy.astype(np.int64) * 19349663) ^ (iz.astype(np.int64) * 83492791) ^ (seed * 1000003)
    h &= MASK
    h = ((h ^ (h >> 13)) * 1274126177) & MASK
    h = ((h ^ (h >> 16)) * 2654435761) & MASK
    return ((h ^ (h >> 15)) & 0xFFFF) / 65535.0


def vnoise(P, seed):
    f = np.floor(P)
    i = f.astype(np.int64)
    t = P - f
    t = t * t * (3 - 2 * t)
    out = 0.0
    for dx in (0, 1):
        wx = t[:, 0] if dx else 1 - t[:, 0]
        for dy in (0, 1):
            wy = t[:, 1] if dy else 1 - t[:, 1]
            for dz in (0, 1):
                wz = t[:, 2] if dz else 1 - t[:, 2]
                out = out + wx * wy * wz * hash3(i[:, 0] + dx, i[:, 1] + dy, i[:, 2] + dz, seed)
    return out


def fbm(P, octaves, seed):
    a, s, tot = 1.0, 1.0, 0.0
    out = 0.0
    for o in range(octaves):
        out = out + a * vnoise(P * s + 17.3 * o, seed + o)
        tot += a
        a *= 0.5
        s *= 2.03
    return out / tot


def texel_rand(xs, ys, seed):
    return hash3(xs, ys, np.zeros_like(xs), seed)


def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def mix(a, b, t):
    t = np.asarray(t)[..., None] if np.ndim(t) else t
    return a + (b - a) * t


# ----------------------------------------------------------------------------
# The base files

def raster_tri(uv, tol=0.0):
    """(xs, ys, w0, w1, w2): the texel middles a triangle (texel coordinates, the vertices' middles at +0.5) covers."""
    (ax, ay), (bx, by), (cx, cy) = uv
    den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
    if abs(den) < 1e-9:
        return (np.zeros(0, int),) * 2 + (np.zeros(0),) * 3
    x0, x1 = int(math.floor(uv[:, 0].min())), int(math.ceil(uv[:, 0].max()))
    y0, y1 = int(math.floor(uv[:, 1].min())), int(math.ceil(uv[:, 1].max()))
    ys, xs = np.mgrid[y0:y1 + 1, x0:x1 + 1]
    xs, ys = xs.ravel(), ys.ravel()
    px, py = xs + 0.5, ys + 0.5
    w0 = ((by - cy) * (px - cx) + (cx - bx) * (py - cy)) / den
    w1 = ((cy - ay) * (px - cx) + (ax - cx) * (py - cy)) / den
    w2 = 1 - w0 - w1
    k = (w0 >= -tol) & (w1 >= -tol) & (w2 >= -tol)
    return xs[k], ys[k], w0[k], w1[k], w2[k]


def tri_cover(m, tri, skin):
    """The skin's indices under a triangle (a degenerate one: the texel under its middle)."""
    uv = mp.tri_uv(m, tri)
    xs, ys, *_ = raster_tri(uv, 0.02)
    k = (ys >= 0) & (ys < m.sh)
    xs, ys = xs[k] % m.sw, ys[k]
    if len(xs) == 0:
        x, y = uv.mean(0)
        xs, ys = np.array([int(x) % m.sw]), np.array([min(int(y), m.sh - 1)])
    return xs, ys, skin[ys, xs]


def pose_areas(m, tri):
    """The triangle's area in each pose (largest and in pose 0)."""
    a0 = None
    best = 0.0
    for p in range(m.num_poses()):
        P = m.positions(p)
        a = 0.5 * np.linalg.norm(np.cross(P[tri[2]] - P[tri[1]], P[tri[3]] - P[tri[1]]))
        a0 = a if a0 is None else a0
        best = max(best, a)
    return a0, best


def subset_tris(whole, part):
    """Which triangles of `whole` the part file has (it keeps them in order, on the same corners' bytes)."""
    pose = lambda m, v: bytes(m.pose_bytes(0)[4 * v:4 * v + 3])
    out = []
    k = 0
    for t, tri in enumerate(whole.tris):
        if k >= len(part.tris):
            break
        q = part.tris[k]
        if all(pose(whole, tri[i]) == pose(part, q[i]) and whole.st[tri[i]][1:] == part.st[q[i]][1:] for i in (1, 2, 3)):
            out.append(t)
            k += 1
    assert k == len(part.tris), "%d of %d triangles found" % (k, len(part.tris))
    return out


def capture(name):
    """Writes the input from the model in progs (see the header)."""
    path = os.path.join(PROGS, name)
    m = mp.Model(path)
    spec = SPECS[name]
    skin = np.frombuffer(bytes(m.skin()), np.uint8).reshape(m.sh, m.sw)
    means, kept = [], []
    for t, tri in enumerate(m.tris):
        xs, ys, vals = tri_cover(m, tri, skin)
        fb = vals >= mp.FULLBRIGHT
        a0, big = pose_areas(m, tri)
        frac = float(fb.mean())
        means.append(np.round(PAL[vals[~fb]].mean(0)).astype(int).tolist() if (~fb).any() else [0, 0, 0])
        stay = frac >= FB_KEEP or (fb.any() and big < FB_SMALL) or a0 < 1e-3
        if stay:
            kept.append(t)
    data = dict(means=means, kept=kept)
    if spec.get("parts"):
        whole = mp.Model(path)
        data["parts"] = {pn: subset_tris(whole, mp.Model(os.path.join(PROGS, pn))) for pn in spec["parts"]}
    os.makedirs(SRC, exist_ok=True)
    m.write(os.path.join(SRC, name))
    with open(os.path.join(SRC, name + ".json"), "w") as f:
        json.dump(data, f, separators=(",", ":"))
    print("%s: captured, %d triangles stay as they are" % (name, len(kept)))


# ----------------------------------------------------------------------------
# Charts

class Faces:
    """Every triangle's frame-0 geometry and how they meet."""

    def __init__(self, m):
        self.P = m.positions(0)
        q = np.frombuffer(bytes(m.pose_bytes(0)), np.uint8).reshape(-1, 4)[:, :3]
        key = {}
        self.weld = np.zeros(len(q), int)
        for i, k in enumerate(map(bytes, q)):
            self.weld[i] = key.setdefault(k, len(key))
        n = len(m.tris)
        self.tri = np.array([t[1:] for t in m.tris], int)
        a, b, c = (self.P[self.tri[:, i]] for i in range(3))
        cr = np.cross(c - a, b - a)  # Quake's triangles are clockwise seen from outside
        ln = np.linalg.norm(cr, axis=1)
        self.area = 0.5 * ln
        self.n = cr / np.maximum(ln, 1e-12)[:, None]
        self.edges = {}
        for t in range(n):
            if self.area[t] < 1e-5:
                continue
            w = self.weld[self.tri[t]]
            for i in range(3):
                k = (min(w[i], w[(i + 1) % 3]), max(w[i], w[(i + 1) % 3]))
                self.edges.setdefault(k, []).append(t)
        self.corner_groups = {}
        for t in range(n):
            for v in self.weld[self.tri[t]]:
                self.corner_groups.setdefault(v, []).append(t)

    def neighbours(self, t):
        w = self.weld[self.tri[t]]
        out = []
        for i in range(3):
            k = (min(w[i], w[(i + 1) % 3]), max(w[i], w[(i + 1) % 3]))
            out += [o for o in self.edges.get(k, ()) if o != t]
        return out

    def smooth_normal(self, t, v_weld, angle=40.0):
        c = math.cos(math.radians(angle))
        n = np.zeros(3)
        for o in self.corner_groups[v_weld]:
            if self.n[o] @ self.n[t] >= c:
                n += self.n[o] * max(self.area[o], 1e-3)
        ln = np.linalg.norm(n)
        return n / ln if ln > 1e-9 else self.n[t]


def in_box(p, b):
    return b[0] <= p[0] <= b[1] and b[2] <= p[1] <= b[3] and b[4] <= p[2] <= b[5]


def materials(m, F, spec, means, old_nt):
    """Each triangle's material: (kind, wood region or None). kind: 0 metal, 1 wood, 2 steel, 3 rivet."""
    mat = np.zeros(len(m.tris), int)
    region = [None] * len(m.tris)
    whole = dict(along=(1.0, 0.0, 0.0), centre=tuple(F.P.mean(0)))  # (no region given: the model's length)
    for t in range(len(m.tris)):
        if t >= old_nt:
            mat[t] = 3
            continue
        c = np.array(means[t], float)
        lum = c @ (0.3, 0.59, 0.11)
        centre = F.P[F.tri[t]].mean(0)
        chroma = c[0] - c[2]
        wood = None
        if spec["wood"]:
            for r in spec["wood"]:
                if in_box(centre, r["box"]) and chroma >= spec["wood_chroma"]:
                    wood = r
        elif chroma >= spec["wood_chroma"]:
            wood = whole
        if wood is not None:
            mat[t], region[t] = 1, wood
        elif lum > 62 and abs(chroma) < 25:
            mat[t] = 2
    return mat, region


def make_charts(F, faces, mat):
    """Grows charts over welded edges: the faces of one material within CHART_ANGLE of the first."""
    faces = sorted(faces, key=lambda t: -F.area[t])
    done = set()
    wanted = set(faces)
    charts = []
    ca, cn = math.cos(math.radians(CHART_ANGLE)), math.cos(math.radians(NEIGHBOUR_ANGLE))
    for seed in faces:
        if seed in done:
            continue
        chart = [seed]
        done.add(seed)
        stack = [seed]
        while stack:
            t = stack.pop()
            for o in F.neighbours(t):
                if o in wanted and o not in done and mat[o] == mat[seed] and F.n[o] @ F.n[seed] >= ca and F.n[o] @ F.n[t] >= cn:
                    done.add(o)
                    chart.append(o)
                    stack.append(o)
        charts.append(chart)
    return charts


def project(F, chart, D):
    """The chart's corners in texels (integers, from 0): {(tri, corner): (s, t)}, its width and height."""
    ids = np.array(chart)
    n = (F.n[ids] * F.area[ids, None]).sum(0)
    ln = np.linalg.norm(n)
    n = n / ln if ln > 1e-9 else F.n[chart[0]]
    X = np.array([1.0, 0.0, 0.0])
    if abs(n @ X) > 0.75:
        X = np.array([0.0, 1.0, 0.0])
    u = X - n * (n @ X)
    u /= np.linalg.norm(u)
    v = np.cross(n, u)
    if v[2] > 0 or (abs(v[2]) < 1e-6 and v[1] > 0):
        v = -v  # the skin's t runs down
    out = {}
    pts = F.P[F.tri[ids]]
    s = pts @ u * D
    t = pts @ v * D
    s0, t0 = s.min(), t.min()
    si, ti = np.round(s - s0).astype(int), np.round(t - t0).astype(int)
    for k, tr in enumerate(chart):
        for c in range(3):
            out[(tr, c)] = (int(si[k, c]), int(ti[k, c]))
    return out, int(si.max()) + 1, int(ti.max()) + 1


class Packer:
    """Rectangles laid top-left into the texels nothing uses, the skin growing downwards as needed."""

    def __init__(self, used):
        self.used = used.copy()
        self.dirty = True

    def place(self, w, h, from_y=0):
        W = self.used.shape[1]
        if w > W:
            raise ValueError("chart wider than the skin: %d" % w)
        while True:
            H = self.used.shape[0]
            if self.dirty:
                self.sat = np.zeros((H + 1, W + 1), np.int64)
                self.sat[1:, 1:] = self.used.cumsum(0).cumsum(1)
                self.dirty = False
            if h <= H:
                a = self.sat[h:, w:] - self.sat[:-h, w:] - self.sat[h:, :-w] + self.sat[:-h, :-w]
                free = np.argwhere(a == 0)
                if len(free):
                    y, x = free[0]
                    self.used[y:y + h, x:x + w] = True
                    self.dirty = True
                    return int(x), int(y)
            self.used = np.vstack([self.used, np.zeros((16, W), bool)])
            self.dirty = True


# ----------------------------------------------------------------------------
# Painting

def seg_dist(P, a, b):
    ab = b - a
    t = np.clip(((P - a) @ ab) / max(ab @ ab, 1e-9), 0.0, 1.0)
    return np.linalg.norm(P - (a + t[:, None] * ab), axis=1)


def creases(F, chart):
    """The chart's convex and concave edges (point pairs)."""
    inside = set(chart)
    cx, cc = [], []
    for t in chart:
        tv = F.tri[t]
        w = F.weld[tv]
        for i in range(3):
            k = (min(w[i], w[(i + 1) % 3]), max(w[i], w[(i + 1) % 3]))
            who = F.edges.get(k, ())
            others = [o for o in who if o != t]
            a, b = F.P[tv[i]], F.P[tv[(i + 1) % 3]]
            if not others:
                continue  # an open edge: mostly a T-junction across a flat panel, whose worn line would look wrong
            if any(o in inside for o in others):
                continue
            o = others[0]
            if F.n[o] @ F.n[t] > CREASE_COS:
                continue  # a shallow fold of the same panel: no edge to wear
            opp = F.P[F.tri[o]].mean(0)
            if (opp - a) @ F.n[t] > 1e-4:
                cc.append((a, b))
            else:
                cx.append((a, b))
    return cx, cc


def bake_ao(F, occluders, P, N, c_of, seed):
    """Each point's occlusion, 0 open .. 1 closed in: AO_RAYS cosine-weighted rays, turned at random about the normal per
    point, against the triangles `occluders` within AO_RANGE of its chart; a hit counts the more the nearer."""
    tris = F.P[F.tri[occluders]]
    lo, hi = tris.min(1), tris.max(1)
    golden = math.pi * (3.0 - math.sqrt(5.0))
    ks = (np.arange(AO_RAYS) + 0.5) / AO_RAYS
    rad, phi = np.sqrt(ks), golden * np.arange(AO_RAYS)
    out = np.zeros(len(P))
    jitter = hash3(np.arange(len(P)), np.zeros(len(P), np.int64), np.zeros(len(P), np.int64), seed + 40) * 2 * math.pi
    a = np.where(np.abs(N[:, 2:3]) < 0.9, np.array([[0.0, 0.0, 1.0]]), np.array([[1.0, 0.0, 0.0]]))
    e1 = np.cross(N, a)
    e1 /= np.maximum(np.linalg.norm(e1, axis=1, keepdims=True), 1e-9)
    e2 = np.cross(N, e1)
    order = np.argsort(c_of, kind="stable")
    starts = np.flatnonzero(np.r_[True, c_of[order][1:] != c_of[order][:-1]])
    for g0, g1 in zip(starts, np.r_[starts[1:], len(order)]):
        for i0 in range(g0, g1, 128):
            ids = order[i0:min(i0 + 128, g1)]
            box0, box1 = P[ids].min(0) - AO_RANGE, P[ids].max(0) + AO_RANGE
            near = np.nonzero(((hi >= box0) & (lo <= box1)).all(1))[0]
            if not len(near):
                continue
            A, B, C = tris[near, 0], tris[near, 1], tris[near, 2]
            E1, E2 = B - A, C - A
            ang = phi[None] + jitter[ids, None]
            z = np.sqrt(1.0 - ks)[None, :, None]
            d = (rad * np.cos(ang))[..., None] * e1[ids, None] + (rad * np.sin(ang))[..., None] * e2[ids, None] + z * N[ids, None]
            o = P[ids, None] + 0.03 * N[ids, None]
            d, o = d.reshape(-1, 3), np.broadcast_to(o, d.shape).reshape(-1, 3)
            pv = np.cross(d[:, None], E2[None])
            det = (pv * E1[None]).sum(2)
            det = np.where(np.abs(det) < 1e-9, 1e-9, det)
            tv = o[:, None] - A[None]
            u = (tv * pv).sum(2) / det
            qv = np.cross(tv, E1[None])
            v = (qv * d[:, None]).sum(2) / det
            t = (qv * E2[None]).sum(2) / det
            ok = (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 0.03) & (t < AO_RANGE)
            t = np.where(ok, t, AO_RANGE)
            w = 1.0 - t.min(1) / AO_RANGE
            out[ids] = w.reshape(len(ids), AO_RAYS).mean(1)
    return out


def paint(m, F, spec, charts, corner_st, mat, region, chart_base, rivets, old, protected, seed):
    """Fills the skin: returns (palette indices H x W, heights in model units H x W). `old`: old_of(tri, weights) the old
    paint's indices at those points (-1: none), old_ratio per triangle, means per triangle, occluders."""
    H, W = m.sh, m.sw
    tid = -np.ones((H, W), int)
    cid = -np.ones((H, W), int)
    Pm, Nm = np.zeros((H, W, 3)), np.zeros((H, W, 3))
    Dcx, Dcc = np.full((H, W), 9.0), np.full((H, W), 9.0)
    oldm = np.zeros((H, W), np.int16) - 1
    for ci, chart in enumerate(charts):
        lanes = []
        for t in chart:
            uv = np.array([corner_st[(t, c)] for c in range(3)], float) + 0.5
            corners = F.P[F.tri[t]]
            ns = np.array([F.smooth_normal(t, F.weld[F.tri[t, c]]) for c in range(3)])
            xs, ys, w0, w1, w2 = raster_tri(uv, 0.03)
            if len(xs) == 0:  # no texel middle inside: at least the texel under its first corner
                xs, ys = np.array([int(uv[0, 0])]), np.array([int(uv[0, 1])])
                w0, w1, w2 = np.ones(1), np.zeros(1), np.zeros(1)
            k = (xs >= 0) & (xs < W) & (ys >= 0) & (ys < H) & ~protected[ys, xs]
            xs, ys, w0, w1, w2 = xs[k], ys[k], w0[k], w1[k], w2[k]
            wv = np.stack([w0, w1, w2], 1)
            tid[ys, xs] = t
            cid[ys, xs] = ci
            Pm[ys, xs] = wv @ corners
            Nm[ys, xs] = wv @ ns
            oldm[ys, xs] = old["old_of"](t, wv)
            lanes.append((xs, ys))
        xs = np.concatenate([a for a, _ in lanes])
        ys = np.concatenate([b for _, b in lanes])
        cx, cc = creases(F, chart)
        Pc = Pm[ys, xs]
        dx = np.full(len(xs), 9.0)
        for a, b in cx:
            dx = np.minimum(dx, seg_dist(Pc, a, b))
        dc = np.full(len(xs), 9.0)
        for a, b in cc:
            dc = np.minimum(dc, seg_dist(Pc, a, b))
        Dcx[ys, xs], Dcc[ys, xs] = dx, dc

    # The gutter: each chart's edge texels spread one texel out.
    for rnd in range(CHART_MARGIN):
        empty = tid < 0
        for axis, d in ((0, 1), (0, -1), (1, 1), (1, -1)):
            src = np.roll(tid, d, axis=axis) >= 0
            k = empty & src & ~protected
            if axis == 0 and d == 1:
                k[0, :] = False
            if axis == 0 and d == -1:
                k[-1, :] = False
            if axis == 1 and d == 1:
                k[:, 0] = False
            if axis == 1 and d == -1:
                k[:, -1] = False
            ys, xs = np.nonzero(k)
            yo, xo = ys - (d if axis == 0 else 0), xs - (d if axis == 1 else 0)
            tid[ys, xs], cid[ys, xs] = tid[yo, xo], cid[yo, xo]
            Pm[ys, xs], Nm[ys, xs] = Pm[yo, xo], Nm[yo, xo]
            Dcx[ys, xs], Dcc[ys, xs] = Dcx[yo, xo], Dcc[yo, xo]
            oldm[ys, xs] = oldm[yo, xo]
            empty = tid < 0

    ys, xs = np.nonzero(tid >= 0)
    P = Pm[ys, xs]
    N = Nm[ys, xs]
    N /= np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)
    t_of = tid[ys, xs]
    c_of = cid[ys, xs]
    kind = mat[t_of]
    dcx, dcc = Dcx[ys, xs], Dcc[ys, xs]
    base = chart_base[c_of]
    rgb = np.zeros((len(P), 3))
    height = np.zeros(len(P))
    r1, r2, r3 = texel_rand(xs, ys, seed + 1), texel_rand(xs, ys, seed + 2), texel_rand(xs, ys, seed + 3)

    # -- what the old paint shows there
    old_idx = oldm[ys, xs].astype(int)
    src = PAL[np.clip(old_idx, 0, 255)]
    no_paint = (old_idx < 0) | (old_idx >= mp.FULLBRIGHT) | (old["ratio"][t_of] > OLD_RATIO)
    src[no_paint] = old["means"][t_of[no_paint]]
    # A sliver's few texels show a line of the old skin, often the dark round an island: a dark seam on the part. They take
    # the nearest wider face's paint in their chart.
    edges = F.P[F.tri[:, [1, 2, 0]]] - F.P[F.tri]
    longest = np.maximum(np.linalg.norm(edges, axis=2).max(1), 1e-6)
    wide = 2.0 * F.area / longest * spec["D"] >= SLIVER_TEXELS
    thin = ~wide[t_of]
    for ci in np.unique(c_of[thin]):
        here = np.nonzero(c_of == ci)[0]
        donors = here[wide[t_of[here]]]
        if not len(donors):
            continue
        for i in here[thin[here]]:
            j = donors[np.argmin((xs[donors] - xs[i]) ** 2 + (ys[donors] - ys[i]) ** 2)]
            src[i] = src[j]
    occl = bake_ao(F, old["occluders"], P, N, c_of, seed)

    # -- metal, steel, rivets
    me = kind != 1
    pm = P[me]
    nz = N[me, 2]
    src_m = src[me]
    lum_m = src_m @ np.array([0.3, 0.59, 0.11])
    cloud = fbm(pm * 0.24, 3, seed + 10) - 0.5
    mid = fbm(pm * 0.95, 2, seed + 11) - 0.5
    brush = fbm(pm * np.array([0.12, 2.3, 2.3]), 2, seed + 12) - 0.5
    grit = r1[me] - 0.5
    steelish = kind[me] == 2
    c = src_m.copy()
    c += DARK_LIFT * np.array([1.0, 0.98, 0.94])
    c *= (1 + 0.2 * cloud + 0.16 * mid + 0.2 * brush + 0.16 * grit)[:, None]
    c += (3.5 * grit)[:, None]
    c *= WARM
    if spec.get("detail"):
        c *= spec["detail"](P, N, kind == 0)[me][:, None]
    # Dark pits, and fine scratches along the part.
    pits = r2[me] > 0.965
    c[pits] *= 0.72
    sc = fbm(pm * np.array([0.05, 6.5, 6.5]), 2, seed + 13)
    sc_on = fbm(pm * 0.3, 2, seed + 14)
    scratch = (np.abs(sc - 0.5) < 0.008) & (sc_on > 0.6)
    c[scratch] += np.array([4.0, 4.0, 4.0])  # (faint: on near-black metal more reads as pale dashes under a strong light)
    # The relief: the painted detail's lines (lighter or darker than the part round them), grain, scratches. (Not the
    # rust: under a strong light its texel-sized bumps glinted as pale specks.)
    cm = c_of[me]
    n_of = np.maximum(np.bincount(cm, minlength=len(charts)), 1)
    chart_lum = np.bincount(cm, lum_m, minlength=len(charts)) / n_of
    h = 0.035 * np.clip((lum_m - chart_lum[cm]) / 40.0, -1.0, 1.0) + 0.012 * mid
    h[scratch] -= 0.015
    # Convex edges worn lighter, broken here and there: the shapes' outlines.
    d_x, d_c = dcx[me], dcc[me]
    wn = fbm(pm * 3.1, 2, seed + 15)
    worn = c * 1.5 + np.array([22.0, 21.0, 20.0])
    worn[kind[me] == 3] = c[kind[me] == 3] * 1.35 + 8.0  # (a rivet's small facets would be all edge)
    w1 = smooth(0.2, 0.03, d_x) * (0.4 + 0.6 * smooth(0.3, 0.6, wn)) * 0.68
    w2 = smooth(0.6, 0.2, d_x) * (r3[me] > 0.82) * 0.07
    c = mix(c, worn, np.maximum(w1, w2))
    # Rust: patches where the noise, the edges, the creases, the undersides and the rivets' runs say so.
    run = np.zeros(len(pm))
    ring = np.zeros(len(pm))
    for (rp, rn) in rivets:
        q = pm - rp
        d = np.linalg.norm(q, axis=1)
        ring = np.maximum(ring, smooth(0.78, 0.46, d))
        lat = np.hypot(q[:, 0], q[:, 1])
        down = -q[:, 2]
        run = np.maximum(run, np.exp(-(lat / 0.22) ** 2) * smooth(1.7, 0.1, down) * (down > -0.2))
    field = fbm(pm * 0.42 + 31.0, 4, seed + 16) + 0.10 * smooth(0.6, 0.0, d_x) + 0.13 * smooth(0.7, 0.0, d_c) \
        - 0.05 * nz + 0.14 * ring + 0.2 * run * (0.5 + wn)
    field = field - 0.1 * steelish
    # Each texel's surface area is the same: the threshold is the coverage's quantile.
    th = np.quantile(field, 1.0 - spec["rust"])
    rmask = smooth(th - 0.015, th + 0.06, field)
    rn = fbm(pm * 2.6, 3, seed + 17)
    # (A dark, saturated rust: under real light a lighter one read as a pale camouflage on the dark metal.)
    rc = mix(np.array([42.0, 23.0, 13.0]), np.array([72.0, 38.0, 20.0]), smooth(0.3, 0.7, rn))
    rc[r2[me] > 0.95] = np.array([88.0, 48.0, 25.0])
    # Its brightness tied to the metal under it: on near-black metal a fixed one is several times brighter, pale specks
    # under a strong light.
    rlum = rc @ np.array([0.3, 0.59, 0.11])
    rc = rc * (np.minimum(rlum, 1.9 * (c @ np.array([0.3, 0.59, 0.11])) + 10.0) / np.maximum(rlum, 1.0))[:, None]
    alpha = rmask * (0.68 + 0.32 * rn)
    # Freckles round the patches.
    fr = (field > th - 0.06) & (r3[me] > 0.96)
    alpha = np.where(fr, np.maximum(alpha, 0.45), alpha)
    alpha = np.where(steelish, alpha * 0.7, alpha)
    alpha = alpha * (1 - 0.65 * smooth(36.0, 60.0, lum_m))  # bright old metal (the rings, the rods) stays metal
    alpha = alpha * (0.55 + 0.45 * smooth(8.0, 28.0, lum_m))  # and the darkest parts stay dark
    c = mix(c, rc, alpha)
    # A dark ring round each rivet, on what it sits on: it stands out, and its own head stays lit.
    c[kind[me] != 3] *= (1 - 0.34 * ring[kind[me] != 3])[:, None]
    rgb[me] = c
    height[me] = h

    # -- wood: its tone the old paint's, the ridges' contrast with it
    wo = kind == 1
    if wo.any():
        ids = np.nonzero(wo)[0]
        for reg_key in {id(region[t_of[i]]) for i in ids}:
            sel = np.array([i for i in ids if id(region[t_of[i]]) == reg_key])
            r = region[t_of[sel[0]]]
            d = np.array(r["along"], float)
            d /= np.linalg.norm(d)
            e1 = np.cross(d, (0.0, 1.0, 0.0))
            e1 /= np.linalg.norm(e1)
            e2 = np.cross(d, e1)
            q = P[sel] - np.array(r["centre"])
            al, a, b = q @ d, q @ e1, q @ e2
            warp = fbm(np.stack([al * 0.16, a * 0.5, b * 0.5], 1), 3, seed + 20) - 0.5
            rad = np.hypot(a + 1.7, b + 0.9) + 1.1 * warp
            rings = (rad * 1.9) % 1.0
            late = rings ** 2.4
            fibres = fbm(np.stack([al * 0.45, a * 6.5, b * 6.5], 1), 2, seed + 21) - 0.5
            ramp = np.clip(0.3 + 0.42 * late + 0.75 * fibres + 0.14 * (r1[sel] - 0.5), 0.0, 1.0)
            dark, midc, light = np.array([34.0, 20.0, 11.0]), np.array([78.0, 46.0, 23.0]), np.array([118.0, 78.0, 41.0])
            cw = np.where((ramp < 0.5)[:, None], mix(dark, midc, ramp * 2), mix(midc, light, ramp * 2 - 1))
            lum = base[sel] @ np.array([0.3, 0.59, 0.11])
            cw = cw * np.clip(lum / 40.0, 0.3, 1.2)[:, None]
            # the painted lines and shadows on it
            cw = cw * np.clip((src[sel] @ np.array([0.3, 0.59, 0.11])) / np.maximum(lum, 8.0), 0.55, 1.3)[:, None]
            pores = texel_rand(np.floor(al * 1.1).astype(np.int64), np.round(a * 6 + b * 6 * 1.7).astype(np.int64), seed + 22) > 0.85
            cw[pores] *= 0.7
            cw *= (1 + 0.55 * smooth(0.25, 0.04, dcx[sel]) * (0.5 + 0.5 * r2[sel]))[:, None]
            rgb[sel] = cw
            height[sel] = -0.03 * (1 - late) - 0.02 * pores + 0.015 * fibres

    # -- the geometry's own corners and hollows
    rgb *= (1 - AO_STRENGTH * np.clip(occl * 1.5, 0.0, 1.0) ** 1.2)[:, None]

    # -- to the palette: the nearest colour, its brightness ordered-dithered (the chunky look)
    bayer = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0 - 0.47
    rgb = np.clip(rgb, 0, 255) * (1 + 0.08 * bayer[ys % 4, xs % 4])[:, None]
    cand = PAL[CANDIDATES]
    gc = cand - cand.mean(1, keepdims=True)
    out = np.zeros(len(rgb), np.uint8)
    for i0 in range(0, len(rgb), 20000):
        q = rgb[i0:i0 + 20000]
        gq = q - q.mean(1, keepdims=True)
        dist = ((q[:, None, :] - cand[None]) ** 2).sum(2) + 3.0 * ((gq[:, None, :] - gc[None]) ** 2).sum(2)
        out[i0:i0 + 20000] = CANDIDATES[np.argmin(dist, 1)]
    if spec.get("glow_transfer"):
        out = np.where(old_idx >= mp.FULLBRIGHT, old_idx, out).astype(np.uint8)
    skin = np.zeros((H, W), np.uint8)
    hh = np.zeros((H, W))
    skin[ys, xs] = out
    hh[ys, xs] = height
    # The heights a little soft: a texel's dither is no shape (under real light it read as snake skin).
    valid = (tid >= 0).astype(float)
    num, den = hh * valid, valid.copy()
    for ax, d in ((0, 1), (0, -1), (1, 1), (1, -1)):
        num = num + 0.5 * np.roll(hh * valid, d, axis=ax)
        den = den + 0.5 * np.roll(valid, d, axis=ax)
    hh = np.where(tid >= 0, num / np.maximum(den, 1e-6), 0.0)
    return skin, hh, dict(th=float(th), rust=float((alpha > 0.3).mean()), ao=float(occl.mean()))


# ----------------------------------------------------------------------------
# A model

def widen(m, width):
    if m.sw == width:
        return
    assert width > m.sw
    for sk in m.skins:
        for i, im in enumerate(sk[2]):
            a = np.frombuffer(bytes(im), np.uint8).reshape(m.sh, m.sw)
            b = np.zeros((m.sh, width), np.uint8)
            b[:, :m.sw] = a
            sk[2][i] = bytearray(b.tobytes())
    m.sw = width


def grow_rows(m, H):
    if H > m.sh:
        m.grow_skin(H - m.sh)


def add_copy(m, v):
    """A copy of a vertex (its bytes in every pose) at the end."""
    for blk in m.pose_blocks():
        vb = blk[2] if blk[0] == "simple" else blk[1]
        vb += bytes(vb[4 * v:4 * v + 4])
    m.st.append(list(m.st[v]))
    return len(m.st) - 1


def build(name, out_dir, tmp_dir):
    spec = dict(SPECS[name])
    src = os.path.join(SRC, name)
    data = json.load(open(src + ".json"))
    means, kept, parts = data["means"], data["kept"], data.get("parts", {})
    base = mp.Model(src)
    old_nv, old_nt, old_w, old_h = len(base.st), len(base.tris), base.sw, base.sh
    # The rivets first: appended, carried by their pieces; the swatches they ask for are painted over below.
    tmp = os.path.join(tmp_dir, "gl_" + name)
    p = mp.Polisher(src, rows=16)
    rivets = []
    if spec.get("rivets"):
        rivets = [(np.array(a, float), np.array(b, float)) for a, b in spec["rivets"](p)]
    p.finish(tmp)
    m = mp.Model(tmp)
    # Protected: every fullbright texel, and what the triangles that stay use, a texel of gutter round them.
    base_skin = np.frombuffer(bytes(base.skin()), np.uint8).reshape(old_h, old_w)
    keep = np.zeros(base_skin.shape, bool)
    for t in kept:
        if pose_areas(base, base.tris[t])[1] >= 1e-3:
            xs, ys, _ = tri_cover(base, base.tris[t], base_skin)
            keep[ys, xs] = True
    protected_old = keep.copy()
    for ax, d in ((0, 1), (0, -1), (1, 1), (1, -1)):
        protected_old |= np.roll(keep, d, axis=ax)
    protected_old |= base_skin >= mp.FULLBRIGHT
    F = Faces(m)
    mat, region = materials(m, F, spec, means, old_nt)
    kept_set = set(kept)
    faces = [t for t in range(len(m.tris)) if t not in kept_set and F.area[t] >= 1e-5]
    # A triangle with no area at rest that has none anywhere either has been left in kept; any other stays too (kept).
    faces_set = set(faces)
    stays = [t for t in range(len(m.tris)) if t not in faces_set]

    # What the old skin shows on each face, read through the old UVs, before they change; how stretched that was.
    old_uv = {t: mp.tri_uv(base, base.tris[t]) for t in faces if t < old_nt}
    old_ratio = np.zeros(len(m.tris))
    for t, uv in old_uv.items():
        A = F.P[F.tri[t]]
        e1, e2 = A[1] - A[0], A[2] - A[0]
        u = e1 / max(np.linalg.norm(e1), 1e-9)
        n = np.cross(e1, e2)
        ln = np.linalg.norm(n)
        if ln < 1e-9:
            continue
        v = np.cross(n / ln, u)
        M = np.array([[e1 @ u, e2 @ u], [e1 @ v, e2 @ v]])
        sv = np.linalg.svd(np.array([uv[1] - uv[0], uv[2] - uv[0]]).T @ np.linalg.inv(M), compute_uv=False)
        old_ratio[t] = sv[0] / max(sv[1], 1e-3)

    def old_of(t, wv):
        if t not in old_uv:
            return np.full(len(wv), -1, np.int16)
        q = wv @ old_uv[t]
        x, y = q[:, 0].astype(int) % old_w, np.clip(q[:, 1].astype(int), 0, old_h - 1)
        return base_skin[y, x].astype(np.int16)

    # Triangles that stay: their texels explicit (the skin's width changes what an onseam vertex means).
    claimed = {}
    new_tris = {t: list(m.tris[t]) for t in range(len(m.tris))}
    copies = 0

    def claim(v, st):
        nonlocal copies
        if v not in claimed:
            claimed[v] = st
            m.st[v] = [0, st[0], st[1]]
            return v
        if claimed[v] == st:
            return v
        key = (v,) + st
        if key in copy_of:
            return copy_of[key]
        n = add_copy(m, v)
        m.st[n] = [0, st[0], st[1]]
        copy_of[key] = n
        copies += 1
        return n

    copy_of = {}
    for t in stays:
        front, a, b, c = m.tris[t]
        for i, v in enumerate((a, b, c)):
            on, s, tt = m.st[v] if v not in claimed else (0, claimed[v][0], claimed[v][1])
            # (read the file's own st: a claimed vertex has been made explicit, so take the base's)
            if v < old_nv:
                on, s, tt = base.st[v]
            if on and not front:
                s += old_w // 2
            new_tris[t][i + 1] = claim(v, (s, tt))

    widen(m, spec["width"])
    # The charts, laid out.
    charts = make_charts(F, faces, mat)
    used = np.zeros((max(old_h, 16), m.sw), bool)
    used[:old_h, :old_w] = protected_old
    pk = Packer(used)
    placed = []
    for chart in charts:
        st, w, h = project(F, chart, spec["D"])
        placed.append((chart, st, w, h))
    order = sorted(range(len(placed)), key=lambda i: -placed[i][3])
    corner_st = {}
    for i in order:
        chart, st, w, h = placed[i]
        x, y = pk.place(w + 2 * CHART_MARGIN, h + 2 * CHART_MARGIN)
        for key, (s, t) in st.items():
            corner_st[key] = (s + x + CHART_MARGIN, t + y + CHART_MARGIN)
    # One chart's corners on one vertex share it; another chart's (or another texel's) use a copy.
    charts_of = {}
    for ci, chart in enumerate(charts):
        for t in chart:
            charts_of[t] = ci
    for t in faces:
        for i in range(3):
            new_tris[t][i + 1] = claim(int(F.tri[t, i]), corner_st[(t, i)])
    # A vertex that served several faces may now serve only faces its normal points away from (lit from behind): such
    # corners get the normal of those faces, on a copy (the old vertex's bytes stay as they were).
    table = mp.anorms()
    poses = [m.positions(pp) for pp in range(m.num_poses())]
    users = {}
    for t in faces:
        for i in range(3):
            users.setdefault(new_tris[t][i + 1], []).append((t, i))
    pose0 = np.frombuffer(bytes(m.pose_bytes(0)), np.uint8).reshape(-1, 4)
    turned = 0
    for v, who in users.items():
        if v < len(pose0) and all(table[pose0[v, 3]] @ F.n[t] < 0 for t, _ in who):
            n = add_copy(m, v)
            m.st[n] = list(m.st[v])
            for pp, blk in enumerate(m.pose_blocks()):
                sum_n = np.zeros(3)
                for t, _ in who:
                    A = poses[pp][F.tri[t]]
                    sum_n += np.cross(A[2] - A[0], A[1] - A[0])
                (blk[2] if blk[0] == "simple" else blk[1])[4 * n + 3] = int(np.argmax(table @ sum_n))
            for t, i in who:
                new_tris[t][i + 1] = n
            turned += 1
    m.tris = [tuple(new_tris[t]) for t in range(len(m.tris))]
    m.old_nv = len(m.st)
    H = pk.used.shape[0]
    grow_rows(m, H)
    # Skin rows and the mean colours of each chart
    means_all = np.array(list(means) + [RIVET_BASE.tolist()] * (len(m.tris) - old_nt), float)
    chart_base = np.zeros((len(charts), 3))
    for ci, chart in enumerate(charts):
        ids = np.array(chart)
        a = F.area[ids] + 1e-6
        chart_base[ci] = (means_all[ids] * a[:, None]).sum(0) / a.sum()
    # The protected mask at the final size.
    protected = np.zeros((m.sh, m.sw), bool)
    protected[:old_h, :old_w] = protected_old
    F2 = F  # (geometry as loaded: corners addressed by the triangles' old vertices)
    occluders = np.array([t for t in range(len(m.tris)) if t not in kept_set and F.area[t] >= 0.01])
    old = dict(old_of=old_of, ratio=old_ratio, means=means_all, occluders=occluders)
    skin, heights, stat = paint(m, F2, spec, charts, corner_st, mat, region, chart_base, rivets, old, protected, spec["seed"])
    # The final skin: the input's protected texels, the paint over the rest.
    final = np.zeros((m.sh, m.sw), np.uint8)
    final[protected] = np.pad(base_skin, ((0, m.sh - old_h), (0, m.sw - old_w)))[protected]
    final[~protected] = skin[~protected]
    # A face that came out in one palette index (the dark colours are far apart) is dithered with the nearest other.
    flat = 0
    for rnd in (0, 1):  # (twice: faces can share texels)
        for t in faces:
            xs, ys, *_ = raster_tri(mp.tri_uv(m, m.tris[t]), 0.0)
            if len(xs) >= FLAT_TEXELS and len(np.unique(final[ys, xs])) < 2 and final[ys[0], xs[0]] < mp.FULLBRIGHT:
                k = np.arange(len(xs)) % 2 == rnd
                dist = ((PAL[CANDIDATES] - PAL[final[ys[0], xs[0]]]) ** 2).sum(1)
                dist[CANDIDATES == final[ys[0], xs[0]]] = 1e18
                final[ys[k], xs[k]] = CANDIDATES[int(np.argmin(dist))]
                flat += 1
    m.skins[0][2][0] = bytearray(final.tobytes())
    path = os.path.join(out_dir, name)
    m.write(path)
    # The relief's dilated heights live beside the input.
    if spec.get("relief"):
        np.savez_compressed(os.path.join(SRC, name + ".relief.npz"), h=np.round(heights / 0.002).astype(np.int16),
                            density=np.array(spec["D"]))
    print("%s: %d new triangles (%d rivets), %d charts, %d vertex copies, skin %dx%d, %d vertices; rust %.0f%%, mean occlusion %.2f, %d faces broken up, %d normals turned" %
          (name, len(m.tris) - old_nt, len(rivets), len(charts), copies, m.sw, m.sh, len(m.st), 100 * stat["rust"], stat["ao"], flat, turned))
    for pn, tris in parts.items():
        # The part files: their triangles, plus the rivets in the body.
        tt = list(tris)
        if "pumpbody" in pn:
            tt += list(range(old_nt, len(m.tris)))
        write_subset(path, tt, os.path.join(out_dir, pn))
    return path


def write_subset(src, tris, path):
    """As polish_weapons.py's: `src` with only the triangles `tris` and the vertices they use."""
    m = mp.Model(src)
    used = sorted({v for t in tris for v in m.tris[t][1:]})
    remap = {v: i for i, v in enumerate(used)}
    m.st = [m.st[v] for v in used]
    m.tris = [(m.tris[t][0],) + tuple(remap[v] for v in m.tris[t][1:]) for t in tris]
    for blk in m.pose_blocks():
        vb = blk[2] if blk[0] == "simple" else blk[1]
        vb[:] = b"".join(bytes(vb[4 * v:4 * v + 4]) for v in used)
    m.old_nv = len(used)
    m.write(path)


def gl_old_triangles(name):
    return len(mp.Model(os.path.join(SRC, name)).tris)


MASK_FILE = os.path.join(HERE, "..", "..", "quakevr", "data", "weapons.txt")


def extend_masks(counts):
    """The gun's barrel mask (weapons.txt's fx_gfx_mask, "<triangles>:<bits>", six bits a character, vr_barrelmask.hpp)
    names its triangle count, or reads as none: extended over the new triangles (the rivets: not the barrel's)."""
    with open(MASK_FILE, newline="") as f:
        lines = f.read().split("\n")
    current = None
    done = 0
    for i, line in enumerate(lines):
        st = line.strip()
        if st.startswith('id "progs/'):
            current = st.split('"')[1][len("progs/"):]
        if st.startswith("fx_gfx_mask") and current in counts:
            head, value, tail = line.split('"')
            n, bits = value.split(":")
            n, want = int(n), counts[current]
            keep = (want + 5) // 6
            assert want >= gl_old_triangles(current) and not bits[keep:].strip("A"), "only the rivets' bits may go"
            bits = bits[:keep] + "A" * (keep - len(bits))
            lines[i] = '%s"%d:%s"%s' % (head, want, bits, tail)
            done += 1
    assert done == len(counts), "%d of %d masks found" % (done, len(counts))
    with open(MASK_FILE, "w", newline="") as f:
        f.write("\n".join(lines))


def relief(name):
    """For the bake (normaltiles.relief_heights): (heights, skin texels a height unit, the heights' texels per skin texel)."""
    d = np.load(os.path.join(SRC, os.path.basename(name) + ".relief.npz"))
    return d["h"].astype(np.float64) * 0.002, float(d["density"]), 1


def main():
    args = sys.argv[1:]
    if "--capture" in args:
        for name in SPECS:
            capture(name)
        return
    import tempfile
    import genguard
    names = [a for a in sys.argv[1:] if not a.startswith("-")] or list(SPECS)
    outs = []
    for n in names:
        outs.append(os.path.join(PROGS, n))
        outs += [os.path.join(PROGS, x) for x in SPECS[n].get("parts", ())]
    guard = genguard.Guard("gunlooks.py", outs)
    tmp = tempfile.mkdtemp(prefix="gl_")
    for n in names:
        build(n, PROGS, tmp)
    guard.finish()
    extend_masks({n: mp.Model(os.path.join(PROGS, n)).h[16] for n in names if n.startswith("v_")})


if __name__ == "__main__":
    main()
