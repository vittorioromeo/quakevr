#!/usr/bin/env python3
# improve_weapons2.py -- details added to three of Quake VR's view models (round 16's voice notes:
# "the weapon models look a bit amateurish"), in Quake's low-poly style:
#
#   quakevr/progs/v_nail2.mdl   the super nailgun: the three grooves painted across the top of its
#                               hexagonal body are cut into the geometry (1 unit deep, with bevelled
#                               walls where the skin paints them); the skin is unchanged, its dark
#                               groove texels now lie on the recesses.
#   quakevr/progs/v_light.mdl   the lightning gun: a proper pistol grip (leaning back, knurled sides,
#                               a flared butt) under the hand, a trigger guard round the index
#                               finger and a trigger; the old stub of a handle folds away inside the
#                               grip. The gun sits 2.5 model units higher over the hand (the fist
#                               used to be half inside its body).
#   quakevr/progs/v_nail.mdl    the nailgun: a trigger guard round the index finger and a trigger.
#
# Usage: python Misc/quakevr/improve_weapons2.py [output game folder, default quakevr]
#
# The inputs are the models as Quake VR shipped them, kept in Misc/quakevr/src_models/ (the outputs
# overwrite quakevr/progs/). Running it again gives the same files. It prints each model's anchors
# and the lightning gun's new vr_weapons.inc settings (slot 7).
#
# Rules the edits keep:
# - Existing vertices keep their indices (new ones are appended) and, but for the lightning gun's
#   requantisation (its grip reaches below and behind the old bounding box, so the header's scale
#   and origin grow and every vertex is re-rounded, moving it by at most half a new step, < 0.04
#   units) and its folded-away stub, their positions. The weapon's Scale applies about the header's
#   origin, so the lightning gun's weapon offsets change with it to keep the gun where it was drawn.
#   Anchors (vr_weapons.inc's *_av settings) are indices into the vertex order of QuakeSpasm's old
#   triangle-strip builder (vr_anchor.cpp), which depends on the triangle list: added parts share
#   no vertices with the old triangles, so their strips come after the old ones and the old
#   indices stand; the super nailgun's body is re-triangulated, so the script prints each anchor's
#   new index (vr_weapons.inc slot 4 uses them).
# - Added parts follow the part of the gun they are fixed to in every animation frame (the
#   lightning gun and super nailgun recoil; the super nailgun's barrels spin, and nothing is fixed
#   to them): each part's frame-0 vertices move with the reference vertices' translation.
# - Grips and guards are laid out in the drawn hand's space (see HandSpace), as improve_weapons.py
#   does for the double shotgun and the rocket launcher.
# - The skins grow downwards (new rows) for the added parts' textures, so existing UVs stay; the
#   new texels are painted from the gun's own palette ramps (dark metal, speckled, lit top edges,
#   shadowed lower ones) and never use fullbright indices.

import math
import os
import random
import struct
import sys

import mdlgen
from mdlgen import HEADER, add, sub, mul, dot, cross, norm

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "src_models")


def lerp(a, b, k):
    return (a[0] + (b[0] - a[0]) * k, a[1] + (b[1] - a[1]) * k, a[2] + (b[2] - a[2]) * k)


def length(a):
    return math.sqrt(dot(a, a))


# ---------------------------------------------------------------------------------------------
# MDL reading and writing (single skin, single frames: as these three models are).

class Model:
    def __init__(self, path):
        data = open(path, "rb").read()
        h = list(HEADER.unpack_from(data, 0))
        assert h[0] == b"IDPO" and h[1] == 6
        self.h = h
        self.scale = list(h[2:5])
        self.origin = list(h[5:8])
        num_skins, self.sw, self.sh, num_verts, num_tris, num_frames = h[12:18]
        assert num_skins == 1
        off = HEADER.size
        (group,) = struct.unpack_from("<i", data, off)
        assert group == 0
        self.skin = bytearray(data[off + 4 : off + 4 + self.sw * self.sh])
        off += 4 + self.sw * self.sh
        self.st = [list(struct.unpack_from("<3i", data, off + 12 * i)) for i in range(num_verts)]
        off += 12 * num_verts
        self.tris = [list(struct.unpack_from("<4i", data, off + 16 * i)) for i in range(num_tris)]
        off += 16 * num_tris
        self.frames = []  # [name, positions, normal indices]
        for _ in range(num_frames):
            (kind,) = struct.unpack_from("<i", data, off)
            assert kind == 0, "frame groups are not handled"
            name = data[off + 12 : off + 28].split(b"\0")[0].decode("latin-1")
            off += 28
            pos, nrm = [], []
            for i in range(num_verts):
                b = data[off + 4 * i : off + 4 * i + 4]
                pos.append(tuple(self.origin[k] + self.scale[k] * b[k] for k in range(3)))
                nrm.append(b[3])
            off += 4 * num_verts
            self.frames.append([name, pos, nrm])
        assert off == len(data)
        self.num_old = num_verts
        self.old_tris = [list(t) for t in self.tris]
        self.parts = []
        self.new_normals = {}

    def pos(self, v, frame=0):
        return self.frames[frame][1][v]

    # -- adding geometry --

    def part(self, refs):
        p = Part(self, refs)
        self.parts.append(p)
        return p

    def add_tri(self, a, b, c):
        self.tris.append([1, a, b, c])

    def grow_skin(self, rows):
        """Adds `rows` rows under the skin (palette index 0); returns the first new row."""
        first = self.sh
        self.skin += bytes(self.sw * rows)
        self.sh += rows
        return first

    # -- finishing --

    def _place_parts(self):
        for p in self.parts:
            for f in range(len(self.frames)):
                shift = p.shift(f)
                for v, p0 in zip(p.verts, p.p0):
                    self.frames[f][1][v] = add(p0, shift)

    def _normals(self, table):
        """Normals of the new vertices, per frame: the area-weighted outward normals of their
        triangles (clockwise seen from outside), as the nearest of Quake's table."""
        for f in range(len(self.frames)):
            P = self.frames[f][1]
            acc = {}
            for _, a, b, c in self.tris:
                if max(a, b, c) < self.num_old:
                    continue
                n = cross(sub(P[c], P[a]), sub(P[b], P[a]))  # outward, |n| = 2 area
                for v in (a, b, c):
                    if v >= self.num_old:
                        acc[v] = add(acc.get(v, (0.0, 0.0, 0.0)), n)
            for v in range(self.num_old, len(P)):
                n = acc.get(v, (0.0, 0.0, 1.0))
                if length(n) < 1e-9:
                    n = (0.0, 0.0, 1.0)
                n = norm(n)
                self.frames[f][2][v] = max(range(len(table)), key=lambda i: dot(table[i], n))

    def out_pos(self, frame, v):
        """A vertex as written (quantised)."""
        p = self.frames[frame][1][v]
        return tuple(self.out_origin[k] + self.out_scale[k] *
                     max(0, min(255, int(round((p[k] - self.out_origin[k]) / self.out_scale[k])))) for k in range(3))

    def write(self, path):
        self._place_parts()
        self._normals(mdlgen.anorms())
        nverts = len(self.st)
        for fr in self.frames:
            assert len(fr[1]) == nverts
        # The header's box: kept if everything fits (the old vertices then keep their bytes), else
        # grown to take the new parts in.
        lo = [min(p[k] for fr in self.frames for p in fr[1]) for k in range(3)]
        hi = [max(p[k] for fr in self.frames for p in fr[1]) for k in range(3)]
        scale, origin = list(self.scale), list(self.origin)
        for k in range(3):
            top = origin[k] + 255 * scale[k]
            if lo[k] < origin[k] - 0.5 * scale[k] or hi[k] > top + 0.5 * scale[k]:
                new_lo, new_hi = min(lo[k], origin[k]), max(hi[k], top)
                origin[k], scale[k] = new_lo, (new_hi - new_lo) / 255.0
                print("  %s: axis %d requantised (%.3f .. %.3f, step %.4f)" % (os.path.basename(path), k, new_lo, new_hi, scale[k]))
        self.out_origin, self.out_scale = origin, scale
        h = list(self.h)
        h[2:5] = scale
        h[5:8] = origin
        h[8] = max(self.h[8], max(length(p) for fr in self.frames for p in fr[1]))
        h[13], h[14], h[15], h[16] = self.sw, self.sh, nverts, len(self.tris)
        out = bytearray(HEADER.pack(*h))
        out += struct.pack("<i", 0) + bytes(self.skin)
        for s in self.st:
            out += struct.pack("<3i", *s)
        for t in self.tris:
            out += struct.pack("<4i", *t)
        for name, P, N in self.frames:
            q = [[max(0, min(255, int(round((p[k] - origin[k]) / scale[k])))) for k in range(3)] for p in P]
            bmin = [min(v[k] for v in q) for k in range(3)]
            bmax = [max(v[k] for v in q) for k in range(3)]
            out += struct.pack("<i", 0) + bytes(bmin + [0]) + bytes(bmax + [0])
            out += name.encode("latin-1").ljust(16, b"\0")[:16]
            for v, n in zip(q, N):
                out += bytes((v[0], v[1], v[2], n))
        with open(path, "wb") as f:
            f.write(out)


class Part:
    """New vertices fixed to some old ones (`refs`): they move with the refs' translation."""

    def __init__(self, model, refs):
        self.m = model
        self.refs = refs
        self.verts = []
        self.p0 = []

    def vert(self, p, st):
        m = self.m
        m.st.append([0, int(round(st[0])), int(round(st[1]))])
        for fr in m.frames:
            fr[1].append(p)
            fr[2].append(0)
        self.verts.append(len(m.st) - 1)
        self.p0.append(p)
        return len(m.st) - 1

    def pin(self, v, p):
        """Moves an existing vertex to p (frame 0), fixed to the refs in every frame."""
        self.verts.append(v)
        self.p0.append(p)

    def shift(self, f):
        P0 = [self.m.pos(v, 0) for v in self.refs]
        P = [self.m.pos(v, f) for v in self.refs]
        c0 = mul(sum_v(P0), 1.0 / len(P0))
        c = mul(sum_v(P), 1.0 / len(P))
        t = sub(c, c0)
        # The refs must move rigidly, without turning (recoil): check the shape is kept.
        err = max(length(sub(sub(p, p0), t)) for p, p0 in zip(P, P0))
        assert err < 0.3, "reference vertices do not just translate (frame %d, %.2f)" % (f, err)
        return t

    # -- building blocks --

    def quad(self, p0, p1, p2, p3, rect, outward):
        """A quad (p0 p1 p2 p3 round its edge) textured with the whole skin rectangle `rect`
        (s0, t0, s1, t1): p0 at its top left, p1 top right, p2 bottom right, p3 bottom left."""
        s0, t0, s1, t1 = rect
        i = [self.vert(p0, (s0, t0)), self.vert(p1, (s1 - 1, t0)), self.vert(p2, (s1 - 1, t1 - 1)),
             self.vert(p3, (s0, t1 - 1))]
        tri_out(self.m, i[0], i[1], i[2], outward)
        tri_out(self.m, i[0], i[2], i[3], outward)

    def sweep(self, rings, rects, centre_of, cap_start=None, cap_end=None):
        """Quads between successive rings (lists of points, the same count, going round the same
        way); ring edge k of every band is textured with rects[k]. `centre_of(p)` is a point inside
        the solid near p (for the outward direction). Caps close the first/last ring (a fan)."""
        n = len(rings[0])
        for r in range(len(rings) - 1):
            A, B = rings[r], rings[r + 1]
            for k in range(n):
                k2 = (k + 1) % n
                mid = mul(add(add(A[k], A[k2]), add(B[k], B[k2])), 0.25)
                self.quad(A[k], A[k2], B[k2], B[k], rects[k], sub(mid, centre_of(mid)))
        for ring, rect, sign in ((rings[0], cap_start, -1), (rings[-1], cap_end, 1)):
            if rect is None:
                continue
            c = mul(sum_v(ring), 1.0 / n)
            other = rings[1] if sign < 0 else rings[-2]
            out = sub(c, mul(sum_v(other), 1.0 / n))
            s0, t0, s1, t1 = rect
            ci = self.vert(c, ((s0 + s1) / 2, (t0 + t1) / 2))
            # Planar mapping of the cap onto its rectangle, by the ring's own extent.
            u_axis = norm(sub(ring[0], c))
            v_axis = norm(cross(out, u_axis))
            us = [dot(sub(p, c), u_axis) for p in ring]
            vs = [dot(sub(p, c), v_axis) for p in ring]
            ru, rv = max(map(abs, us)) or 1, max(map(abs, vs)) or 1
            idx = []
            for p, u, v in zip(ring, us, vs):
                idx.append(self.vert(p, (s0 + (u / ru + 1) / 2 * (s1 - s0 - 1), t0 + (v / rv + 1) / 2 * (t1 - t0 - 1))))
            for k in range(n):
                tri_out(self.m, ci, idx[k], idx[(k + 1) % n], out)


def sum_v(ps):
    s = (0.0, 0.0, 0.0)
    for p in ps:
        s = add(s, p)
    return s


def tri_out(m, a, b, c, outward):
    """A triangle, clockwise seen from where `outward` points (Quake's front faces)."""
    P = m.frames[0][1]
    if dot(cross(sub(P[c], P[a]), sub(P[b], P[a])), outward) < 0:
        b, c = c, b
    m.add_tri(a, b, c)


# ---------------------------------------------------------------------------------------------
# Anchors: vr_anchor.cpp's port of QuakeSpasm's BuildTris strip/fan search (the vertex order that
# vr_weapons.inc's anchor indices index).

def vertex_order(tris):
    n = len(tris)
    used = [0] * n
    order = []

    def clear(start):
        for j in range(start + 1, n):
            if used[j] == 2:
                used[j] = 0

    def run(start, sv, fan):
        used[start] = 2
        f, *vi = tris[start]
        verts = [vi[sv % 3], vi[(sv + 1) % 3], vi[(sv + 2) % 3]]
        ts = [start]
        if fan:
            m1, m2 = vi[sv % 3], vi[(sv + 2) % 3]
        else:
            m1, m2 = vi[(sv + 2) % 3], vi[(sv + 1) % 3]
        extended = True
        while extended and len(ts) < 126:
            extended = False
            for j in range(start + 1, n):
                cf, *cv = tris[j]
                if cf != f:
                    continue
                hit = False
                for k in range(3):
                    if cv[k] != m1 or cv[(k + 1) % 3] != m2:
                        continue
                    if used[j]:
                        clear(start)
                        return verts, ts
                    nv = cv[(k + 2) % 3]
                    if fan or len(ts) & 1:
                        m2 = nv
                    else:
                        m1 = nv
                    verts.append(nv)
                    ts.append(j)
                    used[j] = 2
                    extended = hit = True
                    break
                if hit:
                    break
        clear(start)
        return verts, ts

    for i in range(n):
        if used[i]:
            continue
        best = None
        for fan in (True, False):
            for sv in range(3):
                v, t = run(i, sv, fan)
                if best is None or len(t) > len(best[1]):
                    best = (v, t)
        for t in best[1]:
            used[t] = 1
        order += best[0]
    return order


def report_anchors(m, name, anchors):
    old = vertex_order(m.old_tris)
    new = vertex_order(m.tris)
    for key, index in anchors:
        v = old[index]
        same = index < len(new) and new[index] == v
        moved = index if same else new.index(v)
        print("  %s %s: anchor %d (vertex %d) -> %d%s" % (name, key, index, v, moved, "" if same else "  CHANGED"))


# ---------------------------------------------------------------------------------------------
# Skin painting.

def paint_panel(m, rect, ramp, seed, knurl=False, grain=0.25):
    """A metal panel on the skin: `ramp` from dark to light palette indices; mostly its middle,
    speckled, the top row and left column lit, the bottom rows and right column in shadow
    (id's hand-painted bevel). `knurl` crosses it with a diagonal diamond pattern."""
    s0, t0, s1, t1 = rect
    rng = random.Random(seed)
    mid = len(ramp) // 2
    for t in range(t0, t1):
        for s in range(s0, s1):
            k = mid
            r = rng.random()
            if r < grain * 0.5:
                k -= 1
            elif r < grain:
                k += 1
            if knurl:
                u, v = s - s0, t - t0
                a, b = (u + v) % 4, (u - v) % 4
                if a == 0 or b == 0:
                    k = mid - 2  # the cuts
                elif a == 1 and b == 1:
                    k = mid + 1  # the diamonds' lit tops
            if t == t0:
                k = len(ramp) - 2
            elif s == s0:
                k += 1
            if t == t1 - 1:
                k = 0
            elif t == t1 - 2 or s == s1 - 1:
                k = min(k, mid - 1)
            m.skin[t * m.sw + s] = ramp[max(0, min(len(ramp) - 1, k))]


# ---------------------------------------------------------------------------------------------
# The super nailgun: grooves cut into the top of its body.

# The body's outer shell is a hexagonal prism along x (x -2.58 at the back .. 15.85 at the front)
# unwrapped round its top half onto the skin's s 400..492, t 2 (front) .. 105 (back). Its top
# three faces (with a thin chamfer triangle on each top corner) are replaced by a grid over the
# same surface, cut along the painted grooves. Columns, left (+y) to right: back edge, front edge.
NAIL2_COLUMNS = [((168, 163), (166, 164)),  # upper left face
                 ((163, 163), (164, 162)),  # left chamfer (a sliver: its back edge is a point)
                 ((163, 27), (162, 25)),    # top face
                 ((27, 27), (25, 26)),      # right chamfer
                 ((27, 161), (26, 160))]    # upper right face
NAIL2_FRONT_T, NAIL2_BACK_T = 2, 105
# Each groove along t: rim (lit edge), floor start, floor end, back rim; across s: rim, floor,
# floor, rim (the painted walls lie between rim and floor).
NAIL2_GROOVES_T = [(14, 21, 27, 28), (43, 50, 56, 57), (72, 79, 85, 86)]
NAIL2_GROOVE_S = (412, 419, 476, 483)
NAIL2_DEPTH = 1.0


def nail2(m):
    body = sorted({v for pair in NAIL2_COLUMNS for edge in pair for v in edge})
    replaced = [i for i, (_, a, b, c) in enumerate(m.tris) if {a, b, c} <= set(body)]
    assert len(replaced) == 8, replaced
    m.tris = [t for i, t in enumerate(m.tris) if i not in replaced]
    part = m.part([158, 159, 160, 161, 162, 163, 165, 166, 167, 168])

    rows = [NAIL2_FRONT_T] + [t for g in NAIL2_GROOVES_T for t in g] + [NAIL2_BACK_T]
    s_rim_l, s_floor_l, s_floor_r, s_rim_r = NAIL2_GROOVE_S

    def st_of(v):
        return m.st[v][1], m.st[v][2]

    def column_at(c, t):
        """Column c's left and right edge points and s at row t."""
        (bl, br), (fl, fr) = NAIL2_COLUMNS[c]
        k = (NAIL2_BACK_T - t) / (NAIL2_BACK_T - NAIL2_FRONT_T)  # 0 at the back, 1 at the front
        L = lerp(m.pos(bl), m.pos(fl), k)
        R = lerp(m.pos(br), m.pos(fr), k)
        sl = st_of(bl)[0] + (st_of(fl)[0] - st_of(bl)[0]) * k
        sr = st_of(br)[0] + (st_of(fr)[0] - st_of(br)[0]) * k
        return L, R, sl, sr

    def normal_at(c, t):
        (bl, br), (fl, fr) = NAIL2_COLUMNS[c]
        L, R, _, _ = column_at(c, t)
        along = sub(lerp(m.pos(fl), m.pos(fr), 0.5), lerp(m.pos(bl), m.pos(br), 0.5))
        n = norm(cross(along, sub(R, L)))
        centre = (L[0], -0.1, 0.3)  # the body's axis
        return n if dot(n, sub(L, centre)) > 0 else mul(n, -1.0)

    def is_floor(s, t):
        return s_floor_l <= s <= s_floor_r and any(g[1] <= t <= g[2] for g in NAIL2_GROOVES_T)

    corner = {(0, NAIL2_BACK_T): 168, (0, NAIL2_FRONT_T): 166}
    for b in range(1, 6):
        (bl, br), (fl, fr) = NAIL2_COLUMNS[b - 1]
        corner[(b, NAIL2_BACK_T)], corner[(b, NAIL2_FRONT_T)] = br, fr

    # Nodes of each row: the column boundaries (b = 0..5) and the groove lines inside columns.
    grid = []  # per row: list of (s, vertex, boundary)
    surf = {}  # vertex -> its point on the uncut surface (for the triangles' winding)
    for t in rows:
        nodes = []
        for b in range(6):
            c = min(b, 4)
            L, R, sl, sr = column_at(c, t)
            s = sl if b < 5 else sr
            p = L if b < 5 else R
            if (b, t) in corner:
                nodes.append((s, corner[(b, t)], b))
                surf[corner[(b, t)]] = m.pos(corner[(b, t)])
                continue
            q = p
            if is_floor(s, t):
                if 0 < b < 5:
                    na, nb = normal_at(b - 1, t), normal_at(b, t)
                    p = add(p, mul(add(na, nb), -NAIL2_DEPTH / (1 + dot(na, nb))))
                else:
                    p = add(p, mul(normal_at(c, t), -NAIL2_DEPTH))
            nodes.append((s, part.vert(p, (s, t)), b))
            surf[nodes[-1][1]] = q
        for s in NAIL2_GROOVE_S:
            for c in range(5):
                L, R, sl, sr = column_at(c, t)
                if sl < s < sr:
                    p = q = lerp(L, R, (s - sl) / (sr - sl))
                    if is_floor(s, t):
                        p = add(p, mul(normal_at(c, t), -NAIL2_DEPTH))
                    nodes.append((s, part.vert(p, (s, t)), None))
                    surf[nodes[-1][1]] = q
        nodes.sort()
        grid.append(nodes)

    P = m.frames[0][1]

    def add_tri(x, y, z):
        """A triangle wound clockwise seen from outside: judged on the uncut surface, where every
        triangle (walls included) faces out."""
        if len({x, y, z}) < 3 or length(cross(sub(P[y], P[x]), sub(P[z], P[x]))) < 1e-6:
            return
        S = [surf[x], surf[y], surf[z]]
        c = mul(sum_v(S), 1.0 / 3)
        if dot(cross(sub(S[2], S[0]), sub(S[1], S[0])), sub(c, (c[0], -0.1, 0.3))) < 0:
            y, z = z, y
        m.add_tri(x, y, z)

    # Cells a (row i, left), b (row i, right), c (row i + 1, right), d (row i + 1, left): split
    # along the diagonal through the cell's floor node if it has only one (a groove's mitred
    # corner). The outer columns' strips (s 400..412 and 483..492) have no groove: they are fanned
    # from the old corner vertices, so that their outer edges (shared with the lower faces) get no
    # new vertices (T-junctions).
    for i in range(len(rows) - 1):
        A, B = grid[i], grid[i + 1]
        for j in range(len(A) - 1):
            if A[j][2] == 0 or A[j + 1][2] == 5:
                continue
            a, b = A[j][1], A[j + 1][1]
            d, c = B[j][1], B[j + 1][1]
            fl = [v for (s, v, _), t in ((A[j], rows[i]), (A[j + 1], rows[i]), (B[j + 1], rows[i + 1]), (B[j], rows[i + 1]))
                  if is_floor(s, t)]
            if len(fl) == 1 and fl[0] in (b, d):
                add_tri(a, b, d)
                add_tri(b, c, d)
            else:
                add_tri(a, b, c)
                add_tri(a, c, d)
    for side in (0, 5):
        col = [g[0][1] if side == 0 else g[-1][1] for g in grid]    # 166 .. 168 (or 160 .. 161)
        inner = [g[1][1] if side == 0 else g[-2][1] for g in grid]  # the s 412 (483) line
        front, back = col[0], col[-1]
        half = len(inner) // 2
        for k in range(len(inner) - 1):
            add_tri(front if k < half else back, inner[k], inner[k + 1])
        add_tri(front, inner[half], back)


# ---------------------------------------------------------------------------------------------
# Swept bars (trigger guards, triggers) and the lightning gun's grip.

def bar_rings(path, y, half_w, half_h):
    """Rectangular rings along a polyline in the xz plane (at y): half_w across (y), half_h
    across the path in the plane (mitred at the bends)."""
    rings = []
    for i, p in enumerate(path):
        d0 = sub(path[i], path[i - 1]) if i > 0 else sub(path[1], path[0])
        d1 = sub(path[i + 1], path[i]) if i + 1 < len(path) else d0
        d0, d1 = norm(d0), norm(d1)
        t = norm(add(d0, d1))
        n = (-t[2], 0.0, t[0])  # in the plane, across the path
        k = half_h / max(0.35, dot(n, (-d0[2], 0.0, d0[0])))
        c = (p[0], y, p[1] if len(p) == 2 else p[2])
        rings.append([add(add(c, mul(n, k)), (0, half_w, 0)), add(add(c, mul(n, -k)), (0, half_w, 0)),
                      add(add(c, mul(n, -k)), (0, -half_w, 0)), add(add(c, mul(n, k)), (0, -half_w, 0))])
    return rings


def xz(path):
    return [(p[0], 0.0, p[1]) for p in path]


def bar(part, path, y, half_w, half_h, rect_side, rect_edge, caps=True):
    path3 = xz(path)
    rings = bar_rings(path3, y, half_w, half_h)

    def centre_of(p):
        # nearest point of the path (by x, z)
        best = min(range(len(path3) - 1), key=lambda i: seg_dist(p, path3[i], path3[i + 1]))
        a, b = path3[best], path3[best + 1]
        q = (p[0], 0.0, p[2])
        k = max(0.0, min(1.0, dot(sub(q, a), sub(b, a)) / max(1e-9, dot(sub(b, a), sub(b, a)))))
        c = lerp(a, b, k)
        return (c[0], y, c[2])

    part.sweep(rings, [rect_side, rect_edge, rect_side, rect_edge], centre_of,
               rect_edge if caps else None, rect_edge if caps else None)


def seg_dist(p, a, b):
    q = (p[0], 0.0, p[2])
    ab = sub(b, a)
    k = max(0.0, min(1.0, dot(sub(q, a), ab) / max(1e-9, dot(ab, ab))))
    return length(sub(q, lerp(a, b, k)))


# ---------------------------------------------------------------------------------------------
# The hand's space. vr_view.cpp draws a held weapon's hand at its hand anchor vertex plus the hand
# offsets, turned with the weapon; so a point q of the hand's space (Quake units at the weapon
# settings' k, before the weapon's Scale Sw) is at p0 + q / Sw in the weapon's model, p0 being the
# hand's origin there: anchor + HandOffset / (k Sw). The fist, measured on hand_base.mdl and the
# finger models as they are drawn on a weapon (Misc/quakevr/improve_weapons.py, which lays out the
# double shotgun's and the rocket launcher's grips the same way): the fingers curl round a line
# through (-1.3, -0.16, -1.9) leaning back 16 degrees, the index finger on top (z -0.9..-1.8, its
# top at -0.75), the pinky at the bottom (-2.5..-3.4).

K = (1.25 / 0.75) * 0.7  # weapons::modelTransform's k at the default world and gun model scales
GRIP_LEAN = math.radians(16.0)
GRIP_Y = -0.16


def grip_x(z):
    return -1.3 + (z + 1.9) * math.tan(GRIP_LEAN)


class HandSpace:
    def __init__(self, p0, sw):
        self.p0 = p0
        self.sw = sw

    def m(self, x, z, y=GRIP_Y):
        return (self.p0[0] + x / self.sw, self.p0[1] + y / self.sw, self.p0[2] + z / self.sw)


# The pistol grip in the hand's space: (z, half depth, half width, back swell), from inside the
# gun down to the butt (the double shotgun's profile, a little chunkier), and the trigger guard's
# and trigger's paths (x, z): the guard's bar runs between the index and middle fingers.
GRIP_PROFILE = [(0.45, 0.74, 0.50, 0.0), (-0.95, 0.78, 0.53, 0.05), (-2.0, 0.83, 0.57, 0.1),
                (-3.0, 0.79, 0.55, 0.05), (-3.35, 0.88, 0.62, 0.05), (-3.62, 0.95, 0.66, 0.05)]
GUARD_PATH = [(-0.55, -1.90), (0.05, -2.04), (0.72, -1.96), (1.08, -1.62), (1.20, -1.12), (1.24, -0.40)]
TRIGGER_PATH = [(0.30, -0.40), (0.34, -0.82), (0.24, -1.20), (0.04, -1.46)]


# ---------------------------------------------------------------------------------------------
# The lightning gun.

BROWN = [16, 174, 173, 172, 171, 170, 169]  # its dark brown metal, dark to light
LIGHT_Y = -0.15                            # the gun's middle across
# Its settings before this (vr_weapons.inc slot 7): the hand at anchor 57 (vertex 25) plus the
# hand offsets.
LIGHT_SW = 0.35
LIGHT_HAND_ANCHOR = 57
LIGHT_HAND_OFFSET = (2.4, 0.700002, -1.35)
LIGHT_OFFSET = (2.25, 6.05, -3.95)
# The fist went up into the gun's body (its index finger inside it): the gun goes up over the
# hand by this many model units (about 2 cm), so that the index finger's top meets its underside.
LIGHT_RAISE = 2.5
LIGHT_BODY = [149, 150, 151, 152, 174, 175, 176, 177]  # the back block and the body (they recoil together)
LIGHT_STUB = [166, 167, 168, 169, 230, 231, 232, 233]  # the old stub of a handle


def light_hand_space(m):
    anchor = m.pos(vertex_order(m.old_tris)[LIGHT_HAND_ANCHOR])
    p0 = add(anchor, mul(LIGHT_HAND_OFFSET, 1.0 / (K * LIGHT_SW)))
    # Lowered on the gun, and centred on the grip across.
    return HandSpace((p0[0], LIGHT_Y - GRIP_Y / LIGHT_SW, p0[2] - LIGHT_RAISE), LIGHT_SW)


def light(m):
    row = m.grow_skin(32)
    R = {"knurl": (0, row, 48, row + 32), "metal": (48, row, 80, row + 32), "strap": (80, row, 104, row + 32),
         "dark": (104, row, 128, row + 32), "trigger": (128, row, 144, row + 32)}
    paint_panel(m, R["knurl"], [16, 16, 174, 173, 172, 171], 1, knurl=True)
    paint_panel(m, R["metal"], [16, 174, 173, 172, 171], 2)
    paint_panel(m, R["strap"], [16, 16, 174, 173, 172], 3, grain=0.35)
    paint_panel(m, R["dark"], [16, 16, 16, 174, 173, 172], 4)
    paint_panel(m, R["trigger"], BROWN[1:], 5)

    hs = light_hand_space(m)
    part = m.part(LIGHT_BODY)

    # The grip: octagonal rings square to its leaning axis, from inside the body down to a
    # flared butt below the pinky; knurled sides, plain front and back straps.
    fwd = (math.cos(GRIP_LEAN), 0.0, -math.sin(GRIP_LEAN))
    rings = []
    for z, hd, hw, swell in GRIP_PROFILE:
        c = hs.m(grip_x(z), z)
        hd, hw, sw, ch = hd / hs.sw, hw / hs.sw, swell / hs.sw, 0.28 / hs.sw
        pts = []
        for u, v in ((hd, hw - ch), (hd - ch, hw), (-hd + ch - sw, hw), (-hd - sw, hw - ch),
                     (-hd - sw, -hw + ch), (-hd + ch - sw, -hw), (hd - ch, -hw), (hd, -hw + ch)):
            pts.append(add(add(c, mul(fwd, u)), (0.0, v, 0.0)))
        rings.append(pts)
    axis_top, axis_bottom = hs.m(grip_x(0.45), 0.45), hs.m(grip_x(-3.62), -3.62)

    def grip_centre(p):
        d = norm(sub(axis_bottom, axis_top))
        return add(axis_top, mul(d, dot(sub(p, axis_top), d)))

    # Ring edges: 0 front-left chamfer, 1 left side, 2 back-left chamfer, 3 back, 4 back-right
    # chamfer, 5 right side, 6 front-right chamfer, 7 front.
    rects = [R["metal"], R["knurl"], R["metal"], R["strap"], R["metal"], R["knurl"], R["metal"], R["strap"]]
    part.sweep(rings, rects, grip_centre, None, R["dark"])

    # The old stub of a handle would poke out behind the grip: it folds away inside it.
    inside = hs.m(grip_x(-0.95), -0.95)
    for v in LIGHT_STUB:
        part.pin(v, inside)

    def to_model(path):
        return [(p[0], p[2]) for p in (hs.m(x, z) for x, z in path)]

    bar(part, to_model(GUARD_PATH), LIGHT_Y, 0.26 / hs.sw, 0.13 / hs.sw, R["metal"], R["dark"])
    bar(part, to_model(TRIGGER_PATH), LIGHT_Y, 0.1 / hs.sw, 0.1 / hs.sw, R["trigger"], R["trigger"])


def light_settings(m):
    """The slot's new defaults: the hand offsets putting the hand at the lowered p0, and the
    weapon offsets keeping the gun where it was drawn (the model's origin moved, and the weapon's
    Scale applies about it) but LIGHT_RAISE higher."""
    hs = light_hand_space(m)
    anchor = m.out_pos(0, vertex_order(m.tris)[LIGHT_HAND_ANCHOR])
    hand = mul(sub(hs.p0, anchor), K * LIGHT_SW)
    d = [(1 - LIGHT_SW) * (m.origin[k] - m.out_origin[k]) for k in range(3)]
    offset = (LIGHT_OFFSET[0] + d[0], LIGHT_OFFSET[1] + d[1], LIGHT_OFFSET[2] + d[2] + LIGHT_RAISE * LIGHT_SW)
    return {"HandOffsetX": hand[0], "HandOffsetY": hand[1], "HandOffsetZ": hand[2],
            "OffsetX": offset[0], "OffsetY": offset[1], "OffsetZ": offset[2]}


# ---------------------------------------------------------------------------------------------
# The nailgun.

NAIL_Y = 0.0            # the grip's middle across
NAIL_SW = 0.48          # vr_weapons.inc slot 3
NAIL_HAND_ANCHOR = 5
NAIL_HAND_OFFSET = (0.8, -1.299998, 0.1)
NAIL_GRIP_FRONT = -3.0  # inside the grip, at the guard's height


def nail(m):
    row = m.grow_skin(32)
    R = {"metal": (0, row, 32, row + 32), "dark": (32, row, 56, row + 32), "trigger": (56, row, 72, row + 32)}
    paint_panel(m, R["metal"], [0, 32, 33, 34, 35], 11)
    paint_panel(m, R["dark"], [0, 0, 32, 33, 34], 12)
    paint_panel(m, R["trigger"], [32, 33, 34, 35, 36], 13)
    part = m.part([21, 23, 25, 26, 131, 132])  # the lower body (it does not move)
    anchor = m.pos(vertex_order(m.old_tris)[NAIL_HAND_ANCHOR])
    hs = HandSpace(add(anchor, mul(NAIL_HAND_OFFSET, 1.0 / (K * NAIL_SW))), NAIL_SW)
    # The guard round the index finger (the hand stays where it was: the fist fits the old grip),
    # its bar carried back into the grip's front (further back than the double shotgun's).
    path = [(p[0], p[2]) for p in (hs.m(x, z) for x, z in GUARD_PATH)]
    path = [(NAIL_GRIP_FRONT, path[0][1])] + path
    bar(part, path, NAIL_Y, 0.26 / hs.sw, 0.19 / hs.sw, R["metal"], R["dark"])
    # The trigger: the index finger's lower half shows under the body; the blade hangs there.
    bar(part, [(0.8, -1.3), (0.9, -1.85), (0.7, -2.2), (0.35, -2.35)], NAIL_Y, 0.36, 0.24, R["trigger"], R["trigger"])


# ---------------------------------------------------------------------------------------------

MODELS = [  # file, builder, vr_weapons.inc slot, anchors, the slot's new settings
    ("v_nail2.mdl", nail2, 4, [("hand", 28), ("muzzle", 129), ("2h", 94), ("wpnbtn", 28), ("wpntxt", 28)], None),
    ("v_light.mdl", light, 7, [("hand", 57), ("muzzle", 104), ("2h", 230), ("wpnbtn", 57), ("wpntxt", 57)],
     light_settings),
    ("v_nail.mdl", nail, 3, [("hand", 5), ("muzzle", 33), ("2h", 51), ("wpnbtn", 97), ("wpntxt", 0)], None),
]


def main():
    game = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "quakevr")
    for name, build, slot, anchors, settings in MODELS:
        m = Model(os.path.join(SRC, name))
        verts, tris = len(m.st), len(m.tris)
        build(m)
        out = os.path.join(game, "progs", name)
        m.write(out)
        print("%s: %d -> %d vertices, %d -> %d triangles, skin %dx%d" % (name, verts, len(m.st), tris, len(m.tris), m.sw, m.sh))
        report_anchors(m, name, anchors)
        if settings:
            for key, value in settings(m).items():
                print('  QVR_WEAPON_DEFAULT(%d, %s, "%s")' % (slot, key, ("%.6f" % value).rstrip("0").rstrip(".")))


if __name__ == "__main__":
    main()
