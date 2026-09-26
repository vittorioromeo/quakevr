#!/usr/bin/env python3
# make_shell.py -- generates the spent shotgun shell (vr_shells.cpp):
#   quakevr/progs/vr_shell.mdl   a fired 12-gauge shell, low poly: a red plastic hull (its crimp
#                                opened by the shot, dark inside) on a brass head with a rim and a
#                                primer. 7 cm long, 2 cm across, 8-sided.
#
# Usage: python Misc/quakevr/make_shell.py [output game folder]
#
# Model space: Quake units at vr_world_scale 1 (1 m = 1 / 0.0381 units; vr_shells.cpp scales the
# entity by vr_world_scale), the shell's axis along +x -- the open end forward, as it sits in the
# chamber -- and the origin at its middle (it tumbles about it). vr_shells.cpp's shellRadius (the
# rim's, 1.12 cm) keeps a lying shell on the floor.
# Palette indices from gfx/palette.lmp: reds 64..79, dull brass from the browns 28..31 and the olive 199, greys 0..15.

import math
import os
import random
import sys

import mdlgen
from mdlgen import add, sub, mul, dot, cross, norm

UNITS = 1.0 / 0.0381
SIDES = 8

SKIN_W, SKIN_H = 32, 32
REGIONS = {"hull": (0, 0, 16, 16), "brass": (16, 0, 32, 16), "base": (0, 16, 16, 32), "mouth": (16, 16, 32, 32)}

LENGTH = 0.070
BACK, FRONT = -LENGTH / 2, LENGTH / 2
RIM_R, HEAD_R, HULL_R = 0.0112, 0.0102, 0.0099
RIM_END = BACK + 0.0015   # the rim's thickness,
HEAD_END = BACK + 0.0145  # the brass head's length (a "low brass" field load),
CRIMP = FRONT - 0.0025    # where the opened crimp starts to flare,
FLARE_R = 0.0104          # and how far out it flares.


class Builder:
    def __init__(self):
        self.mesh = mdlgen.Mesh(SKIN_W, SKIN_H, REGIONS)

    def vert(self, p, n, st):
        self.mesh.verts.append((mul(p, UNITS), n, (int(round(st[0])), int(round(st[1])))))
        return len(self.mesh.verts) - 1

    def tri(self, a, b, c, n):
        """A triangle, turned clockwise seen from where `n` points."""
        v = self.mesh.verts
        p0, p1, p2 = v[a][0], v[b][0], v[c][0]
        if dot(cross(sub(p1, p0), sub(p2, p0)), n) > 0.0:
            b, c = c, b
        self.mesh.tris.append((a, b, c))

    def ring_point(self, x, r, k):
        a = 2 * math.pi * (k + 0.5) / SIDES
        return (x, r * math.cos(a), r * math.sin(a))

    def band(self, xa, ra, xb, rb, region, facing=None):
        """A band of quads round the axis between (xa, ra) and (xb, rb), facing away from the axis
        (or along `facing`, for a flat step)."""
        s0, t0, s1, t1 = REGIONS[region]
        for k in range(SIDES):
            pts = [self.ring_point(xa, ra, k), self.ring_point(xa, ra, k + 1), self.ring_point(xb, rb, k + 1),
                   self.ring_point(xb, rb, k)]
            n = norm(cross(sub(pts[1], pts[0]), sub(pts[3], pts[0])))
            centre = mul(add(add(pts[0], pts[1]), add(pts[2], pts[3])), 0.25)
            want = facing if facing else (0.0, centre[1], centre[2])
            if dot(n, want) < 0.0:
                n = mul(n, -1.0)
            # Along the axis down the region's t, round it across its s.
            st = [(s0 + 1, t0 + 1), (s1 - 1, t0 + 1), (s1 - 1, t1 - 1), (s0 + 1, t1 - 1)]
            i = [self.vert(p, n, uv) for p, uv in zip(pts, st)]
            self.tri(i[0], i[1], i[2], n)
            self.tri(i[0], i[2], i[3], n)

    def disc(self, x, r, facing, region):
        """A flat octagon across the axis at `x`, facing +x (1) or -x (-1), the region's picture on it."""
        n = (float(facing), 0.0, 0.0)
        s0, t0, s1, t1 = REGIONS[region]
        cs, ct = (s0 + s1) / 2, (t0 + t1) / 2
        centre = self.vert((x, 0.0, 0.0), n, (cs, ct))
        ring = []
        for k in range(SIDES):
            a = 2 * math.pi * (k + 0.5) / SIDES
            y, z = math.cos(a), math.sin(a)
            ring.append(self.vert((x, r * y, r * z), n, (cs + y * (s1 - s0 - 2) / 2, ct + z * (t1 - t0 - 2) / 2)))
        for k in range(SIDES):
            self.tri(centre, ring[k], ring[(k + 1) % SIDES], n)


def build():
    b = Builder()
    back, front = (-1.0, 0.0, 0.0), (1.0, 0.0, 0.0)
    # The head: the base with its primer, the rim, the step down to the head, the head.
    b.disc(BACK, RIM_R, -1, "base")
    b.band(BACK, RIM_R, RIM_END, RIM_R, "brass")
    b.band(RIM_END, RIM_R, RIM_END, HEAD_R, "brass", front)
    b.band(RIM_END, HEAD_R, HEAD_END, HEAD_R, "brass")
    # The hull, a shade thinner than the brass it comes out of, and its opened crimp flaring.
    b.band(HEAD_END, HEAD_R, HEAD_END, HULL_R, "brass", front)
    b.band(HEAD_END, HULL_R, CRIMP, HULL_R, "hull")
    b.band(CRIMP, HULL_R, FRONT, FLARE_R, "hull")
    # The open mouth: dark inside, the hull's edge round it.
    b.disc(FRONT - 0.0008, FLARE_R, 1, "mouth")
    return b.mesh


def paint():
    rng = random.Random(1912)
    px = bytearray()
    for t in range(SKIN_H):
        for s in range(SKIN_W):
            region = next(r for r, (s0, t0, s1, t1) in REGIONS.items() if s0 <= s < s1 and t0 <= t < t1)
            s0, t0, s1, t1 = REGIONS[region]
            u, v = (s - s0 + 0.5) / (s1 - s0), (t - t0 + 0.5) / (t1 - t0)
            k = rng.random()
            if region == "hull":
                # Red plastic with faint ribs along it, darker towards the brass.
                rib = int(u * 6) % 2 == 0
                shade = 77 if k < 0.6 else 78 if k < 0.85 else 76
                if rib:
                    shade -= 2
                if v < 0.15:
                    shade -= 2
                px.append(shade)
                continue
            if region == "brass":
                px.append(30 if k < 0.5 else 31 if k < 0.75 else 199 if k < 0.92 else 28)
                continue
            d = math.hypot(u - 0.5, v - 0.5) * 2  # 0 at the middle, 1 at the rim
            if region == "base":
                # The primer: a grey cup in a dark ring; brass round it, a few dark specks.
                if d < 0.28:
                    px.append(10 if k < 0.6 else 9)
                elif d < 0.38:
                    px.append(4)
                else:
                    px.append(30 if k < 0.5 else 31 if k < 0.9 else 28)
                continue
            # The mouth: sooty black inside, the hull's red edge.
            if d > 0.82:
                px.append(76 if k < 0.7 else 75)
            elif d > 0.66:
                px.append(4 if k < 0.5 else 5)
            else:
                px.append(1 if k < 0.7 else 2)
    return bytes(px)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    game = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr")

    mesh = build()
    path = os.path.join(game, "progs", "vr_shell.mdl")
    mdlgen.write_mdl(path, mesh, [paint()], "shell")
    print("vr_shell.mdl: %d vertices, %d triangles -> %s" % (len(mesh.verts), len(mesh.tris), os.path.normpath(path)))
    print("  %.2f units long, rim radius %.3f units" % (LENGTH * UNITS, RIM_R * UNITS))


if __name__ == "__main__":
    main()
