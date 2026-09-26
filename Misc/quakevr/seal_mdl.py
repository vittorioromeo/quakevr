#!/usr/bin/env python3
# seal_mdl.py -- closes the holes in a view model being generated (feedback round 20: missing faces,
# see-through barrels and handles). Used by improve_weapons.py, improve_weapons2.py and improve_weapons3.py
# on the model they build, after every part is in place and before it is written; check_mdl_holes.py
# checks the written files the same way.
#
# Quake's alias renderer culls back faces, so every open edge of a surface is a hole seen from one side.
# The sealer finds them on the welded surface (check_mdl_holes.Topology: the vertices at the same place in
# every frame are one) and closes them with new triangles only:
#
# - cracks (T-junctions: an edge with the corners of the faces on its other side along it): a fan of
#   slivers along the edge. They have no area before the positions are rounded to the file's byte grid;
#   after it they cover exactly the gap the rounding opens.
# - boundary loops: a flat cap (ear-clipped in the loop's plane: the loop must be flat), or for a muzzle a
#   bore (a rim, a wall sunk into the barrel, a dark floor).
#
# - flipped triangles (wound against their neighbours: the lightning gun's keel end) and inside-out
#   pieces (the double shotgun's bead), on request: backed by copies wound the other way.
#
# The orientation comes from the surface itself: a cap runs along each open edge the other way round from
# the triangle that has it, so it faces the way its neighbours do (clockwise seen from outside) whatever
# the shape. Every new vertex is a new index (a copy of a loop's corner, or a point derived from the
# loop's corners): the old triangles and vertices are untouched and the new triangles share no vertex with
# them, so vr_anchor.cpp's strip order of the old vertices, and every anchor index, stays. The new vertices
# are computed per frame from the loop's own corners (a copy follows its corner exactly; a bore's rings are
# fixed in the loop's frame), so they follow the gun through recoil and spinning barrels.

import math

import mdlgen
from check_mdl_holes import Topology
from mdlgen import add, cross, dot, mul, norm, sub


def centroid(pts):
    return mul(tuple(map(sum, zip(*pts))), 1.0 / len(pts))


def newell(pts):
    """The polygon's normal, right-handed round its corners (for a cap: pointing into the model), times
    twice its area."""
    n = [0.0, 0.0, 0.0]
    for i, p in enumerate(pts):
        q = pts[(i + 1) % len(pts)]
        n[0] += (p[1] - q[1]) * (p[2] + q[2])
        n[1] += (p[2] - q[2]) * (p[0] + q[0])
        n[2] += (p[0] - q[0]) * (p[1] + q[1])
    return tuple(n)


def plane_basis(n):
    """u, v with u x v along n."""
    n = norm(n)
    a = (1.0, 0.0, 0.0) if abs(n[0]) < 0.9 else (0.0, 1.0, 0.0)
    u = norm(cross(a, n))
    return u, cross(n, u)


def ear_clip(pts2):
    """Triangles (index triples, in the polygon's turning direction) of a simple polygon given
    counter-clockwise in 2D; collinear corners are clipped as flat ears."""
    idx = list(range(len(pts2)))
    out = []

    def cr(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    def inside(p, a, b, c):
        return cr(a, b, p) > 1e-9 and cr(b, c, p) > 1e-9 and cr(c, a, p) > 1e-9

    while len(idx) > 3:
        best = None
        for k in range(len(idx)):
            i0, i1, i2 = idx[k - 1], idx[k], idx[(k + 1) % len(idx)]
            a, b, c = pts2[i0], pts2[i1], pts2[i2]
            area = cr(a, b, c)
            if area < -1e-9:
                continue  # reflex
            if any(inside(pts2[j], a, b, c) for j in idx if j not in (i0, i1, i2)):
                continue
            # The fattest ear first (a flat one, a collinear corner, scores 0).
            la, lb, lc = math.dist(b, c), math.dist(a, c), math.dist(a, b)
            q = area / max(1e-12, la * la + lb * lb + lc * lc)
            if best is None or q > best[0]:
                best = (q, k, (i0, i1, i2))
        if best is None:
            # Not simple (edges crossing by a hair where parts meet): clip the flattest corner anyway.
            k = min(range(len(idx)), key=lambda k: abs(cr(pts2[idx[k - 1]], pts2[idx[k]], pts2[idx[(k + 1) % len(idx)]])))
            best = (0, k, (idx[k - 1], idx[k], idx[(k + 1) % len(idx)]))
        out.append(best[2])
        del idx[best[1]]
    out.append(tuple(idx))
    return out


class Sealer:
    """`model`: .st, .tris ([front, a, b, c]) and .frames ([name, positions, normal indices]), the
    positions final (every part placed). New vertices and triangles are appended to it."""

    def __init__(self, model, grid=1e-4, crack_tol=1e-3):
        self.m = model
        self.grid = grid
        self.crack_tol = crack_tol  # how far off an edge a crack's corner may be (1e-3: exactly on it)
        self.new = []  # (vertex, fn(positions) -> point), in order (a derived point may use earlier ones)
        self.first = len(model.st)
        self.log = []

    # -- building blocks --

    def vert(self, fn, st):
        m = self.m
        m.st.append(type(m.st[0])((0, int(round(st[0])), int(round(st[1])))))
        for fr in m.frames:
            fr[1].append(fn(fr[1]))
            fr[2].append(0)
        v = len(m.st) - 1
        self.new.append((v, fn))
        return v

    def copy(self, v, st=None):
        if st is None:
            on, s, t = self.m.st[v]
            st = (s, t)
        return self.vert(lambda P, v=v: P[v], st)

    def tri(self, a, b, c):
        self.m.tris.append(type(self.m.tris[0])((1, a, b, c)))

    def topology(self):
        return Topology(self.m.tris, [fr[1] for fr in self.m.frames], grid=self.grid, tol=self.crack_tol)

    # -- flipped triangles --

    def fix_flipped(self, all_of_them=False):
        """Backs the triangles wound against their neighbours (the one of a flipped edge's two with more
        flipped edges; on ties, or `all_of_them`, both) with a copy wound the other way (new vertices): the
        wrong side then shows whichever side one looks from. The old triangle stays as it was (its edges
        may also bound a loop the other way: the lightning gun's keel end is the body's bottom's edge too),
        so this is done after the loops are capped. Returns how many."""
        topo = self.topology()
        count = {}
        for _, _, ts in topo.flipped:
            for i in ts:
                count[i] = count.get(i, 0) + 1
        worst = sorted(i for i, n in count.items() if n >= 2)
        if not worst or all_of_them:  # ties (each triangle has one flipped edge): back them all
            worst = sorted(count)
        for i in worst:
            _, a, b, c = self.m.tris[i]
            self.tri(self.copy(a), self.copy(c), self.copy(b))
        if worst:
            self.log.append("%d flipped triangles backed" % len(worst))
        return len(worst)

    def back_inverted(self):
        """Backs every closed piece that is inside out (its triangles facing inwards: it sums to a positive
        volume; the double shotgun's bead) with a copy wound the other way. Returns how many pieces."""
        topo = self.topology()
        parent = {}

        def find(x):
            parent.setdefault(x, x)
            while parent[x] != x:
                parent[x] = parent[parent[x]]
                x = parent[x]
            return x

        for A, B, C, _ in topo.faces:
            parent[find(A)] = find(B)
            parent[find(B)] = find(C)
        pieces = {}
        for f in topo.faces:
            pieces.setdefault(find(f[0]), []).append(f)
        open_roots = {find(v) for e in topo.open_edges for v in e}
        n = 0
        for root, piece in pieces.items():
            if root in open_roots:
                continue
            P = topo.pos
            if sum(dot(P[A], cross(P[B], P[C])) for A, B, C, _ in piece) <= 1e-6:
                continue
            for _, _, _, i in piece:
                _, a, b, c = self.m.tris[i]
                self.tri(self.copy(a), self.copy(c), self.copy(b))
            n += 1
            self.log.append("an inside-out piece of %d triangles round (%.1f, %.1f, %.1f) backed"
                            % ((len(piece),) + centroid([P[v] for f in piece for v in f[:3]])))
        return n

    # -- cracks --

    def fill_cracks(self):
        """Slivers along every crack; returns how many."""
        topo = self.topology()
        for a, b, ms, _ in topo.cracks:
            # The triangle has a -> b; the other side runs b -> m_k .. m_1 -> a: the fill runs
            # a -> m_1 .. m_k -> b -> a, fanned from a.
            chain = [topo.rep[w] for w in [a] + ms + [b]]
            ids = [self.copy(v) for v in chain]
            for i in range(1, len(ids) - 1):
                self.tri(ids[0], ids[i], ids[i + 1])
        if topo.cracks:
            self.log.append("%d cracks filled" % len(topo.cracks))
        return len(topo.cracks)

    # -- loops --

    def loops(self):
        """The open loops (after the cracks), each as model vertices in the order a cap runs round them."""
        topo = self.topology()
        return [[topo.rep[w] for w in reversed(loop)] for loop in topo.loops]

    def points(self, loop, P=None):
        P = P or self.m.frames[0][1]
        return [P[v] for v in loop]

    def centre(self, loop):
        return centroid(self.points(loop))

    def size(self, loop):
        pts = self.points(loop)
        c = centroid(pts)
        return max(math.dist(p, c) for p in pts)

    def flatness(self, loop):
        """How far the loop's corners are off its plane (frame 0)."""
        pts = self.points(loop)
        n = newell(pts)
        if dot(n, n) < 1e-12:
            return 0.0
        n = norm(n)
        c = centroid(pts)
        return max(abs(dot(sub(p, c), n)) for p in pts)

    def cap(self, loop, region, what="cap", max_off=0.05):
        """A flat cap over the loop, its texture the loop's plane projected onto `region` (s0, t0, s1,
        t1)."""
        pts = self.points(loop)
        n = newell(pts)
        size = self.size(loop)
        if size < 1e-3 or dot(n, n) < 1e-12:
            return False  # collapsed in frame 0 (a muzzle flash's base): nothing to see
        off = self.flatness(loop)
        assert off <= max_off * max(1.0, size), "%s: the loop round %s is not flat (%.2f)" % (what, self.centre(loop), off)
        u, v = plane_basis(n)
        c = centroid(pts)
        p2 = [(dot(sub(p, c), u), dot(sub(p, c), v)) for p in pts]
        s0, t0, s1, t1 = region
        ext = max(max(abs(a), abs(b)) for a, b in p2) or 1.0
        ids = [self.copy(w, ((s0 + s1) / 2 + a / ext * (s1 - s0 - 1) / 2, (t0 + t1) / 2 + b / ext * (t1 - t0 - 1) / 2))
               for w, (a, b) in zip(loop, p2)]
        for i, j, k in ear_clip(p2):
            self.tri(ids[i], ids[j], ids[k])
        self.log.append("%s: %d corners at (%.1f, %.1f, %.1f)" % ((what, len(loop)) + c))
        return True

    def bore(self, loop, rim, wall, floor, inner=0.62, depth=3.0, what="bore"):
        """A muzzle: a flat rim from the loop in to a ring `inner` of its size, a wall from there `depth`
        units into the barrel, a floor. `rim`, `wall`, `floor`: skin regions."""
        n0 = len(loop)
        pts0 = self.points(loop)
        c0 = centroid(pts0)
        u0, v0 = plane_basis(newell(pts0))

        def frame_of(P):
            pts = [P[w] for w in loop]
            c = centroid(pts)
            n = norm(newell(pts))  # into the barrel
            return c, n

        def ring_point(w, k, sink):
            def fn(P):
                c, n = frame_of(P)
                return add(add(c, mul(sub(P[w], c), k)), mul(n, sink))
            return fn

        def st_round(region, i, row):
            s0, t0, s1, t1 = region
            return (s0 + 0.5 + (s1 - s0 - 1) * i / n0, t0 + 0.5 + (t1 - t0 - 1) * row)

        outer = [self.copy(w, st_round(rim, i, 0)) for i, w in enumerate(loop)] + [None]
        rim_in = [self.vert(ring_point(w, inner, 0.0), st_round(rim, i, 1)) for i, w in enumerate(loop)]
        wall_top = [self.vert(ring_point(w, inner, 0.0), st_round(wall, i, 0)) for i, w in enumerate(loop)]
        wall_bot = [self.vert(ring_point(w, inner, depth), st_round(wall, i, 1)) for i, w in enumerate(loop)]
        for i in range(n0):
            j = (i + 1) % n0
            # The rim runs along the loop as a cap would (outer i -> i + 1), and so on inwards.
            self.tri(outer[i], outer[j], rim_in[j])
            self.tri(outer[i], rim_in[j], rim_in[i])
            self.tri(wall_top[i], wall_top[j], wall_bot[j])
            self.tri(wall_top[i], wall_bot[j], wall_bot[i])
        # The floor, over the wall's bottom ring (which the wall runs the other way round).
        s0, t0, s1, t1 = floor
        p2 = [(dot(sub(p, c0), u0), dot(sub(p, c0), v0)) for p in pts0]
        ext = max(max(abs(a), abs(b)) for a, b in p2) or 1.0
        fl = [self.vert(ring_point(w, inner, depth), ((s0 + s1) / 2 + a / ext * (s1 - s0 - 1) / 2,
                                                       (t0 + t1) / 2 + b / ext * (t1 - t0 - 1) / 2))
              for w, (a, b) in zip(loop, p2)]
        for i, j, k in ear_clip(p2):
            self.tri(fl[i], fl[j], fl[k])
        self.log.append("%s: %d corners at (%.1f, %.1f, %.1f), %.1f deep" % ((what, n0) + c0 + (depth,)))

    def split(self, loop, pred):
        """Cuts a loop that turns a corner (two flat faces missing along an edge: a box's bottom and front)
        into the corners where `pred` holds (a run of them) and the rest, each closed by the chord between
        the run's ends: two loops to cap."""
        n = len(loop)
        inside = [pred(self.m.frames[0][1][v]) for v in loop]
        assert any(inside) and not all(inside)
        start = next(i for i in range(n) if inside[i] and not inside[i - 1])
        run = []
        i = start
        while inside[i % n]:
            run.append(loop[i % n])
            i += 1
        rest = [loop[(i + k) % n] for k in range(n - len(run))]
        assert len(run) + len(rest) == n and all(not pred(self.m.frames[0][1][v]) for v in rest)
        return run, [run[-1]] + rest + [run[0]]

    # -- finishing --

    def finish(self):
        """Every new vertex's position (in creation order) and normal in every frame."""
        table = mdlgen.anorms()
        m = self.m
        mine = {v for v, _ in self.new}
        tris = [t for t in m.tris if t[1] in mine]
        for fr in m.frames:
            P = fr[1]
            for v, fn in self.new:
                P[v] = fn(P)
            acc = {}
            for _, a, b, c in tris:
                nrm = cross(sub(P[c], P[a]), sub(P[b], P[a]))  # outward (clockwise from outside)
                for v in (a, b, c):
                    acc[v] = add(acc.get(v, (0.0, 0.0, 0.0)), nrm)
            for v in mine:
                nv = acc.get(v, (0.0, 0.0, 1.0))
                nv = norm(nv) if dot(nv, nv) > 1e-18 else (0.0, 0.0, 1.0)
                fr[2][v] = max(range(len(table)), key=lambda i: dot(table[i], nv))


def seal(model, cap_region, special=None, name="model", fix_flips=False, fix_inverted=False, crack_tol=1e-3):
    """Fills the cracks, then each loop: `special(sealer, loop)` handles it if it returns True, else a flat
    cap on `cap_region`; then (`fix_flips`: True, or "all" for both triangles of every flipped edge) backs
    the triangles wound against their neighbours and
    (`fix_inverted`) the inside-out pieces. Returns the sealer (finished) for its log."""
    s = Sealer(model, crack_tol=crack_tol)
    s.fill_cracks()
    for loop in s.loops():
        if len(loop) < 3:
            s.log.append("a %d-corner loop left (no area)" % len(loop))
            continue
        if special and special(s, loop):
            continue
        if s.flatness(loop) > 0.05 * max(1.0, s.size(loop)):
            s.log.append("left a loop round (%.1f, %.1f, %.1f): not flat" % s.centre(loop))
            continue
        s.cap(loop, cap_region, "cap")
    if fix_flips:
        s.fix_flipped(all_of_them=fix_flips == "all")
    if fix_inverted:
        s.back_inverted()
    s.finish()
    print("  %s sealed: %s" % (name, "; ".join(s.log) if s.log else "nothing to do"))
    return s
