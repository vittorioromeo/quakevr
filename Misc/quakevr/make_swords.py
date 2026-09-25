#!/usr/bin/env python3
# make_swords.py -- the knights' swords as weapons: progs/v_ksword.mdl (the knight's) and
# progs/v_hksword.mdl (the hell knight's), cut out of Quake VR's knight models (quakevr/progs), or
# with --id, of id's (from the Quake folder's paks).
#
# Usage: python Misc/quakevr/make_swords.py [--id <Quake folder>] [output progs folder]
#
# The sword is found as the engine finds it to hide it in the knights' death frames
# (Quake/vr/vr_monstermods.cpp): in Quake VR's models by the known vertices, in id's the knight's by
# the blade's strips on the skin and the hell knight's as its longest separate piece. Only the blade
# is kept (its longest separate piece): the knights' hands cover their hilts, so the models have
# none, or only the parts the hand leaves out (the knight's guard, lopsided and open where the hand
# was). Both swords get the same new hilt, centred on the blade's axis and low-poly as the knights
# are: a crossguard with a block round the blade and arms flaring at their ends, a square grip
# (a little thicker than the axe's handle), a pommel. Each face is its own piece of the knight's own
# skin, at about the blade's texel size: in Quake VR's skins the knight's guard metal and a woven
# strip for the grip, the hell knight's the hilt id painted beside its blade (a guard and a grip);
# in id's, the blade's lowest texels and the darkest leather-brown block.
#
# The sword is laid where the axe's handle is in progs/v_axe.mdl: its grip along the handle's axis,
# from its bottom over the stretch the hand holds, the crossguard a little above the hand, the blade
# going on along the axis with its edges the way the axe's head points. With the axe's weapon settings the
# hand then holds the grip as it holds the axe, and the sword turns about it. Anchor indices (the
# hand's, the tip's for a swing's reach) follow the old engine's strip order, not the file's: the
# engine's vr_anchor_nearest finds them (see vr_weapons.inc, slots 18 and 19). Scaled so that the
# weapon settings' 0.34 gives about the knights' own sword length. Nine identical frames (a weapon's
# frame numbers).

import math
import os
import struct
import sys

import quakepak
from mdlgen import HEADER, add, anorms, cross, dot, mul, norm, read_skins, sub

SCALE = 2.6  # knight units to weapon model units (the weapon draws at 0.34): about 1 m

# The axe's handle (v_axe.mdl, frame 0): the centre of its bottom end and its direction (to its band
# 12.3 units along it; the head starts at 25).
HANDLE_BOTTOM = (0.2, 0.0, -3.55)
HANDLE_DIR = (4.2, 0.0, 11.55)
GRIP = 15.3          # grip length from the handle's bottom: the axe's hand closes over its first 12
                     # or so, and the rest keeps the fist off the crossguard
GRIP_HALF = 1.5      # the square grip's half width (the axe's handle is about 1.2 in radius)
GUARD_H = 4.0        # the crossguard's height along the blade; the blade goes 1.2 into it
DENSITY = 1.25       # texels per unit on the hilt's faces, about the blades'


def pak_files(quake):
    out = {}
    for name in ('PAK0.PAK', 'PAK1.PAK', 'pak0.pak', 'pak1.pak'):
        p = os.path.join(quake, 'id1', name)
        if os.path.isfile(p):
            out.update(quakepak.read_pak(p))
    return out


class Mdl:
    def __init__(self, data):
        h = HEADER.unpack_from(data, 0)
        self.scale, self.origin = h[2:5], h[5:8]
        self.nskins, self.sw, self.sh, self.nverts, self.ntris, self.nframes = h[12:18]
        skins, off = read_skins(data, HEADER.size, self.nskins, self.sw, self.sh)
        g, = struct.unpack_from('<i', skins[0], 0)
        assert g == 0
        self.skin = skins[0][4:]
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


def pieces(m, tris):
    """The triangles `tris` split into separate pieces (sharing no vertex position: the skin's
    seams split vertices)."""
    parent = {}

    def find(v):
        a = m.pose[v]
        parent.setdefault(a, a)
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a
    for i in tris:
        _, a, b, c = m.tris[i]
        parent[find(a)] = find(b)
        parent[find(b)] = find(c)
    out = {}
    for i in tris:
        out.setdefault(find(m.tris[i][1]), []).append(i)
    return list(out.values())


def extent(m, tris):
    pts = {m.pos(v) for i in tris for v in m.tris[i][1:]}
    return max(math.dist(p, q) for p in pts for q in pts)


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
    """The longest separate piece of the mesh, apart from the body (the most triangles)."""
    parts = pieces(m, range(m.ntris))
    body = max(parts, key=len)
    return max((p for p in parts if p is not body), key=lambda p: extent(m, p))


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
    flat, z along the blade from the crossguard's foot), their skin coordinates, normals, triangles."""

    def __init__(self):
        self.p, self.st, self.n, self.tris = [], [], [], []
        self.faces = 0

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

    def face(self, pts, region):
        """A flat face (3 or 4 points, counter-clockwise seen from outside) with its own vertices,
        showing a piece of the skin's `region` (s0, t0, s1, t1) at DENSITY texels per unit (squeezed
        where the region is smaller), the face's longer side along the region's; each face a piece
        a little further along, so that neighbours don't repeat."""
        n = cross(sub(pts[1], pts[0]), sub(pts[-1], pts[0]))
        if dot(n, n) < 1e-8:
            return
        n = norm(n)
        e1 = norm(sub(pts[1], pts[0]))
        e2 = cross(n, e1)
        uv = [(dot(sub(p, pts[0]), e1), dot(sub(p, pts[0]), e2)) for p in pts]
        s0, t0, s1, t1 = region
        w, h = s1 - s0, t1 - t0
        du = max(u for u, _ in uv) - min(u for u, _ in uv)
        dv = max(v for _, v in uv) - min(v for _, v in uv)
        if (du >= dv) != (w >= h):
            uv = [(v, u) for u, v in uv]
            du, dv = dv, du
        su = min(DENSITY, w / du) if du > 1e-6 else 0.0
        sv = min(DENSITY, h / dv) if dv > 1e-6 else 0.0
        k = self.faces
        self.faces += 1
        os_ = (w - du * su) * ((k * 0.37) % 1.0)
        ot = (h - dv * sv) * ((k * 0.61) % 1.0)
        umin = min(u for u, _ in uv)
        vmin = min(v for _, v in uv)
        idx = []
        for p, (u, v) in zip(pts, uv):
            s = min(s1 - 1, max(s0, int(round(s0 + os_ + (u - umin) * su))))
            t = min(t1 - 1, max(t0, int(round(t0 + ot + (v - vmin) * sv))))
            idx.append(self.vert(p, (0, s, t), n))
        self.tri(idx[0], idx[1], idx[2], n)
        if len(idx) == 4:
            self.tri(idx[0], idx[2], idx[3], n)

    def loft(self, rings, along, region, caps=True):
        """Rectangular rings (w, u0, u1, v0, v1) along the z axis (`along` 'z': u x, v y) or the x
        axis ('x': u y, v z), faces between them (a step where two follow at the same w) and, with
        `caps`, over both ends."""
        if along == 'z':
            at = lambda u, v, w: (u, v, w)
        else:
            at = lambda u, v, w: (w, u, v)
        # Counter-clockwise about the axis (u, v, w right-handed).
        R = [[at(u1, v0, w), at(u1, v1, w), at(u0, v1, w), at(u0, v0, w)] for w, u0, u1, v0, v1 in rings]
        for r in range(len(R) - 1):
            for k in range(4):
                a, b = R[r][k], R[r][(k + 1) % 4]
                c, d = R[r + 1][k], R[r + 1][(k + 1) % 4]
                self.face([a, b, d, c], region)
        if caps:
            self.face(R[0][::-1], region)
            self.face(R[-1], region)


def blade_frame(pts):
    """The blade's frame: its axis (from the middle of its foot towards the middle of its upper
    part, off the tip; the principal axis orders them and says which end is the tip, the one farther
    from the knight's middle) and across it the widest spread (the edges); the origin the middle of
    the foot. Points are distinct positions."""
    centre = mul(tuple(sum(p[i] for p in pts) for i in range(3)), 1.0 / len(pts))
    cov = [[sum((p[i] - centre[i]) * (p[j] - centre[j]) for p in pts) for j in range(3)] for i in range(3)]
    axis = (1.0, 1.0, 1.0)
    for _ in range(64):
        axis = norm(tuple(sum(cov[i][j] * axis[j] for j in range(3)) for i in range(3)))
    proj = lambda p: dot(sub(p, centre), axis)
    lo_end, hi_end = min(pts, key=proj), max(pts, key=proj)
    body = (0.0, 0.0, 10.0)
    if math.dist(lo_end, body) > math.dist(hi_end, body):
        axis = mul(axis, -1.0)
        lo_end, hi_end = hi_end, lo_end
    z0, span = proj(lo_end), proj(hi_end) - proj(lo_end)
    mean = lambda q: mul(tuple(sum(p[i] for p in q) for i in range(3)), 1.0 / len(q))
    foot = [p for p in pts if proj(p) - z0 < span * 0.1]
    upper = [p for p in pts if span * 0.5 < proj(p) - z0 < span * 0.97]
    origin = mean(foot)
    z = norm(sub(mean(upper), origin)) if upper else axis
    best, x = 0.0, (1.0, 0.0, 0.0)
    for p in pts:
        d = sub(p, origin)
        d = sub(d, mul(z, dot(d, z)))
        if dot(d, d) > best:
            best, x = dot(d, d), norm(d)
    x = norm(sub(x, mul(z, dot(x, z))))
    origin = add(origin, mul(z, min(dot(sub(p, origin), z) for p in pts)))  # the foot's lowest point
    return origin, x, cross(z, x), z


def build(m, tris, normals, skin_regions):
    tris = max(pieces(m, tris), key=lambda p: extent(m, p))  # the blade, without any separate guard
    verts = sorted({v for i in tris for v in m.tris[i][1:]})
    base, x, y, z = blade_frame(sorted({m.pos(v) for v in verts}))

    out = Out()
    index = {}
    lift = GUARD_H - 1.2  # the blade's foot inside the crossguard
    for v in verts:
        d = sub(m.pos(v), base)
        index[v] = out.vert((dot(d, x) * SCALE, dot(d, y) * SCALE, dot(d, z) * SCALE + lift), None, None)
        out.st[index[v]] = m.stverts[v]
    # The blade's triangles, facing away from its middle (it is convex; its foot is open, the
    # knight's hand hid it, and goes into the crossguard).
    points = {out.p[i] for i in index.values()}
    middle = mul(tuple(sum(p[k] for p in points) for k in range(3)), 1.0 / len(points))
    flipped = 0
    for front, a, b, c in (m.tris[i] for i in tris):
        a, b, c = index[a], index[b], index[c]
        mid = mul(add(out.p[a], add(out.p[b], out.p[c])), 1.0 / 3.0)
        out.tri(a, b, c, sub(mid, middle))
        flipped += out.tris[-1][1:] != (a, b, c)
        out.tris[-1] = (front,) + out.tris[-1][1:]
    if flipped:
        print('  %d of the blade\'s triangles turned to face out' % flipped)
    # The blade's normals: the face normals' sum.
    acc = {i: (0.0, 0.0, 0.0) for i in index.values()}
    for _, a, b, c in out.tris:
        n = cross(sub(out.p[b], out.p[a]), sub(out.p[c], out.p[a]))
        for i in (a, b, c):
            acc[i] = add(acc[i], mul(n, -1.0))  # clockwise front faces
    for i, n in acc.items():
        out.n[i] = norm(n) if dot(n, n) > 1e-12 else (0.0, 0.0, 1.0)

    # The hilt, sized by the blade's foot: its half width (to the edges) and half thickness.
    foot = [out.p[i] for i in index.values() if out.p[i][2] < lift + 0.25 * GUARD_H]
    bw = max(abs(p[0]) for p in foot)
    bt = max(abs(p[1]) for p in foot)
    metal, grip, pommel = skin_regions(m)
    # The crossguard: a block round the blade's foot, arms narrowing to a waist, then flaring (more
    # towards the blade) to square ends.
    bx, by = bw + 1.0, bt + 1.0
    L = max(11.0, bw * 3.0)
    wx = bx + (L - bx) * 0.45
    ay, wy, ty = bt * 0.8, bt * 0.65, bt * 0.85
    arm = [(-L, -ty, ty, 0.2, GUARD_H + 0.9), (-wx, -wy, wy, 0.9, GUARD_H - 1.4), (-bx, -ay, ay, 0.6, GUARD_H - 0.9)]
    block = [(-bx, -by, by, 0.0, GUARD_H), (bx, -by, by, 0.0, GUARD_H)]
    out.loft(arm + block + [(-w, u0, u1, v0, v1) for w, u0, u1, v0, v1 in reversed(arm)], 'x', metal)
    # The grip below it, a little fuller in the middle, and the pommel.
    g = GRIP_HALF
    out.loft([(-GRIP, -g * 0.9, g * 0.9, -g * 0.9, g * 0.9), (-GRIP * 0.5, -g, g, -g, g),
              (0.3, -g * 0.9, g * 0.9, -g * 0.9, g * 0.9)], 'z', grip)
    rings = [(-GRIP - 3.6, 1.3), (-GRIP - 2.8, 2.4), (-GRIP - 0.9, 2.4), (-GRIP + 0.3, 1.5)]
    out.loft([(w, -r, r, -r, r) for w, r in rings], 'z', pommel)

    # Into the axe's space: the grip along its handle, the crossguard's foot where the head starts.
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
# The knight's blade among them (its guard touches the blade's foot).
QVR_KNIGHT_BLADE = {557, 558, 559, 560, 561, 562, 563, 564, 565, 566, 612, 613, 614, 615}

# Their skins' hilt texels (s0, t0, s1, t1): the crossguard's, the grip's, the pommel's. The knight's:
# the metal of its own guard and the woven strip of its skin; the hell knight's: the guard and grip
# id painted beside its blade.
QVR_KNIGHT_SKIN = ((224, 190, 240, 222), (78, 28, 106, 50), (224, 190, 240, 222))
QVR_HKNIGHT_SKIN = ((23, 5, 34, 59), (3, 27, 22, 37), (23, 5, 34, 59))


def by_verts(verts):
    return lambda m: [i for i, t in enumerate(m.tris) if all(v in verts for v in t[1:])]


def id_skin(pal):
    """In id's skins: the blade's lowest texels for the metal, the darkest leather-brown block for
    the grip (3 x 3 texels each)."""
    def regions(m):
        low = min((v for t in m.tris for v in t[1:]), key=lambda v: m.pos(v)[2])
        _, s, t = m.stverts[low]
        ls, lt = darkest_texel_block(m, pal=pal)
        metal = (max(0, s - 1), max(0, t - 1), s + 2, t + 2)
        return metal, (ls - 1, lt - 1, ls + 2, lt + 2), metal
    return regions


def main():
    args = sys.argv[1:]
    here = os.path.dirname(os.path.abspath(__file__))
    normals = anorms()
    files = None
    if args and args[0] == '--id':
        files = pak_files(args[1])
        args = args[2:]
        pal = files.get('gfx/palette.lmp')
        sources = (('progs/knight.mdl', knight_sword, 'v_ksword.mdl', id_skin(pal)),
                   ('progs/hknight.mdl', hknight_sword, 'v_hksword.mdl', id_skin(pal)))
        load = lambda name: files[name]
    else:
        progs = os.path.join(here, '..', '..', 'quakevr', 'progs')
        sources = (('knight.mdl', by_verts(QVR_KNIGHT & QVR_KNIGHT_BLADE), 'v_ksword.mdl', lambda m: QVR_KNIGHT_SKIN),
                   ('hknight.mdl', by_verts(QVR_HKNIGHT), 'v_hksword.mdl', lambda m: QVR_HKNIGHT_SKIN))
        load = lambda name: open(os.path.join(progs, name), 'rb').read()
    out_dir = args[0] if args else os.path.join(here, '..', '..', 'quakevr', 'progs')
    for src, find, dst, skin_regions in sources:
        m = Mdl(load(src))
        data, nv, nt, tip = build(m, find(m), normals, skin_regions)
        open(os.path.join(out_dir, dst), 'wb').write(data)
        print('%s: %d vertices, %d triangles, tip at %.2f %.2f %.2f' % (dst, nv, nt, *tip))


if __name__ == '__main__':
    main()
