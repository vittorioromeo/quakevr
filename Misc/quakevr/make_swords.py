#!/usr/bin/env python3
# make_swords.py -- the knights' swords as weapons: progs/v_ksword.mdl (the knight's) and
# progs/v_hksword.mdl (the hell knight's), cut out of Quake VR's knight models (quakevr/progs), or
# with --id, of id's (from the Quake folder's paks).
#
# Usage: python Misc/quakevr/make_swords.py [--id <Quake folder>] [output progs folder]
#
# The sword is found as the engine finds it to hide it in the knights' death frames
# (Quake/vr/vr_monstermods.cpp): in Quake VR's models by the known vertices (blade, guard and grip;
# the hell knight's blade), in id's the knight's by the blade's strips on the skin and the hell
# knight's as its longest separate piece. Its first pose is turned so that the blade points along
# +z, pommel at the origin, like the axe (v_axe.mdl), and scaled so that the weapon settings' 0.34
# gives about the knights' own sword length. The skin is the knight's. Nine identical frames (a
# weapon's frame numbers). Vertex 0 is the pommel (the hand's anchor), vertex 1 the tip (the
# "muzzle", the reach of a swing).

import math
import os
import re
import struct
import sys

HEADER = struct.Struct('<4si3f3ff3f8if')
SCALE = 2.6  # knight units to weapon model units (the weapon draws at 0.34): about 1 m


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

    def uv(self, front, v):
        onseam, s, t = self.stverts[v]
        return (s + self.sw // 2 if onseam and not front else s), t


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
        if all((s < 22 or half - 2 <= s < half + 22) and t < 120 for s, t in (m.uv(front, v) for v in (a, b, c))):
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


def uv_triangle(stverts, t, sw):
    front = t[0]
    out = []
    for v in t[1:]:
        onseam, s, tt = stverts[v]
        out.append((s + sw // 2 if onseam and not front else s, tt))
    return out


def paint_steel(skin, sw, sh, stverts, blade):
    """The blade's texels as steel: bright edges, a darker flat and a light ridge down the middle,
    across each of its strips on the skin (their narrower side)."""
    tris = [uv_triangle(stverts, t, sw) for t in blade]
    # The strips: the triangles' UV boxes, merged while they overlap.
    boxes = []
    for uv in tris:
        box = [min(p[0] for p in uv), min(p[1] for p in uv), max(p[0] for p in uv), max(p[1] for p in uv)]
        boxes.append(box)
    strips = []
    for box in boxes:
        box = list(box)
        merged = True
        while merged:
            merged = False
            for other in strips:
                if box[0] <= other[2] + 1 and other[0] <= box[2] + 1 and box[1] <= other[3] + 1 and other[1] <= box[3] + 1:
                    strips.remove(other)
                    box = [min(box[0], other[0]), min(box[1], other[1]), max(box[2], other[2]), max(box[3], other[3])]
                    merged = True
                    break
        strips.append(box)

    # Quake's greys are palette indices 0 (black) to 15 (white).
    def shade(across):
        edge = abs(across - 0.5) * 2  # 0 at the ridge, 1 at the edges
        if edge > 0.8:
            return 13
        if edge < 0.15:
            return 11
        return 9 if edge < 0.5 else 10

    for uv in tris:
        box = next(b for b in strips if b[0] <= uv[0][0] <= b[2] and b[1] <= uv[0][1] <= b[3])
        wide = (box[2] - box[0]) >= (box[3] - box[1])
        (x0, y0), (x1, y1), (x2, y2) = uv
        d = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        if abs(d) < 1e-9:
            continue
        for y in range(max(0, min(y0, y1, y2) - 1), min(sh, max(y0, y1, y2) + 2)):
            for x in range(max(0, min(x0, x1, x2) - 1), min(sw, max(x0, x1, x2) + 2)):
                # The texel's centre, with a little slack so that the strips' borders are covered.
                px, py = x + 0.5, y + 0.5
                l1 = ((px - x0) * (y2 - y0) - (x2 - x0) * (py - y0)) / d
                l2 = ((x1 - x0) * (py - y0) - (px - x0) * (y1 - y0)) / d
                if l1 < -0.1 or l2 < -0.1 or l1 + l2 > 1.1:
                    continue
                if wide:
                    across = (py - box[1]) / max(1, box[3] - box[1])
                else:
                    across = (px - box[0]) / max(1, box[2] - box[0])
                skin[y * sw + x] = shade(min(1.0, max(0.0, across)))


def build(m, tris, normals):
    verts = sorted({v for i in tris for v in m.tris[i][1:]})
    pts = {v: m.pos(v) for v in verts}

    # The blade's axis: the vertices' principal axis (power iteration on their covariance); the
    # tip is its end farther from the model's middle (the hilt is in the hand, by the body), the
    # pommel the other end.
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
    pommel, tip = lo_end, hi_end
    z = axis
    # Across the blade: the direction of widest spread perpendicular to it.
    best, x = 0.0, (1.0, 0.0, 0.0)
    for v in verts:
        d = sub(pts[v], centre)
        d = sub(d, mul(z, dot(d, z)))
        if dot(d, d) > best:
            best, x = dot(d, d), norm(d)
    y = cross(z, x)

    # The pommel end on the axis at the origin.
    base = add(centre, mul(z, dot(sub(pts[pommel], centre), z)))

    def local(p):
        d = sub(p, base)
        return (dot(d, x) * SCALE, dot(d, y) * SCALE, dot(d, z) * SCALE)

    order = [pommel, tip] + [v for v in verts if v not in (pommel, tip)]
    index = {v: i for i, v in enumerate(order)}
    lp = [local(pts[v]) for v in order]

    # Normals: the sum of the adjoining faces', as the nearest of Quake's 162.
    acc = [(0.0, 0.0, 0.0)] * len(order)
    for i in tris:
        _, a, b, c = m.tris[i]
        pa, pb, pc = lp[index[a]], lp[index[b]], lp[index[c]]
        n = cross(sub(pb, pa), sub(pc, pa))
        for v in (a, b, c):
            acc[index[v]] = add(acc[index[v]], n)
    nidx = []
    for n in acc:
        if dot(n, n) < 1e-9:
            nidx.append(0)
            continue
        n = norm(n)
        nidx.append(max(range(len(normals)), key=lambda k: dot(normals[k], n)))

    # The skin: the model's own, whole (the sword's texels are spread over it), with the blade
    # repainted as clean steel (the knights' blades are smudged with blood, which up close in the
    # hand reads as noise).
    sw, sh = m.sw, m.sh
    skin = bytearray(m.skin)
    stverts = [m.stverts[v] for v in order]
    newtris = [(front, index[a], index[b], index[c]) for front, a, b, c in (m.tris[i] for i in tris)]
    top = max(p[2] for p in lp)
    blade = [t for t in newtris if any(lp[v][2] > top * 0.3 for v in t[1:])]  # the guard is at the bottom
    paint_steel(skin, sw, sh, stverts, blade)

    lo = [min(p[i] for p in lp) for i in range(3)]
    hi = [max(p[i] for p in lp) for i in range(3)]
    scale = [max((hi[i] - lo[i]) / 255.0, 1e-4) for i in range(3)]
    radius = max(math.sqrt(dot(p, p)) for p in lp)
    frame = b''
    for i, p in enumerate(lp):
        frame += bytes([round((p[k] - lo[k]) / scale[k]) for k in range(3)] + [nidx[i]])

    out = bytearray(HEADER.pack(b'IDPO', 6, *scale, *lo, radius, 0.0, 0.0, 0.0,
                                1, sw, sh, len(order), len(newtris), 9, 0, 0, 1.0))
    out += struct.pack('<i', 0) + bytes(skin)
    for st in stverts:
        out += struct.pack('<3i', *st)
    for t in newtris:
        out += struct.pack('<4i', *t)
    bmin = bytes([0, 0, 0, 0])
    bmax = bytes([255, 255, 255, 0])
    for f in range(9):
        out += struct.pack('<i', 0) + bmin + bmax + ('frame%d' % (f + 1)).encode().ljust(16, b'\0') + frame
    return bytes(out), len(order), len(newtris), hi[2]


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
    if args and args[0] == '--id':
        files = pak_files(args[1])
        args = args[2:]
        sources = (('progs/knight.mdl', knight_sword, 'v_ksword.mdl'), ('progs/hknight.mdl', hknight_sword, 'v_hksword.mdl'))
        load = lambda name: files[name]
    else:
        progs = os.path.join(here, '..', '..', 'quakevr', 'progs')
        sources = (('knight.mdl', by_verts(QVR_KNIGHT), 'v_ksword.mdl'), ('hknight.mdl', by_verts(QVR_HKNIGHT), 'v_hksword.mdl'))
        load = lambda name: open(os.path.join(progs, name), 'rb').read()
    out_dir = args[0] if args else os.path.join(here, '..', '..', 'quakevr', 'progs')
    for src, find, dst in sources:
        m = Mdl(load(src))
        data, nv, nt, length = build(m, find(m), normals)
        open(os.path.join(out_dir, dst), 'wb').write(data)
        print('%s: %d vertices, %d triangles, %.1f units long' % (dst, nv, nt, length))


if __name__ == '__main__':
    main()
