#!/usr/bin/env python3
# check_mdl_holes.py -- finds holes in Quake alias models (feedback round 20: "missing faces", "see-through",
# "I can see the face from the top and not from the bottom").
#
# Usage: python Misc/quakevr/check_mdl_holes.py [-v | -vv] [model.mdl ...]
#        (default: every quakevr/progs/v_*.mdl; -v lists the hidden loops too, -vv every loop's corners)
#
# Quake's alias renderer draws only the front of a triangle (clockwise seen from outside: gl_vidsdl.c
# glFrontFace(GL_CW), back faces culled), so a surface is only closed if every edge is shared by two
# triangles that run along it in opposite directions. The check welds the vertices that are at the same
# place in every frame (a model's vertices are split along its skin's seams, and generated parts share
# none with the old triangles) and then, per connected piece:
#
# - open edges: used by one triangle only. They form boundary loops, each a hole (or the open end of a
#   tube) unless it is hidden: the loop is "hidden" when rays from its middle hit the model in every
#   direction (inside another part: the top of a grip inside the gun's body). A loop that is not hidden is
#   see-through from some side.
# - cracks: an open edge with the corners of other open edges along it (a T-junction: one side of a seam
#   has more vertices than the other). Once the file's positions are rounded to its byte grid the corners
#   are off the edge and a sliver of background shows through (the super nailgun's back, round 20). A crack
#   narrower than CRACK_OK is harmless.
# - flipped edges: two triangles running along an edge in the same direction (one of them is wound the
#   wrong way: seen from outside it is culled, a hole from one side and a back face from the other); not
#   an error when hidden inside another part (the rays test, as for loops).
# - inverted pieces: a closed piece whose triangles all face inwards (it sums to a positive volume).
# - non-manifold edges (three or more triangles) are counted, not errors (the exporters' fins); so are stray
#   open edges (an open edge whose chain closes no loop: where parts meet, it bounds no area).
#
# Degenerate triangles (two corners welded, or no area) are skipped: they cover nothing.
#
# `analyse` is also what seal_mdl.py (the generators' hole filler) uses on the unrounded positions.

import math
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
HEADER = struct.Struct("<4si3f3ff3f8if")
CRACK_OK = 0.01   # model units: a crack this narrow cannot show


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def read(path):
    """Triangles, every frame's positions (frame groups: each of their frames), as the engine decodes them."""
    data = open(path, "rb").read()
    h = HEADER.unpack_from(data, 0)
    scale, origin = h[2:5], h[5:8]
    num_skins, sw, sh, nv, nt, nf = h[12:18]
    off = HEADER.size
    for _ in range(num_skins):
        (group,) = struct.unpack_from("<i", data, off)
        off += 4
        if group == 0:
            off += sw * sh
        else:
            (n,) = struct.unpack_from("<i", data, off)
            off += 4 + 4 * n + n * sw * sh
    off += 12 * nv
    tris = [struct.unpack_from("<4i", data, off + 16 * i) for i in range(nt)]
    off += 16 * nt
    frames = []

    def simple(off):
        off += 24
        b = data[off: off + 4 * nv]
        frames.append([tuple(origin[k] + scale[k] * b[4 * i + k] for k in range(3)) for i in range(nv)])
        return off + 4 * nv

    for _ in range(nf):
        (kind,) = struct.unpack_from("<i", data, off)
        off += 4
        if kind == 0:
            off = simple(off)
        else:
            (n,) = struct.unpack_from("<i", data, off)
            off += 12 + 4 * n
            for _ in range(n):
                off = simple(off)
    return tris, frames


class Topology:
    """The welded surface of a model: `weld[v]` its welded vertex, `pos[w]` (frame 0), and per piece its
    open edges (as the triangles run along them), cracks and boundary loops."""

    def __init__(self, tris, frames, grid=None, tol=1e-3):
        """`frames`: every frame's positions. `grid`: welds positions that round alike at this step
        (None: exactly equal). `tol`: how far off an edge a crack's corner may be (frame 0)."""
        key = {}
        self.weld = []
        for v in range(len(frames[0])):
            if grid:
                k = tuple(tuple(int(round(c / grid)) for c in fr[v]) for fr in frames)
            else:
                k = tuple(fr[v] for fr in frames)
            self.weld.append(key.setdefault(k, len(key)))
        self.pos = {}
        self.rep = {}  # welded -> a model vertex
        for v, w in enumerate(self.weld):
            self.pos.setdefault(w, frames[0][v])
            self.rep.setdefault(w, v)
        self.faces = []  # (welded a, b, c, triangle index)
        for i, t in enumerate(tris):
            A, B, C = (self.weld[v] for v in t[1:])
            if len({A, B, C}) < 3:
                continue
            n = cross(sub(self.pos[B], self.pos[A]), sub(self.pos[C], self.pos[A]))
            if dot(n, n) < 1e-14:
                continue
            self.faces.append((A, B, C, i))
        self.directed = {}
        for A, B, C, i in self.faces:
            for e in ((A, B), (B, C), (C, A)):
                self.directed.setdefault(e, []).append(i)
        self.flipped = []      # edges run the same way by two triangles: (a, b, [triangles])
        self.nonmanifold = 0
        open_edges = []
        seen = set()
        for (a, b), ts in self.directed.items():
            k = (min(a, b), max(a, b))
            if k in seen:
                continue
            seen.add(k)
            back = self.directed.get((b, a), [])
            n = len(ts) + len(back)
            if n == 1:
                open_edges.append((a, b) if ts else (b, a))
            elif n == 2 and not back:
                self.flipped.append((a, b, ts))
            elif n > 2:
                self.nonmanifold += 1
        # Cracks: an open edge with other open edges' corners along it.
        corners = {v for e in open_edges for v in e}
        self.cracks = []   # (a, b, [corners along a -> b], widest distance off the edge)
        split = {}
        for a, b in open_edges:
            pa, pb = self.pos[a], self.pos[b]
            ab = sub(pb, pa)
            L2 = dot(ab, ab)
            if L2 < 1e-12:
                continue
            on = []
            for m in corners:
                if m in (a, b):
                    continue
                am = sub(self.pos[m], pa)
                u = dot(am, ab) / L2
                if not 1e-4 < u < 1 - 1e-4:
                    continue
                off = cross(am, ab)
                d = math.sqrt(dot(off, off) / L2)
                if d < tol:
                    on.append((u, m, d))
            if on:
                on.sort()
                self.cracks.append((a, b, [m for _, m, _ in on], max(d for _, _, d in on)))
                split[(a, b)] = [a] + [m for _, m, _ in on] + [b]
        # The open edges once the cracks are closed: the cracked edges' pieces run the other way round
        # the crack than the edge did, and cancel the other side's edges.
        remaining = {}
        for a, b in open_edges:
            chain = split.get((a, b), [a, b])
            for x, y in zip(chain, chain[1:]):
                if (y, x) in remaining:
                    remaining[(y, x)] -= 1
                    if not remaining[(y, x)]:
                        del remaining[(y, x)]
                else:
                    remaining[(x, y)] = remaining.get((x, y), 0) + 1
        self.open_edges = [e for e, n in remaining.items() for _ in range(n)]
        # Boundary loops, as the open edges run (a cap must run the other way).
        nxt = {}
        for a, b in self.open_edges:
            nxt.setdefault(a, []).append(b)
        self.loops = []
        used = set()
        for a, b in self.open_edges:
            if (a, b) in used:
                continue
            loop = [a]
            cur = (a, b)
            while cur not in used:
                used.add(cur)
                loop.append(cur[1])
                cand = [c for c in nxt.get(cur[1], []) if (cur[1], c) not in used]
                if not cand:
                    break
                cur = (cur[1], cand[0])
            if loop[-1] == loop[0]:
                loop.pop()
            self.loops.append(loop)
        # Chains that did not close (a corner with two ways on): join each to the one starting where it ends.
        merged = True
        while merged:
            merged = False
            for x in self.loops:
                if len(x) < 2:
                    continue
                y = next((y for y in self.loops if y is not x and y and y[0] == x[-1]), None)
                if y is not None:
                    x.extend(y[1:])
                    self.loops.remove(y)
                    if len(x) > 1 and x[-1] == x[0]:
                        x.pop()
                    merged = True
                    break

    def centre(self, loop):
        return tuple(sum(self.pos[v][k] for v in loop) / len(loop) for k in range(3))


def ray_hits(tris_pos, o, d):
    """Does the ray o + t d (t > 0) hit any triangle (either side)?"""
    for a, b, c in tris_pos:
        e1, e2 = sub(b, a), sub(c, a)
        p = cross(d, e2)
        det = dot(e1, p)
        if abs(det) < 1e-12:
            continue
        inv = 1.0 / det
        s = sub(o, a)
        u = dot(s, p) * inv
        if u < 0.0 or u > 1.0:
            continue
        q = cross(s, e1)
        v = dot(d, q) * inv
        if v < 0.0 or u + v > 1.0:
            continue
        if dot(e2, q) * inv > 1e-3:
            return True
    return False


DIRS = [(x, y, z) for x in (-1, 0, 1) for y in (-1, 0, 1) for z in (-1, 0, 1) if (x, y, z) != (0, 0, 0)]
DIRS = [tuple(k / math.sqrt(dot(d, d)) + 0.013 * (i % 3 - 1) for k in d) for i, d in enumerate(DIRS)]  # off the axes


class Report:
    def __init__(self, path):
        self.name = os.path.basename(path)
        tris, frames = read(path)
        topo = self.topo = Topology(tris, frames, tol=0.2)
        tri_pos = [tuple(topo.pos[v] for v in f[:3]) for f in topo.faces]
        self.loops = []   # (corners, centre, hidden)
        self.stray = []   # open edges that close nothing (a chain of fewer than 3 corners: no area)
        for loop in topo.loops:
            c = topo.centre(loop)
            if len(loop) < 3:
                self.stray.append(c)
                continue
            self.loops.append(([topo.pos[v] for v in loop], c, all(ray_hits(tri_pos, c, d) for d in DIRS)))
        self.cracks = [(tuple((topo.pos[a][k] + topo.pos[b][k]) / 2 for k in range(3)), w) for a, b, _, w in topo.cracks]
        # Flipped edges, and whether they are hidden (inside another part: rays from them hit it everywhere).
        self.flipped = []
        for a, b, _ in topo.flipped:
            c = tuple((topo.pos[a][k] + topo.pos[b][k]) / 2 for k in range(3))
            self.flipped.append((c, all(ray_hits(tri_pos, c, d) for d in DIRS)))
        self.nonmanifold = topo.nonmanifold
        # Pieces, and the closed ones' volume (clockwise from outside: a closed piece sums negative).
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
        self.pieces = len(pieces)
        open_verts = {find(v) for e in topo.open_edges for v in e}
        self.inverted = []
        for root, piece in pieces.items():
            if root in open_verts:
                continue
            P = topo.pos
            vol = sum(dot(P[A], cross(P[B], P[C])) for A, B, C, _ in piece)
            if vol > 1e-6:
                pts = [P[v] for f in piece for v in f[:3]]
                self.inverted.append(tuple(sum(p[k] for p in pts) / len(pts) for k in range(3)))

    @property
    def visible_loops(self):
        return [l for l in self.loops if not l[2]]

    @property
    def wide_cracks(self):
        return [c for c in self.cracks if c[1] >= CRACK_OK]

    @property
    def visible_flipped(self):
        return [f for f in self.flipped if not f[1]]

    def ok(self):
        return not self.visible_loops and not self.wide_cracks and not self.visible_flipped and not self.inverted


def fmt_p(p):
    return "(%.1f, %.1f, %.1f)" % tuple(p)


def main():
    args = [a for a in sys.argv[1:] if a not in ("-v", "-vv")]
    verbose = "-v" in sys.argv or "-vv" in sys.argv
    corners = "-vv" in sys.argv
    if not args:
        progs = os.path.join(HERE, "..", "..", "quakevr", "progs")
        args = sorted(os.path.join(progs, f) for f in os.listdir(progs) if f.startswith("v_") and f.endswith(".mdl"))
    bad = 0
    for path in args:
        rep = Report(path)
        print("%-14s %s  pieces %d, open loops %d (%d see-through), cracks %d (%d wider than %g), flipped edges %d "
              "(%d in sight), inverted pieces %d, stray open edges %d, non-manifold edges %d"
              % (rep.name, "ok  " if rep.ok() else "HOLE", rep.pieces, len(rep.loops), len(rep.visible_loops),
                 len(rep.cracks), len(rep.wide_cracks), CRACK_OK, len(rep.flipped), len(rep.visible_flipped),
                 len(rep.inverted), len(rep.stray), rep.nonmanifold))
        for pts, c, hidden in rep.loops:
            if verbose or not hidden:
                print("    %s loop of %d edges at %s" % ("hidden" if hidden else "OPEN  ", len(pts), fmt_p(c)))
                if corners:
                    print("        " + " ".join(fmt_p(p) for p in pts))
        for c, w in rep.cracks:
            if verbose or w >= CRACK_OK:
                print("    %s at %s, %.3f wide" % ("CRACK " if w >= CRACK_OK else "crack ", fmt_p(c), w))
        for p, hidden in rep.flipped:
            if verbose or not hidden:
                print("    %s edge at %s" % ("flipped (hidden)" if hidden else "FLIPPED", fmt_p(p)))
        for p in rep.stray:
            print("    stray open edge at %s (no area: nothing to close)" % fmt_p(p))
        for p in rep.inverted:
            print("    INVERTED piece round %s" % fmt_p(p))
        bad += not rep.ok()
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
