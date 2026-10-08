#!/usr/bin/env python3
# make_rounds.py -- generates the launchers' rounds for immersive reloading (QC vr_reload.qc, "Front-loaded launchers";
# docs/vr-port/RELOAD_PLAN.md phases 3 and 4): taken from the ammo pouch, put butt first into the muzzle.
#   quakevr/progs/vr_round_rocket.mdl   the rocket launcher's rocket: a nozzle and four fins at its butt, an olive body
#                                       with a yellow band, a red nose cone. 30 cm long, 6 cm across the body (fits the
#                                       launcher's 8 cm tube). Skin 1: the multi-rocket (a dark body, red bands).
#   quakevr/progs/vr_round_grenade.mdl  the grenade launcher's grenade: a brass case with a rim and a primer at its butt,
#                                       an olive body, a rounded nose. 12 cm long, 9 cm across (the launcher's bore is
#                                       11). Skin 1: the multi-grenade (a red body). Skins 2 and 3: the two armed (QC
#                                       vr_grenade.qc VR_HandGrenade_Arm: the pouches' grenades are these): the stripe
#                                       round the body glowing, red (the grenade's, as Quake's grenade's band) or amber.
#   quakevr/progs/vr_round_prox.mdl     the proximity launcher's grenade: a dark steel ball with six short spikes and a
#                                       red lens band; 8 cm across (any way round goes in). Skin 1: armed, the lenses lit.
# Since 2026-10-08 only the rocket is used: the grenades are Quake's own models again (the author: "I want it to look
# exactly the same"; QC vr_grenade.qc VR_RELOAD_GRENADE, _MULTI, _PROX; ROUND21.md, "Grenades back to the original
# models"). vr_round_grenade.mdl and vr_round_prox.mdl stay in the repo, made here, to switch back: point those QC
# strings at them (VR_HandGrenade_Look then wants their skins: 1 multi, +2 armed; the prox 1 armed), make the ammo
# pouch with make_ammo_pouch.py --generated-rounds and empty vr_view.cpp's pouchGrenades.
#
# Usage: python Misc/quakevr/make_rounds.py [output game folder]
#
# Model space: Quake units at vr_world_scale 1 (1 m = 1 / 0.0381 units), drawn at Size 1 (the models are their own:
# missile.mdl, grenade.mdl and proxbomb.mdl stay as the projectiles are drawn). The rocket's and the grenade's axis
# along +x, their nose forward (+x), their butt back (-x): QC VR_Reload_RoundAxis; the origin at their middle.
# Palette indices from gfx/palette.lmp: olive 52..57, reds 66..76, yellows 193..196, brass 28..31 and 199, greys 0..15.

import math
import os
import random
import sys

import genguard
import mdlgen
from mdlgen import add, sub, mul, dot, cross, norm

UNITS = 1.0 / 0.0381
SIDES = 10

SKIN_W, SKIN_H = 64, 32
REGIONS = {
    "body": (0, 0, 16, 32), "nose": (16, 0, 32, 16), "band": (16, 16, 32, 32), "metal": (32, 0, 48, 16),
    "base": (32, 16, 48, 32), "brass": (48, 0, 64, 16), "dark": (48, 16, 64, 32),
}

# The rocket.
ROCKET_LEN = 0.30
ROCKET_R = 0.030           # the body,
ROCKET_NOZZLE_R = 0.020    # the nozzle's mouth at the butt,
ROCKET_FIN_R = 0.040       # how far out the fins reach (the tube: 4.1 cm),
ROCKET_FIN_LEN = 0.060     # their length along it,
ROCKET_FIN_T = 0.003       # and thickness.
ROCKET_NOSE_LEN = 0.075

# The grenade.
GREN_LEN = 0.12
GREN_R = 0.045
GREN_RIM_R = 0.046
GREN_CASE_END = 0.035      # the brass case's length from the butt,

# The proximity grenade.
PROX_R = 0.040
PROX_SPIKE_R = 0.007
PROX_SPIKE_LEN = 0.012


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

    def quad(self, pts, n, region):
        s0, t0, s1, t1 = REGIONS[region]
        st = [(s0 + 1, t0 + 1), (s1 - 1, t0 + 1), (s1 - 1, t1 - 1), (s0 + 1, t1 - 1)]
        i = [self.vert(p, n, uv) for p, uv in zip(pts, st)]
        self.tri(i[0], i[1], i[2], n)
        self.tri(i[0], i[2], i[3], n)

    @staticmethod
    def ring_point(x, r, k):
        a = 2 * math.pi * (k + 0.5) / SIDES
        return (x, r * math.cos(a), r * math.sin(a))

    def band(self, xa, ra, xb, rb, region, facing=None):
        """A band of quads round the x axis between (xa, ra) and (xb, rb), facing away from it (or along `facing`, for a
        flat step)."""
        s0, t0, s1, t1 = REGIONS[region]
        for k in range(SIDES):
            pts = [self.ring_point(xa, ra, k), self.ring_point(xa, ra, k + 1), self.ring_point(xb, rb, k + 1),
                   self.ring_point(xb, rb, k)]
            n = norm(cross(sub(pts[1], pts[0]), sub(pts[3], pts[0])))
            centre = mul(add(add(pts[0], pts[1]), add(pts[2], pts[3])), 0.25)
            want = (0.0, centre[1], centre[2])
            if abs(ra - rb) > abs(xa - xb) * 4:
                want = (1.0 if (rb < ra) == (xb > xa) else -1.0, 0.0, 0.0) if xa != xb else \
                    ((1.0 if ra > rb else -1.0), 0.0, 0.0)
            if dot(n, want) < 0.0:
                n = mul(n, -1.0)
            # Round it across the region's s, along it down its t.
            u0 = s0 + 1 + (s1 - s0 - 2) * k / SIDES
            u1 = s0 + 1 + (s1 - s0 - 2) * (k + 1) / SIDES
            st = [(u0, t0 + 1), (u1, t0 + 1), (u1, t1 - 1), (u0, t1 - 1)]
            i = [self.vert(p, n, uv) for p, uv in zip(pts, st)]
            self.tri(i[0], i[1], i[2], n)
            self.tri(i[0], i[2], i[3], n)

    def disc(self, x, r, facing, region):
        """A flat disc across the x axis at `x`, facing +x (1) or -x (-1), the region's picture on it."""
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

    def plate(self, corners, normal, region):
        """A thin flat plate both ways (a fin): `corners` its outline on one face, `normal` that face's way."""
        self.quad(corners, normal, region)
        self.quad(list(reversed(corners)), mul(normal, -1.0), region)


def build_rocket():
    b = Builder()
    back, front = -ROCKET_LEN / 2, ROCKET_LEN / 2
    # The butt: the nozzle's dark mouth, its flare out to the body.
    b.disc(back, ROCKET_NOZZLE_R, -1, "dark")
    b.band(back, ROCKET_NOZZLE_R, back + 0.018, ROCKET_R * 0.85, "metal")
    b.band(back + 0.018, ROCKET_R * 0.85, back + 0.018, ROCKET_R, "metal", (-1.0, 0.0, 0.0))
    # The body, its band, the nose cone.
    nose0 = front - ROCKET_NOSE_LEN
    b.band(back + 0.018, ROCKET_R, nose0 - 0.03, ROCKET_R, "body")
    b.band(nose0 - 0.03, ROCKET_R, nose0, ROCKET_R, "band")
    b.band(nose0, ROCKET_R, nose0 + ROCKET_NOSE_LEN * 0.55, ROCKET_R * 0.72, "nose")
    b.band(nose0 + ROCKET_NOSE_LEN * 0.55, ROCKET_R * 0.72, front - 0.006, ROCKET_R * 0.22, "nose")
    b.disc(front - 0.006, ROCKET_R * 0.22, 1, "nose")
    # Four fins at the butt, between the ring's sides (at 45 degrees off them: square to the body's flats).
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        ry, rz = math.cos(a), math.sin(a)
        ty, tz = -rz, ry  # across the fin
        x0, x1 = back + 0.010, back + 0.010 + ROCKET_FIN_LEN
        r0, r1 = ROCKET_R * 0.95, ROCKET_FIN_R
        off = ROCKET_FIN_T / 2
        pts = [(x0, ry * r0 + ty * off, rz * r0 + tz * off), (x1, ry * r0 + ty * off, rz * r0 + tz * off),
               (x1 - 0.025, ry * r1 + ty * off, rz * r1 + tz * off), (x0, ry * r1 + ty * off, rz * r1 + tz * off)]
        b.plate(pts, (0.0, ty, tz), "metal")
    return b.mesh


def build_grenade():
    b = Builder()
    back, front = -GREN_LEN / 2, GREN_LEN / 2
    # The butt: the case's base (its primer), the rim, the case.
    b.disc(back, GREN_RIM_R, -1, "base")
    b.band(back, GREN_RIM_R, back + 0.005, GREN_RIM_R, "brass")
    b.band(back + 0.005, GREN_RIM_R, back + 0.005, GREN_R * 0.97, "brass", (1.0, 0.0, 0.0))
    b.band(back + 0.005, GREN_R * 0.97, back + GREN_CASE_END, GREN_R * 0.97, "brass")
    b.band(back + GREN_CASE_END, GREN_R * 0.97, back + GREN_CASE_END, GREN_R, "band", (-1.0, 0.0, 0.0))
    # The body and its rounded nose.
    b.band(back + GREN_CASE_END, GREN_R, front - 0.040, GREN_R, "body")
    b.band(front - 0.040, GREN_R, front - 0.022, GREN_R * 0.88, "body")
    b.band(front - 0.022, GREN_R * 0.88, front - 0.009, GREN_R * 0.6, "nose")
    b.band(front - 0.009, GREN_R * 0.6, front, GREN_R * 0.22, "nose")
    b.disc(front, GREN_R * 0.22, 1, "nose")
    return b.mesh


def build_prox():
    b = Builder()
    # The ball: latitude bands from -x to +x (its lens band round the middle).
    rings = 8
    prev = None
    for i in range(rings + 1):
        a = math.pi * i / rings
        x, r = -PROX_R * math.cos(a), PROX_R * math.sin(a)
        if prev is not None:
            region = "band" if i == rings // 2 or i == rings // 2 + 1 else "body"
            if prev[1] < 1e-6:
                b.band(prev[0], 0.0005, x, r, region)
            elif r < 1e-6:
                b.band(prev[0], prev[1], x, 0.0005, region)
            else:
                b.band(prev[0], prev[1], x, r, region)
        prev = (x, r)
    # Six spikes: square pyramids along the axes.
    for axis in ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1)):
        u = (axis[1], axis[2], axis[0]) if axis[0] == 0 else (0.0, 1.0, 0.0)
        u = norm(cross(axis, u)) if abs(dot(u, axis)) > 0.5 else u
        v = cross(axis, u)
        base = mul(axis, PROX_R * 0.92)
        tip = mul(axis, PROX_R + PROX_SPIKE_LEN)
        corners = [add(base, add(mul(u, PROX_SPIKE_R * c), mul(v, PROX_SPIKE_R * s))) for c, s in
                   ((1, 0), (0, 1), (-1, 0), (0, -1))]
        s0, t0, s1, t1 = REGIONS["metal"]
        for k in range(4):
            p0, p1 = corners[k], corners[(k + 1) % 4]
            n = norm(add(mul(axis, 0.5), norm(add(p0, p1))))
            i = [b.vert(tip, n, ((s0 + s1) / 2, t0 + 1)), b.vert(p0, n, (s0 + 1, t1 - 1)), b.vert(p1, n, (s1 - 1, t1 - 1))]
            b.tri(i[0], i[1], i[2], n)
    return b.mesh


GREN_STRIPE = (0.30, 0.42)  # the grenade's stripe round its body (of the body region's length, from the case)


def paint(kind, special, armed=False):
    """The skin: `kind` rocket, grenade or prox; `special` the multi-rocket's or multi-grenade's; `armed` a pouch grenade
    armed (its stripe, or the proximity grenade's lenses, glowing: fullbright)."""
    rng = random.Random(1996 + len(kind) * 7 + special)
    px = bytearray()
    for t in range(SKIN_H):
        for s in range(SKIN_W):
            region = next(r for r, (s0, t0, s1, t1) in REGIONS.items() if s0 <= s < s1 and t0 <= t < t1)
            s0, t0, s1, t1 = REGIONS[region]
            u, v = (s - s0 + 0.5) / (s1 - s0), (t - t0 + 0.5) / (t1 - t0)
            k = rng.random()
            d = math.hypot(u - 0.5, v - 0.5) * 2
            if region == "body":
                if kind == "prox":
                    shade = 5 if k < 0.6 else 6 if k < 0.85 else 4  # dark steel
                elif special:
                    shade = (70 if k < 0.6 else 71 if k < 0.85 else 69) if kind == "grenade" else \
                        (3 if k < 0.6 else 4 if k < 0.85 else 2)  # a red multi-grenade, a dark multi-rocket
                else:
                    shade = 55 if k < 0.55 else 56 if k < 0.8 else 54  # olive drab
                if k > 0.97:
                    shade = 7  # a scuff
                if kind == "grenade" and GREN_STRIPE[0] <= v < GREN_STRIPE[1]:
                    # Its stripe: a yellow marking (the multi-grenade's dark), glowing once armed (red; amber).
                    if armed:
                        shade = (250 if k < 0.7 else 251) if not special else (235 if k < 0.7 else 236)
                    else:
                        shade = (193 if k < 0.6 else 194) if not special else (2 if k < 0.6 else 3)
                px.append(shade)
            elif region == "nose":
                if kind == "rocket":
                    px.append(73 if k < 0.6 else 74 if k < 0.85 else 72)
                else:
                    px.append(54 if k < 0.6 else 53 if k < 0.85 else 55)
            elif region == "band":
                if kind == "prox":
                    lens = 0.3 < u < 0.4 or 0.8 < u < 0.9
                    px.append((250 if armed else 70) if lens else 73)  # its lenses (lit, fullbright, once armed) on red
                elif special:
                    px.append(74 if k < 0.7 else 73)
                else:
                    px.append(195 if k < 0.6 else 194 if k < 0.85 else 196)
            elif region == "metal":
                px.append(8 if k < 0.5 else 9 if k < 0.8 else 7)
            elif region == "brass":
                px.append(30 if k < 0.5 else 31 if k < 0.75 else 199 if k < 0.92 else 28)
            elif region == "base":
                if d < 0.28:
                    px.append(10 if k < 0.6 else 9)  # the primer
                elif d < 0.38:
                    px.append(4)
                else:
                    px.append(30 if k < 0.5 else 31 if k < 0.9 else 28)
            else:  # dark: the nozzle's mouth, sooty
                px.append(1 if d < 0.7 else 3 if k < 0.6 else 4)
    return bytes(px)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    game = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr")
    # (name, mesh, its skins: (multi, armed) each)
    outputs = [("rocket", build_rocket(), [(0, False), (1, False)]),
               ("grenade", build_grenade(), [(0, False), (1, False), (0, True), (1, True)]),
               ("prox", build_prox(), [(0, False), (0, True)])]
    paths = [os.path.join(game, "progs", "vr_round_%s.mdl" % name) for name, _, _ in outputs]
    # The files edited by hand since this wrote them are not overwritten (genguard.py: --keep-edited, --force).
    guard = genguard.Guard("make_rounds.py", paths)
    for (name, mesh, skins), path in zip(outputs, paths):
        if path in guard.kept:
            continue
        mdlgen.write_mdl(path, mesh, [paint(name, special, armed) for special, armed in skins], "round_" + name)
        xs = [p[0][0] for p in mesh.verts]
        rs = [math.hypot(p[0][1], p[0][2]) for p in mesh.verts]
        print("vr_round_%s.mdl: %d vertices, %d triangles, %.2f units long, %.2f across -> %s" % (
            name, len(mesh.verts), len(mesh.tris), max(xs) - min(xs), 2 * max(rs), os.path.normpath(path)))
    guard.finish()


if __name__ == "__main__":
    main()
