#!/usr/bin/env python3
# improve_weapons.py -- the double shotgun and the rocket launcher, reworked for being held in one
# hand (feedback round 16):
#   quakevr/progs/v_shot2.mdl   the sawn-off double shotgun: the drooping handle made for the VR
#                               port (and only in its first three frames: the later ones kept the old
#                               stock) is replaced by a receiver behind the breech, a stock cut down
#                               to a wrist and a pistol grip (walnut, checkered, a steel butt plate),
#                               a trigger and a trigger guard.
#   quakevr/progs/v_rock2.mdl   the rocket launcher: a pistol grip under the back of the tube (ribbed,
#                               a butt plate), a trigger and a trigger guard, and a back-blast nozzle
#                               behind the tube.
#
# Usage: python Misc/quakevr/improve_weapons.py [output progs folder]   (default: quakevr/progs)
#
# The inputs are the port's models as they were before (Misc/quakevr/src_models/, byte for byte
# the models of commit 036342b5): running it again gives the same files. Pure Python, like the
# other generators here.
#
# What is kept: every frame (the firing animations), the skin's texels (the new parts are painted
# in the skin's unused corner, where id's old hand texture was: the sights' glowing texels, which
# vr_weapons' sight hues key on by palette index, are not touched) and the old geometry except the
# shotgun's handle. The new parts are low-poly and faceted as id's are (bevelled boxes and
# octagons), and follow the gun rigidly through the animation: each frame's pose of a few reference
# vertices of the body (a frame from three clusters of them) carries them.
#
# Placement: the grips are laid out in the hand's own space, where the fist that the engine draws
# holds them (hand_base.mdl and the finger models, placed as vr_view.cpp places them on a held
# weapon, at the weapon's hand anchor vertex and hand offsets, at the default scales): the grip goes
# through the curled fingers, index finger on top at the trigger, the pinky just above the butt,
# leaning back 16 degrees. The same grip in the hand's space is the same size in both weapons.
# The hand stays where it was relative to the controller (and so to the player's hand); only on
# the rocket launcher the gun sits higher over the hand (the tube went through the fist: now it is
# over the index finger, as a pistol's slide), and each hand is centred on its grip sideways.
#
# Anchors: the engine's anchor indices count the old engine's triangle strips (vr_anchor.cpp),
# not the file's vertices; the new triangles come after the old ones, so the kept vertices keep
# their indices (checked below). The shotgun's hand anchor was on the handle that goes: it moves
# to a vertex of the new grip. The models' bounds grow (the grips hang lower), which moves the
# origin their weapon Scale is applied about: the weapon offsets are compensated so that the old
# parts stay exactly where they were. The script prints the new values for Quake/vr/vr_weapons.inc
# (slots 2 and 6; vr_weapons.cpp's settingsVersion resets them in existing configs).

import math
import os
import struct
import sys

from mdlgen import HEADER, add, anorms, cross, dot, mul, norm, sub

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "src_models")

# Default weapon scales (vr_world_scale 1.25, vr_gunmodelscale 0.7): weapons::modelTransform's k
# and weapons::offsetScale (1 at the defaults).
K = (1.25 / 0.75) * 0.7


# ----------------------------------------------------------------------------
# MDL reading and writing (one plain skin, plain frames: as these two models are)

class Mdl:
    def __init__(self, data):
        h = list(HEADER.unpack_from(data, 0))
        self.h = h
        scale, origin = h[2:5], h[5:8]
        num_skins, self.sw, self.sh, num_verts, num_tris, num_frames = h[12:18]
        assert num_skins == 1
        off = HEADER.size
        assert struct.unpack_from("<i", data, off)[0] == 0, "a plain skin"
        self.skin = bytearray(data[off + 4 : off + 4 + self.sw * self.sh])
        off += 4 + self.sw * self.sh
        self.st = [tuple(struct.unpack_from("<3i", data, off + 12 * i)) for i in range(num_verts)]
        off += 12 * num_verts
        self.tris = [tuple(struct.unpack_from("<4i", data, off + 16 * i)) for i in range(num_tris)]
        off += 16 * num_tris
        self.frames = []  # [name, positions, normal indices]
        for _ in range(num_frames):
            assert struct.unpack_from("<i", data, off)[0] == 0, "plain frames"
            name = data[off + 12 : off + 28].split(b"\0")[0].decode("latin-1")
            off += 28
            pos, nrm = [], []
            for i in range(num_verts):
                x, y, z, n = data[off + 4 * i : off + 4 * i + 4]
                pos.append((x * scale[0] + origin[0], y * scale[1] + origin[1], z * scale[2] + origin[2]))
                nrm.append(n)
            off += 4 * num_verts
            self.frames.append([name, pos, nrm])
        self.origin = tuple(origin)

    def write(self, path):
        """Quantizes the frames over their new bounds; returns the new origin."""
        allp = [p for f in self.frames for p in f[1]]
        lo = [min(p[k] for p in allp) for k in range(3)]
        hi = [max(p[k] for p in allp) for k in range(3)]
        scale = [(hi[k] - lo[k]) / 255.0 or 1.0 for k in range(3)]
        h = list(self.h)
        h[2:5] = scale
        h[5:8] = lo
        h[8] = max(math.sqrt(dot(p, p)) for p in allp)
        h[15], h[16], h[17] = len(self.st), len(self.tris), len(self.frames)
        out = bytearray(HEADER.pack(*h))
        out += struct.pack("<i", 0) + self.skin
        for st in self.st:
            out += struct.pack("<3i", *st)
        for t in self.tris:
            out += struct.pack("<4i", *t)
        for name, pos, nrm in self.frames:
            q = [[max(0, min(255, int(round((p[k] - lo[k]) / scale[k])))) for k in range(3)] for p in pos]
            bmin = bytes(min(v[k] for v in q) for k in range(3)) + b"\0"
            bmax = bytes(max(v[k] for v in q) for k in range(3)) + b"\0"
            out += struct.pack("<i", 0) + bmin + bmax + name.encode("latin-1").ljust(16, b"\0")[:16]
            for v, n in zip(q, nrm):
                out += bytes((v[0], v[1], v[2], n))
        with open(path, "wb") as f:
            f.write(out)
        return tuple(lo)

    def components(self):
        parent = list(range(len(self.st)))

        def find(a):
            while parent[a] != a:
                parent[a] = parent[parent[a]]
                a = parent[a]
            return a

        for _, a, b, c in self.tris:
            for x, y in ((a, b), (b, c)):
                parent[find(x)] = find(y)
        comps = {}
        for v in range(len(self.st)):
            comps.setdefault(find(v), []).append(v)
        return sorted(comps.values(), key=min)


def strip_order(tris):
    """Strip-order index -> vertex (vr_anchor.cpp's StripBuilder, QuakeSpasm's old BuildTris)."""
    n = len(tris)
    used = [0] * n
    order = []

    def clear(start):
        for j in range(start + 1, n):
            if used[j] == 2:
                used[j] = 0

    def length(start, sv, strip):
        used[start] = 2
        last = tris[start]
        vi = last[1:]
        verts = [vi[sv % 3], vi[(sv + 1) % 3], vi[(sv + 2) % 3]]
        got = [start]
        m1, m2 = (vi[(sv + 2) % 3], vi[(sv + 1) % 3]) if strip else (vi[sv % 3], vi[(sv + 2) % 3])
        extended = True
        while extended and len(got) < 126:
            extended = False
            for j in range(start + 1, n):
                check = tris[j]
                if check[0] != last[0]:
                    continue
                cv = check[1:]
                hit = next((k for k in range(3) if cv[k] == m1 and cv[(k + 1) % 3] == m2), None)
                if hit is None:
                    continue
                if used[j]:
                    clear(start)
                    return verts, got
                nv = cv[(hit + 2) % 3]
                if strip:
                    if len(got) & 1:
                        m2 = nv
                    else:
                        m1 = nv
                else:
                    m2 = nv
                verts.append(nv)
                got.append(j)
                used[j] = 2
                extended = True
                break
        clear(start)
        return verts, got

    for i in range(n):
        if used[i]:
            continue
        best = None
        for strip in (False, True):
            for sv in range(3):
                v, t = length(i, sv, strip)
                if best is None or len(t) > len(best[1]):
                    best = (v, t)
        for j in best[1]:
            used[j] = 1
        order += best[0]
    return order


# ----------------------------------------------------------------------------
# Rigid frames: the new parts follow three clusters of the body's vertices

def centroid(pts):
    return mul(tuple(map(sum, zip(*pts))), 1.0 / len(pts))


class Carrier:
    def __init__(self, model, clusters):
        self.frames = []
        for _, pos, _ in model.frames:
            a, b, c = (centroid([pos[v] for v in cl]) for cl in clusters)
            e1 = norm(sub(b, a))
            ca = sub(c, a)
            e2 = norm(sub(ca, mul(e1, dot(ca, e1))))
            self.frames.append((a, e1, e2, cross(e1, e2)))

    def place(self, f, p, direction=False):
        """Frame 0's point (or direction) `p` in frame `f`."""
        a0, *axes0 = self.frames[0]
        af, *axesf = self.frames[f]
        d = p if direction else sub(p, a0)
        local = [dot(d, e) for e in axes0]
        out = (0.0, 0.0, 0.0)
        for l, e in zip(local, axesf):
            out = add(out, mul(e, l))
        return out if direction else add(af, out)


# ----------------------------------------------------------------------------
# Low-poly parts

class Parts:
    """New vertices (frame 0 position, skin s, t) and triangles, oriented as Quake's (clockwise
    seen from outside)."""

    def __init__(self):
        self.verts = []
        self.tris = []

    def vert(self, p, s, t):
        self.verts.append((p, (int(round(s)), int(round(t)))))
        return len(self.verts) - 1

    def tri(self, a, b, c, outward):
        p0, p1, p2 = (self.verts[i][0] for i in (a, b, c))
        if dot(cross(sub(p1, p0), sub(p2, p0)), outward) > 0.0:
            b, c = c, b
        self.tris.append((a, b, c))

    def loft(self, rings, region, cap_start=None, cap_end=None):
        """Rings of the same number of points joined by quads; the region's s goes round them, its t
        along them. `cap_*`: a region for a flat cap on that end."""
        s0, t0, s1, t1 = region
        n = len(rings[0])
        ring0 = rings[0]
        edges = [math.dist(ring0[k], ring0[(k + 1) % n]) for k in range(n)]
        around = [sum(edges[:k]) / sum(edges) for k in range(n + 1)]
        cents = [centroid(r) for r in rings]
        steps = [0.0] + [math.dist(cents[j], cents[j + 1]) for j in range(len(rings) - 1)]
        along = [sum(steps[: j + 1]) / sum(steps) for j in range(len(rings))]
        ids = [[self.vert(r[k % n], s0 + 0.5 + around[k] * (s1 - s0 - 1), t0 + 0.5 + along[j] * (t1 - t0 - 1))
                for k in range(n + 1)] for j, r in enumerate(rings)]
        for j in range(len(rings) - 1):
            for k in range(n):
                a, b, c, d = ids[j][k], ids[j][k + 1], ids[j + 1][k + 1], ids[j + 1][k]
                mid = centroid([self.verts[i][0] for i in (a, b, c, d)])
                axis_pt = mul(add(cents[j], cents[j + 1]), 0.5)
                out = sub(mid, axis_pt)
                self.tri(a, b, c, out)
                self.tri(a, c, d, out)
        if cap_start:
            self.cap(rings[0], cap_start, sub(cents[0], cents[1]))
        if cap_end:
            self.cap(rings[-1], cap_end, sub(cents[-1], cents[-2]))

    def cap(self, ring, region, outward):
        s0, t0, s1, t1 = region
        c = centroid(ring)
        u = norm(sub(ring[0], c))
        v = norm(cross(norm(outward), u))
        ext = max(max(abs(dot(sub(p, c), u)), abs(dot(sub(p, c), v))) for p in ring)

        def st(p):
            d = sub(p, c)
            return (s0 + s1) / 2 + dot(d, u) / ext * (s1 - s0 - 2) / 2, (t0 + t1) / 2 + dot(d, v) / ext * (t1 - t0 - 2) / 2

        centre = self.vert(c, *st(c))
        ids = [self.vert(p, *st(p)) for p in ring]
        for k in range(len(ring)):
            self.tri(centre, ids[k], ids[(k + 1) % len(ring)], outward)


def section(c, ex, ey, hx, hy, ch):
    """A bevelled rectangle round `c` in the plane of ex, ey: 8 points (4 unbevelled), round the
    axis ex x ey."""
    if ch <= 0.0:
        return [add(c, add(mul(ex, u), mul(ey, v))) for u, v in ((hx, -hy), (hx, hy), (-hx, hy), (-hx, -hy))]
    ch = min(ch, hx * 0.6, hy * 0.6)
    pts = [(hx, -hy + ch), (hx, hy - ch), (hx - ch, hy), (-hx + ch, hy), (-hx, hy - ch), (-hx, -hy + ch),
           (-hx + ch, -hy), (hx - ch, -hy)]
    return [add(c, add(mul(ex, u), mul(ey, v))) for u, v in pts]


def octagon(c, ex, ey, r):
    return [add(c, add(mul(ex, r * math.cos(math.pi * (k + 0.5) / 4)), mul(ey, r * math.sin(math.pi * (k + 0.5) / 4))))
            for k in range(8)]


def sweep(parts, path, hx, hy, region, ch=0.0):
    """A bar of bevelled-rectangle section along a path in a plane of constant y (a trigger guard,
    a trigger): hx across the path in that plane, hy along y; capped ends."""
    ey = (0.0, 1.0, 0.0)
    rings = []
    for i, p in enumerate(path):
        a = path[max(0, i - 1)]
        b = path[min(len(path) - 1, i + 1)]
        tangent = norm(sub(b, a))
        ex = norm(cross(ey, tangent))
        rings.append(section(p, ex, ey, hx, hy, ch))
    parts.loft(rings, region, cap_start=region, cap_end=region)


# ----------------------------------------------------------------------------
# The hand's space (vr_view.cpp: a held weapon's hand is drawn at its hand anchor vertex plus the
# hand offsets; its model and the weapon's turn together): a point q of the hand's space (Quake
# units at the weapon settings' k, before the weapon's Scale) is at p0 + q / Sw in the weapon's
# model, p0 being the hand's origin there. Measured on hand_base.mdl and the finger models as
# they are drawn holding a weapon (grip curled, trigger released): the fist spans x -5.3..-0.2
# (the fingers from -2.2), y -1.5..1.2 (the thumb on +y) and z -3.4..-0.3 (index finger on top,
# -0.9..-1.8; pinky at the bottom, -2.5..-3.4); the fingers curl round a line through
# (-1.3, -0.16, -1.9) that leans back going down.

GRIP_LEAN = math.radians(16.0)
GRIP_Y = -0.16
CEILING = -0.75  # the index finger's top, and above it the gun's underside


def grip_x(z):
    return -1.3 + (z + 1.9) * math.tan(GRIP_LEAN)


class HandSpace:
    def __init__(self, p0, sw, y_centre):
        self.p0 = (p0[0], y_centre - GRIP_Y / sw, p0[2])
        self.sw = sw

    def m(self, x, z, y=GRIP_Y):
        return (self.p0[0] + x / self.sw, self.p0[1] + y / self.sw, self.p0[2] + z / self.sw)


def grip(parts, hs, wood, butt, bottom, profile):
    """The pistol grip: rings square to its leaning axis, from inside the gun down to the butt;
    then a thin plate and its bottom."""
    ex = (math.cos(GRIP_LEAN), 0.0, -math.sin(GRIP_LEAN))  # forward, square to the axis
    ey = (0.0, 1.0, 0.0)
    k = 1.0 / hs.sw

    def ring(z, hd, hw, ch):
        return section(hs.m(grip_x(z), z), ex, ey, hd * k, hw * k, ch * k)

    parts.loft([ring(*r) for r in profile], wood)
    z, hd, hw, ch = profile[-1]
    parts.loft([ring(z, hd, hw, ch), ring(z - 0.05, hd + 0.03, hw + 0.03, ch), ring(z - 0.16, hd + 0.02, hw + 0.02, ch)],
               butt, cap_end=bottom)


GRIP_PROFILE = [  # (z, half depth, half width, bevel) in the hand's space
    (-0.25, 0.74, 0.50, 0.22),
    (-0.95, 0.76, 0.52, 0.24),
    (-2.00, 0.81, 0.56, 0.26),
    (-3.00, 0.77, 0.54, 0.25),
    (-3.30, 0.85, 0.60, 0.27),
    (-3.58, 0.92, 0.64, 0.28),
]
GUARD_PATH = [(-0.55, -1.90), (0.05, -2.04), (0.72, -1.96), (1.08, -1.62), (1.20, -1.12), (1.24, -0.40)]
TRIGGER_PATH = [(0.30, -0.40), (0.34, -0.82), (0.24, -1.20), (0.04, -1.46)]


def guard_and_trigger(parts, hs, metal, trigger_region):
    k = 1.0 / hs.sw
    sweep(parts, [hs.m(x, z) for x, z in GUARD_PATH], 0.09 * k, 0.26 * k, metal)
    sweep(parts, [hs.m(x, z) for x, z in TRIGGER_PATH], 0.075 * k, 0.11 * k, trigger_region)


# ----------------------------------------------------------------------------
# Skin painting, in Quake's palette (gfx/palette.lmp): dithered ramps with some grain and wear,
# darker round the edges as id's skins are.

class Noise:
    def __init__(self, seed):
        self.seed = seed

    def hash(self, x, y):
        h = (x * 374761393 + y * 668265263 + self.seed * 2147483647) & 0xFFFFFFFF
        h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
        return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0

    def smooth(self, x, y):
        xi, yi = math.floor(x), math.floor(y)
        fx, fy = x - xi, y - yi
        fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
        a, b = self.hash(xi, yi), self.hash(xi + 1, yi)
        c, d = self.hash(xi, yi + 1), self.hash(xi + 1, yi + 1)
        return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy


BAYER4 = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


def pick(ramp, v, s, t):
    """Ramp entry for a value 0..1, ordered-dithered between neighbours."""
    x = max(0.0, min(1.0, v)) * (len(ramp) - 1)
    i = int(x)
    if x - i > (BAYER4[t % 4][s % 4] + 0.5) / 16.0:
        i += 1
    return ramp[min(i, len(ramp) - 1)]


def paint(skin, sw, region, fn):
    s0, t0, s1, t1 = region
    w, h = s1 - s0, t1 - t0
    for t in range(t0, t1):
        for s in range(s0, s1):
            v = fn(s - s0, t - t0, w, h)
            assert v < 224, "no fullbright texels"
            skin[t * sw + s] = v


def edge(u, w, width=2.0):
    """0 at a region's edge, 1 inside."""
    return min(1.0, min(u + 0.5, w - u - 0.5) / width)


def section_faces(hx, hy, ch):
    """For a loft of `section` rings: the fractions of the way round where its faces start (front,
    bevel, left, bevel, back, bevel, right, bevel)."""
    pts = section((0, 0, 0), (1, 0, 0), (0, 1, 0), hx, hy, ch)
    edges = [math.dist(pts[k], pts[(k + 1) % 8]) for k in range(8)]
    return [sum(edges[:k]) / sum(edges) for k in range(9)]


WALNUT = [16, 17, 18, 96, 19, 97, 20, 98, 21, 99]                 # dark to light reddish browns
GUNMETAL = [0, 1, 2, 3, 4, 5, 6, 7]                               # the barrels' greys
STEEL = [0, 1, 2, 3, 4, 5, 6, 7, 8]
LAUNCHER = [16, 174, 17, 173, 18, 19, 172, 20, 171, 170, 169]      # the rocket launcher's browns
GRIPBROWN = [16, 174, 17, 173, 18, 19, 172, 20]                   # the launcher's darkest browns


def wood(noise, checker_faces=None, faces=None):
    """Walnut, the grain along t; optional checkering on the faces (fractions of the way round) in the
    middle of t."""
    def fn(s, t, w, h):
        g = noise.smooth(s * 0.45, t * 0.06) * 0.7 + noise.smooth(s * 1.3, t * 0.2) * 0.3
        v = 0.18 + 0.4 * g
        if (g * 9.0) % 1.0 < 0.16:
            v -= 0.18  # grain lines
        v += (noise.hash(s, t) - 0.5) * 0.12
        v *= 0.75 + 0.25 * edge(t, h, 3.0)
        if checker_faces and 0.22 * h < t < 0.78 * h:
            u = (s + 0.5) / w
            for f in checker_faces:
                a, b = faces[f], faces[f + 1]
                if a + 0.02 < u < b - 0.02:
                    if (s + t) % 4 == 0 or (s - t) % 4 == 0:
                        v = 0.12
                    else:
                        v = min(v, 0.55) + 0.05
        return pick(WALNUT, v, s, t)
    return fn


def metal(noise, ramp, base=0.45, spread=0.2, lines=None, grain=0.06):
    """Worn metal: mottled, lighter scratches along s, darker round the region's edges."""
    def fn(s, t, w, h):
        v = base + spread * (noise.smooth(s * 0.25, t * 0.25) - 0.5) * 2
        v += (noise.hash(s, t) - 0.5) * grain
        if noise.hash(s // 7, t) > 0.985:
            v += 0.25  # a scratch
        if lines and t % lines == 0:
            v -= 0.25
        v *= 0.7 + 0.3 * edge(s, w, 2.0) * edge(t, h, 2.0)
        return pick(ramp, v, s, t)
    return fn


def ribbed(noise, ramp, pitch):
    """A moulded grip: ribs across t, lighter on their crests."""
    def fn(s, t, w, h):
        phase = (t % pitch) / pitch
        v = 0.35 + 0.18 * math.sin(phase * 2 * math.pi) + (noise.hash(s, t) - 0.5) * 0.12
        v *= 0.75 + 0.25 * edge(t, h, 3.0)
        return pick(ramp, v, s, t)
    return fn


def end_grain(noise):
    """A sawn-off stock's end: the wood's rings, lighter than its sides."""
    def fn(s, t, w, h):
        r = math.hypot((s + 0.5) / w - 0.5, (t + 0.5) / h - 0.62) * 9 + noise.smooth(s * 0.3, t * 0.3)
        v = 0.45 + (0.15 if r % 1.0 < 0.35 else 0.0) + (noise.hash(s, t) - 0.5) * 0.1
        return pick(WALNUT, v * (0.7 + 0.3 * edge(s, w, 1.5) * edge(t, h, 1.5)), s, t)
    return fn


def nozzle_inside(noise):
    """A nozzle's mouth: soot-black in the middle, the metal's rim round it."""
    def fn(s, t, w, h):
        r = math.hypot((s + 0.5) / w - 0.5, (t + 0.5) / h - 0.5) * 2
        v = 0.02 + 0.5 * max(0.0, r - 0.55) + (noise.hash(s, t) - 0.5) * 0.08
        return pick(LAUNCHER, v, s, t)
    return fn


# ----------------------------------------------------------------------------
# Building

def free_region_check(model, regions):
    """The regions must be unused by the old triangles."""
    for front, a, b, c in model.tris:
        uv = []
        for v in (a, b, c):
            on, s, t = model.st[v]
            uv.append((s + model.sw // 2 if on and not front else s, t))
        for name, (s0, t0, s1, t1) in regions.items():
            if max(u[0] for u in uv) >= s0 and min(u[0] for u in uv) < s1 and max(u[1] for u in uv) >= t0 and min(u[1] for u in uv) < t1:
                raise SystemExit("region %s overlaps an old triangle" % name)


def assemble(model, keep_tris, parts, carrier):
    """The kept old triangles (their vertices renumbered), then the new parts, carried through the
    frames. Returns the old -> new vertex map."""
    used = sorted({v for t in keep_tris for v in t[1:]})
    remap = {v: i for i, v in enumerate(used)}
    table = anorms()
    st = [model.st[v] for v in used]
    tris = [(t[0], remap[t[1]], remap[t[2]], remap[t[3]]) for t in keep_tris]
    base = len(used)
    st += [(0, s, t) for _, (s, t) in parts.verts]
    tris += [(1, base + a, base + b, base + c) for a, b, c in parts.tris]

    # Vertex normals of the new parts (their faces', averaged), carried with them.
    normals = [(0.0, 0.0, 0.0)] * len(parts.verts)
    for a, b, c in parts.tris:
        p0, p1, p2 = (parts.verts[i][0] for i in (a, b, c))
        n = cross(sub(p2, p0), sub(p1, p0))  # outward (the triangles are clockwise from outside)
        for i in (a, b, c):
            normals[i] = add(normals[i], n)
    normals = [norm(n) if dot(n, n) > 0 else (0.0, 0.0, 1.0) for n in normals]

    for f, frame in enumerate(model.frames):
        _, pos, nrm = frame
        new_pos = [pos[v] for v in used] + [carrier.place(f, p) for p, _ in parts.verts]
        new_nrm = [nrm[v] for v in used]
        for n in normals:
            nf = carrier.place(f, n, direction=True)
            new_nrm.append(max(range(len(table)), key=lambda i: dot(table[i], nf)))
        frame[1], frame[2] = new_pos, new_nrm
    model.st, model.tris = st, tris
    return remap


def check_anchors(before, after, remap, anchors):
    """Each kept anchor index still names the same vertex."""
    old_order, new_order = strip_order(before), strip_order(after)
    for name, index in anchors.items():
        v = old_order[index]
        assert v in remap and new_order[index] == remap[v], "anchor %s (%d) moved" % (name, index)


def anchor_index(tris, vertex):
    return strip_order(tris).index(vertex)


def fmt(x):
    return ("%.6f" % x).rstrip("0").rstrip(".")


# ----------------------------------------------------------------------------
# The double shotgun

def build_shotgun(out_dir):
    src = open(os.path.join(SRC, "v_shot2.mdl"), "rb").read()
    model = Mdl(src)
    old_tris = list(model.tris)
    old_origin = model.origin
    comps = model.components()
    frame0 = model.frames[0][1]

    # The weapon settings the old model was tuned with (vr_weapons.inc slot 2 before round 16).
    sw = 0.48
    hand_av, hand_ofs = 65, (-3.25, 1.1, 1.25)
    offset = (6.099997, 1.35, -2.350011)
    kept_anchors = {"muzzle": 13, "2H hand": 29, "ammo screen": 0, "shells (vr_shells.cpp)": 17}

    old_hand_vertex = strip_order(old_tris)[hand_av]
    handle = next(c for c in comps if old_hand_vertex in c)
    assert len(handle) == 43
    keep = [t for t in old_tris if t[1] not in set(handle)]

    anchor_pos = frame0[old_hand_vertex]
    p0 = add(anchor_pos, mul(hand_ofs, 1.0 / (K * sw)))
    hs = HandSpace(p0, sw, y_centre=-0.03)  # between the barrels
    yc = -0.03

    regions = {
        "grip": (0, 0, 88, 60), "wrist": (88, 0, 168, 44), "receiver": (0, 64, 120, 104),
        "butt": (168, 0, 232, 12), "bottom": (168, 12, 200, 36), "caps": (200, 12, 232, 36),
        "guard": (120, 64, 216, 76), "trigger": (120, 80, 168, 92), "tang": (168, 44, 208, 56),
        "cutend": (208, 44, 232, 60), "knuckle": (0, 106, 64, 126), "breech": (240, 0, 280, 40),
    }
    free_region_check(model, regions)
    parts = Parts()

    # The grip first: the hand's anchor is on it, and its strip-order index then does not depend on
    # the parts after it.
    faces = section_faces(0.81, 0.56, 0.26)
    grip(parts, hs, regions["grip"], regions["butt"], regions["bottom"], GRIP_PROFILE)
    guard_and_trigger(parts, hs, regions["guard"], regions["trigger"])

    # The receiver behind the breech (the barrels hinge into it) and the stock cut down to its
    # wrist, which runs back over the web of the hand and down into the grip.
    def rsec(x, hw, zb, zt, ch):
        return section((x, yc, (zb + zt) / 2), (0.0, 0.0, 1.0), (0.0, 1.0, 0.0), (zt - zb) / 2, hw, ch)

    # Under the barrels its knuckle reaches the fore-end; above, the breech face stands just behind
    # the barrels' ends (the chambers, where vr_shells.cpp ejects the casings from).
    parts.loft([rsec(12.75, 2.95, 2.75, 5.35, 0.7), rsec(11.1, 2.95, 2.85, 5.35, 0.7)],
               regions["knuckle"], cap_start=regions["caps"])
    parts.loft([rsec(11.1, 2.95, 2.85, 6.95, 0.7), rsec(8.6, 2.95, 3.05, 6.95, 0.7), rsec(5.9, 2.8, 3.05, 6.85, 0.7)],
               regions["receiver"], cap_start=regions["breech"])
    parts.loft([rsec(5.9, 2.8, 3.05, 6.85, 0.7), rsec(3.4, 2.2, 3.3, 6.05, 0.7), rsec(1.9, 1.65, 3.55, 5.2, 0.55),
                rsec(1.2, 1.3, 3.75, 4.65, 0.4)], regions["wrist"], cap_end=regions["cutend"])
    # The top lever over the breech, and the tang behind it.
    parts.loft([rsec(9.4, 0.55, 6.8, 7.25, 0.0), rsec(6.6, 0.55, 6.8, 7.2, 0.0), rsec(5.4, 0.4, 6.6, 6.95, 0.0)],
               regions["tang"], cap_start=regions["tang"], cap_end=regions["tang"])


    noise = Noise(16)
    paint(model.skin, model.sw, regions["grip"], wood(noise, checker_faces=(2, 6), faces=faces))
    paint(model.skin, model.sw, regions["wrist"], wood(Noise(17)))
    paint(model.skin, model.sw, regions["receiver"], metal(Noise(18), GUNMETAL, 0.36, 0.08, grain=0.03))
    paint(model.skin, model.sw, regions["cutend"], end_grain(Noise(25)))
    paint(model.skin, model.sw, regions["knuckle"], metal(Noise(26), GUNMETAL, 0.36, 0.08, grain=0.03))
    paint(model.skin, model.sw, regions["breech"], metal(Noise(27), GUNMETAL, 0.5, 0.08, grain=0.03))
    paint(model.skin, model.sw, regions["tang"], metal(Noise(19), GUNMETAL, 0.45, 0.08, grain=0.03))
    paint(model.skin, model.sw, regions["caps"], metal(Noise(20), GUNMETAL, 0.3, 0.08, grain=0.03))
    paint(model.skin, model.sw, regions["butt"], metal(Noise(21), STEEL, 0.45, 0.12))
    paint(model.skin, model.sw, regions["bottom"], metal(Noise(22), STEEL, 0.38, 0.15, lines=3))
    paint(model.skin, model.sw, regions["guard"], metal(Noise(23), GUNMETAL, 0.45, 0.1, grain=0.03))
    paint(model.skin, model.sw, regions["trigger"], metal(Noise(24), STEEL, 0.55, 0.1))

    # Carried by the barrels' breech ends, their muzzles and the fore-end's underside.
    body = [v for c in comps if c is not handle for v in c]
    clusters = [[v for v in body if frame0[v][0] < 13.0 and frame0[v][2] > 5.0],
                [v for v in body if frame0[v][0] > 26.0],
                [v for v in body if frame0[v][2] < 3.0]]
    carrier = Carrier(model, clusters)
    remap = assemble(model, keep, parts, carrier)
    check_anchors(old_tris, model.tris, remap, kept_anchors)

    # The hand's anchor: the new grip's vertex nearest the middle of the hand's hold.
    base = len(remap)
    target = hs.m(grip_x(-1.9), -1.9)
    new_v = min(range(base, len(model.st)), key=lambda v: math.dist(model.frames[0][1][v], target))
    new_av = anchor_index(model.tris, new_v)
    new_hand = mul(sub(hs.p0, model.frames[0][1][new_v]), K * sw)

    origin = model.write(os.path.join(out_dir, "v_shot2.mdl"))
    new_offset = tuple(offset[k] + (old_origin[k] - origin[k]) * (1 - sw) for k in range(3))
    return "v_shot2.mdl (slot 2)", {
        "HandAnchorVertex": str(new_av),
        "HandOffsetX": fmt(new_hand[0]), "HandOffsetY": fmt(new_hand[1]), "HandOffsetZ": fmt(new_hand[2]),
        "OffsetX": fmt(new_offset[0]), "OffsetY": fmt(new_offset[1]), "OffsetZ": fmt(new_offset[2]),
    }, model


# ----------------------------------------------------------------------------
# The rocket launcher

RAISE = 2.0  # model units the tube goes up over the hand (it went through the fist)


def build_rocket_launcher(out_dir):
    src = open(os.path.join(SRC, "v_rock2.mdl"), "rb").read()
    model = Mdl(src)
    old_tris = list(model.tris)
    old_origin = model.origin
    comps = model.components()
    frame0 = model.frames[0][1]

    sw = 0.34  # vr_weapons.inc slot 6 before round 16
    hand_av, hand_ofs = 12, (2.100005, -0.8, 1.0)
    offset = (6.60001, 7.1, -0.350044)
    kept_anchors = {"hand, ammo screen, button": 12, "muzzle": 17, "2H hand": 3}

    order = strip_order(old_tris)
    anchor_pos = frame0[order[hand_av]]
    p0 = add(anchor_pos, mul(hand_ofs, 1.0 / (K * sw)))
    p0 = (p0[0], p0[1], p0[2] - RAISE)
    yc = -0.08  # the tube's axis
    hs = HandSpace(p0, sw, y_centre=yc)

    regions = {
        "grip": (0, 0, 72, 48), "butt": (72, 0, 136, 10), "bottom": (72, 10, 104, 34),
        "guard": (0, 52, 96, 64), "trigger": (104, 10, 144, 22), "nozzle": (0, 68, 96, 100),
        "mouth": (96, 68, 128, 100), "rear": (104, 24, 136, 48),
    }
    free_region_check(model, regions)
    parts = Parts()

    grip(parts, hs, regions["grip"], regions["butt"], regions["bottom"], GRIP_PROFILE)
    guard_and_trigger(parts, hs, regions["guard"], regions["trigger"])

    # The back-blast nozzle: out of the tube's pointed back, narrowing, a band, then a flared bell
    # (kept narrow and high: the back of the hand is under it).
    ay, az = (0.0, 1.0, 0.0), (0.0, 0.0, 1.0)
    zc = 4.75

    def ring(x, r):
        return octagon((x, yc, zc), az, ay, r)

    parts.loft([ring(0.2, 2.15), ring(-1.4, 1.75), ring(-2.1, 1.75), ring(-2.3, 1.95), ring(-2.9, 1.95),
                ring(-3.1, 1.7), ring(-5.4, 2.05), ring(-6.0, 2.2)], regions["nozzle"])
    parts.loft([ring(-6.0, 2.2), ring(-6.0, 1.75), ring(-5.2, 1.5)], regions["rear"], cap_end=regions["mouth"])

    paint(model.skin, model.sw, regions["grip"], ribbed(Noise(30), GRIPBROWN, 4))
    paint(model.skin, model.sw, regions["butt"], metal(Noise(31), LAUNCHER, 0.5, 0.15))
    paint(model.skin, model.sw, regions["bottom"], metal(Noise(32), LAUNCHER, 0.35, 0.15, lines=4))
    paint(model.skin, model.sw, regions["guard"], metal(Noise(33), LAUNCHER, 0.5, 0.15))
    paint(model.skin, model.sw, regions["trigger"], metal(Noise(34), STEEL, 0.5, 0.1))
    paint(model.skin, model.sw, regions["nozzle"], metal(Noise(35), LAUNCHER, 0.3, 0.12, lines=11))
    paint(model.skin, model.sw, regions["rear"], metal(Noise(36), LAUNCHER, 0.22, 0.1))
    paint(model.skin, model.sw, regions["mouth"], nozzle_inside(Noise(37)))

    # Carried by the tube's back end, its underside further on and its top at the back.
    tube = [v for c in comps if len(c) > 1 and min(frame0[v][0] for v in c) < 30 for v in c]
    clusters = [[v for v in tube if frame0[v][0] < 2.0],
                [v for v in tube if 18.0 < frame0[v][0] < 30.0 and frame0[v][2] < 3.5],
                [v for v in tube if frame0[v][0] < 8.0 and frame0[v][2] > 7.0]]
    carrier = Carrier(model, clusters)
    remap = assemble(model, old_tris, parts, carrier)
    check_anchors(old_tris, model.tris, remap, kept_anchors)

    new_hand = mul(sub(hs.p0, anchor_pos), K * sw)
    origin = model.write(os.path.join(out_dir, "v_rock2.mdl"))
    new_offset = [offset[k] + (old_origin[k] - origin[k]) * (1 - sw) for k in range(3)]
    new_offset[2] += RAISE * sw
    return "v_rock2.mdl (slot 6)", {
        "HandOffsetX": fmt(new_hand[0]), "HandOffsetY": fmt(new_hand[1]), "HandOffsetZ": fmt(new_hand[2]),
        "OffsetX": fmt(new_offset[0]), "OffsetY": fmt(new_offset[1]), "OffsetZ": fmt(new_offset[2]),
    }, model


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "quakevr", "progs")
    for build in (build_shotgun, build_rocket_launcher):
        name, settings, model = build(out_dir)
        print("%s: %d vertices, %d triangles, %d frames" % (name, len(model.st), len(model.tris), len(model.frames)))
        for key, value in settings.items():
            print("    %s = %s" % (key, value))


if __name__ == "__main__":
    main()
