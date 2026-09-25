#!/usr/bin/env python3
# make_swords.py -- the knights' swords as weapons: progs/v_ksword.mdl (the knight's) and
# progs/v_hksword.mdl (the hell knight's), cut out of Quake VR's knight models (quakevr/progs), or
# with --id, of id's (from the Quake folder's paks).
#
# Usage: python Misc/quakevr/make_swords.py [--id <Quake folder>] [output progs folder]
#
# The sword is found as the engine finds it to hide it in the knights' death frames
# (Quake/vr/vr_monstermods.cpp): in Quake VR's models by the known vertices (the knight's blade and
# guard; the hell knight's blade), in id's the knight's by the blade's strips on the skin and the
# hell knight's as its longest separate piece. The knights' hands cover their grips, so the models
# have none: a leather-wrapped grip and a pommel are added (and the hell knight's sword, a bare blade,
# gets a crossguard). The skin is the knight's own, so the sword looks as it does in its hand.
#
# The sword is laid where the axe's handle is in progs/v_axe.mdl: its grip along the handle's axis,
# over the stretch the hand holds, the guard where the handle meets the head, the blade going on
# along the axis with its edges the way the axe's head points. With the axe's weapon settings the
# hand then holds the grip as it holds the axe, and the sword turns about it. Anchor indices (the
# hand's, the tip's for a swing's reach) follow the old engine's strip order, not the file's: the
# engine's vr_anchor_nearest finds them (see vr_weapons.inc, slots 19 and 20). Scaled so that the
# weapon settings' 0.34 gives about the knights' own sword length. Nine identical frames (a weapon's
# frame numbers).

import math
import os
import re
import struct
import sys

HEADER = struct.Struct('<4si3f3ff3f8if')
SCALE = 2.6  # knight units to weapon model units (the weapon draws at 0.34): about 1 m

# The axe's handle (v_axe.mdl, frame 0): the centre of its bottom end and its direction, up to where
# the head starts (12.3 units along it).
HANDLE_BOTTOM = (0.2, 0.0, -3.55)
HANDLE_DIR = (4.2, 0.0, 11.55)
GRIP = 12.3          # grip length: the handle up to the head
GRIP_RADIUS = 1.5    # a little thicker than the axe's handle, a hand's grip
POMMEL = 2.2         # pommel length below the grip
SIDES = 4            # the grip and pommel: square in section, as low-poly as the knights


def pak_files(quake):
    out = {}
    for name in ('PAK0.PAK', 'PAK1.PAK', 'pak0.pak', 'pak1.pak'):
        p = os.path.join(quake, 'id1', name)
        if not os.path.isfile(p):
            continue
        d = open(p, 'rb').read()
        _, off, ln = struct.unpack('<4sii', d[:12])
        for i in range(ln // 64):
            n, fo, fl = struct.unpack('<56sii', d[off + i * 64:off + i * 64 + 64])
            out[n.split(b'\0')[0].decode('latin-1').lower()] = d[fo:fo + fl]
    return out


def anorms():
    here = os.path.dirname(os.path.abspath(__file__))
    text = open(os.path.join(here, '..', '..', 'Quake', 'anorms.h')).read()
    return [tuple(float(x) for x in m) for m in re.findall(r'\{\s*(-?[\d.]+),\s*(-?[\d.]+),\s*(-?[\d.]+)\s*\}', text)]


def palette(quake_files):
    return quake_files.get('gfx/palette.lmp') if quake_files else None


class Mdl:
    def __init__(self, data):
        h = HEADER.unpack_from(data, 0)
        self.scale, self.origin = h[2:5], h[5:8]
        self.nskins, self.sw, self.sh, self.nverts, self.ntris, self.nframes = h[12:18]
        off = HEADER.size
        g, = struct.unpack_from('<i', data, off)
        assert g == 0
        self.skin = data[off + 4:off + 4 + self.sw * self.sh]
        for _ in range(self.nskins):
            g, = struct.unpack_from('<i', data, off)
            off += 4
            if g == 0:
                off += self.sw * self.sh
            else:
                n, = struct.unpack_from('<i', data, off)
                off += 4 + 4 * n + n * self.sw * self.sh
        self.stverts = [struct.unpack_from('<3i', data, off + 12 * i) for i in range(self.nverts)]
        off += 12 * self.nverts
        self.tris = [struct.unpack_from('<4i', data, off + 16 * i) for i in range(self.ntris)]
        off += 16 * self.ntris
        t, = struct.unpack_from('<i', data, off)
        assert t == 0  # stand1 is a single frame
        off += 4 + 8 + 16
        self.pose = [(data[off + 4 * i], data[off + 4 * i + 1], data[off + 4 * i + 2]) for i in range(self.nverts)]

    def pos(self, v):
        return tuple(self.pose[v][i] * self.scale[i] + self.origin[i] for i in range(3))


def sub(a, b): return tuple(a[i] - b[i] for i in range(3))
def add(a, b): return tuple(a[i] + b[i] for i in range(3))
def mul(a, k): return tuple(x * k for x in a)
def dot(a, b): return sum(a[i] * b[i] for i in range(3))
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def norm(a): return mul(a, 1.0 / math.sqrt(dot(a, a)))


def knight_sword(m):
    """Triangles on the blade's strips of the skin (left of each half)."""
    half = m.sw // 2
    out = []
    for i, (front, a, b, c) in enumerate(m.tris):
        uvs = []
        for v in (a, b, c):
            onseam, s, t = m.stverts[v]
            uvs.append((s + half if onseam and not front else s, t))
        if all((s < 22 or half - 2 <= s < half + 22) and t < 120 for s, t in uvs):
            out.append(i)
    return out


def hknight_sword(m):
    """The longest separate piece of the mesh, apart from the body (the most vertices)."""
    parent = list(range(m.nverts))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a
    for _, a, b, c in m.tris:
        for x, y in ((a, b), (b, c)):
            parent[find(x)] = find(y)
    comps = {}
    for v in range(m.nverts):
        comps.setdefault(find(v), set()).add(v)
    body = max(comps.values(), key=len)

    def length(c):
        pts = [m.pos(v) for v in c]
        return max(math.dist(p, q) for p in pts for q in pts)
    sword = max((c for c in comps.values() if c is not body), key=length)
    return [i for i, t in enumerate(m.tris) if t[1] in sword]


def darkest_texel_block(m, want=(45, 34, 24), pal=None, size=3):
    """(s, t) of a size x size block of the skin closest to a dark leather brown (`want`), for the
    grip. Without the palette, Quake's leather entries (170-175) are looked for."""
    best, where = 1e18, (0, 0)
    step = 2
    for t in range(0, m.sh - size, step):
        for s in range(0, m.sw - size, step):
            cost = 0
            for dt in range(size):
                for ds in range(size):
                    c = m.skin[(t + dt) * m.sw + s + ds]
                    if pal:
                        r, g, b = pal[c * 3:c * 3 + 3]
                        cost += (r - want[0]) ** 2 + (g - want[1]) ** 2 + (b - want[2]) ** 2
                    else:
                        cost += 0 if 170 <= c <= 175 else 10000
            if cost < best:
                best, where = cost, (s + size // 2, t + size // 2)
    return where


class Out:
    """The sword being built: vertices in the sword's own frame (x across the edges, y through the
    flat, z along the blade from the guard's foot), their skin coordinates, normals, triangles."""

    def __init__(self):
        self.p, self.st, self.n, self.tris = [], [], [], []

    def vert(self, p, st, n):
        self.p.append(p)
        self.st.append(st)
        self.n.append(n)
        return len(self.p) - 1

    def tri(self, a, b, c, outward):
        """Clockwise seen from outside (Quake's front faces)."""
        p0, p1, p2 = self.p[a], self.p[b], self.p[c]
        if dot(cross(sub(p1, p0), sub(p2, p0)), outward) > 0:
            b, c = c, b
        self.tris.append((1, a, b, c))

    def ring_loft(self, rings, st, sides=SIDES):
        """Rings (z, radius x, radius y) along z, closed at both ends, all at the texel `st`; the
        corners at 45 degrees, so the flat faces look along the edges and the flat of the blade."""
        first = len(self.p)
        for z, rx, ry in rings:
            for k in range(sides):
                a = 2 * math.pi * (k + 0.5) / sides
                d = (math.cos(a), math.sin(a), 0.0)
                self.vert((rx * d[0], ry * d[1], z), st, d)
        for r in range(len(rings) - 1):
            for k in range(sides):
                a = first + r * sides + k
                b = first + r * sides + (k + 1) % sides
                c = first + (r + 1) * sides + k
                d = first + (r + 1) * sides + (k + 1) % sides
                ang = 2 * math.pi * (k + 1) / sides
                out = (math.cos(ang), math.sin(ang), 0.0)
                self.tri(a, c, b, out)
                self.tri(b, c, d, out)
        for end, sign in ((0, -1.0), (len(rings) - 1, 1.0)):
            z = rings[end][0]
            c = self.vert((0.0, 0.0, z), st, (0.0, 0.0, sign))
            base = first + end * sides
            for k in range(sides):
                self.tri(c, base + k, base + (k + 1) % sides, (0.0, 0.0, sign))

    def box(self, centre, half, st):
        cx, cy, cz = centre
        hx, hy, hz = half
        faces = [((1, 0, 0), [(1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)]),
                 ((-1, 0, 0), [(-1, -1, -1), (-1, -1, 1), (-1, 1, 1), (-1, 1, -1)]),
                 ((0, 1, 0), [(-1, 1, -1), (-1, 1, 1), (1, 1, 1), (1, 1, -1)]),
                 ((0, -1, 0), [(-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)]),
                 ((0, 0, 1), [(-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]),
                 ((0, 0, -1), [(-1, -1, -1), (-1, 1, -1), (1, 1, -1), (1, -1, -1)])]
        for n, corners in faces:
            idx = [self.vert((cx + sx * hx, cy + sy * hy, cz + sz * hz), st, n) for sx, sy, sz in corners]
            self.tri(idx[0], idx[1], idx[2], n)
            self.tri(idx[0], idx[2], idx[3], n)


def build(m, tris, normals, pal, add_guard):
    verts = sorted({v for i in tris for v in m.tris[i][1:]})
    pts = {v: m.pos(v) for v in verts}

    # The blade's axis: the vertices' principal axis (power iteration on their covariance); the
    # tip is its end farther from the model's middle (the hilt is in the hand, by the body).
    centre = mul(tuple(sum(pts[v][i] for v in verts) for i in range(3)), 1.0 / len(verts))
    cov = [[sum((pts[v][i] - centre[i]) * (pts[v][j] - centre[j]) for v in verts) for j in range(3)] for i in range(3)]
    axis = (1.0, 1.0, 1.0)
    for _ in range(64):
        axis = norm(tuple(sum(cov[i][j] * axis[j] for j in range(3)) for i in range(3)))
    ends = sorted(verts, key=lambda v: dot(sub(pts[v], centre), axis))
    lo_end, hi_end = ends[0], ends[-1]
    body = (0.0, 0.0, 10.0)
    if math.dist(pts[lo_end], body) > math.dist(pts[hi_end], body):
        axis = mul(axis, -1.0)
        lo_end, hi_end = hi_end, lo_end
    z = axis
    # Across the blade: the direction of widest spread perpendicular to it (the guard's, the edges').
    best, x = 0.0, (1.0, 0.0, 0.0)
    for v in verts:
        d = sub(pts[v], centre)
        d = sub(d, mul(z, dot(d, z)))
        if dot(d, d) > best:
            best, x = dot(d, d), norm(d)
    y = cross(z, x)
    base = add(centre, mul(z, dot(sub(pts[lo_end], centre), z)))  # the hilt's end, on the axis
    # The grip goes on under the middle of the blade (the principal axis runs off it where the
    # hilt is lopsided, the knight's guard to one side): the middle of the bounds, across and
    # through, of the blade's vertices above its lowest fifth (all, if none are).
    span = dot(sub(pts[hi_end], base), z)
    low = [sub(pts[v], base) for v in verts if dot(sub(pts[v], base), z) > span * 0.2]
    low = low or [sub(pts[v], base) for v in verts]
    mid_x = (min(dot(d, x) for d in low) + max(dot(d, x) for d in low)) * 0.5
    mid_y = (min(dot(d, y) for d in low) + max(dot(d, y) for d in low)) * 0.5
    base = add(base, add(mul(x, mid_x), mul(y, mid_y)))

    out = Out()
    index = {}
    for v in verts:
        d = sub(pts[v], base)
        index[v] = out.vert((dot(d, x) * SCALE, dot(d, y) * SCALE, dot(d, z) * SCALE), None, None)
    for front, a, b, c in (m.tris[i] for i in tris):
        out.tris.append((front, index[a], index[b], index[c]))
    # The blade's own skin coordinates and normals (the face normals' sum).
    for v in verts:
        onseam, s, t = m.stverts[v]
        out.st[index[v]] = (onseam, s, t)
    acc = {i: (0.0, 0.0, 0.0) for i in index.values()}
    for _, a, b, c in out.tris:
        n = cross(sub(out.p[b], out.p[a]), sub(out.p[c], out.p[a]))
        for i in (a, b, c):
            acc[i] = add(acc[i], mul(n, -1.0))  # clockwise front faces
    for i, n in acc.items():
        out.n[i] = norm(n) if dot(n, n) > 1e-12 else (0.0, 0.0, 1.0)

    # The hilt's metal: the skin where the lowest part of the blade (the guard) is.
    lowest = min(index.values(), key=lambda i: out.p[i][2])
    metal = out.st[lowest][1:]
    leather = darkest_texel_block(m, pal=pal)
    flat = lambda st: (0, st[0], st[1])

    # Grip and pommel below the guard's foot (z = 0), and for a bare blade, a crossguard.
    blade_width = max(abs(out.p[i][0]) for i in index.values())
    out.ring_loft([(-GRIP, GRIP_RADIUS, GRIP_RADIUS), (0.0, GRIP_RADIUS * 0.9, GRIP_RADIUS * 0.9)], flat(leather))
    out.ring_loft([(-GRIP - POMMEL, 1.4, 1.4), (-GRIP - POMMEL * 0.5, 2.5, 2.5), (-GRIP, 1.7, 1.7)], flat(metal))
    if add_guard:
        out.box((0.0, 0.0, -0.9), (max(5.0, blade_width * 2.6), 1.1, 0.9), flat(metal))
    for i in range(len(out.st)):
        if len(out.st[i]) == 2:
            out.st[i] = flat(out.st[i])

    # Into the axe's space: the grip along its handle, the guard's foot where the head starts.
    A = norm(HANDLE_DIR)
    X = norm(sub((1.0, 0.0, 0.0), mul(A, A[0])))  # the edges face the way the axe's head does
    Y = cross(A, X)
    guard_foot = add(HANDLE_BOTTOM, mul(A, GRIP))

    def place(p):
        return add(guard_foot, add(mul(X, p[0]), add(mul(Y, p[1]), mul(A, p[2]))))

    def turn(n):
        return add(mul(X, n[0]), add(mul(Y, n[1]), mul(A, n[2])))

    positions = [place(p) for p in out.p]
    norms_ = [turn(n) for n in out.n]
    nidx = [max(range(len(normals)), key=lambda k: dot(normals[k], n)) for n in norms_]

    lo = [min(p[i] for p in positions) for i in range(3)]
    hi = [max(p[i] for p in positions) for i in range(3)]
    scale = [max((hi[i] - lo[i]) / 255.0, 1e-4) for i in range(3)]
    radius = max(math.sqrt(dot(p, p)) for p in positions)
    frame = b''
    for i, p in enumerate(positions):
        frame += bytes([max(0, min(255, round((p[k] - lo[k]) / scale[k]))) for k in range(3)] + [nidx[i]])

    data = bytearray(HEADER.pack(b'IDPO', 6, *scale, *lo, radius, 0.0, 0.0, 0.0,
                                 1, m.sw, m.sh, len(positions), len(out.tris), 9, 0, 0, 1.0))
    data += struct.pack('<i', 0) + bytes(m.skin)
    for st in out.st:
        data += struct.pack('<3i', *st)
    for t in out.tris:
        data += struct.pack('<4i', *t)
    bmin, bmax = bytes([0, 0, 0, 0]), bytes([255, 255, 255, 0])
    for f in range(9):
        data += struct.pack('<i', 0) + bmin + bmax + ('frame%d' % (f + 1)).encode().ljust(16, b'\0') + frame
    tip = max(range(len(positions)), key=lambda i: dot(sub(positions[i], HANDLE_BOTTOM), A))
    return bytes(data), len(positions), len(out.tris), positions[tip]


# Quake VR's models: the swords' vertices (the same lists as vr_monstermods.cpp).
QVR_KNIGHT = {334, 335, 336, 363, 364, 365, 524, 535, 536, 537, 538, 539, 540, 557, 558, 559, 560, 561, 562, 563, 564,
              565, 566, 582, 583, 584, 585, 586, 587, 588, 589, 590, 591, 592, 593, 594, 595, 596, 597, 598, 599, 600,
              601, 602, 603, 604, 607, 608, 609, 612, 613, 614, 615, 617, 626, 627, 628, 639, 640, 645, 646}
QVR_HKNIGHT = {43, 45, 46, 47, 48, 526, 527, 531, 532}


def by_verts(verts):
    return lambda m: [i for i, t in enumerate(m.tris) if all(v in verts for v in t[1:])]


def main():
    args = sys.argv[1:]
    here = os.path.dirname(os.path.abspath(__file__))
    normals = anorms()
    files = None
    if args and args[0] == '--id':
        files = pak_files(args[1])
        args = args[2:]
        sources = (('progs/knight.mdl', knight_sword, 'v_ksword.mdl', False),
                   ('progs/hknight.mdl', hknight_sword, 'v_hksword.mdl', True))
        load = lambda name: files[name]
    else:
        progs = os.path.join(here, '..', '..', 'quakevr', 'progs')
        sources = (('knight.mdl', by_verts(QVR_KNIGHT), 'v_ksword.mdl', False),
                   ('hknight.mdl', by_verts(QVR_HKNIGHT), 'v_hksword.mdl', True))
        load = lambda name: open(os.path.join(progs, name), 'rb').read()
    out_dir = args[0] if args else os.path.join(here, '..', '..', 'quakevr', 'progs')
    for src, find, dst, add_guard in sources:
        m = Mdl(load(src))
        data, nv, nt, tip = build(m, find(m), normals, palette(files), add_guard)
        open(os.path.join(out_dir, dst), 'wb').write(data)
        print('%s: %d vertices, %d triangles, tip at %.2f %.2f %.2f' % (dst, nv, nt, *tip))


if __name__ == '__main__':
    main()
