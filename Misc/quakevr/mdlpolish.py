#!/usr/bin/env python3
# mdlpolish.py -- what polish_weapons.py (round 21: "improve their look while keeping the same art style and
# low-poly look") needs to add detail to a finished Quake alias model without moving anything the engine or the
# author's settings rely on:
#
# - Model: reads any MDL (plain or grouped skins and frames) and writes it back with the SAME header scale and
#   origin, every old vertex's bytes (position and normal index) and index, every old triangle and every old UV
#   unchanged. New vertices are appended (quantised on the same grid: they must lie inside the model's old byte
#   box in every frame, or the write fails), new triangles after the old ones and sharing no vertex with them, so
#   vr_anchor.cpp's strip order of the old vertices -- every anchor index -- is unchanged (polish_weapons.py checks
#   it), and so are the hotspots (model space) and the offsets the weapon Scale applies about (the header's origin).
# - Carriers: a new part rides the old piece it sits on (a connected piece of the old triangles): per frame, the
#   best rigid fit (Kabsch) of that piece's vertices from frame 0, leaving out the vertices that do not move rigidly
#   with the rest (muzzle flashes opening out). So parts recoil, pump and spin with what they sit on.
# - Parts (Polisher): closed low-poly solids -- bands on the outline of what they go round, hexagonal bolt heads,
#   bevelled bars and boxes -- wound as Quake's (clockwise seen from outside) and flat-shaded (each face its own
#   vertices, as the faceted id models look). Each face samples one swatch (a shade of a material ramp, dithered,
#   painted lighter facing up as Quake's skins are, chamfers lit) in rows added under the skin: the old UVs and
#   texels stay.
# - Edge wear (edge_wear): every old triangle is rasterised into its texels; the texels along a convex edge
#   between two big enough faces (a box's corner, a bevel) are moved lighter within their own colours (worn, lit
#   metal), some of the next row scuffed. The id models reuse and overlap their UVs, so a texel changes only when
#   every triangle using it agrees. Fullbright indices (224 and up: the sights, screens, glows) are never touched
#   and never produced.

import math
import os
import struct

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
HEADER = struct.Struct("<4si3f3ff3f8if")
FULLBRIGHT = 224


def anorms():
    import re
    text = open(os.path.join(HERE, "..", "..", "Quake", "anorms.h")).read()
    return np.array([[float(x) for x in m.groups()] for m in
                     re.finditer(r"\{\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\}", text)])


# ----------------------------------------------------------------------------
# Palette ramps


# Quake's palette (id's gfx/palette.lmp, as GPL tools such as ericw-tools embed it): the colours the wear is
# worked out in. Indices 224..255 are fullbright.
PALETTE_HEX = (
    "0000000f0f0f1f1f1f2f2f2f3f3f3f4b4b4b5b5b5b6b6b6b7b7b7b8b8b8b9b9b9babababbbbbbbcbcbcbdbdbdbebebeb"
    "0f0b07170f0b1f170b271b0f2f2313372b173f2f174b371b533b1b5b431f634b1f6b531f73571f7b5f238367238f6f23"
    "0b0b0f13131b1b1b272727332f2f3f37374b3f3f574747674f4f735b5b7f63638b6b6b977373a37b7baf8383bb8b8bcb"
    "0000000707000b0b001313001b1b002323002b2b072f2f073737073f3f074747074b4b0b53530b5b5b0b63630b6b6b0f"
    "0700000f00001700001f00002700002f00003700003f00004700004f00005700005f00006700006f00007700007f0000"
    "1313001b1b002323002f2b00372f004337004b3b075743075f47076b4b0b77530f8357138b5b13975f1ba3631faf6723"
    "2313072f170b3b1f0f4b2313572b17632f1f7337237f3b2b8f43339f4f33af632fbf772fcf8f2bdfab27efcb1ffff31b"
    "0b07001b13002b230f372b1347331b533723633f2b6f47337f533f8b5f479b6b53a77b5fb7876bc3937bd3a38be3b397"
    "ab8ba39f7f979373878b677b7f5b6f7753636b4b575f3f4b5737434b2f3743272f371f232b171b231313170b0b0f0707"
    "bb739faf6b8fa35f839757778b4f6b7f4b5f7343536b3b4b5f333f532b3747232b3b1f232f171b231313170b0b0f0707"
    "dbc3bbcbb3a7bfa39baf978ba3877b977b6f876f5f7b63536b57475f4b3b533f33433327372b1f271f171b130f0f0b07"
    "6f837b677b6f5f7367576b5f4f6357475b4f3f5347374b3f2f43372b3b2f2333271f2b1f1723170f1b130b130b070b07"
    "fff31befdf17dbcb13cbb70fbba70fab970b9b83078b73077b63076b53005b47004b37003b2b002b1f001b0f000b0700"
    "0000ff0b0bef1313df1b1bcf2323bf2b2baf2f2f9f2f2f8f2f2f7f2f2f6f2f2f5f2b2b4f23233f1b1b2f13131f0b0b0f"
    "2b00003b00004b07005f07006f0f007f1707931f07a3270bb7330fc34b1bcf632bdb7f3be3974fe7ab5fefbf77f7d38b"
    "a77b3bb79b37c7c337e7e3577fbfffabe7ffd7ffff6700008b0000b30000d70000ff0000fff393fff7c7ffffff9f5b53"
)


def palette():
    return np.frombuffer(bytes.fromhex("".join(PALETTE_HEX)), np.uint8).reshape(256, 3).astype(np.float64)


class Ramps:
    """Moves palette indices lighter or darker: the colour scaled (and lifted a little, so near-black metal
    still shows a lit edge), then the nearest non-fullbright palette entry of a similar hue. Quake's ramps are
    rows of 16, but the darkest entries of every row are nearly black: stepping along the row from one of them
    would bring out that row's hue (green on black metal); matching colours does not."""

    def __init__(self):
        self.pal = palette()
        self.cache = {}

    def step(self, i, k, lo=None, hi=None):
        if i >= FULLBRIGHT or k == 0:
            return i
        key = (i, k)
        if key in self.cache:
            return self.cache[key]
        c = self.pal[i]
        if k > 0:
            t = c * (1.0 + 0.2 * k) + 7.0 * k
        else:
            t = c * (1.0 - 0.18 * -k)
        cand = self.pal[:FULLBRIGHT]
        # Distance in RGB, with the chroma kept: the difference of the colours' offsets from their grey.
        d = ((cand - t) ** 2).sum(1)
        g = cand - cand.mean(1, keepdims=True)
        gt = t - t.mean()
        d += 2.0 * ((g - gt) ** 2).sum(1)
        lum = cand @ np.array([0.299, 0.587, 0.114])
        l0 = float(c @ np.array([0.299, 0.587, 0.114]))
        d[(lum <= l0) if k > 0 else (lum >= l0)] = 1e18  # always lighter (darker)
        j = int(np.argmin(d))
        if d[j] >= 1e18:
            j = i
        self.cache[key] = j
        return j


# ----------------------------------------------------------------------------
# The model


class Model:
    def __init__(self, path):
        d = open(path, "rb").read()
        self.path = path
        self.h = list(HEADER.unpack_from(d, 0))
        self.scale = np.array(self.h[2:5], np.float64)
        self.origin = np.array(self.h[5:8], np.float64)
        ns, self.sw, self.sh, nv, nt, nf = self.h[12:18]
        off = HEADER.size
        self.skins = []  # [group flag, intervals bytes, [bytearray per image]]
        for _ in range(ns):
            g, = struct.unpack_from("<i", d, off)
            off += 4
            if g == 0:
                self.skins.append([0, b"", [bytearray(d[off:off + self.sw * self.sh])]])
                off += self.sw * self.sh
            else:
                n, = struct.unpack_from("<i", d, off)
                iv = d[off + 4:off + 4 + 4 * n]
                off += 4 + 4 * n
                ims = []
                for _ in range(n):
                    ims.append(bytearray(d[off:off + self.sw * self.sh]))
                    off += self.sw * self.sh
                self.skins.append([1, iv, ims])
        self.st = [list(struct.unpack_from("<3i", d, off + 12 * i)) for i in range(nv)]
        off += 12 * nv
        self.tris = [tuple(struct.unpack_from("<4i", d, off + 16 * i)) for i in range(nt)]
        off += 16 * nt
        self.frames = []  # ["simple", header 24 bytes, verts] or ["group", n, bbox 8, intervals, [[hdr, verts]]]
        for _ in range(nf):
            t, = struct.unpack_from("<i", d, off)
            off += 4
            if t == 0:
                self.frames.append(["simple", bytearray(d[off:off + 24]), bytearray(d[off + 24:off + 24 + 4 * nv])])
                off += 24 + 4 * nv
            else:
                n, = struct.unpack_from("<i", d, off)
                bbox = d[off + 4:off + 12]
                iv = d[off + 12:off + 12 + 4 * n]
                off += 12 + 4 * n
                subs = []
                for _ in range(n):
                    subs.append([bytearray(d[off:off + 24]), bytearray(d[off + 24:off + 24 + 4 * nv])])
                    off += 24 + 4 * nv
                self.frames.append(["group", n, bytearray(bbox), iv, subs])
        assert off == len(d), "trailing bytes in %s" % path
        self.old_nv, self.old_nt, self.old_sh = nv, nt, self.sh
        # Every pose's byte box: new vertices must fit in the model's old grid.
        self.new_pos = []  # per new vertex: list of positions per pose
        self.new_nrm = []

    # Poses: every simple frame, and every frame of each group, in file order.
    def pose_blocks(self):
        out = []
        for fr in self.frames:
            if fr[0] == "simple":
                out.append(fr)
            else:
                out.extend(fr[4])
        return out

    def num_poses(self):
        return len(self.pose_blocks())

    def pose_bytes(self, p):
        b = self.pose_blocks()[p]
        return b[2] if b[0] == "simple" else b[1]

    def positions(self, p):
        b = np.frombuffer(bytes(self.pose_bytes(p)), np.uint8).reshape(-1, 4)[: self.old_nv]
        return b[:, :3].astype(np.float64) * self.scale + self.origin

    def world_box(self):
        return self.origin, self.origin + 255.0 * self.scale

    def add_vertex(self, s, t, per_pose, normal_per_pose=None):
        self.st.append([0, int(s), int(t)])
        self.new_pos.append(per_pose)
        self.new_nrm.append(normal_per_pose)
        return len(self.st) - 1

    def grow_skin(self, rows):
        """Adds `rows` rows under every skin image (filled with index 0); returns the first new row."""
        first = self.sh
        for sk in self.skins:
            for i, im in enumerate(sk[2]):
                sk[2][i] = im + bytearray(self.sw * rows)
        self.sh += rows
        return first

    def skin(self, k=0, im=0):
        return self.skins[k][2][im]

    def write(self, path):
        table = anorms()
        nv = len(self.st)
        blocks = self.pose_blocks()
        # Normal indices for new vertices: the nearest of Quake's table to each pose's normal.
        for p, blk in enumerate(blocks):
            vb = blk[2] if blk[0] == "simple" else blk[1]
            extra = bytearray()
            for i, per_pose in enumerate(self.new_pos):
                q = np.round((np.asarray(per_pose[p]) - self.origin) / self.scale).astype(int)
                if (q < 0).any() or (q > 255).any():
                    raise ValueError("%s: new vertex %d outside the model's grid in pose %d: %s" %
                                     (os.path.basename(path), self.old_nv + i, p, per_pose[p]))
                n = self.new_nrm[i][p] if self.new_nrm[i] is not None else (0.0, 0.0, 1.0)
                ni = int(np.argmax(table @ np.asarray(n)))
                extra += bytes((int(q[0]), int(q[1]), int(q[2]), ni))
            vb[:] = vb[: 4 * self.old_nv] + extra
            # The pose's own bounds (bytes), over every vertex.
            arr = np.frombuffer(bytes(vb), np.uint8).reshape(-1, 4)
            hdr = blk[1] if blk[0] == "simple" else blk[0]
            hdr[0:3] = bytes(int(x) for x in arr[:, :3].min(0))
            hdr[4:7] = bytes(int(x) for x in arr[:, :3].max(0))
        h = list(self.h)
        h[13], h[14], h[15], h[16] = self.sw, self.sh, nv, len(self.tris)
        out = bytearray(HEADER.pack(*h))
        for g, iv, ims in self.skins:
            if g == 0:
                out += struct.pack("<i", 0) + ims[0]
            else:
                out += struct.pack("<ii", 1, len(ims)) + iv
                for im in ims:
                    out += im
        for st in self.st:
            out += struct.pack("<3i", *st)
        for t in self.tris:
            out += struct.pack("<4i", *t)
        for fr in self.frames:
            if fr[0] == "simple":
                out += struct.pack("<i", 0) + fr[1] + fr[2]
            else:
                # The group's bounds cover its frames'.
                mins = [min(s[0][k] for s in fr[4]) for k in range(3)]
                maxs = [max(s[0][4 + k] for s in fr[4]) for k in range(3)]
                fr[2][0:3] = bytes(mins)
                fr[2][4:7] = bytes(maxs)
                out += struct.pack("<ii", 1, fr[1]) + fr[2] + fr[3]
                for hdr, vb in fr[4]:
                    out += hdr + vb
        with open(path, "wb") as f:
            f.write(out)


# ----------------------------------------------------------------------------
# Topology of the old mesh (frame 0)


def tri_normal(p0, p1, p2):
    """Outward normal of a Quake triangle (front faces clockwise seen from outside)."""
    n = -np.cross(p1 - p0, p2 - p0)
    l = np.linalg.norm(n)
    return n / l if l > 1e-12 else n


class Mesh0:
    """The old triangles in frame 0: welded vertices, normals, pieces."""

    def __init__(self, model, pose=0):
        self.m = model
        P = model.positions(pose)
        self.P = P
        q = np.frombuffer(bytes(model.pose_bytes(pose)), np.uint8).reshape(-1, 4)[: model.old_nv, :3]
        key = {}
        self.weld = np.zeros(model.old_nv, int)
        for i, k in enumerate(map(bytes, q)):
            self.weld[i] = key.setdefault(k, len(key))
        self.tris = [t for t in model.tris[: model.old_nt]]
        self.normals = []
        self.areas = []
        for _, a, b, c in self.tris:
            n = tri_normal(P[a], P[b], P[c])
            self.normals.append(n)
            self.areas.append(0.5 * np.linalg.norm(np.cross(P[b] - P[a], P[c] - P[a])))
        # Pieces (connected by shared vertex indices, as the file has them: rigid parts are separate pieces).
        parent = list(range(model.old_nv))

        def find(x):
            while parent[x] != x:
                parent[x] = parent[parent[x]]
                x = parent[x]
            return x

        for _, a, b, c in self.tris:
            parent[find(a)] = find(b)
            parent[find(b)] = find(c)
        self.piece = np.array([find(v) for v in range(model.old_nv)])

    def edge_neighbours(self):
        """{(welded a, welded b) sorted: [(tri, opposite vertex)]}."""
        e = {}
        for ti, (_, a, b, c) in enumerate(self.tris):
            if self.areas[ti] < 1e-6:
                continue
            for x, y, z in ((a, b, c), (b, c, a), (c, a, b)):
                k = tuple(sorted((self.weld[x], self.weld[y])))
                e.setdefault(k, []).append((ti, z))
        return e

    def nearest_tri(self, p, exclude=None):
        """The old triangle (with area) nearest to point p, and the distance."""
        best, bd = None, 1e30
        for ti, (_, a, b, c) in enumerate(self.tris):
            if self.areas[ti] < 1e-4 or (exclude and ti in exclude):
                continue
            d = point_tri_dist(p, self.P[a], self.P[b], self.P[c])
            if d < bd:
                best, bd = ti, d
        return best, bd

    def raycast(self, o, d):
        """Nearest hit (distance, tri) of the ray o + t d (t > 0) on the old front faces."""
        best = (None, None)
        for ti, (_, a, b, c) in enumerate(self.tris):
            if self.areas[ti] < 1e-6:
                continue
            t = ray_tri(o, d, self.P[a], self.P[b], self.P[c])
            if t is not None and t > 1e-6 and (best[0] is None or t < best[0]):
                best = (t, ti)
        return best


def point_tri_dist(p, a, b, c):
    # Ericson, Real-Time Collision Detection 5.1.5.
    ab, ac, ap = b - a, c - a, p - a
    d1, d2 = ab @ ap, ac @ ap
    if d1 <= 0 and d2 <= 0:
        return np.linalg.norm(p - a)
    bp = p - b
    d3, d4 = ab @ bp, ac @ bp
    if d3 >= 0 and d4 <= d3:
        return np.linalg.norm(p - b)
    vc = d1 * d4 - d3 * d2
    if vc <= 0 and d1 >= 0 and d3 <= 0:
        v = d1 / (d1 - d3)
        return np.linalg.norm(p - (a + v * ab))
    cp = p - c
    d5, d6 = ab @ cp, ac @ cp
    if d6 >= 0 and d5 <= d6:
        return np.linalg.norm(p - c)
    vb = d5 * d2 - d1 * d6
    if vb <= 0 and d2 >= 0 and d6 <= 0:
        w = d2 / (d2 - d6)
        return np.linalg.norm(p - (a + w * ac))
    va = d3 * d6 - d5 * d4
    if va <= 0 and (d4 - d3) >= 0 and (d5 - d6) >= 0:
        w = (d4 - d3) / ((d4 - d3) + (d5 - d6))
        return np.linalg.norm(p - (b + w * (c - b)))
    denom = 1.0 / (va + vb + vc)
    v, w = vb * denom, vc * denom
    return np.linalg.norm(p - (a + ab * v + ac * w))


def ray_tri(o, d, a, b, c):
    e1, e2 = b - a, c - a
    h = np.cross(d, e2)
    det = e1 @ h
    if abs(det) < 1e-12:
        return None
    f = 1.0 / det
    s = o - a
    u = f * (s @ h)
    if u < 0 or u > 1:
        return None
    q = np.cross(s, e1)
    v = f * (d @ q)
    if v < 0 or u + v > 1:
        return None
    return f * (e2 @ q)


# ----------------------------------------------------------------------------
# Rigid carriers


def kabsch(A, B):
    """R, t with R A_i + t ~ B_i."""
    ca, cb = A.mean(0), B.mean(0)
    H = (A - ca).T @ (B - cb)
    U, S, Vt = np.linalg.svd(H)
    dd = np.sign(np.linalg.det(Vt.T @ U.T))
    D = np.diag([1.0, 1.0, dd])
    R = Vt.T @ D @ U.T
    return R, cb - R @ ca


class Carrier:
    """The rigid motion, per pose, of a set of the old vertices (a piece), from pose 0."""

    def __init__(self, model, verts, tol=0.35):
        verts = np.asarray(sorted(set(int(v) for v in verts)))
        P0 = model.positions(0)[verts]
        self.xf = []
        for p in range(model.num_poses()):
            Pp = model.positions(p)[verts]
            keep = np.ones(len(verts), bool)
            for _ in range(4):
                R, t = kabsch(P0[keep], Pp[keep])
                res = np.linalg.norm(P0 @ R.T + t - Pp, axis=1)
                nk = res < max(tol, 2.5 * np.median(res[keep]) + 1e-9)
                if nk.sum() < 3 or (nk == keep).all():
                    break
                keep = nk
            self.xf.append((R, t))

    def place(self, p, pose):
        R, t = self.xf[pose]
        return R @ np.asarray(p) + t

    def turn(self, n, pose):
        return self.xf[pose][0] @ np.asarray(n)


# ----------------------------------------------------------------------------
# Skin space for the new parts: a shelf allocator in the rows added under the skin


class Atlas:
    def __init__(self, model, rows):
        self.m = model
        self.t0 = model.grow_skin(rows)
        self.x = 0
        self.y = self.t0
        self.shelf = 0

    def alloc(self, w, h):
        if self.x + w > self.m.sw:
            self.x = 0
            self.y += self.shelf
            self.shelf = 0
        assert self.y + h <= self.m.sh, "atlas full"
        r = (self.x, self.y, self.x + w, self.y + h)
        self.x += w
        self.shelf = max(self.shelf, h)
        return r

    def used_rows(self):
        return self.y + self.shelf - self.t0


class Painter:
    """Paints swatches in a model's skin (every skin image), in palette indices."""

    def __init__(self, model, ramps, seed=1):
        self.m = model
        self.r = ramps
        self.seed = seed

    def rand(self, s, t):
        h = (s * 73856093) ^ (t * 19349663) ^ (self.seed * 83492791)
        h = (h ^ (h >> 13)) * 1274126177 & 0xFFFFFFFF
        return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0

    def fill(self, rect, fn):
        """fn(u, v, noise) -> palette index; u, v in 0..1 across the rect."""
        s0, t0, s1, t1 = rect
        for sk in self.m.skins:
            for im in sk[2]:
                for t in range(t0, t1):
                    for s in range(s0, s1):
                        u = (s - s0 + 0.5) / (s1 - s0)
                        v = (t - t0 + 0.5) / (t1 - t0)
                        im[t * self.m.sw + s] = fn(u, v, self.rand(s, t))


def shade(ramp, x, noise, spread=0.12):
    """The ramp's colour at x (0 dark .. 1 light), dithered by noise."""
    x = x + (noise - 0.5) * spread
    k = int(round(max(0.0, min(1.0, x)) * (len(ramp) - 1)))
    return ramp[k]


# ----------------------------------------------------------------------------
# Parts


class Parts:
    """New vertices (frame-0 model space) and triangles, each wound clockwise seen from outside."""

    def __init__(self):
        self.verts = []  # (position, (s, t), carrier key)
        self.tris = []
        self.carrier = None  # the current carrier key for new vertices

    def vert(self, p, st):
        self.verts.append((np.asarray(p, np.float64), (int(math.floor(st[0])), int(math.floor(st[1]))), self.carrier))
        return len(self.verts) - 1

    def tri(self, a, b, c, outward):
        p0, p1, p2 = (self.verts[i][0] for i in (a, b, c))
        if tri_normal(p0, p1, p2) @ np.asarray(outward) < 0:
            b, c = c, b
        self.tris.append((a, b, c))



def bevel_rect(hx, hy, b):
    b = min(b, 0.49 * hx, 0.49 * hy)
    return [(hx, -hy + b), (hx, hy - b), (hx - b, hy), (-hx + b, hy), (-hx, hy - b), (-hx, -hy + b), (-hx + b, -hy), (hx - b, -hy)]


def piece_of_tri(mesh, ti):
    root = mesh.piece[mesh.tris[ti][1]]
    return np.nonzero(mesh.piece == root)[0]


# ----------------------------------------------------------------------------
# Edge wear on the old skin


def tri_uv(model, tri):
    front, a, b, c = tri
    uv = []
    for v in (a, b, c):
        on, s, t = model.st[v]
        if on and not front:
            s += model.sw // 2
        uv.append((s + 0.5, t + 0.5))
    return np.array(uv, np.float64)


def uv_inradius(uv):
    a = np.linalg.norm(uv[1] - uv[0])
    b = np.linalg.norm(uv[2] - uv[1])
    c = np.linalg.norm(uv[0] - uv[2])
    area = 0.5 * abs((uv[1, 0] - uv[0, 0]) * (uv[2, 1] - uv[0, 1]) - (uv[2, 0] - uv[0, 0]) * (uv[1, 1] - uv[0, 1]))
    return 2 * area / max(1e-9, a + b + c)


def edge_wear(model, ramps, mesh=None, light=1, min_angle=50.0, wear=0.25, seed=7, protect=None, tris=None,
              max_t=None, min_area=0.4, min_inradius=1.8, min_edge=1.2, wear_inradius=3.0, max_width=0.6):
    """Lightens texels along convex edges (the first texel row in from the edge, by `light` steps and one more
    on sharp edges; some texels of the next row, at random, by one: wear). Conservative, as the old models reuse
    and overlap their UVs: only edges between two triangles big enough in the model (`min_area` square units)
    and in the skin (`min_inradius` texels), at least `min_edge` units long, with a texel's every user agreeing;
    only old texels above row `max_t` (the texels earlier rounds painted with their own highlights are below);
    `protect(s, t)` True keeps a texel. Returns the count of changed texels."""
    mesh = mesh or Mesh0(model)
    P = mesh.P
    en = mesh.edge_neighbours()
    max_t = model.old_sh if max_t is None else max_t
    ok_tri = {}
    roomy = {}
    for ti, tri in enumerate(mesh.tris):
        uv = tri_uv(model, tri)
        roomy[ti] = uv_inradius(uv) >= wear_inradius
        ok_tri[ti] = (mesh.areas[ti] >= min_area and uv_inradius(uv) >= min_inradius and uv[:, 1].max() <= max_t + 0.5)
    kind = {}  # (tri, welded edge) -> angle (convex edges only)
    for k, lst in en.items():
        if len(lst) != 2:
            continue
        (t1, o1), (t2, o2) = lst
        if not (ok_tri[t1] and ok_tri[t2]):
            continue
        if np.linalg.norm(mesh.P[np.nonzero(mesh.weld == k[0])[0][0]] - mesh.P[np.nonzero(mesh.weld == k[1])[0][0]]) < min_edge:
            continue
        n1, n2 = mesh.normals[t1], mesh.normals[t2]
        ang = math.degrees(math.acos(max(-1.0, min(1.0, float(n1 @ n2)))))
        if ang < min_angle:
            continue
        if float((P[o2] - P[o1]) @ n1) < 0 and float((P[o1] - P[o2]) @ n2) < 0:  # convex both ways
            kind[(t1, k)] = ang
            kind[(t2, k)] = ang
    sw = model.sw
    votes = {}
    rng = np.random.default_rng(seed)
    allowed = set(tris) if tris is not None else None
    for ti in range(len(mesh.tris)):  # every old triangle votes; only the allowed ones lighten
        tri = mesh.tris[ti]
        if mesh.areas[ti] < 1e-6:
            continue
        uv = tri_uv(model, tri)
        area2 = (uv[1, 0] - uv[0, 0]) * (uv[2, 1] - uv[0, 1]) - (uv[2, 0] - uv[0, 0]) * (uv[1, 1] - uv[0, 1])
        if abs(area2) < 1e-9:
            continue
        sgn = 1.0 if area2 > 0 else -1.0
        vids = tri[1:]
        # The mapping from the skin to the model (units per texel along s and t).
        duv = np.array([uv[1] - uv[0], uv[2] - uv[0]]).T
        dP = np.array([P[vids[1]] - P[vids[0]], P[vids[2]] - P[vids[0]]]).T
        J = dP @ np.linalg.inv(duv)
        edges = []
        for e in range(3):
            x, y = vids[e], vids[(e + 1) % 3]
            k = tuple(sorted((mesh.weld[x], mesh.weld[y])))
            p0, p1 = uv[e], uv[(e + 1) % 3]
            d = p1 - p0
            L = math.hypot(*d)
            nrm = np.array([-d[1], d[0]]) / L * sgn if L > 1e-9 else np.zeros(2)
            emit = ok_tri[ti] and (allowed is None or ti in allowed)
            if emit:
                # How wide one texel in from this edge is on the model: a sheared or stretched mapping would
                # turn the one-texel line into a band.
                emit = float(np.linalg.norm(J @ nrm)) <= max_width
            edges.append((p0, nrm, kind.get((ti, k), 0.0) if emit else 0.0))
        s_lo, s_hi = int(math.floor(uv[:, 0].min() - 1)), int(math.ceil(uv[:, 0].max() + 1))
        t_lo, t_hi = int(math.floor(uv[:, 1].min() - 1)), int(math.ceil(uv[:, 1].max() + 1))
        for t in range(max(0, t_lo), min(model.old_sh, t_hi)):
            for s in range(max(0, s_lo), min(sw, s_hi)):
                pc = np.array([s + 0.5, t + 0.5])
                ds = [float((pc - p0) @ nr) for p0, nr, _ in edges]
                if min(ds) < -0.5:
                    continue
                steps = 0
                for (p0, nr, ang), dd in zip(edges, ds):
                    if ang > 0 and dd <= 1.0:
                        steps = max(steps, light + (1 if ang > 60 else 0))
                if steps == 0 and wear > 0 and roomy[ti]:
                    for (p0, nr, ang), dd in zip(edges, ds):  # (rng drawn only here: the same wear every run)
                        if ang > 0 and 1.0 < dd <= 2.0 and rng.random() < wear:
                            steps = 1
                votes.setdefault((s, t), []).append(steps)
    changed = 0
    for (s, t), vs in votes.items():
        # A texel several triangles use (mirrored sides, overlapping UVs) changes only if they all agree.
        steps = vs[0]
        if steps == 0 or t >= max_t or any(v != steps for v in vs) or (protect and protect(s, t)):
            continue
        for sk in model.skins:
            for im in sk[2]:
                i = im[t * sw + s]
                if i >= FULLBRIGHT:
                    continue
                j = ramps.step(i, steps)
                if j != i:
                    im[t * sw + s] = j
                    changed += 1
    return changed


# ----------------------------------------------------------------------------
# Detail parts, flat-shaded (each face its own vertices, as the faceted id models look) and painted face by face:
# each face samples one swatch row (two texel rows of one shade of one material, dithered), its s along the face.

MATERIALS = {  # palette indices, dark to light
    "steel": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12],
    "blued": [0, 32, 1, 33, 2, 34, 3, 35, 36, 37, 38, 39, 40, 41],
    "brown": [16, 174, 17, 173, 18, 19, 172, 20, 171, 170, 169, 168],   # the launchers' browns
    "bronze": [16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27],
    "walnut": [16, 17, 18, 96, 19, 97, 20, 98, 21, 99, 100],
    "red": [64, 65, 142, 66, 141, 67, 68, 69, 70, 71],                  # the proximity gun's dark reds
    "black": [0, 0, 1, 1, 2, 2, 3],
}


def convex_hull(pts):
    """Andrew's monotone chain: the hull of 2D points, counter-clockwise."""
    pts = sorted(set((round(x, 6), round(y, 6)) for x, y in pts))
    if len(pts) < 3:
        return pts

    def cr(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    lo, hi = [], []
    for p in pts:
        while len(lo) >= 2 and cr(lo[-2], lo[-1], p) <= 0:
            lo.pop()
        lo.append(p)
    for p in reversed(pts):
        while len(hi) >= 2 and cr(hi[-2], hi[-1], p) <= 0:
            hi.pop()
        hi.append(p)
    return lo[:-1] + hi[:-1]


def simplify(poly, tol, min_edge=0.0):
    """Drops the corners of a closed polygon within `tol` of the line through their neighbours, or of the
    previous corner."""
    out = list(poly)
    changed = True
    while changed and len(out) > 3:
        changed = False
        for i in range(len(out)):
            a, b, c = np.array(out[i - 1]), np.array(out[i]), np.array(out[(i + 1) % len(out)])
            ac = c - a
            L = np.linalg.norm(ac)
            d = abs(ac[0] * (b - a)[1] - ac[1] * (b - a)[0]) / L if L > 1e-9 else 0.0
            if d < tol or np.linalg.norm(b - a) < max(tol, min_edge):
                out.pop(i)
                changed = True
                break
    return out


def offset_polygon(poly, off):
    """A convex counter-clockwise polygon moved out by `off` (its edges parallel, the corners mitred)."""
    n = len(poly)
    out = []
    for i in range(n):
        a, b, c = np.array(poly[i - 1]), np.array(poly[i]), np.array(poly[(i + 1) % n])
        e1, e2 = b - a, c - b
        n1 = np.array([e1[1], -e1[0]]) / np.linalg.norm(e1)
        n2 = np.array([e2[1], -e2[0]]) / np.linalg.norm(e2)
        m = (n1 + n2) / (1.0 + float(n1 @ n2))
        out.append((float(b[0] + m[0] * off), float(b[1] + m[1] * off)))
    return out


class Polisher:
    """Adds detail parts to a model (`band`, `stud`, `bar`, `box`, `revolve`) and writes it (`finish`)."""

    def __init__(self, path, rows=12, seed=1, width=16):
        self.m = Model(path)
        self.mesh = Mesh0(self.m)
        self.ramps = Ramps()
        self.atlas = Atlas(self.m, rows)
        self.painter = Painter(self.m, self.ramps, seed)
        self.width = width
        self.polys = []  # (points, outward, material, level, carrier key)
        self.carriers = {}
        self.swatches = {}

    # -- where things are ---------------------------------------------------
    def hit(self, origin, direction):
        """The first old surface point along the ray: (point, outward normal, triangle)."""
        o = np.asarray(origin, np.float64)
        d = np.asarray(direction, np.float64)
        d = d / np.linalg.norm(d)
        t, ti = self.mesh.raycast(o, d)
        if ti is None:
            raise ValueError("%s: nothing hit from %s along %s" % (os.path.basename(self.m.path), origin, direction))
        n = self.mesh.normals[ti]
        if n @ d > 0:
            n = -n
        return o + t * d, n, ti

    def carrier_key(self, ti):
        root = int(self.mesh.piece[self.mesh.tris[ti][1]])
        if root not in self.carriers:
            self.carriers[root] = Carrier(self.m, piece_of_tri(self.mesh, ti))
        return root

    def carrier_at(self, p):
        ti, _ = self.mesh.nearest_tri(np.asarray(p, np.float64))
        return self.carrier_key(ti)

    # -- faces -----------------------------------------------------------------
    def face(self, pts, outward, material, level, key):
        pts = [np.asarray(p, np.float64) for p in pts]
        clean = []  # repeated corners dropped (a profile point on the axis)
        for p in pts:
            if not clean or np.linalg.norm(p - clean[-1]) > 1e-7:
                clean.append(p)
        if len(clean) > 1 and np.linalg.norm(clean[0] - clean[-1]) < 1e-7:
            clean.pop()
        if len(clean) < 3:
            return
        self.polys.append((clean, np.asarray(outward, np.float64), material, float(level), key))

    def lit(self, n, base=0.45, gain=0.35):
        """A face's painted shade: lighter facing up (Quake's skins are painted lit from above)."""
        n = np.asarray(n, np.float64)
        n = n / (np.linalg.norm(n) or 1.0)
        return base + gain * float(n[2])

    # -- primitives ----------------------------------------------------------
    def revolve(self, centre, axis, ref, profile, material, key, sides=8, base=None, phase=0.5, shades=None, skip=()):
        """A solid of revolution round `axis` through `centre`: `profile` a closed polygon of (along, out) points
        (out from `base(k)`, the radius at side k, or from the axis), faceted into `sides`. `shades[j]` (optional)
        adds to segment j's shade; `skip`: segments left open (hidden inside the gun: a stud's foot)."""
        c = np.asarray(centre, np.float64)
        ax = np.asarray(axis, np.float64)
        ax = ax / np.linalg.norm(ax)
        u = np.asarray(ref, np.float64)
        u = u - ax * (u @ ax)
        u = u / np.linalg.norm(u)
        v = np.cross(ax, u)
        dirs = [math.cos(2 * math.pi * (k + phase) / sides) * u + math.sin(2 * math.pi * (k + phase) / sides) * v
                for k in range(sides)]
        rb = [base(k) if callable(base) else (base or 0.0) for k in range(sides)]
        n = len(profile)
        area = sum(profile[j][0] * profile[(j + 1) % n][1] - profile[(j + 1) % n][0] * profile[j][1] for j in range(n))
        orient = 1.0 if area > 0 else -1.0

        def P(k, j):
            a, r = profile[j % n]
            return c + ax * a + dirs[k % sides] * (rb[k % sides] + r)

        for j in range(n):
            if j in skip:
                continue
            (a0, r0), (a1, r1) = profile[j], profile[(j + 1) % n]
            da, dr = a1 - a0, r1 - r0
            na, nr = dr * orient, -da * orient  # the segment's outward normal in the (along, out) plane
            for k in range(sides):
                mid_dir = dirs[k] + dirs[(k + 1) % sides]
                mid_dir = mid_dir / (np.linalg.norm(mid_dir) or 1.0)
                out = na * ax + nr * mid_dir
                if np.linalg.norm(out) < 1e-9:
                    continue
                lvl = self.lit(out) + (shades[j] if shades else 0.0)
                self.face([P(k, j), P(k + 1, j), P(k + 1, j + 1), P(k, j + 1)], out, material, lvl, key)

    def stud(self, origin, direction, radius=0.45, height=0.3, sides=6, material="steel", sink=0.15, bevel=None,
             ref=None, slot=False):
        """A bolt head (hexagonal by default) on the surface the ray from `origin` along `direction` meets."""
        p, n, ti = self.hit(origin, direction)
        key = self.carrier_key(ti)
        b = min(radius * 0.4, height * 0.6) if bevel is None else bevel
        ref = np.asarray(ref if ref is not None else ((0.0, 0.0, 1.0) if abs(n[2]) < 0.9 else (1.0, 0.0, 0.0)), np.float64)
        prof = [(-sink, 0.0), (-sink, radius), (height - b, radius), (height, radius - b), (height, 0.0)]
        # The chamfer lit (a worn edge), the top a little lighter, the sides as they face.
        self.revolve(p, n, ref, prof, material, key, sides=sides, shades=[0, -0.05, 0.2, 0.1, 0], skip=(0,))
        if slot:
            # A slot across the head: a dark bar sunk into the top.
            u = ref - n * (ref @ n)
            u = u / np.linalg.norm(u)
            w = np.cross(n, u)
            L = (radius - b) * 0.95
            self.box(p + n * (height - 0.03), u, w, n, (L, radius * 0.16, 0.05), "black", key, levels=(0.1, 0.1))
        return p, n

    def box(self, centre, u, v, w, half, material, key, bevel=0.0, levels=None):
        """A box (half sizes along u, v, w), its edges along w bevelled; `levels` (sides, caps) or lit faces."""
        c = np.asarray(centre, np.float64)
        u, v, w = (np.asarray(x, np.float64) for x in (u, v, w))
        hu, hv, hw = half
        prof = bevel_rect(hu, hv, bevel) if bevel > 0 else [(hu, -hv), (hu, hv), (-hu, hv), (-hu, -hv)]
        lo = [c + x * u + y * v - hw * w for x, y in prof]
        hi = [c + x * u + y * v + hw * w for x, y in prof]
        m = len(prof)
        for i in range(m):
            j = (i + 1) % m
            mid = 0.5 * (lo[i] + lo[j]) - (c - hw * w)
            lvl = levels[0] if levels else self.lit(mid) + (0.15 if bevel > 0 and i % 2 == 1 else 0.0)
            self.face([lo[i], lo[j], hi[j], hi[i]], mid, material, lvl, key)
        self.face(hi, w, material, levels[1] if levels else self.lit(w) + 0.05, key)
        self.face(lo[::-1], -w, material, levels[1] if levels else self.lit(-w), key)

    def outline(self, centre, axis, ref, width, reach=30.0, rays=72, samples=3, only=None, clearance=0.02):
        """The convex outline, in the plane through `centre` square to `axis` (as (u, v) coordinates on the basis
        from `ref`), of what crosses that plane over `width` along the axis: rays in towards the axis, the hull of
        their hits, `clearance` out. Returns (hull points (counter-clockwise), u, v, a triangle hit)."""
        c = np.asarray(centre, np.float64)
        ax = np.asarray(axis, np.float64)
        ax = ax / np.linalg.norm(ax)
        u = np.asarray(ref, np.float64)
        u = u - ax * (u @ ax)
        u = u / np.linalg.norm(u)
        v = np.cross(ax, u)
        if isinstance(only, str) and only == "piece":
            # The piece the band goes round: the first thing a ray from the `ref` side meets.
            _, ti0 = self.mesh.raycast(c + u * reach, -u)
            if ti0 is None:
                raise ValueError("band at %s: nothing on the %s side" % (centre, ref))
            root = self.mesh.piece[self.mesh.tris[ti0][1]]
            only = [ti for ti, t in enumerate(self.mesh.tris) if self.mesh.piece[t[1]] == root]
        pts = []
        hit_tri = None
        for k in range(rays):
            ang = 2 * math.pi * k / rays
            d = math.cos(ang) * u + math.sin(ang) * v
            for s_ in np.linspace(-width / 2, width / 2, samples):
                o = c + ax * s_ + d * reach
                t, ti = self.mesh.raycast(o, -d) if only is None else self._raycast_only(o, -d, only)
                if ti is not None and t < reach:
                    q = o - d * t - c
                    pts.append((float(q @ u), float(q @ v)))
                    hit_tri = ti if hit_tri is None else hit_tri
        if len(pts) < 3:
            raise ValueError("band at %s: nothing to go round" % (centre,))
        hull = convex_hull(pts)
        hull = simplify(hull, 0.06, min_edge=0.35)
        return offset_polygon(hull, clearance), u, v, hit_tri

    def band(self, centre, axis, ref, width, thickness, material, key=None, chamfer=None, sink=0.12, only="piece",
             shades=(0.05, 0.22, 0.0, 0.22, 0.05, -0.3), outline=None):
        """A band round whatever crosses the plane through `centre` square to `axis`: its inside on the outline of
        what it goes round (boxes, hexagonal tubes, barrels side by side), `thickness` out from it, chamfered at both
        edges, closed inside."""
        c = np.asarray(centre, np.float64)
        ax = np.asarray(axis, np.float64)
        ax = ax / np.linalg.norm(ax)
        hull, u, v, ti = outline if outline is not None else self.outline(c, ax, ref, width, only=only)
        key = key if key is not None else self.carrier_key(ti)
        # As thick as fits the model's old bounds in every frame (a band near the top of a recoiling barrel).
        while True:
            ring = offset_polygon(hull, thickness)
            pts = [c + ax * s * width / 2 + u * x + v * y for x, y in ring for s in (-1, 1)]
            if self.fits(pts, key):
                break
            thickness -= 0.02
            if thickness < 0.08:
                raise ValueError("band at %s: does not fit in the model's bounds" % (centre,))
        ch = min(thickness * 0.5, width * 0.25) if chamfer is None else chamfer
        w2 = width / 2
        prof = [(-w2, -sink), (-w2, thickness - ch), (-w2 + ch, thickness), (w2 - ch, thickness), (w2, thickness - ch),
                (w2, -sink)]
        n = len(hull)
        rings = [offset_polygon(hull, off) for _, off in prof]
        for j in range(len(prof)):
            j2 = (j + 1) % len(prof)
            (a0, _), (a1, _) = prof[j], prof[j2]
            for k in range(n):
                k2 = (k + 1) % n
                pts = [c + ax * a0 + u * rings[j][k][0] + v * rings[j][k][1],
                       c + ax * a0 + u * rings[j][k2][0] + v * rings[j][k2][1],
                       c + ax * a1 + u * rings[j2][k2][0] + v * rings[j2][k2][1],
                       c + ax * a1 + u * rings[j2][k][0] + v * rings[j2][k][1]]
                # Outward: the edge's normal in the plane, and the profile segment's.
                ex, ey = hull[k2][0] - hull[k][0], hull[k2][1] - hull[k][1]
                en = np.array([ey, -ex]) / (math.hypot(ex, ey) or 1.0)  # outward for a counter-clockwise hull
                out = self._profile_normal(prof, j)  # the profile segment's, from its winding
                out3 = out[0] * ax + out[1] * (en[0] * u + en[1] * v)
                lvl = self.lit(out3) + (shades[j] if shades else 0.0)
                self.face(pts, out3, material, lvl, key)
        return hull

    @staticmethod
    def _profile_normal(prof, j):
        n = len(prof)
        area = sum(prof[i][0] * prof[(i + 1) % n][1] - prof[(i + 1) % n][0] * prof[i][1] for i in range(n))
        orient = 1.0 if area > 0 else -1.0
        (a0, r0), (a1, r1) = prof[j], prof[(j + 1) % n]
        return (orient * (r1 - r0), -orient * (a1 - a0))

    def fits(self, pts, key, margin=0.0):
        """Whether frame-0 points carried by `key` stay inside the model's byte box in every frame."""
        lo, hi = self.m.world_box()
        car = self.carriers[key]
        for k in range(self.m.num_poses()):
            for q in pts:
                w = car.place(q, k)
                if (w < lo - 0.5 * self.m.scale + margin).any() or (w > hi + 0.5 * self.m.scale - margin).any():
                    return False
        return True

    def _raycast_only(self, o, d, only):
        best = (None, None)
        for ti in only:
            _, a, b, c = self.mesh.tris[ti]
            t = ray_tri(o, d, self.mesh.P[a], self.mesh.P[b], self.mesh.P[c])
            if t is not None and t > 1e-6 and (best[0] is None or t < best[0]):
                best = (t, ti)
        return best

    def bar(self, p0, p1, up, width, height, material, key=None, bevel=0.0, levels=None):
        """A box from p0 to p1 (its middle line), `height` along `up` and `width` across."""
        p0 = np.asarray(p0, np.float64)
        p1 = np.asarray(p1, np.float64)
        w = p1 - p0
        L = np.linalg.norm(w)
        w = w / L
        u = np.asarray(up, np.float64) - w * (np.asarray(up, np.float64) @ w)
        u = u / np.linalg.norm(u)
        v = np.cross(w, u)
        key = key if key is not None else self.carrier_at(0.5 * (p0 + p1))
        self.box(0.5 * (p0 + p1), u, v, w, (height / 2, width / 2, L / 2), material, key, bevel=bevel, levels=levels)
        return key

    # -- output ------------------------------------------------------------------
    def swatch(self, material, level):
        ramp = MATERIALS[material]
        k = max(0, min(len(ramp) - 1, int(round(level * (len(ramp) - 1)))))
        key = (material, k)
        if key not in self.swatches:
            rect = self.atlas.alloc(self.width, 2)
            x = k / max(1, len(ramp) - 1)
            self.painter.fill(rect, lambda uu, vv, nz: shade(ramp, x, nz, spread=1.6 / len(ramp)))
            self.swatches[key] = rect
        return self.swatches[key]

    def finish(self, path):
        parts = Parts()
        poses = self.m.num_poses()
        base = len(self.m.st)
        for pts, outward, material, level, key in self.polys:
            s0, t0, s1, t1 = self.swatch(material, level)
            # s along the face's longest edge, from one end of the swatch to the other; t the swatch's first row.
            _, i = max((np.linalg.norm(pts[(i + 1) % len(pts)] - pts[i]), i) for i in range(len(pts)))
            e = pts[(i + 1) % len(pts)] - pts[i]
            e = e / np.linalg.norm(e)
            proj = [float(p @ e) for p in pts]
            lo, hi = min(proj), max(proj)
            ids = []
            for p, x in zip(pts, proj):
                s = s0 + (x - lo) / max(1e-9, hi - lo) * (s1 - s0 - 1)
                ids.append(parts.vert(p, (s, t0)))
                parts.verts[-1] = (parts.verts[-1][0], (int(round(s)), t0), key)
            for k in range(1, len(ids) - 1):
                a, b, c = ids[0], ids[k], ids[k + 1]
                pa, pb, pc = (parts.verts[x][0] for x in (a, b, c))
                if np.linalg.norm(np.cross(pb - pa, pc - pa)) < 1e-9:
                    continue
                parts.tri(a, b, c, outward)
        normals = {}  # flat: each face's own
        for a, b, c in parts.tris:
            nrm = tri_normal(parts.verts[a][0], parts.verts[b][0], parts.verts[c][0])
            for x in (a, b, c):
                normals.setdefault(x, nrm)
        for i, (p, st, key) in enumerate(parts.verts):
            car = self.carriers[key]
            n0 = normals.get(i, np.array([0.0, 0.0, 1.0]))
            self.m.add_vertex(st[0], st[1], [car.place(p, k) for k in range(poses)], [car.turn(n0, k) for k in range(poses)])
        for a, b, c in parts.tris:
            self.m.tris.append((1, base + a, base + b, base + c))
        # Only the rows used stay.
        used = self.atlas.used_rows()
        extra = (self.m.sh - self.m.old_sh) - used
        if extra > 0:
            for sk in self.m.skins:
                for i, im in enumerate(sk[2]):
                    sk[2][i] = im[: self.m.sw * (self.m.sh - extra)]
            self.m.sh -= extra
        self.m.write(path)
        return len(parts.tris), len(parts.verts), used
