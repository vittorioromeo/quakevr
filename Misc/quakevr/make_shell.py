#!/usr/bin/env python3
# make_shell.py -- generates the spent shotgun shell (vr_shells.cpp):
#   quakevr/progs/vr_shell.mdl   a fired 12-gauge shell, low poly: a red plastic hull (its crimp
#                                opened by the shot, dark inside) on a brass head with a rim and a
#                                primer. 7 cm long, 2 cm across, 8-sided.
#   quakevr/progs/vr_shell_live.mdl  an unfired shell, as taken from the front ammo pouch (immersive reloading, QC
#                                vr_reload.qc; docs/vr-port/RELOAD.md): the same head and hull, its end closed
#                                by a six-fold star crimp, sunk a little.
#   quakevr/progs/vr_shell_pair.mdl  two unfired shells side by side (along y), taped together round the middle with
#                                a band of grey cloth tape (the pouch's Shell Pairs: both go in at once).
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

import genguard
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


# The live shells' skin, 64 x 32: the spent shell's four regions (the crimp's star painted where its mouth was) and the
# tape on the right half.
LIVE_W, LIVE_H = 64, 32
LIVE_REGIONS = dict(REGIONS, crimp=REGIONS["mouth"], tape=(32, 0, 64, 32))
CRIMP_SINK = 0.0018  # how far the star crimp's middle sits into the hull
TAPE_X, TAPE_HALF, TAPE_T = 0.004, 0.0085, 0.0006  # the pair's tape: its middle, half its width, its thickness


class Builder:
    def __init__(self, w=SKIN_W, h=SKIN_H, regions=REGIONS, y=0.0):
        self.regions = regions
        self.mesh = mdlgen.Mesh(w, h, regions)
        self.y = y  # the shell's axis, sideways (a pair's two)

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
        return (x, self.y + r * math.cos(a), r * math.sin(a))

    def band(self, xa, ra, xb, rb, region, facing=None):
        """A band of quads round the axis between (xa, ra) and (xb, rb), facing away from the axis
        (or along `facing`, for a flat step)."""
        s0, t0, s1, t1 = self.regions[region]
        for k in range(SIDES):
            pts = [self.ring_point(xa, ra, k), self.ring_point(xa, ra, k + 1), self.ring_point(xb, rb, k + 1),
                   self.ring_point(xb, rb, k)]
            n = norm(cross(sub(pts[1], pts[0]), sub(pts[3], pts[0])))
            centre = mul(add(add(pts[0], pts[1]), add(pts[2], pts[3])), 0.25)
            want = facing if facing else (0.0, centre[1] - self.y, centre[2])
            if dot(n, want) < 0.0:
                n = mul(n, -1.0)
            # Along the axis down the region's t, round it across its s.
            st = [(s0 + 1, t0 + 1), (s1 - 1, t0 + 1), (s1 - 1, t1 - 1), (s0 + 1, t1 - 1)]
            i = [self.vert(p, n, uv) for p, uv in zip(pts, st)]
            self.tri(i[0], i[1], i[2], n)
            self.tri(i[0], i[2], i[3], n)

    def disc(self, x, r, facing, region, sink=0.0):
        """A flat octagon across the axis at `x`, facing +x (1) or -x (-1), the region's picture on it; `sink`: its
        middle that far back into the shell (a shallow cone: the star crimp's)."""
        n = (float(facing), 0.0, 0.0)
        s0, t0, s1, t1 = self.regions[region]
        cs, ct = (s0 + s1) / 2, (t0 + t1) / 2
        centre = self.vert((x - facing * sink, self.y, 0.0), n, (cs, ct))
        ring = []
        for k in range(SIDES):
            a = 2 * math.pi * (k + 0.5) / SIDES
            y, z = math.cos(a), math.sin(a)
            ring.append(self.vert((x, self.y + r * y, r * z), n, (cs + y * (s1 - s0 - 2) / 2, ct + z * (t1 - t0 - 2) / 2)))
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


def build_live(b):
    """An unfired shell into the builder `b` (at its sideways place): the spent one's head and hull, the hull straight
    to its end, rounding in a little where the star crimp's folds turn over, and the crimp closing it."""
    front = (1.0, 0.0, 0.0)
    b.disc(BACK, RIM_R, -1, "base")
    b.band(BACK, RIM_R, RIM_END, RIM_R, "brass")
    b.band(RIM_END, RIM_R, RIM_END, HEAD_R, "brass", front)
    b.band(RIM_END, HEAD_R, HEAD_END, HEAD_R, "brass")
    b.band(HEAD_END, HEAD_R, HEAD_END, HULL_R, "brass", front)
    b.band(HEAD_END, HULL_R, CRIMP, HULL_R, "hull")
    b.band(CRIMP, HULL_R, FRONT, HULL_R * 0.93, "hull")
    b.disc(FRONT, HULL_R * 0.93, 1, "crimp", CRIMP_SINK)


def build_single():
    b = Builder(LIVE_W, LIVE_H, LIVE_REGIONS)
    build_live(b)
    return b.mesh


def pair_outline(r):
    """The outline round both shells of a pair (their axes HULL_R either side of y 0) at radius `r` from each axis:
    the +y shell's far half-octagon, then the -y shell's, as (y, z) points going round."""
    pts = []
    for centre, start in ((HULL_R, -math.pi / 2), (-HULL_R, math.pi / 2)):
        for k in range(SIDES // 2 + 1):
            a = start + math.pi * k / (SIDES // 2)
            pts.append((centre + r * math.cos(a), r * math.sin(a)))
    return pts


def build_pair():
    """Two shells touching side by side, and the tape round both from TAPE_X - TAPE_HALF to TAPE_X + TAPE_HALF: its
    outside a little out of the hulls, its two edges closed down to them."""
    b = Builder(LIVE_W, LIVE_H, LIVE_REGIONS, HULL_R)
    build_live(b)
    b.y = -HULL_R
    build_live(b)
    outside, inside = pair_outline(HULL_R + TAPE_T), pair_outline(HULL_R)
    x0, x1 = TAPE_X - TAPE_HALF, TAPE_X + TAPE_HALF
    s0, t0, s1, t1 = LIVE_REGIONS["tape"]
    n = len(outside)
    lengths = [math.hypot(outside[(i + 1) % n][0] - outside[i][0], outside[(i + 1) % n][1] - outside[i][1])
               for i in range(n)]
    run = 0.0
    for i in range(n):
        j = (i + 1) % n
        (ya, za), (yb, zb) = outside[i], outside[j]
        ua = s0 + 1 + run / sum(lengths) * (s1 - s0 - 2)
        run += lengths[i]
        ub = s0 + 1 + run / sum(lengths) * (s1 - s0 - 2)
        out = norm((0.0, (ya + yb) / 2 - (HULL_R if (ya + yb) > 0 else -HULL_R) * (abs(ya + yb) > 1e-9), (za + zb) / 2))
        ids = [b.vert(p, out, uv) for p, uv in zip(
            [(x0, ya, za), (x0, yb, zb), (x1, yb, zb), (x1, ya, za)],
            [(ua, t0 + 1), (ub, t0 + 1), (ub, t1 - 2), (ua, t1 - 2)])]
        b.tri(ids[0], ids[1], ids[2], out)
        b.tri(ids[0], ids[2], ids[3], out)
    for x, facing in ((x0, -1.0), (x1, 1.0)):
        nrm = (facing, 0.0, 0.0)
        for i in range(n):
            j = (i + 1) % n
            ids = [b.vert((x, y, z), nrm, (s0 + 2, t1 - 2)) for y, z in (outside[i], outside[j], inside[j], inside[i])]
            b.tri(ids[0], ids[1], ids[2], nrm)
            b.tri(ids[0], ids[2], ids[3], nrm)
    return b.mesh


def paint_live():
    """The live shells' skin: the spent shell's hull, brass and base as they are painted there, the crimp (a red star
    of six folds, each fold's edge a shade darker, its middle pinched shut) and the tape (grey cloth tape, its weave in
    faint streaks along it, its edges a shade darker, a little grime)."""
    spent = paint()
    rng = random.Random(2026)
    px = bytearray(LIVE_W * LIVE_H)
    c0, ct0, c1, ct1 = LIVE_REGIONS["crimp"]
    g0, gt0, g1, gt1 = LIVE_REGIONS["tape"]
    for t in range(LIVE_H):
        for s in range(LIVE_W):
            k = rng.random()
            if c0 <= s < c1 and ct0 <= t < ct1:
                u, v = (s - c0 + 0.5) / (c1 - c0) - 0.5, (t - ct0 + 0.5) / (ct1 - ct0) - 0.5
                d = math.hypot(u, v) * 2
                a = (math.atan2(v, u) / (2 * math.pi) * 6) % 1.0  # six folds round it
                if d < 0.14:
                    shade = 72
                elif a < 0.14 or a > 0.94:
                    shade = 73 if k < 0.7 else 72  # a fold's edge
                else:
                    shade = (77 if k < 0.6 else 78 if k < 0.85 else 76) - (1 if d > 0.85 else 0)
                px[t * LIVE_W + s] = shade
            elif s < SKIN_W:
                px[t * LIVE_W + s] = spent[t * SKIN_W + s]
            else:
                v = (t - gt0 + 0.5) / (gt1 - gt0)
                streak = (t * 7 + (s // 5) * 3) % 4
                shade = 9 if streak == 0 else 8 if streak < 3 else 10
                if v < 0.08 or v > 0.92:
                    shade -= 2
                if k < 0.06:
                    shade -= 2
                px[t * LIVE_W + s] = shade
    return bytes(px)


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


def paint_spent():
    """The fired shell lying about (vr_shells.cpp), made plain apart from a live one (the author's note, 2026-10-07): its
    hull a darker, duller red with scuffs and soot from the mouth down, its brass dulled and smudged, its primer dented
    and blackened. The live shells keep paint()'s fresh red and bright brass."""
    rng = random.Random(1913)
    px = bytearray(paint())
    for t in range(SKIN_H):
        for s in range(SKIN_W):
            region = next(r for r, (s0, t0, s1, t1) in REGIONS.items() if s0 <= s < s1 and t0 <= t < t1)
            s0, t0, s1, t1 = REGIONS[region]
            u, v = (s - s0 + 0.5) / (s1 - s0), (t - t0 + 0.5) / (t1 - t0)
            k = rng.random()
            i = t * SKIN_W + s
            if region == "hull":
                shade = 71 if k < 0.5 else 72 if k < 0.8 else 70
                if int(u * 6) % 2 == 0:
                    shade -= 1
                if k > 0.975:
                    shade = 6  # a scuff
                if v > 0.86 and k < 0.5:
                    shade = 3 if v > 0.93 else 4  # soot from the mouth
                px[i] = shade
            elif region == "brass":
                px[i] = 26 if k < 0.5 else 27 if k < 0.85 else 25 if k < 0.95 else 4  # dull, smudged
            elif region == "base":
                d = math.hypot(u - 0.5, v - 0.5) * 2
                if d < 0.12:
                    px[i] = 2       # the firing pin's dent
                elif d < 0.28:
                    px[i] = 5 if k < 0.6 else 4
                elif d < 0.38:
                    px[i] = 2
                else:
                    px[i] = 27 if k < 0.5 else 26 if k < 0.85 else 3
    return bytes(px)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    game = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr")

    mesh = build()
    path = os.path.join(game, "progs", "vr_shell.mdl")
    # The files edited by hand since this wrote them are not overwritten (genguard.py: --keep-edited, --force).
    live_path = os.path.join(game, "progs", "vr_shell_live.mdl")
    pair_path = os.path.join(game, "progs", "vr_shell_pair.mdl")
    guard = genguard.Guard("make_shell.py", [path, live_path, pair_path])
    mdlgen.write_mdl(path, mesh, [paint_spent()], "shell")
    print("vr_shell.mdl: %d vertices, %d triangles -> %s" % (len(mesh.verts), len(mesh.tris), os.path.normpath(path)))
    print("  %.2f units long, rim radius %.3f units" % (LENGTH * UNITS, RIM_R * UNITS))
    live = paint_live()
    for p, m, name in ((live_path, build_single(), "shell_live"), (pair_path, build_pair(), "shell_pair")):
        mdlgen.write_mdl(p, m, [live], name)
        print("%s: %d vertices, %d triangles -> %s" % (os.path.basename(p), len(m.verts), len(m.tris),
                                                       os.path.normpath(p)))
    guard.finish()


if __name__ == "__main__":
    main()
