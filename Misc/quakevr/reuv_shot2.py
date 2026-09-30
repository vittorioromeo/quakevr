#!/usr/bin/env python3
# reuv_shot2.py -- the double shotgun's fore-end, re-mapped and sealed (round 21 notes: "the UV mapping on the bottom
# and sides of the super shotgun is very stretched"; "a small hole near the bottom side, close to the trigger guard").
#
# Usage: python Misc/quakevr/reuv_shot2.py [model]   (default: quakevr/progs/v_shot2.mdl; rewrites it in place)
#        python Misc/quakevr/reuv_shot2.py --report [model]   (texel density and holes only, writes nothing)
# polish_weapons.py runs it on the v_shot2.mdl it writes, so rerunning that still gives the shipped file.
#
# The stretch. id's fore-end (the block under the barrels, x 12.6..26) and the block at its front (up to the muzzle,
# x 26..28.5) were mapped onto a 20 x 64 texel strip: their sides at about 2.3 texels per model unit, their bottom
# folded onto a line (one of its two triangles had no area in the skin), where the barrels, receiver and grip have
# 6.5..8. Each block is now unwrapped as one piece (least squares conformal: sides, chamfers and bottom unfolded
# round the block, the back faces beside them) at the barrels' density, laid in skin space nothing used (id's old
# hand texture corner, below the receiver's panels) and painted there in the fore-end's own dark browns: grain
# along the gun, the edges lit and worn as mdlpolish.edge_wear does.
#
# Only UVs change: a vertex has one UV in an alias model, so each block is one piece in the skin (no vertex is
# split, no triangle changed): vr_anchor.cpp's strip order, and so every anchor, stays (polish_weapons.py checks).
# The front face under the muzzle block (hidden between the two blocks) is left out of the unwrap.
#
# The hole. Round 16 removed the drooping handle that closed the fore-end's back; the knuckle put under the hinge
# (x 11..12.8) covers most of the opening, but not its bottom 0.4 units: seen from below and behind, the inside of
# the fore-end (culled: the background) showed between the knuckle and the trigger guard. That loop turns a corner
# into the barrels' breech (not flat, so seal_mdl.seal left it). It is split: the fore-end's back gets a flat cap
# painted like the fore-end, the breech part (hidden in the knuckle) a cap of its own; the six hinge-pin and bolt
# bases and the three zero-width cracks are closed too (seal_mdl.Sealer: copies of the corners, new triangles
# only, every frame's positions the corners' own bytes).

import math
import os
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import mdlpolish as mp  # noqa: E402
from check_mdl_holes import Topology  # noqa: E402
from improve_weapons import BAYER4, Mdl, Noise  # noqa: E402
from mdlgen import HEADER  # noqa: E402
from seal_mdl import Sealer, centroid, ear_clip, newell, plane_basis  # noqa: E402
from mdlgen import dot, norm, sub  # noqa: E402

DEFAULT = os.path.join(HERE, "..", "..", "quakevr", "progs", "v_shot2.mdl")

DENSITY = 6.5            # texels per model unit: the barrels have 6.7, the receiver 7.3
FOREEND_V = 14           # a vertex of each block (the two-handed grip anchor's corner, the muzzle block's)
BLOCK_V = 70
HIDDEN_TRIS = (32, 33)   # the fore-end's front face, inside the muzzle block: left out of the unwrap
# The fore-end's dark browns, darkest first (id painted it in 49 with streaks of these).
RAMP = [0, 49, 112, 143, 16, 17, 174, 18, 19]


# ----------------------------------------------------------------------------
# Measures

def tri_uv(m, t, st=None):
    st = st or m.st
    out = []
    for v in t[1:]:
        on, s, tt = st[v]
        if on and not t[0]:
            s += m.sw // 2
        out.append((s + 0.5, tt + 0.5))
    return np.array(out)


def stretch(P, uv):
    """(texels per unit: sqrt of the areas' ratio, the map's smallest and largest scale)."""
    e1, e2 = P[1] - P[0], P[2] - P[0]
    n = np.cross(e1, e2)
    a3 = np.linalg.norm(n) / 2
    f1, f2 = uv[1] - uv[0], uv[2] - uv[0]
    auv = abs(f1[0] * f2[1] - f1[1] * f2[0]) / 2
    if a3 < 1e-6:
        return 0.0, 0.0, 0.0, 0.0
    ex = e1 / np.linalg.norm(e1)
    ey = np.cross(n / (2 * a3), ex)
    X = np.array([[e1 @ ex, e2 @ ex], [e1 @ ey, e2 @ ey]])
    U = np.array([[f1[0], f2[0]], [f1[1], f2[1]]])
    sv = np.linalg.svd(U @ np.linalg.inv(X), compute_uv=False)
    return a3, math.sqrt(auv / a3), sv[1], sv[0]


def density_report(m, tris, label):
    P = np.array(m.frames[0][1])
    rows = [stretch(P[list(m.tris[i][1:])], tri_uv(m, m.tris[i])) for i in tris]
    rows = [r for r in rows if r[0] > 1e-4]
    A = np.array([r[0] for r in rows])
    d, lo, hi = (np.array([r[k] for r in rows]) for k in (1, 2, 3))
    return "%-22s %5.1f sq units: %.2f texels/unit (smallest axis %.2f, largest %.2f)" % (
        label, A.sum(), (d * A).sum() / A.sum(), (lo * A).sum() / A.sum(), (hi * A).sum() / A.sum())


def open_edges(m):
    """Edges of one triangle only, the vertices welded where they are at the same place in every frame and
    every triangle with three distinct corners counted (a crack's sliver, flat in frame 0, too): [(a, b)]."""
    weld = {}
    wid = [weld.setdefault(tuple(tuple(round(c, 4) for c in f[1][v]) for f in m.frames), len(weld))
           for v in range(len(m.st))]
    directed = {}
    for t in m.tris:
        a, b, c = (wid[v] for v in t[1:])
        if len({a, b, c}) == 3:
            for e in ((a, b), (b, c), (c, a)):
                directed[e] = directed.get(e, 0) + 1
    rep = {w: v for v, w in reversed(list(enumerate(wid)))}
    return [(rep[a], rep[b]) for (a, b), n in directed.items() if (b, a) not in directed]


def hole_report(m):
    T = Topology(m.tris, [f[1] for f in m.frames], tol=0.2)
    loops = [l for l in T.loops if len(l) >= 3]
    return "open edges %d, loops %d, cracks %d, flipped %d, stray open edges %d" % (
        len(open_edges(m)), len(loops), len(T.cracks), len(T.flipped), len(T.loops) - len(loops))


# ----------------------------------------------------------------------------
# Unwrapping

def lscm(P, tris):
    """Least squares conformal map of the triangles (vertex triples); {vertex: (u, v)} in model units."""
    verts = sorted({v for t in tris for v in t})
    col = {v: i for i, v in enumerate(verts)}
    n = len(verts)
    rows = []
    for a, b, c in tris:
        p = P[[a, b, c]]
        e1, e2 = p[1] - p[0], p[2] - p[0]
        nn = np.cross(e1, e2)
        area2 = np.linalg.norm(nn)
        ex = e1 / np.linalg.norm(e1)
        ey = np.cross(nn / area2, ex)
        z = [0j, complex(e1 @ ex, 0.0), complex(e2 @ ex, e2 @ ey)]
        W = [z[2] - z[1], z[0] - z[2], z[1] - z[0]]
        w = 1.0 / math.sqrt(area2)
        re, im = np.zeros(2 * n), np.zeros(2 * n)
        for k, v in enumerate((a, b, c)):
            i = col[v]
            re[i] += W[k].real * w
            re[n + i] -= W[k].imag * w
            im[i] += W[k].imag * w
            im[n + i] += W[k].real * w
        rows += [re, im]
    A = np.array(rows)
    # Pin the two corners farthest apart, at their distance.
    pts = P[verts]
    d = np.linalg.norm(pts[:, None] - pts[None], axis=2)
    i0, i1 = np.unravel_index(np.argmax(d), d.shape)
    fixed = {i0: (0.0, 0.0), i1: (d[i0, i1], 0.0)}
    fcols = [i0, n + i0, i1, n + i1]
    fvals = [fixed[i0][0], fixed[i0][1], fixed[i1][0], fixed[i1][1]]
    free = [k for k in range(2 * n) if k not in fcols]
    rhs = -A[:, fcols] @ np.array(fvals)
    x, *_ = np.linalg.lstsq(A[:, free], rhs, rcond=None)
    sol = np.zeros(2 * n)
    sol[free] = x
    sol[fcols] = fvals
    uv = {v: np.array([sol[col[v]], sol[n + col[v]]]) for v in verts}
    # Scale to the 3D area, and turn the gun's length (x) along s.
    a3 = auv = 0.0
    J = np.zeros(2)
    for a, b, c in tris:
        p = P[[a, b, c]]
        q = np.array([uv[a], uv[b], uv[c]])
        a3 += np.linalg.norm(np.cross(p[1] - p[0], p[2] - p[0])) / 2
        f1, f2 = q[1] - q[0], q[2] - q[0]
        auv += abs(f1[0] * f2[1] - f1[1] * f2[0]) / 2
        # How the net moves along x: the UV gradient of the x coordinate, summed.
        e1, e2 = p[1] - p[0], p[2] - p[0]
        M = np.array([[f1[0], f2[0]], [f1[1], f2[1]]])
        if abs(np.linalg.det(M)) > 1e-9:
            gx = np.linalg.solve(M.T, np.array([e1[0], e2[0]]))  # d x / d (u, v)
            J += gx * np.linalg.norm(np.cross(e1, e2))
    k = math.sqrt(a3 / auv)
    ang = -math.atan2(J[1], J[0])
    R = np.array([[math.cos(ang), -math.sin(ang)], [math.sin(ang), math.cos(ang)]]) * k
    return {v: R @ q for v, q in uv.items()}


def component(m, v0):
    return next(c for c in m.components() if v0 in c)


def chart_tris(m, comp, P):
    out = []
    for i, t in enumerate(m.tris):
        if t[1] in comp and i not in HIDDEN_TRIS:
            if np.linalg.norm(np.cross(P[t[2]] - P[t[1]], P[t[3]] - P[t[1]])) > 1e-6:
                out.append(i)
    return out




# ----------------------------------------------------------------------------
# The skin: rasterizing, packing, painting

def raster(tris_uv, W, H):
    """{texel (t, s): (triangle, barycentrics)} for the texel centres inside each triangle (texel space: a
    vertex's st + 0.5); a later triangle wins a texel."""
    own = {}
    for k, q in enumerate(tris_uv):
        lo = np.floor(q.min(0)).astype(int)
        hi = np.ceil(q.max(0)).astype(int)
        (x0, y0), (x1, y1), (x2, y2) = q
        det = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        if abs(det) < 1e-9:
            continue
        for t in range(max(0, lo[1]), min(H, hi[1] + 1)):
            for s in range(max(0, lo[0]), min(W, hi[0] + 1)):
                x, y = s + 0.5, t + 0.5
                b0 = ((y1 - y2) * (x - x2) + (x2 - x1) * (y - y2)) / det
                b1 = ((y2 - y0) * (x - x2) + (x0 - x2) * (y - y2)) / det
                b2 = 1 - b0 - b1
                if min(b0, b1, b2) >= -1e-6:
                    own[(t, s)] = (k, (b0, b1, b2))
    return own


def bary(q, x, y):
    (x0, y0), (x1, y1), (x2, y2) = q
    det = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
    b0 = ((y1 - y2) * (x - x2) + (x2 - x1) * (y - y2)) / det
    b1 = ((y2 - y0) * (x - x2) + (x0 - x2) * (y - y2)) / det
    return b0, b1, 1 - b0 - b1


def grow(mask, n):
    out = mask.copy()
    for _ in range(n):
        g = out.copy()
        g[1:] |= out[:-1]
        g[:-1] |= out[1:]
        g[:, 1:] |= out[:, :-1]
        g[:, :-1] |= out[:, 1:]
        out = g
    return out


def used_mask(m, skip, H):
    """The texels the other triangles map (and the engine's background texel at (0, 0))."""
    used = np.zeros((H, m.sw), bool)
    for i, t in enumerate(m.tris):
        if i in skip:
            continue
        q = tri_uv(m, t)
        for (tt, s) in raster([q], m.sw, H):
            used[tt, s] = True
        for s, tt in q.astype(int):  # thin and collapsed triangles: their corners
            used[min(tt, H - 1), min(s, m.sw - 1)] = True
    used[0:2, 0:2] = True
    return used


def place(used, shape_mask):
    """Top-most, then left-most offset (t, s) where the mask fits on free texels; None if none."""
    H, W = used.shape
    h, w = shape_mask.shape
    if h > H or w > W:
        return None
    F = np.fft.rfft2(used.astype(float), s=(H + h, W + w))
    G = np.fft.rfft2(shape_mask[::-1, ::-1].astype(float), s=(H + h, W + w))
    corr = np.fft.irfft2(F * G, s=(H + h, W + w))[h - 1:H, w - 1:W]  # overlap at each offset
    ok = np.argwhere(corr < 0.5)
    if not len(ok):
        return None
    t, s = ok[np.lexsort((ok[:, 1], ok[:, 0]))[0]]
    return int(t), int(s)


ORIENT = [(k, f) for f in (False, True) for k in range(4)]


def orient(q, k, f):
    q = q.copy()
    if f:
        q[:, 0] = -q[:, 0]
    for _ in range(k):
        q = np.stack([-q[:, 1], q[:, 0]], 1)
    return q


def snapped(net, verts, k, f):
    """The net in whole texels (vertex st), turned (k quarter turns, mirrored), from (2, 2)."""
    q = orient(np.array([net[v] for v in verts]) * DENSITY, k, f)
    return np.round(q - q.min(0)).astype(int) + 2


def island(m, tris, verts, st):
    col = {v: i for i, v in enumerate(verts)}
    H, W = st[:, 1].max() + 4, st[:, 0].max() + 4
    q = [np.array([st[col[v]] for v in m.tris[i][1:]], float) + 0.5 for i in tris]
    mask = np.zeros((H, W), bool)
    for (t, s) in raster(q, W, H):
        mask[t, s] = True
    return grow(mask, 2)


def pack(m, charts, squares, skip):
    """Lays the charts (tris, verts, net) and squares (sizes) in free skin space, growing the skin by the
    fewest rows (none if they fit). Returns (skin height, [chart st arrays], [square origins (s, t)])."""
    for extra in range(0, 256, 4):
        H = m.sh + extra
        used = grow(used_mask(m, skip, H), 1)
        placed, ok = [], True
        for tris, verts, net in charts:
            best = None
            for k, f in ORIENT:
                st = snapped(net, verts, k, f)
                mask = island(m, tris, verts, st)
                at = place(used, mask)
                if at and (best is None or (at[0] + mask.shape[0], at[1]) < best[0]):
                    best = ((at[0] + mask.shape[0], at[1]), at, st, mask)
            if best is None:
                ok = False
                break
            _, (t0, s0), st, mask = best
            used[t0:t0 + mask.shape[0], s0:s0 + mask.shape[1]] |= mask
            placed.append(st + np.array([s0, t0]))
        if not ok:
            continue
        origins = []
        for n in squares:
            at = place(used, np.ones((n + 2, n + 2), bool))
            if at is None:
                ok = False
                break
            used[at[0]:at[0] + n + 2, at[1]:at[1] + n + 2] = True
            origins.append((at[1] + 1, at[0] + 1))
        if ok:
            return H, placed, origins
    raise SystemExit("reuv_shot2: no room in the skin")


def feature_edges(P, tris):
    """Segments (a, b) where the surface turns a convex corner (over 30 degrees), or ends."""
    key = lambda p: tuple(np.round(p, 3))
    edges = {}
    for a, b, c in tris:
        n = np.cross(P[c] - P[a], P[b] - P[a])  # outward: clockwise seen from outside
        n = n / (np.linalg.norm(n) or 1.0)
        for x, y, z in ((a, b, c), (b, c, a), (c, a, b)):
            k = tuple(sorted((key(P[x]), key(P[y]))))
            edges.setdefault(k, []).append((n, P[z], P[x]))
    out = []
    for k, users in edges.items():
        if len(users) == 1:
            out.append(k)
        elif len(users) == 2:
            (n1, o1, p1), (n2, o2, p2) = users
            if n1 @ n2 < math.cos(math.radians(30)) and (o2 - p1) @ n1 < 0:
                out.append(k)
    return [(np.array(a), np.array(b)) for a, b in out]


def seg_dist(p, a, b):
    ab = b - a
    u = max(0.0, min(1.0, ((p - a) @ ab) / (ab @ ab)))
    return float(np.linalg.norm(p - (a + u * ab)))


def paint(skin, sw, m, P, tris, edges, seed=21):
    """Paints the texels the triangles map, and a one-texel margin, in the fore-end's style: its dark browns,
    grain along the gun (x), a few scratches, the convex edges' first texel row lit and the next one worn."""
    noise = Noise(seed)
    H = len(skin) // sw
    qs = [tri_uv(m, m.tris[i]) for i in tris]
    own = raster(qs, sw, H)
    # The margin: each free neighbour takes a covering neighbour's triangle (extrapolated).
    ring = {}
    for (t, s), (k, _) in own.items():
        for dt, ds in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)):
            n = (t + dt, s + ds)
            if n not in own and n not in ring and 0 <= n[0] < H and 0 <= n[1] < sw:
                ring[n] = (k, bary(qs[k], n[1] + 0.5, n[0] + 0.5))
    count = 0
    for (t, s), (k, b) in list(own.items()) + list(ring.items()):
        tri = m.tris[tris[k]][1:]
        p = sum(b[j] * P[tri[j]] for j in range(3))
        d = min(seg_dist(p, a, e) for a, e in edges) * DENSITY  # texels from the nearest edge
        v = 0.28 + 0.24 * (noise.smooth(p[0] * 0.7, (p[1] + p[2]) * 3.5) - 0.5)
        v += 0.08 * (noise.hash(s, t) - 0.5)
        if noise.hash(int(p[0] * 1.5), int((p[1] - p[2]) * DENSITY)) > 0.985:
            v += 0.2  # a scratch along the gun
        if d < 1.0:
            v += 0.3
        elif d < 2.0 and noise.hash(t * 7 + 3, s * 5 + 1) > 0.55:
            v += 0.12
        i = max(0.0, min(1.0, v)) * (len(RAMP) - 1)
        j = int(i)
        if i - j > (BAYER4[t % 4][s % 4] + 0.5) / 16.0:
            j += 1
        skin[t * sw + s] = RAMP[min(j, len(RAMP) - 1)]
        count += 1
    return count


def on_edges(m, loop, extra):
    """The loop with each of `extra` that lies on one of its edges (frame 0) put in between: id's flat triangle
    along the fore-end's bottom edge has a corner there, and the cap must run through it."""
    P = m.frames[0][1]
    out = []
    for a, b in zip(loop, loop[1:] + loop[:1]):
        out.append(a)
        ab = sub(P[b], P[a])
        L2 = dot(ab, ab)
        on = []
        for w in set(extra) - {a, b}:
            aw = sub(P[w], P[a])
            u = dot(aw, ab) / L2 if L2 else -1
            off = sub(aw, tuple(x * u for x in ab))
            if 1e-4 < u < 1 - 1e-4 and dot(off, off) < 1e-6:
                on.append((u, w))
        out += [w for _, w in sorted(on)]
    return out


def collapsed_loops(m):
    """Open loops (as a cap runs round them) whose corners are all at one place in frame 0."""
    nxt = {}
    for a, b in open_edges(m):
        nxt.setdefault(a, []).append(b)
    P = m.frames[0][1]
    loops, seen = [], set()
    for a in list(nxt):
        if a in seen:
            continue
        chain = [a]
        while nxt.get(chain[-1]) and nxt[chain[-1]][0] not in chain:
            chain.append(nxt[chain[-1]][0])
        seen |= set(chain)
        if len(chain) >= 3 and chain[0] in nxt.get(chain[-1], []) and max(math.dist(P[v], P[a]) for v in chain) < 1e-3:
            loops.append(chain[::-1])
    return loops


def cap_in_frame(sealer, loop, region):
    """Sealer.cap for a loop collapsed in frame 0: laid out in the frame where it is largest."""
    frames = sealer.m.frames
    f = max(range(len(frames)), key=lambda k: max(math.dist(frames[k][1][v], frames[k][1][loop[0]]) for v in loop))
    pts = [frames[f][1][v] for v in loop]
    u, v = plane_basis(norm(newell(pts)))
    c = centroid(pts)
    p2 = [(dot(sub(p, c), u), dot(sub(p, c), v)) for p in pts]
    s0, t0, s1, t1 = region
    ids = [sealer.copy(w, ((s0 + s1) // 2, (t0 + t1) // 2)) for w in loop]
    for i, j, k in ear_clip(p2):
        sealer.tri(ids[i], ids[j], ids[k])
    sealer.log.append("a flash's base (frame %d): %d corners" % (f + 1, len(loop)))


# ----------------------------------------------------------------------------
# Writing: the file's own bytes for every old vertex; a copy's bytes for each new one

def write(path, data, m, sources, skin, H):
    h = list(HEADER.unpack_from(data, 0))
    nv, nt, nf = h[15], h[16], h[17]
    off = HEADER.size + 4 + h[13] * h[14] + 12 * nv + 16 * nt
    h[14], h[15], h[16] = H, len(m.st), len(m.tris)
    out = bytearray(HEADER.pack(*h))
    out += struct.pack("<i", 0) + bytes(skin)
    for st in m.st:
        out += struct.pack("<3i", *st)
    for t in m.tris:
        out += struct.pack("<4i", *t)
    for f in range(nf):
        assert struct.unpack_from("<i", data, off)[0] == 0, "plain frames"
        out += data[off: off + 28 + 4 * nv]
        verts = off + 28
        for v in range(nv, len(m.st)):
            src = verts + 4 * sources[v]
            out += data[src: src + 3] + bytes((m.frames[f][2][v],))
        off += 28 + 4 * nv
    with open(path, "wb") as fh:
        fh.write(out)


# ----------------------------------------------------------------------------

def fix(path, report_only=False):
    data = open(path, "rb").read()
    m = Mdl(data)
    P = np.array(m.frames[0][1])
    n_v = len(m.st)
    charts, skip, lines = [], set(), []
    for name, v0 in (("fore-end", FOREEND_V), ("muzzle block", BLOCK_V)):
        comp = set(component(m, v0))
        tris = chart_tris(m, comp, P)
        skip |= {i for i, t in enumerate(m.tris) if t[1] in comp}
        verts = sorted({v for i in tris for v in m.tris[i][1:]})
        charts.append((name, tris, verts, lscm(P, [m.tris[i][1:] for i in tris])))
        lines.append(density_report(m, tris, name + " before"))
    lines.append("holes before: " + hole_report(m))
    if report_only:
        print("\n".join(lines))
        return
    BACK, HIDDEN = 32, 6
    old_sh = m.sh
    H, sts, (back_at, hidden_at) = pack(m, [c[1:] for c in charts], (BACK, HIDDEN), skip)
    skin = bytearray(m.skin) + bytearray(m.sw * (H - m.sh))
    # The blocks' flat triangles (a corner on an edge) follow their charted corners.
    charted = {v for c in charts for v in c[2]}
    chart_degenerate = [i for i in skip if i not in HIDDEN_TRIS and any(v not in charted for v in m.tris[i][1:])]
    for (name, tris, verts, net), st in zip(charts, sts):
        for v, (s, t) in zip(verts, st):
            m.st[v] = (0, int(s), int(t))
    m.sh = H
    for i in chart_degenerate:
        for v in m.tris[i][1:]:
            if v not in charted:
                m.st[v] = m.st[next(w for w in m.tris[i][1:] if w in charted)]

    # Seal, on the new UVs (the copies take their corners').
    sealer = Sealer(m)
    sources = {}
    plain_copy = sealer.copy

    def copy(v, st=None):
        w = plain_copy(v, st)
        sources[w] = sources.get(v, v)
        return w
    sealer.copy = copy
    sealer.fill_cracks()
    for v in range(n_v, len(m.st)):  # the cracks' slivers: one dark texel (they span the old and new UVs)
        m.st[v] = (0, hidden_at[0] + 3, hidden_at[1] + 3)
    back_region = (back_at[0], back_at[1], back_at[0] + BACK, back_at[1] + BACK)
    hidden_region = (hidden_at[0], hidden_at[1], hidden_at[0] + HIDDEN, hidden_at[1] + HIDDEN)
    in_back = lambda p: p[0] > 12.5 and 1.8 <= abs(p[1]) <= 1.95
    back_tris = []
    for loop in sealer.loops():
        if len(loop) < 3:
            continue
        if any(in_back(p) for p in sealer.points(loop)):
            run, rest = sealer.split(loop, in_back)
            run = on_edges(m, run, [v for i in chart_degenerate for v in m.tris[i][1:]])
            n0 = len(m.tris)
            sealer.cap(run, back_region, "the fore-end's back", max_off=0.3)
            back_tris = list(range(n0, len(m.tris)))
            sealer.cap(rest, hidden_region, "the breech (in the knuckle)", max_off=1.0)
        else:
            sealer.cap(loop, hidden_region, "a bolt's base")
    # What only opens in the firing frames: the muzzle flashes' bases (in the bores).
    for loop in collapsed_loops(m):
        cap_in_frame(sealer, loop, hidden_region)
    sealer.finish()
    assert all(v in sources for v in range(n_v, len(m.st))), "every new vertex is a copy"
    P = np.array(m.frames[0][1])

    # Paint: the charts, the back cap; the hidden caps plain.
    paint_tris = [i for c in charts for i in c[1]] + back_tris
    edges = feature_edges(P, [m.tris[i][1:] for i in paint_tris])
    texels = paint(skin, m.sw, m, P, paint_tris, edges)
    s0, t0, s1, t1 = hidden_region
    for t in range(t0, t1):
        for s in range(s0, s1):
            skin[t * m.sw + s] = 49
    m.skin = skin
    for name, tris, verts, net in charts:
        lines.append(density_report(m, tris, name + " after"))
    lines.append(density_report(m, back_tris, "fore-end back cap"))
    lines.append("skin %d x %d -> %d x %d, %d texels painted; %s" % (m.sw, old_sh, m.sw, H, texels, "; ".join(sealer.log)))
    write(path, data, m, sources, skin, H)
    lines.append("holes after: " + hole_report(Mdl(open(path, "rb").read())))
    print("  v_shot2.mdl re-mapped:\n    " + "\n    ".join(lines))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    fix(args[0] if args else DEFAULT, report_only="--report" in sys.argv)


if __name__ == "__main__":
    main()
