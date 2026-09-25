#!/usr/bin/env python3
# make_pauldron.py -- generates the body's pauldrons (vr_view.cpp, vr_body_pauldrons), after the
# Quake ranger's (progs/player.mdl): quilted brown leather pads over the shoulders.
#   quakevr/progs/vrpauldron.mdl      the cap over the top of the shoulder
#   quakevr/progs/vrpauldron_arm.mdl  two lames around the top of the upper arm, under the cap
#
# Usage: python Misc/quakevr/make_pauldron.py [output progs folder]
#
# Model space is the body's bind pose (make_vrbody.py): Quake units at vr_world_scale 1
# (1 m = 1 / 0.0381 units), +x forward, +y the body's left, +z up, with the origin at the LEFT
# shoulder joint (upperarm_l); the right shoulder draws the model mirrored. The arm hangs in the
# bind pose's A-pose, 30 degrees out from vertical. Sized for the athletic build; the engine scales
# it for the others. The cap follows the clavicle (turning partly with the arm), the lames the upper
# arm.
#
# Skins (vr_body_pauldron_style): 0 the ranger's brown leather, 1-3 the green, yellow and red
# armours' colours (as progs/armor.mdl's skins), 4 steel. Palette indices from gfx/palette.lmp.

import math
import os
import struct
import sys

import mdlgen
from mdlgen import add, sub, mul, dot, cross, norm

UNITS = 1.0 / 0.0381

SKIN_W, SKIN_H = 128, 64
REGIONS = {"cap": (0, 0, 64, 48), "lame": (64, 0, 128, 48), "inner": (0, 48, 64, 64), "rim": (64, 48, 128, 64)}

# Dark to light.
RAMPS = [
    [174, 173, 172, 171, 170, 169],  # the ranger's leather
    [187, 186, 185, 184, 183, 182],  # green armour
    [205, 204, 203, 202, 201, 200],  # yellow armour
    [70, 72, 74, 76, 78, 79],        # red armour
    [3, 4, 5, 6, 7, 8],              # steel
]
RIVET = [9, 11, 12]
INNER = [175, 174]


class Builder:
    def __init__(self):
        self.mesh = mdlgen.Mesh(SKIN_W, SKIN_H, REGIONS)

    def vert(self, p, n, st):
        self.mesh.verts.append((mul(p, UNITS), n, (int(round(st[0])), int(round(st[1])))))
        return len(self.mesh.verts) - 1

    def tri(self, a, b, c):
        """A triangle, turned clockwise seen from where its vertices' normals point."""
        v = self.mesh.verts
        p0, p1, p2 = v[a][0], v[b][0], v[c][0]
        n = add(add(v[a][1], v[b][1]), v[c][1])
        if dot(cross(sub(p1, p0), sub(p2, p0)), n) > 0.0:
            b, c = c, b
        self.mesh.tris.append((a, b, c))

    def shell(self, point, nu, nv, region, thickness, centre_of):
        """A curved plate: point(u, v) (u, v in 0..1) is its outer surface; the inner one is
        `thickness` inside it (towards centre_of(u, v)), and a rim closes the edges. The outer
        surface takes `region` (u across, v down), the inner "inner", the rim "rim"."""
        def outward(u, v):
            p = point(u, v)
            e = 1e-3
            du = sub(point(min(1, u + e), v), point(max(0, u - e), v))
            dv = sub(point(u, min(1, v + e)), point(u, max(0, v - e)))
            n = norm(cross(du, dv))
            return n if dot(n, sub(p, centre_of(u, v))) > 0 else mul(n, -1.0)

        s0, t0, s1, t1 = REGIONS[region]
        i0, j0, i1, j1 = REGIONS["inner"]
        outer, inner = {}, {}
        for j in range(nv + 1):
            for i in range(nu + 1):
                u, v = i / nu, j / nv
                p, n = point(u, v), outward(u, v)
                outer[i, j] = self.vert(p, n, (s0 + 0.5 + u * (s1 - s0 - 1), t0 + 0.5 + v * (t1 - t0 - 1)))
                inner[i, j] = self.vert(sub(p, mul(n, thickness)), mul(n, -1.0),
                                        (i0 + 0.5 + u * (i1 - i0 - 1), j0 + 0.5 + v * (j1 - j0 - 1)))
        for j in range(nv):
            for i in range(nu):
                for grid in (outer, inner):
                    a, b, c, d = grid[i, j], grid[i + 1, j], grid[i, j + 1], grid[i + 1, j + 1]
                    self.tri(a, b, c)
                    self.tri(b, d, c)

        # The rim: around the edge, a strip from the outer surface to the inner one, facing out
        # of the plate's edge.
        edge = [(i, 0) for i in range(nu)] + [(nu, j) for j in range(nv)] + \
               [(i, nv) for i in range(nu, 0, -1)] + [(0, j) for j in range(nv, 0, -1)]
        r0, q0, r1, q1 = REGIONS["rim"]
        mid = point(0.5, 0.5)
        for k in range(len(edge)):
            (ia, ja), (ib, jb) = edge[k], edge[(k + 1) % len(edge)]
            pa, pb = point(ia / nu, ja / nv), point(ib / nu, jb / nv)
            na, nb = outward(ia / nu, ja / nv), outward(ib / nu, jb / nv)
            qa, qb = sub(pa, mul(na, thickness)), sub(pb, mul(nb, thickness))
            side = norm(cross(sub(pb, pa), na))
            if dot(side, sub(pa, mid)) < 0:
                side = mul(side, -1.0)
            s = r0 + 0.5 + (k / len(edge)) * (r1 - r0 - 1)
            s2 = r0 + 0.5 + ((k + 1) / len(edge)) * (r1 - r0 - 1)
            a = self.vert(pa, side, (s, q0 + 1))
            b = self.vert(pb, side, (s2, q0 + 1))
            c = self.vert(qa, side, (s, q1 - 2))
            d = self.vert(qb, side, (s2, q1 - 2))
            self.tri(a, b, c)
            self.tri(b, d, c)


def radians(d):
    return math.radians(d)


def cap():
    """A dome over the top of the shoulder: from the slope towards the neck (u = 0 the front),
    over the top and down the outside to the top of the arm."""
    b = Builder()
    r = 0.108

    def point(u, v):
        beta = radians(-54 + 108 * u)       # front (u = 0) to back
        alpha = radians(-32 + 107 * v)     # from inwards over the top (v = 0) to outwards and down
        # Slightly longer front to back, lower over the top, bulging a little at its middle.
        bulge = 1.0 + 0.06 * math.cos(beta * 1.6) * math.sin(math.pi * v)
        return (r * 1.05 * bulge * math.sin(beta),
                r * bulge * math.cos(beta) * math.sin(alpha),
                0.004 + r * 0.86 * bulge * math.cos(beta) * math.cos(alpha))

    b.shell(point, 12, 7, "cap", 0.011, lambda u, v: (0.0, 0.0, 0.0))
    return b.mesh


# The upper arm in the bind pose: 30 degrees out from vertical; "out" is across it, up and away
# from the body.
ARM = (0.0, math.sin(radians(30)), -math.cos(radians(30)))
OUT = (0.0, math.cos(radians(30)), math.sin(radians(30)))
FWD = (1.0, 0.0, 0.0)


def lames():
    """Two bands around the outside of the upper arm, the upper one over the lower, each flaring
    a little at its lower edge."""
    b = Builder()
    for d0, d1, r, half in ((0.012, 0.072, 0.094, 78), (0.058, 0.112, 0.089, 72)):
        def point(u, v, d0=d0, d1=d1, r=r, half=half):
            phi = radians(-half + 2 * half * u)  # front (u = 0) to back, round the outside
            d = d0 + (d1 - d0) * v
            rr = r * (1.0 + 0.08 * v)
            around = add(mul(OUT, math.cos(phi)), mul(FWD, -math.sin(phi)))
            return add(mul(ARM, d), mul(around, rr))

        b.shell(point, 10, 3, "lame", 0.008, lambda u, v, d0=d0, d1=d1: mul(ARM, d0 + (d1 - d0) * v))
    return b.mesh


def paint(style):
    """Palette indices for one skin: quilted ridges across the plates, a light edge along their
    lower rims, rivets at the corners; dark leather inside."""
    ramp = RAMPS[style]
    rng = 4321 + style
    px = bytearray()
    for t in range(SKIN_H):
        for s in range(SKIN_W):
            rng = (rng * 1103515245 + 12345) & 0x7FFFFFFF
            k = (rng >> 16) % 10
            region = next(r for r, (s0, t0, s1, t1) in REGIONS.items() if s0 <= s < s1 and t0 <= t < t1)
            s0, t0, s1, t1 = REGIONS[region]
            u, v = (s - s0) / (s1 - s0), (t - t0) / (t1 - t0)
            if region == "inner":
                px.append(INNER[k % 2])
                continue
            if region == "rim":
                px.append(ramp[1] if k < 7 else ramp[2])
                continue

            # Base: the middle of the ramp, darker towards the plate's ends (front and back).
            i = 3 - (1 if abs(u - 0.5) > 0.38 else 0) + (1 if k > 7 else -1 if k < 2 else 0)
            ridges = 6 if region == "cap" else 2
            band = v * ridges - int(v * ridges)
            if band < 0.1:
                i = 0  # the stitched seam between two ridges
            elif band < 0.25:
                i += 1  # the ridge's lit upper side
            elif band > 0.85:
                i -= 1  # its shaded lower side
            if v > 0.93:
                i = len(ramp) - 1  # the light lower edge
            index = ramp[max(0, min(len(ramp) - 1, i))]

            # Stitches along the seams, and rivets at the corners.
            if band < 0.1 and int(u * 40) % 2 == 0:
                index = ramp[2]
            for cu, cv in ((0.1, 0.12), (0.9, 0.12), (0.1, 0.86), (0.9, 0.86)):
                if region == "cap" and math.hypot((u - cu) * 2, v - cv) < 0.055:
                    index = RIVET[2] if math.hypot((u - cu) * 2 + 0.02, v - cv + 0.02) < 0.03 else RIVET[0]
            if region == "lame" and abs(v - 0.55) < 0.09 and (abs(u - 0.07) < 0.012 or abs(u - 0.93) < 0.012):
                index = RIVET[1]
            px.append(index)
    return bytes(px)


def write(path, mesh, skins, name):
    assert mesh.check_winding() == 0, "counter-clockwise triangles"
    table = mdlgen.anorms()
    positions = [v[0] for v in mesh.verts]
    lo = [min(p[k] for p in positions) for k in range(3)]
    hi = [max(p[k] for p in positions) for k in range(3)]
    scale = [(hi[k] - lo[k]) / 255.0 or 1.0 for k in range(3)]
    radius = max(math.sqrt(dot(p, p)) for p in positions)

    data = bytearray(struct.pack("<4si3f3f f3f 8i f", b"IDPO", 6, *scale, *lo, radius, 0.0, 0.0, 0.0,
                                 len(skins), SKIN_W, SKIN_H, len(mesh.verts), len(mesh.tris), 1, 0, 0, 1.0))
    for skin in skins:
        data += struct.pack("<i", 0) + skin
    for _, _, (s, t) in mesh.verts:
        data += struct.pack("<3i", 0, s, t)
    for a, b, c in mesh.tris:
        data += struct.pack("<4i", 1, a, b, c)
    data += struct.pack("<i", 0)
    data += bytes((0, 0, 0, 0)) + bytes((255, 255, 255, 0))
    data += name.encode().ljust(16, b"\0")[:16]
    for p, n, _ in mesh.verts:
        q = [max(0, min(255, int(round((p[k] - lo[k]) / scale[k])))) for k in range(3)]
        best = max(range(len(table)), key=lambda i: dot(table[i], n))
        data += bytes((q[0], q[1], q[2], best))
    with open(path, "wb") as f:
        f.write(data)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "progs")
    skins = [paint(style) for style in range(len(RAMPS))]
    for name, mesh in (("vrpauldron", cap()), ("vrpauldron_arm", lames())):
        path = os.path.join(out, name + ".mdl")
        write(path, mesh, skins, name)
        print("%s.mdl: %d vertices, %d triangles, %d skins -> %s" % (name, len(mesh.verts), len(mesh.tris), len(skins),
                                                                     os.path.normpath(path)))


if __name__ == "__main__":
    main()
