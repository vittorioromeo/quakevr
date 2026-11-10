#!/usr/bin/env python3
# make_ssg_open.py -- the super shotgun broken open (immersive reloading, phase 2b: docs/vr-port/RELOAD.md; QC
# vr_reload.qc; the engine's vr_view.cpp ssgParts): v_shot2.mdl cut in two at its hinge, written as
#
#   quakevr/progs/vr_ssg_frame_on_v_shot2.mdl    the frame: the grip, the trigger guard, the receiver; the barrels'
#                                                tapers into it cut off flat at BREECH_X (the standing breech: a steel
#                                                face with the two firing pins); the step under the barrels (the water
#                                                table) closed in front where the barrels' back meets it, the hinge's
#                                                knuckle round the hinge pin under that
#   quakevr/progs/vr_ssg_barrels_on_v_shot2.mdl  the barrels with the fore-end and the rear sight; their back (the breech
#                                                face) one flat steel face with the two bores open in it (short tubes, dark
#                                                inside): skin 0 both empty, 1 the left one (model +y) loaded, 2 both
#                                                loaded (the shells' brass heads and primers at the bores' bottoms)
#
# Both keep the gun's vertices, poses, scale and origin (so they lie on it: the engine draws them with the gun's place,
# the barrels turned about the hinge), its skin with rows added under it for the new faces. The gun closed is drawn as
# v_shot2.mdl itself; only an open (or opening, closing) gun is drawn in its two parts.
#
# The cut (2026-11-10, the author's marked screenshot of the open gun: no stray, floating or flickering faces, nothing
# open, the two parts meeting at the hinge from every side): a triangle whose middle is in front of x = CUT is the
# barrels', the rest the frame's; the tapers joining the barrels to the receiver (triangles across the cut higher than
# z 5) are the frame's up to BREECH_X (each cut there, a new vertex where an edge crosses), and the step's top under
# them is kept, so nothing opens between the parts seen from above; a small piece across the cut (a rivet by the
# hinge) goes whole to the part its middle is on. Every barrels' vertex just behind their back (rear_x, where the step
# ends) is moved onto it, so their back is one plane: its old faces there (the block's back, the rib's back, the
# step's front) give way to one breech face over their outline, round the rear sight's back (its lit inserts kept).
# Run after v_shot2.mdl changes (the .blend's export runs it, and bakes the normal maps), then check vr_view.cpp's ssg*
# constants (the hinge, the chambers' middle) still match. By hand: python Misc/quakevr/make_ssg_open.py, then python
# Misc/quakevr/bake_normals.py vr_ssg_frame_on_v_shot2.mdl vr_ssg_barrels_on_v_shot2.mdl. Checked by
# reload/ssg_checks.py openmodel (no holes round the hinge) and ring (the sight on the barrels).

import collections
import math
import os
import random
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import genguard  # noqa: E402
import mdlpolish as mp  # noqa: E402

PROGS = os.path.join(HERE, "..", "..", "quakevr", "progs")
SRC = os.path.join(PROGS, "v_shot2.mdl")
CUT = 12.5           # model x of the cut (the receiver's front)
TAPER_Z = 5.0        # triangles across the cut above this are the tapers (the frame's)
RING_BOX = (12.0, 12.7, 0.6, 6.95)  # x from, x to, |y| under, z over: the rear sight's ring (its whole piece moves; gone since 2026-11: the notch sight stands past CUT)
PIECE_MAX = 1.2      # a piece across the cut smaller than this (a rivet) goes whole to the part its middle is on
HINGE = (12.6, 3.2)  # (x, z) of the hinge pin: vr_view.cpp's ssgHinge
BREECH_X = 11.9      # the standing breech: the tapers cut off flat here (the model's x grid's nearest), low enough in
                     # front of the receiver to see into the opened chambers over it
STEP_TOP = 5.4       # the frame's step under the barrels (the water table) is under this: kept whole, the tapers over it cut
CHAMBERS = ((1.25, 7.0), (-1.25, 7.0))  # (y, z) of the chambers' (and the firing pins') middles
CHAMBER_R = 0.6      # the bores' radius at the breech face
BORE_SIDES = 8       # the bores' tubes: sides (the model's low-poly look)
BORE_STEPS = 2       # their depth in the model's x steps (0.227 each): you see into them a little, the loaded chambers'
                     # brass heads at their bottoms
KNUCKLE_SIDES = 8    # the hinge's knuckle round the pin: an octagonal tube between the frame's sides, its bottom level
KNUCKLE_R = 0.76     # with the barrels' underside (the hinge's height over it), so it fills the turning barrels' heel
PX = 12.0            # skin pixels per model unit on the new faces
ROWS = 72            # skin rows added for them

# Quake palette indices (gfx/palette.lmp): the gun's blued steel is the near-blacks 1..5 and the blue-greys 32..36;
# brass from the browns 28..31 and the olive 199 (make_shell.py's); the primers' greys.
STEEL = (2, 3, 33, 2, 34, 3)
EDGE = (35, 36, 5)
BORE = (0, 0, 1)
CHAMFER = (36, 5, 35)
BRASS = (30, 31, 199, 28)


def welded(P):
    key, weld = {}, np.zeros(len(P), int)
    for i, p in enumerate(P):
        weld[i] = key.setdefault(tuple(np.round(p, 3)), len(key))
    return weld


def small_pieces(P, T, size):
    """(triangle ids, middle) of every piece (triangles joined by vertices in the same place) smaller than `size`."""
    weld = welded(P)
    parent = list(range(weld.max() + 1))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a
    for t in T:
        a, b, c = (find(weld[v]) for v in t)
        parent[b] = a
        parent[find(c)] = a
    groups = collections.defaultdict(list)
    for i, t in enumerate(T):
        groups[find(weld[t[0]])].append(i)
    for tris in groups.values():
        q = P[T[tris]].reshape(-1, 3)
        lo, hi = q.min(0), q.max(0)
        if (hi - lo).max() < size:
            yield np.array(tris), (lo + hi) / 2


def classify(m):
    P = m.positions(0)
    T = np.array([t[1:] for t in m.tris])
    X = P[T][:, :, 0]
    C = P[T].mean(1)
    taper = (X.min(1) < CUT) & (X.max(1) > CUT) & (C[:, 2] >= TAPER_Z)
    barrels = (C[:, 0] > CUT) & ~taper
    # The rivets by the hinge (the author: they floated, cut in two, half on each part).
    for tris, mid in small_pieces(P, T, PIECE_MAX):
        if barrels[tris].any() and not barrels[tris].all():
            barrels[tris] = mid[0] > CUT
    # The rear sight's ring (a piece of its own on the barrels' rib, over the cut: x 12.14-12.6) goes with the barrels
    # (the author: it stayed on the frame and floated there as the gun broke open).
    mesh = mp.Mesh0(m)
    for ti in np.nonzero((C[:, 0] > RING_BOX[0]) & (C[:, 0] < RING_BOX[1]) & (np.abs(C[:, 1]) < RING_BOX[2]) &
                         (C[:, 2] > RING_BOX[3]))[0]:
        piece = mp.piece_of_tri(mesh, ti)
        if P[piece][:, 0].min() < RING_BOX[0] or P[piece][:, 0].max() > RING_BOX[1]:
            continue  # (a taper, part of the receiver's piece: not the ring)
        vs = set(piece.tolist())
        barrels |= np.array([t[0] in vs for t in T])
    return P, T, ~barrels, barrels


def open_loops(P, T, sel, keep_edge):
    """The loops of the open edges of the triangles `sel` whose ends pass keep_edge(p): lists of points."""
    weld = welded(P)
    rep = {}
    for i in range(len(P)):
        rep.setdefault(weld[i], i)
    cnt = collections.Counter()
    for t in T[sel]:
        a, b, c = weld[t]
        for e in ((a, b), (b, c), (c, a)):
            cnt[tuple(sorted(e))] += 1
    adj = collections.defaultdict(list)
    for (a, b), n in cnt.items():
        if n == 1 and keep_edge(P[rep[a]]) and keep_edge(P[rep[b]]):
            adj[a].append(b)
            adj[b].append(a)
    loops, seen = [], set()
    for s in adj:
        if s in seen:
            continue
        loop, prev, cur = [s], None, s
        seen.add(s)
        while True:
            nxt = [n for n in adj[cur] if n != prev and n not in seen]
            if not nxt:
                break
            prev, cur = cur, nxt[0]
            loop.append(cur)
            seen.add(cur)
        loops.append([P[rep[w]].copy() for w in loop])
    return loops


def area2(poly):
    """Twice the signed area of a (y, z) polygon."""
    return sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(poly, poly[1:] + poly[:1]))


def segs_cross(a, b, c, d):
    """Whether segments ab and cd cross at a point that is not an end of both."""
    def orient(p, q, r):
        return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])
    if min(np.hypot(*np.subtract(a, c)), np.hypot(*np.subtract(a, d)), np.hypot(*np.subtract(b, c)),
           np.hypot(*np.subtract(b, d))) < 1e-6:
        return False
    o1, o2, o3, o4 = orient(a, b, c), orient(a, b, d), orient(c, d, a), orient(c, d, b)
    return (o1 * o2 < 0) and (o3 * o4 < 0)


def bridge(outer, holes):
    """One polygon from `outer` (counter-clockwise) and its holes (each turned clockwise): each hole joined to the
    outline by a cut to the nearest outline point it can see."""
    poly = list(outer)
    for hole in holes:
        hole = list(hole) if area2(list(hole)) < 0 else list(reversed(hole))
        edges = [(poly[i], poly[(i + 1) % len(poly)]) for i in range(len(poly))]
        for h in holes:
            edges += [(h[i], h[(i + 1) % len(h)]) for i in range(len(h))]
        best = None
        for i, o in enumerate(poly):
            for k, h in enumerate(hole):
                d = np.hypot(o[0] - h[0], o[1] - h[1])
                if best is not None and d >= best[0]:
                    continue
                if any(segs_cross(o, h, e0, e1) for e0, e1 in edges):
                    continue
                best = (d, i, k)
        assert best is not None, "a bore can't be joined to the breech face's outline"
        _, i, k = best
        ring = hole[k:] + hole[:k] + [hole[k]]
        poly = poly[:i + 1] + ring + poly[i:]
    return poly


def ear_clip(pts):
    """Triangles (index triples into `pts`, counter-clockwise) filling a simple (y, z) polygon, holes bridged in; each
    step clips the ear with the widest smallest angle."""
    idx = list(range(len(pts)))
    if area2([pts[i] for i in idx]) < 0:
        idx.reverse()
    P = [np.asarray(p, np.float64) for p in pts]
    tris = []

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    def min_angle(a, b, c):
        out = []
        for p, q, r in ((a, b, c), (b, c, a), (c, a, b)):
            u, v = q - p, r - p
            out.append(math.acos(max(-1.0, min(1.0, u @ v / (np.linalg.norm(u) * np.linalg.norm(v) + 1e-12)))))
        return min(out)

    while len(idx) > 3:
        n = len(idx)
        best = None
        for k in range(n):
            a, b, c = idx[k - 1], idx[k], idx[(k + 1) % n]
            A, B, Cc = P[a], P[b], P[c]
            if cross(A, B, Cc) <= 1e-9:
                continue
            ok = True
            for j in idx:
                Q = P[j]
                if min(np.linalg.norm(Q - A), np.linalg.norm(Q - B), np.linalg.norm(Q - Cc)) < 1e-6:
                    continue
                if cross(A, B, Q) >= -1e-9 and cross(B, Cc, Q) >= -1e-9 and cross(Cc, A, Q) >= -1e-9:
                    ok = False
                    break
            if ok:
                q = min_angle(A, B, Cc)
                if best is None or q > best[0]:
                    best = (q, k)
        if best is None:
            # (only points on straight runs left to clip: drop one, the face is flat)
            for k in range(n):
                a, b, c = idx[k - 1], idx[k], idx[(k + 1) % n]
                if abs(cross(P[a], P[b], P[c])) <= 1e-9:
                    idx.pop(k)
                    break
            else:
                raise ValueError("the breech face's outline can't be filled (not a simple polygon)")
            continue
        k = best[1]
        tris.append((idx[k - 1], idx[k], idx[(k + 1) % n]))
        idx.pop(k)
    if abs(cross(P[idx[0]], P[idx[1]], P[idx[2]])) > 1e-9:
        tris.append(tuple(idx))
    return tris


class Builder:
    """New vertices and triangles on a part, carried per pose by `carrier`, skin coordinates given per vertex."""

    def __init__(self, m, carrier):
        self.m, self.c, self.n = m, carrier, m.num_poses()
        self.tris = []

    def vert(self, p, nrm, s, t):
        return self.m.add_vertex(round(s), round(t), [self.c.place(p, k) for k in range(self.n)],
                                 [self.c.turn(nrm, k) for k in range(self.n)])

    def tri(self, a, b, c, pa, pb, pc, outward):
        if mp.tri_normal(np.asarray(pa), np.asarray(pb), np.asarray(pc)) @ np.asarray(outward) < 0:
            b, c = c, b
        self.tris.append((1, a, b, c))

    def face(self, pts3, tri_ids, outward, uv):
        """A flat face: its own vertices (a hard edge round it), triangles over them."""
        ids = [self.vert(p, outward, *uv(p)) for p in pts3]
        for a, b, c in tri_ids:
            self.tri(ids[a], ids[b], ids[c], pts3[a], pts3[b], pts3[c], outward)


def move_verts(m, carrier, verts, x):
    """Moves the old vertices `verts` to model x = `x` in pose 0, by the same shift (carried) in every pose."""
    P0 = m.positions(0)
    for k in range(m.num_poses()):
        vb = m.pose_bytes(k)
        Pk = m.positions(k)
        for i in verts:
            d = carrier.turn(np.array([x - P0[i][0], 0.0, 0.0]), k)
            q = np.round((Pk[i] + d - m.origin) / m.scale).astype(int)
            assert (q >= 0).all() and (q <= 255).all(), "a moved vertex left the model's grid"
            vb[4 * i:4 * i + 3] = bytes(int(v) for v in q)


def paint(img, sw, s0, t0, w, h, ymax, zmax, pixel):
    for t in range(t0, t0 + h):
        for s in range(s0, s0 + w):
            img[t * sw + s] = pixel(ymax - (s - s0 + 0.5) / PX, zmax - (t - t0 + 0.5) / PX)


def edge_distance(poly, y, z):
    d = 1e9
    for i in range(len(poly)):
        (ay, az), (by, bz) = poly[i], poly[(i + 1) % len(poly)]
        ey, ez = by - ay, bz - az
        k = max(0.0, min(1.0, ((y - ay) * ey + (z - az) * ez) / (ey * ey + ez * ez + 1e-12)))
        d = min(d, math.hypot(y - ay - k * ey, z - az - k * ez))
    return d


def steel(rng, poly, y, z):
    if edge_distance(poly, y, z) < 0.14:
        return rng.choice(EDGE)
    return rng.choice(STEEL)


def head_pixel(rng, r, loaded):
    """A bore's bottom at r from its middle: black, or a loaded shell's brass head (its rim a shade darker) with its
    primer."""
    if not loaded:
        return rng.choice(BORE)
    if r < 0.17:
        return 10 if rng.random() < 0.6 else 9
    if r < 0.23:
        return 4
    if r > CHAMBER_R - 0.09:
        return 28 if rng.random() < 0.7 else 30
    return rng.choice(BRASS)


def breech_pixel(rng, poly, y, z):
    """The standing breech: blued steel with the two firing pins (a dark hole in a bright bushing) where the chambers
    close on it, and the extractor's slot below between them."""
    for cy, cz in CHAMBERS:
        r = math.hypot(y - cy, z - cz)
        if r < 0.11:
            return 1
        if r < 0.3:
            return rng.choice(CHAMFER)
    if abs(y) < 0.45 and 6.3 < z < 6.48:
        return 1
    return steel(rng, poly, y, z)


def octagon(cy, cz, ry, rz, sides):
    """A ring of `sides` points round (cy, cz), flat at the bottom and top."""
    return [(cy + ry * math.cos(a), cz + rz * math.sin(a))
            for a in (2 * math.pi * (k + 0.5) / sides - math.pi / 2 for k in range(sides))]


def mp_grid(m, v, axis):
    """The model grid's nearest coordinate to `v` on `axis`."""
    return m.origin[axis] + round((v - m.origin[axis]) / m.scale[axis]) * m.scale[axis]


def clip_tris(m, T, ids, x0, builder):
    """The old triangles `ids` of m cut at model x = x0, their parts behind it kept (into builder.tris; new vertices
    where their edges cross the plane, shared by neighbours with the same skin place, carried per pose between the
    edge's ends); the cut's segments ((y, z) pairs)."""
    n = m.num_poses()
    poses = [m.positions(k) for k in range(n)]
    P0 = poses[0]
    table = np.array(mp.anorms())
    nidx = [np.frombuffer(bytes(m.pose_bytes(k)), np.uint8).reshape(-1, 4)[: m.old_nv, 3] for k in range(n)]
    made, segs = {}, []
    for ti in ids:
        ff, a, b, c = m.tris[ti]
        vs = (a, b, c)
        assert all(abs(P0[v][0] - x0) > 1e-4 for v in vs), "a taper's vertex on the standing breech's plane"
        inside = [P0[v][0] < x0 for v in vs]
        if not any(inside):
            continue

        def st(v):
            s = m.st[v][1] + (m.sw // 2 if m.st[v][0] and not ff else 0)
            return s, m.st[v][2]
        poly, cut = [], []
        for k in range(3):
            u, w = vs[k], vs[(k + 1) % 3]
            if inside[k]:
                poly.append(u)
            if inside[k] != inside[(k + 1) % 3]:
                f = (x0 - P0[u][0]) / (P0[w][0] - P0[u][0])
                (su, tu), (sw_, tw) = st(u), st(w)
                s, t = su + f * (sw_ - su), tu + f * (tw - tu)
                key = (min(u, w), max(u, w), round(s), round(t))
                if key not in made:
                    made[key] = m.add_vertex(round(s), round(t), [poses[k2][u] + f * (poses[k2][w] - poses[k2][u])
                                                                  for k2 in range(n)],
                                             [table[nidx[k2][u]] + f * (table[nidx[k2][w]] - table[nidx[k2][u]])
                                              for k2 in range(n)])
                poly.append(made[key])
                p = P0[u] + f * (P0[w] - P0[u])
                cut.append((float(p[1]), float(p[2])))
        for k in range(1, len(poly) - 1):
            builder.tris.append((ff, poly[0], poly[k], poly[k + 1]))
        assert len(cut) == 2
        segs.append(tuple(cut))
    return segs


def chain_segments(segs):
    """The closed loops (lists of (y, z)) the segments make, end to end."""
    key = lambda p: (round(p[0], 4), round(p[1], 4))  # noqa: E731
    adj = collections.defaultdict(list)
    pt = {}
    for a, b in segs:
        ka, kb = key(a), key(b)
        if ka == kb:
            continue
        pt[ka], pt[kb] = a, b
        adj[ka].append(kb)
        adj[kb].append(ka)
    assert all(len(v) == 2 for v in adj.values()), "the cut through the tapers isn't closed"
    loops, seen = [], set()
    for s in adj:
        if s in seen:
            continue
        loop, prev, cur = [s], None, s
        seen.add(s)
        while True:
            nxt = [k for k in adj[cur] if k != prev and k not in seen]
            if not nxt:
                break
            prev, cur = cur, nxt[0]
            loop.append(cur)
            seen.add(cur)
        loops.append([pt[k] for k in loop])
    return loops


def build(src=SRC, out_dir=PROGS, guarded=True):
    """The two halves of `src`, written into out_dir. Since 2026-11 run by v_shot2.blend's export (source.py SPECS,
    guarded=False: from the model it exports); run by hand, genguard skips the files the .blend owns."""
    out_b = os.path.join(out_dir, "vr_ssg_barrels_on_v_shot2.mdl")
    out_f = os.path.join(out_dir, "vr_ssg_frame_on_v_shot2.mdl")
    guard = genguard.Guard("make_ssg_open.py", [out_b, out_f]) if guarded else None
    base = mp.Model(src)
    P, T, frame_sel, barrel_sel = classify(base)
    near = lambda p: CUT - 0.5 < p[0] < 13.2  # noqa: E731
    rear_x = max(p[0] for loop in open_loops(P, T, barrel_sel, lambda p: near(p) and p[2] > 4.9) for p in loop)
    on = lambda p: abs(p[0] - rear_x) < 1e-3  # noqa: E731
    xstep = base.scale[0]

    # ---- The barrels: their back made one plane at rear_x, its faces there given way to one breech face.
    bm = mp.Model(src)
    t0 = bm.grow_skin(ROWS)
    bverts = np.unique(T[barrel_sel].ravel())
    carrier = mp.Carrier(bm, bverts)
    moved = [i for i in bverts if CUT - 0.5 < P[i][0] < rear_x - 1e-3]
    move_verts(bm, carrier, moved, rear_x)
    Q = bm.positions(0)
    inplane = np.array([all(on(Q[v]) for v in t) for t in T])
    bsel = barrel_sel & ~inplane
    loops = [lp for lp in open_loops(Q, T, bsel, on) if len(lp) > 2]
    yz = [[(p[1], p[2]) for p in lp] for lp in loops]
    main = max(yz, key=lambda lp: abs(area2(lp)))
    ztop = max(z for _, z in main)
    sight = max(yz, key=lambda lp: max(z for _, z in lp))
    assert max(z for _, z in sight) > ztop, "the rear sight doesn't stand over the barrels' back"
    # The notch between the barrels' tops (under the sight): the outline's dip at the middle, between two top corners.
    k = min((i for i, (y, z) in enumerate(main) if abs(y) < 0.3), key=lambda i: main[i][1])
    n = len(main)
    vr, vb, vl = main[(k - 1) % n], main[k], main[(k + 1) % n]
    if vr[0] < vl[0]:
        vr, vl = vl, vr
    assert abs(vr[1] - ztop) < 1e-3 and abs(vl[1] - ztop) < 1e-3 and vb[1] < ztop - 0.3, (vr, vb, vl)
    # The rear sight's back (its lit inserts) stays: the face goes round under it, filling the dip below it (where the
    # rib's back was).
    sy0, sy1 = min(y for y, _ in sight), max(y for y, _ in sight)
    sz0 = min(z for _, z in sight)
    keep_sight = inplane & np.array([all(on(Q[v]) and Q[v][2] > sz0 - 1e-3 and sy0 - 1e-3 < Q[v][1] < sy1 + 1e-3
                                         for v in t) for t in T])
    bsel |= barrel_sel & keep_sight
    assert vl[0] < sy0 < vb[0] < sy1 < vr[0] and vb[1] < sz0 < ztop, (vl, vb, vr, sight)

    def on_dip(end, y):  # (the dip's side from its bottom to `end`, where it is at y)
        return (y, vb[1] + (end[1] - vb[1]) * (y - vb[0]) / (end[0] - vb[0]))
    i = main.index(vb)
    seq = main[i + 1:] + main[:i]  # (from the dip's one neighbour round to its other)
    if seq[0] != vl:
        seq = list(reversed(seq))
    assert seq[0] == vl and seq[-1] == vr
    # (vl round the barrels to vr, down the dip's side to the sight, under it, up the other side back to vl)
    outline = seq + [on_dip(vr, sy1), (sy1, sz0), (sy0, sz0), on_dip(vl, sy0)]
    if area2(outline) < 0:
        outline.reverse()
    bores = [octagon(cy, cz, CHAMBER_R, CHAMBER_R, BORE_SIDES) for cy, cz in CHAMBERS]
    for b in bores:
        assert all(inside_poly(outline, *p) and edge_distance(outline, *p) > 0.05 for p in b), "a bore outside the face"
    poly = bridge(outline, bores)
    tris = ear_clip(poly)

    ys = [p[0] for p in outline]
    zs = [p[1] for p in outline]
    cap_ymax, cap_zmax = max(ys) + 0.1, max(zs) + 0.1
    cap_w = int(math.ceil((cap_ymax - min(ys) + 0.1) * PX)) + 1
    cap_h = int(math.ceil((cap_zmax - min(zs) + 0.1) * PX)) + 1
    depth = BORE_STEPS * xstep
    side = 2 * CHAMBER_R * math.sin(math.pi / BORE_SIDES)
    wall_w = int(math.ceil(side * BORE_SIDES * PX)) + 1
    wall_h = int(math.ceil(depth * PX)) + 1
    disk = int(math.ceil((2 * CHAMBER_R + 0.2) * PX)) + 1
    s_w = cap_w + 4
    assert cap_h <= ROWS and 2 * (wall_h + 2) + disk <= ROWS and s_w + max(wall_w, 2 * disk + 2) <= bm.sw, \
        (cap_w, cap_h, wall_w, wall_h, disk)

    bb = Builder(bm, carrier)
    bb.face([(rear_x, y, z) for y, z in poly], tris, (-1.0, 0.0, 0.0),
            lambda p: ((cap_ymax - p[1]) * PX, t0 + (cap_zmax - p[2]) * PX))
    walls = []
    for bi, (b, (cy, cz)) in enumerate(zip(bores, CHAMBERS)):
        # The tube: the bore's sides from the breech face in to its bottom (facing in), shared vertices round it (one
        # seam), then the bottom (facing back).
        tw = t0 + bi * (wall_h + 2)
        ring = b + b[:1]
        lens = [0.0]
        for a, c in zip(ring, ring[1:]):
            lens.append(lens[-1] + math.hypot(c[0] - a[0], c[1] - a[1]))
        ids = []
        for (y, z), L in zip(ring, lens):
            nrm = (0.0, cy - y, cz - z)
            ids.append((bb.vert((rear_x, y, z), nrm, s_w + L * PX, tw),
                        bb.vert((rear_x + depth, y, z), nrm, s_w + L * PX, tw + depth * PX)))
        for j in range(BORE_SIDES):
            (a0, a1), (c0, c1) = ids[j], ids[j + 1]
            (ya, za), (yc, zc) = ring[j], ring[j + 1]
            inward = (0.0, cy - (ya + yc) / 2, cz - (za + zc) / 2)
            pa0, pa1, pc0, pc1 = (rear_x, ya, za), (rear_x + depth, ya, za), (rear_x, yc, zc), (rear_x + depth, yc, zc)
            bb.tri(a0, a1, c1, pa0, pa1, pc1, inward)
            bb.tri(a0, c1, c0, pa0, pc1, pc0, inward)
        sd = s_w + bi * (disk + 2)
        td = t0 + 2 * (wall_h + 2)
        ymax_d, zmax_d = cy + CHAMBER_R + 0.1, cz + CHAMBER_R + 0.1
        bottom = [(rear_x + depth, y, z) for y, z in b]
        bb.face(bottom, ear_clip(b), (-1.0, 0.0, 0.0),
                lambda p, sd=sd, td=td, ymax_d=ymax_d, zmax_d=zmax_d: (sd + (ymax_d - p[1]) * PX,
                                                                       td + (zmax_d - p[2]) * PX))
        walls.append((tw, sd, td, ymax_d, zmax_d, cy, cz))
    bm.tris = [base.tris[i] for i in np.nonzero(bsel)[0]] + bb.tris
    skins = []
    for loaded in ((False, False), (True, False), (True, True)):
        rng = random.Random(4100)  # (the same steel on every skin)
        img = bytearray(bm.skin(0))

        def cap_pixel(y, z):
            for cy, cz in CHAMBERS:
                if math.hypot(y - cy, z - cz) < CHAMBER_R + 0.12:
                    return rng.choice(CHAMFER)  # (the bores' mouths: a bright chamfer)
            return steel(rng, outline, y, z)
        paint(img, bm.sw, 0, t0, cap_w, cap_h, cap_ymax, cap_zmax, cap_pixel)
        for bi, (tw, sd, td, ymax_d, zmax_d, cy, cz) in enumerate(walls):
            for t in range(tw, tw + wall_h):
                f = (t - tw + 0.5) / max(1.0, depth * PX)  # (0 at the mouth, 1 at the bottom)
                for s in range(s_w, s_w + wall_w):
                    img[t * bm.sw + s] = rng.choice(CHAMFER) if f < 0.15 else (rng.choice((5, 4, 1)) if f < 0.5 else
                                                                               rng.choice(BORE))
            paint(img, bm.sw, sd, td, disk, disk, ymax_d, zmax_d,
                  lambda y, z, cy=cy, cz=cz, bi=bi: head_pixel(rng, math.hypot(y - cy, z - cz), loaded[bi]))
        skins.append([0, b"", [img]])
    bm.skins = skins
    bm.h[12] = len(skins)  # (the header's skin count)
    bm.write(out_b)
    mid = np.array([rear_x, sum(c[0] for c in CHAMBERS) / 2, sum(c[1] for c in CHAMBERS) / 2])

    # ---- The frame: the tapers cut off flat at the standing breech (BREECH_X), the face there with the firing pins;
    # the step under the barrels (the water table, its top kept) closed in front at rear_x; the knuckle round the pin.
    fm = mp.Model(src)
    t0f = fm.grow_skin(ROWS)
    front = [i for i in np.unique(T[frame_sel].ravel()) if P[i][0] > 9.0]
    fcarrier = mp.Carrier(fm, front)
    bx = mp_grid(fm, BREECH_X, 0)
    Z = P[T][:, :, 2]
    cone = frame_sel & (P[T][:, :, 0].max(1) > bx + 1e-3) & (Z.max(1) > STEP_TOP) & (P[T].mean(1)[:, 0] > CUT - 1.6)
    fb = Builder(fm, fcarrier)
    segs = clip_tris(fm, T, np.nonzero(cone)[0], bx, fb)
    rings = chain_segments(segs)
    assert rings, "the tapers don't cross the standing breech's plane"
    floops = [[(p[1], p[2]) for p in lp] for lp in open_loops(P, T, frame_sel & ~cone, on) if len(lp) > 2]
    step = max(floops, key=lambda lp: abs(area2(lp)))
    sp = step if area2(step) > 0 else list(reversed(step))
    zs_ = max(z for _, z in sp)
    assert zs_ < STEP_TOP + 0.1 and max(abs(y) for y, _ in sp) > 2.0, sp
    br = [r if area2(r) > 0 else list(reversed(r)) for r in rings]
    fy = [p[0] for r in br + [sp] for p in r]
    br_ymax = max(fy) + 0.1
    br_w = int(math.ceil((br_ymax - min(fy) + 0.1) * PX)) + 1
    br_zmax = max(z for r in br for _, z in r) + 0.1
    br_h = int(math.ceil((br_zmax - min(z for r in br for _, z in r) + 0.1) * PX)) + 1
    st_zmax = zs_ + 0.1
    st_t = t0f + br_h + 2
    st_h = int(math.ceil((st_zmax - min(z for _, z in sp) + 0.1) * PX)) + 1
    for r in br:
        fb.face([(bx, y, z) for y, z in r], ear_clip(r), (1.0, 0.0, 0.0),
                lambda p: ((br_ymax - p[1]) * PX, t0f + (br_zmax - p[2]) * PX))
    fb.face([(rear_x, y, z) for y, z in sp], ear_clip(sp), (1.0, 0.0, 0.0),
            lambda p: ((br_ymax - p[1]) * PX, st_t + (st_zmax - p[2]) * PX))
    # The knuckle: an octagonal tube round the hinge pin across the step's bottom, its ends capped (the pin's heads).
    hx, hz = HINGE
    kys = [y for y, z in sp if abs(z - min(zz for _, zz in sp)) < 1e-6]
    ky0, ky1 = min(kys), max(kys)
    rx = KNUCKLE_R / math.cos(math.pi / KNUCKLE_SIDES)
    kring = octagon(hx, hz, rx, rx, KNUCKLE_SIDES)  # (x, z) here
    kring = kring + kring[:1]
    lens = [0.0]
    for a, c in zip(kring, kring[1:]):
        lens.append(lens[-1] + math.hypot(c[0] - a[0], c[1] - a[1]))
    s_k = br_w + 4
    kw = int(math.ceil((ky1 - ky0) * PX)) + 1
    kh = int(math.ceil(lens[-1] * PX)) + 1
    kd = int(math.ceil(2 * rx * PX)) + 2
    assert max(br_h + 2 + st_h, kh, kd) <= ROWS and s_k + kw + 4 + 2 * (kd + 2) <= fm.sw, (br_w, br_h, kw, kh, kd)
    kids = []
    for (x, z), L in zip(kring, lens):
        nrm = (x - hx, 0.0, z - hz)
        kids.append([fb.vert((x, y, z), nrm, s_k + (ky1 - y) * PX, t0f + L * PX) for y in (ky1, ky0)])
    for j in range(KNUCKLE_SIDES):
        (a1, a0), (c1, c0) = kids[j], kids[j + 1]
        (xa, za), (xc, zc) = kring[j], kring[j + 1]
        out = ((xa + xc) / 2 - hx, 0.0, (za + zc) / 2 - hz)
        pa1, pa0, pc1, pc0 = (xa, ky1, za), (xa, ky0, za), (xc, ky1, zc), (xc, ky0, zc)
        fb.tri(a1, a0, c0, pa1, pa0, pc0, out)
        fb.tri(a1, c0, c1, pa1, pc0, pc1, out)
    s_e = s_k + kw + 4
    ends = []
    for e, (y, outy) in enumerate(((ky1, 1.0), (ky0, -1.0))):
        se = s_e + e * (kd + 2)
        xz = kring[:-1]
        fb.face([(x, y, z) for x, z in xz], ear_clip([(x, z) for x, z in xz]), (0.0, outy, 0.0),
                lambda p, se=se: (se + (p[0] - (hx - rx) + 0.1) * PX, t0f + (hz + rx + 0.1 - p[2]) * PX))
        ends.append(se)
    fm.tris = [base.tris[i] for i in np.nonzero(frame_sel & ~cone)[0]] + fb.tris
    rng = random.Random(4200)
    img = fm.skin(0)
    face = max(br, key=lambda r: abs(area2(r)))
    paint(img, fm.sw, 0, t0f, br_w, br_h, br_ymax, br_zmax, lambda y, z: breech_pixel(rng, face, y, z))
    paint(img, fm.sw, 0, st_t, br_w, st_h, br_ymax, st_zmax, lambda y, z: steel(rng, sp, y, z))
    for t in range(t0f, t0f + kh):
        L = (t - t0f + 0.5) / PX
        seam = min(abs(L - v) for v in lens) < 0.06  # (the octagon's edges: a lighter line)
        for s in range(s_k, s_k + kw):
            y = ky1 - (s - s_k + 0.5) / PX
            img[t * fm.sw + s] = rng.choice(EDGE) if seam or min(y - ky0, ky1 - y) < 0.1 else rng.choice(STEEL)
    for se in ends:
        for t in range(t0f, t0f + kd):
            for s in range(se, se + kd):
                x = hx - rx - 0.1 + (s - se + 0.5) / PX
                z = hz + rx + 0.1 - (t - t0f + 0.5) / PX
                r = math.hypot(x - hx, z - hz)
                img[t * fm.sw + s] = 1 if r < 0.12 else (rng.choice(CHAMFER) if r < 0.28 else
                                                          (rng.choice(EDGE) if r > KNUCKLE_R - 0.12 else
                                                           rng.choice(STEEL)))
    fm.write(out_f)
    print("v_shot2.mdl: %d triangles: the frame %d, the barrels %d (%d of theirs on their back given way); the breech "
          "plane x %.3f; the barrels' back %d points, the frame's front %d" %
          (len(T), frame_sel.sum(), barrel_sel.sum(), (barrel_sel & inplane).sum(), rear_x, len(outline),
           sum(len(r) for r in br)))
    print("the chambers' middle (the load point): %.2f %.2f %.2f; the standing breech x %.3f" % (*mid, bx))
    for p in (out_f, out_b):
        print("%s: %d bytes" % (os.path.normpath(p), os.path.getsize(p)))
    if guard is not None:
        guard.finish()


def inside_poly(poly, y, z):
    c = False
    for i in range(len(poly)):
        (ay, az), (by, bz) = poly[i], poly[(i + 1) % len(poly)]
        if (az > z) != (bz > z) and y < ay + (z - az) * (by - ay) / (bz - az):
            c = not c
    return c


if __name__ == "__main__":
    build()
