#!/usr/bin/env python3
# make_bayonet.py -- the bayonet (quakevr/progs/bayonet.mdl): an attachment for the shotgun and the super shotgun, and a
# small knife-like prop on its own; built here from scratch (an original design: no model or texture is copied).
#
# A socket bayonet, Quake's way (few facets, chunky, dark steel): a short octagonal socket that clips under a barrel (a
# lug on its top, a steel band at each end, a leather strap wound between), a cranked shank that drops from the socket's
# front to the blade's line (leather-wrapped: the grip when it is held alone), a small steel guard, and a single-edged
# blade (20 cm: a little shorter than the dagger's 27) with a straight spine, a fuller on each flat and a clipped point,
# its edge down (away from the barrel), its tip ahead of the muzzle.
#
# Its own size: one unit is a world unit (26.25 a metre), so on its own it is drawn as it is (a prop's Size 1). Axes:
# +x along the blade (forward), +y left, +z up. The origin is the mounting point: the top of the socket's front end,
# where it meets the barrel's underside at the muzzle. On a gun it is drawn in the gun's model space at the gun's
# Mounts below (position, angles, scale: a view model's scale is 1 / its weapons.txt scale, so the bayonet is its real
# size beside the gun as the game draws it); docs/vr-port/ATTACHMENTS.md, "The bayonet", lists them.
#
# Lofts of rings, each facet its own strip of vertices (flat across, smooth along: the id models' facets): the socket
# (8 sides), the shank (8), the blade (5 points: the spine's two corners, the flats' two ridges, the edge; the tip one
# point); the guard and the lug are boxes. The skin (256 x 128): the socket's and the shank's lofts unrolled along rows
# 2..62, the blade's along rows 68..124, the guard, the lug and the end faces from patches. What the engine draws is
# its full-colour skin, progs/bayonet.mdl_0.png (FULL, 4, times the size: 1024 x 512; paint_full()); the 8-bit skin is
# that one in Quake's palette. Its normal map (bayonet.mdl_0_norm.png) is baked from relief() by bake_normals.py.
#
# Usage: python Misc/quakevr/make_bayonet.py [output progs folder] [--placed folder] [--keep-edited | --force]
#   --placed folder: also writes, there, the bayonet moved into each gun's model space at its mount
#   (bayonet_on_<gun>.mdl), to draw beside the gun (Misc/quakevr/blender/render_views.py gun.mdl+bayonet_on_gun.mdl).
#   Prints the hold's numbers (props.txt) and the dagger-style blade anchors.

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import genguard  # noqa: E402
import mdlgen  # noqa: E402
from mdlgen import add, cross, dot, mul, norm, sub  # noqa: E402

SKIN_W, SKIN_H = 256, 128

# The socket: an octagon (flats up, down and at the sides) round x, its top flat at z = 0 (the band's 0.03 over it).
SOCKET_X = (-0.9, 0.0)
SOCKET_Z = -0.4                                  # its axis
SOCKET = [(-0.9, 0.43), (-0.78, 0.43), (-0.77, 0.41), (-0.13, 0.41), (-0.12, 0.43), (0.0, 0.43)]  # (x, apothem)
BANDS = (-0.775, -0.125)                         # the leather strap between the steel bands
# The shank: from inside the socket's front down to the guard (x, axis z, apothem); leather between WRAP's ends.
SHANK = [(-0.2, -0.5, 0.25), (0.2, -0.66, 0.27), (0.45, -0.86, 0.29), (1.17, -0.94, 0.29)]
WRAP = (0.32, 1.06)
# The guard and the lug: boxes (centre, half sizes).
GUARD = ((1.225, 0.0, -0.99), (0.075, 0.22, 0.43))
LUG = ((-0.47, 0.0, 0.05), (0.15, 0.09, 0.07))
# The blade (x, spine z, edge z, half thickness): a straight spine, the edge rising to a clipped point.
BLADE = [(1.2, -0.62, -1.32, 0.085), (1.6, -0.62, -1.32, 0.08), (3.6, -0.63, -1.29, 0.07), (5.0, -0.65, -1.2, 0.06),
         (5.8, -0.7, -1.05, 0.045), (6.25, -0.76, -0.9, 0.03), (6.5, -0.82, -0.82, 0.0)]
RIDGE = 0.45                                     # the flats' ridge (where the edge's bevel starts): down from the spine
TIP = (6.5, 0.0, -0.82)
FULLER = (0.16, 0.34, 1.7, 5.3)                  # the groove: from/to (shares down from the spine), from/to x

OCT = [(math.cos(math.radians(22.5 + 45 * k)), math.sin(math.radians(22.5 + 45 * k))) for k in range(8)]
OCT_R = 1.0 / math.cos(math.radians(22.5))       # vertex radius per unit of apothem

# The skin's layout (texels).
TPU = 28.0                                       # the socket's and shank's texels a unit along
SOCK_S0, SHANK_S0 = 4, 40
T_ROUND = (2, 62)
BLADE_S0, BPU = 4, 40.0                          # the blade's s at its root, texels a unit
T_BLADE = (68, 124)
GUARD_P = (220, 68, 252, 96)
LUG_P = (220, 100, 252, 124)
HOLE_P = (88, 4, 104, 20)                        # the socket's open back (dark)
CAP_P = (108, 4, 124, 20)                        # the other end faces (steel)

# Palette ramps (Quake's; dark to light).
LEATHER = [16, 175, 174, 17, 173, 18, 19, 172, 20, 171]
IRON = [0, 1, 2, 3, 4, 5, 6, 7, 8]
STEEL = [2, 3, 4, 34, 5, 35, 6, 36, 7, 37, 8, 9, 10, 11]

# Where it goes on each gun (docs/vr-port/ATTACHMENTS.md): the model, its mount in the gun's model space (position,
# angles pitch yaw roll in degrees, scale), and why there. Measured from frame 0 (the guns' idle pose).
VIEW_SCALE = {"v_shot.mdl": 0.44, "v_shot2.mdl": 0.48}   # quakevr/data/weapons.txt slots 1 and 2
MOUNTS = [
    # The shotgun: under its barrel at the muzzle (barrel's underside z 2.81..2.87 from x 31 to 32.9; the crown at
    # 33.07), the socket's back (x 30.85) clear of the pump's front face (x <= 30.8 below the barrel).
    ("v_shot.mdl", (32.9, 0.0, 2.84), (0.0, 0.0, 0.0), 1.0 / 0.44),
    # The super shotgun: under the barrels' shroud, between and below the two barrels, just behind the shroud's
    # chamfered front (its underside z 3.52 at x 24 to 3.72 at x 26, then up to 4.89 at the muzzle, x 28.5): the socket
    # 0.1 into it at its front, 0.1 below it at its back.
    ("v_shot2.mdl", (26.0, 0.0, 3.62), (0.0, 0.0, 0.0), 1.0 / 0.48),
    # The world models (pickups), scaled as the view models' bayonet to the gun (gun length over view gun length, over
    # the view scale): the shotgun's (g_shot0.mdl) under its barrel at the muzzle (underside z 33.79 from x 16.7 to 17.9;
    # the crown at 18.4); the super shotgun's (g_shot.mdl) under its two barrels where they stand out of the shroud
    # (barrels' undersides z 34.84 at y +-1.2, from x 19.6 to the muzzle at 21.8), the socket's back in the shroud's front.
    ("g_shot0.mdl", (18.2, 0.0, 33.79), (0.0, 0.0, 0.0), (33.01 / 32.55) / 0.44),
    ("g_shot.mdl", (21.6, 0.0, 34.86), (0.0, 0.0, 0.0), (39.44 / 27.95) / 0.48),
    # bayomounts (2026-11-28; measured on the models as the engine draws them, vr_attach_mdl_dump: the underside on the
    # barrel's line at the front, the socket's length clear behind; weapons.txt att1_*, vr_attach.cpp pickupRails):
    ("v_nail.mdl", (20.0, -1.48, 2.14), (0.0, 0.0, 0.0), 1.0 / 0.48),     # under the shroud's front, the barrels' line
    ("v_rock.mdl", (29.2, 0.0, 0.55), (0.0, 0.0, 0.0), 1.0 / 0.38),      # the GL: under the body's front (lip too short)
    ("v_prox.mdl", (29.2, 0.0, 0.58), (0.0, 0.0, 0.0), 1.0 / 0.38),      # likewise
    ("v_rock2.mdl", (50.6, 0.0, -1.95), (0.0, 0.0, 0.0), 1.0 / 0.34),    # the RL: where the nose's bevel starts
    ("v_light.mdl", (33.4, 0.0, 7.55), (0.0, 0.0, 0.0), 1.0 / 0.35),     # under the barrels at the muzzle
    ("v_plasma.mdl#vr", (30.3, 0.0, 2.62), (0.0, 0.0, 0.0), 1.0 / 0.35), # (made in the engine) under the body's front
    ("v_gruntgun.mdl", (32.85, 0.0, 6.74), (0.0, 0.0, 0.0), 1.0 / 0.39), # the burst rifle (and its #adv variants)
    ("v_enfrifle.mdl", (40.6, 0.0, 4.28), (0.0, 0.0, 0.0), 1.0 / 0.4),
    ("v_revolver.mdl", (20.26, 0.0, 5.56), (0.0, 0.0, 0.0), 1.0 / 0.45),
    ("g_nail.mdl", (13.33, -3.0, 34.24), (0.0, 0.0, 0.0), (26.61 / 26.01) / 0.48),
    ("g_rock.mdl", (17.3, 0.0, 31.21), (0.0, 0.0, 0.0), (30.75 / 38.4) / 0.38),
    ("g_prox.mdl", (17.4, 0.0, 27.97), (0.0, 0.0, 0.0), (30.8 / 38.4) / 0.38),
    ("g_rock2.mdl", (21.9, 0.0, 28.39), (0.0, 0.0, 0.0), (47.5 / 62.11) / 0.34),
    ("g_light.mdl", (20.3, 0.0, 34.63), (0.0, 0.0, 0.0), (42.74 / 40.28) / 0.35),
]


def ring_t(ring, k, t0, t1):
    """The skin's row of ring point k (its share of the ring's perimeter)."""
    n = len(ring)
    lengths = [math.dist(ring[i], ring[(i + 1) % n]) for i in range(n)]
    return t0 + (t1 - t0) * sum(lengths[:k]) / sum(lengths)


class Out:
    def __init__(self):
        self.mesh = mdlgen.Mesh(SKIN_W, SKIN_H, {})
        self.normals = []

    def vert(self, p, st):
        self.mesh.verts.append((p, (0.0, 0.0, 1.0), (int(round(st[0])), int(round(st[1])))))
        self.normals.append((0.0, 0.0, 0.0))
        return len(self.mesh.verts) - 1

    def tri(self, a, b, c, outward):
        """Clockwise seen from outside (Quake's front faces); degenerate ones dropped."""
        p0, p1, p2 = (self.mesh.verts[i][0] for i in (a, b, c))
        n = cross(sub(p1, p0), sub(p2, p0))
        if dot(n, n) < 1e-10:
            return
        if dot(n, outward) > 0:
            b, c = c, b
            n = mul(n, -1.0)
        self.mesh.tris.append((a, b, c))
        for i in (a, b, c):
            self.normals[i] = add(self.normals[i], mul(n, -1.0))

    def loft(self, stations, s_of, t_rows, caps):
        """`stations`: (centre, ring of 3D points) along x; joined facet by facet, each facet its own strip. `s_of(x)`
        the skin's s; `t_rows` the rows round. `caps`: (first, last) end faces' patches (None: open)."""
        n = len(stations[0][1])
        ring2 = [(p[1], p[2]) for p in stations[0][1]]
        for f in range(n):
            g = (f + 1) % n
            ta, tb = ring_t(ring2, f, *t_rows), ring_t(ring2, f + 1, *t_rows)
            col = [(self.vert(r[f], (s_of(r[f][0]), ta)), self.vert(r[g], (s_of(r[g][0]), tb))) for _, r in stations]
            for i in range(len(stations) - 1):
                a0, b0 = col[i]
                a1, b1 = col[i + 1]
                (c0, r0), (c1, r1) = stations[i], stations[i + 1]
                mid = mul(add(add(r0[f], r0[g]), add(r1[f], r1[g])), 0.25)
                outward = sub(mid, mul(add(c0, c1), 0.5))
                self.tri(a0, b0, b1, outward)
                self.tri(a0, b1, a1, outward)
        for (c, r), patch, sign in ((stations[0][0], stations[0][1]), caps[0], -1.0), \
                                   ((stations[-1][0], stations[-1][1]), caps[1], 1.0):
            if patch is None:
                continue
            s0, t0, s1, t1 = patch
            ids = [self.vert(p, ((s0 + s1) / 2 + (s1 - s0 - 2) / 2 * (p[1] - c[1]) / 0.6,
                                 (t0 + t1) / 2 - (t1 - t0 - 2) / 2 * (p[2] - c[2]) / 0.6)) for p in r]
            for k in range(1, n - 1):
                self.tri(ids[0], ids[k], ids[k + 1], (sign, 0.0, 0.0))

    def box(self, centre, half, patch):
        """A box, each face its own four vertices, all from `patch`."""
        s0, t0, s1, t1 = patch
        cx, cy, cz = centre
        for axis in range(3):
            for sign in (-1.0, 1.0):
                u, v = [k for k in range(3) if k != axis]
                corners = []
                for du, dv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                    p = [0.0, 0.0, 0.0]
                    p[axis] = centre[axis] + sign * half[axis]
                    p[u] = centre[u] + du * half[u]
                    p[v] = centre[v] + dv * half[v]
                    corners.append((tuple(p), (s0 + 1 + (du + 1) / 2 * (s1 - s0 - 2), t0 + 1 + (1 - dv) / 2 * (t1 - t0 - 2))))
                ids = [self.vert(p, st) for p, st in corners]
                outward = [0.0, 0.0, 0.0]
                outward[axis] = sign
                self.tri(ids[0], ids[1], ids[2], tuple(outward))
                self.tri(ids[0], ids[2], ids[3], tuple(outward))
        del cx, cy, cz


def oct_ring(x, zc, apothem):
    r = apothem * OCT_R
    return (x, 0.0, zc), [(x, r * c, zc + r * s) for c, s in OCT]


def blade_ring(x, spine, edge, ht):
    ridge = spine + (edge - spine) * RIDGE
    centre = (x, 0.0, (spine + edge) / 2)
    return centre, [(x, ht, spine), (x, ht, ridge), (x, 0.0, edge), (x, -ht, ridge), (x, -ht, spine)]


def shank_s(x):
    return SHANK_S0 + (x - SHANK[0][0]) * TPU


def build():
    out = Out()
    out.loft([oct_ring(x, SOCKET_Z, a) for x, a in SOCKET], lambda x: SOCK_S0 + (x - SOCKET_X[0]) * TPU, T_ROUND,
             (HOLE_P, CAP_P))
    out.loft([oct_ring(x, z, a) for x, z, a in SHANK], shank_s, T_ROUND, (CAP_P, CAP_P))
    out.loft([blade_ring(*b) for b in BLADE], lambda x: BLADE_S0 + (x - BLADE[0][0]) * BPU, T_BLADE, (CAP_P, None))
    out.box(*GUARD, GUARD_P)
    out.box(*LUG, LUG_P)
    out.mesh.verts = [(p, norm(n) if dot(n, n) > 1e-12 else (0.0, 0.0, 1.0), uv)
                      for (p, _, uv), n in zip(out.mesh.verts, out.normals)]
    return out.mesh


# ----------------------------------------------------------------------------
# The bayonet's mount (bayomounts, his 2026-11-28: "a small/subtle mounting rail/hinge ... in the place where the attachment
# takes place, so that the user is aware"): progs/bayonet_mount.mdl, drawn by the engine on every gun a bayonet fits, at
# its bayonet mount (attach::railMount), always. In the bayonet's own space (its origin the bayonet's mounting point, the
# barrel's underside at the muzzle): a short iron rail along under the barrel behind the socket (x -2.15 .. -0.95, its top
# flush with the barrel's underside), and a ring round the socket's axis just behind where the socket's back sits (x
# -1.12 .. -0.96: the socket slides up to it), hanging under the barrel. The bayonet's skin (its iron bands and patches).

RAIL = ((-1.55, 0.0, -0.1), (0.6, 0.2, 0.1))
RING_X = (-1.12, -0.96)
RING_APOTHEM = (0.47, 0.56)                      # inner, outer (round the socket's axis, SOCKET_Z)


def build_mount():
    out = Out()
    out.box(*RAIL, LUG_P)
    rings = {}
    for which, a in (("in", RING_APOTHEM[0]), ("out", RING_APOTHEM[1])):
        rings[which] = [oct_ring(x, SOCKET_Z, a) for x in RING_X]
    # the outer face (facing out) and the inner (facing the axis), each facet its own strip, on the socket's iron band
    for which, sign in (("out", 1.0), ("in", -1.0)):
        (c0, r0), (c1, r1) = rings[which]
        for f in range(8):
            g = (f + 1) % 8
            ta, tb = T_ROUND[0] + (T_ROUND[1] - T_ROUND[0]) * f / 8, T_ROUND[0] + (T_ROUND[1] - T_ROUND[0]) * (f + 1) / 8
            a0, b0 = out.vert(r0[f], (4.5, ta)), out.vert(r0[g], (4.5, tb))
            a1, b1 = out.vert(r1[f], (7.0, ta)), out.vert(r1[g], (7.0, tb))
            mid = mul(add(add(r0[f], r0[g]), add(r1[f], r1[g])), 0.25)
            outward = mul(sub(mid, mul(add(c0, c1), 0.5)), sign)
            out.tri(a0, b0, b1, outward)
            out.tri(a0, b1, a1, outward)
    # the two end faces (annuli), from the caps' patch
    s0, t0, s1, t1 = CAP_P
    for k, sign in ((0, -1.0), (1, 1.0)):
        ri, ro = rings["in"][k][1], rings["out"][k][1]
        for f in range(8):
            g = (f + 1) % 8
            ids = [out.vert(p, ((s0 + s1) / 2 + (s1 - s0 - 2) / 2 * p[1] / 0.6,
                                (t0 + t1) / 2 - (t1 - t0 - 2) / 2 * (p[2] - SOCKET_Z) / 0.6))
                   for p in (ri[f], ro[f], ro[g], ri[g])]
            out.tri(ids[0], ids[1], ids[2], (sign, 0.0, 0.0))
            out.tri(ids[0], ids[2], ids[3], (sign, 0.0, 0.0))
    out.mesh.verts = [(p, norm(n) if dot(n, n) > 1e-12 else (0.0, 0.0, 1.0), uv)
                      for (p, _, uv), n in zip(out.mesh.verts, out.normals)]
    return out.mesh


# ----------------------------------------------------------------------------
# The skin: full colour (FULL times the 8-bit skin) and its relief (the normal map's: normaltiles.py)

FULL = 4
DEPTH = 2.0


def paint_full(k=FULL):
    import numpy as np
    import make_debris as md
    ys, xs = np.mgrid[0:SKIN_H * k, 0:SKIN_W * k]
    s = (xs + 0.5) / k - 0.5
    t = (ys + 0.5) / k - 0.5

    def inside(p, pad=1.0):
        return (s >= p[0] - pad) & (s < p[2] + pad) & (t >= p[1] - pad) & (t < p[3] + pad)

    def q(cell, stretch=1.0):
        return np.stack([s / (cell * stretch), t / cell, (s + t) * 0.0], -1)

    def ramp(rr, x):
        return md.ramp_rgb(rr, np.clip(x, 0, 1))

    round_rows = (t < T_ROUND[1] + 1)
    blade_rows = (t >= T_BLADE[0] - 1) & (s < BLADE_S0 + (TIP[0] - BLADE[0][0]) * BPU + 2)
    sock = round_rows & (s < 34)
    shank = round_rows & (s >= 34) & (s < 80)
    xs_sock = (s - SOCK_S0) / TPU + SOCKET_X[0]
    xs_shank = (s - SHANK_S0) / TPU + SHANK[0][0]
    xb = (s - BLADE_S0) / BPU + BLADE[0][0]

    # Iron and steel (bands, guard, lug, caps): dark, hammered, pitted, a little brighter where handled.
    pits = md.smoothstep(md.vnoise(q(1.0), 21), 0.74, 0.9)
    lit_iron = 0.42 + 0.3 * (md.fbm(q(3.0), 23, 3) - 0.5) - 0.28 * pits
    c_iron = ramp(IRON, lit_iron)
    # The leather strap: wound on a slant, each turn's edge darker, worn lighter where gripped.
    turn = np.mod(s * 0.9 + (np.clip(t, *T_ROUND) - T_ROUND[0]) * 0.22, 6.0)
    lit_lea = 0.45 + 0.22 * (md.fbm(q(2.0), 13, 3) - 0.5) + 0.2 * md.smoothstep(md.vnoise(q(3.0), 15), 0.7, 0.9)
    lit_lea = np.where(turn < 0.8, lit_lea - 0.3, lit_lea)
    c_lea = ramp(LEATHER, lit_lea)
    leather = (sock & (xs_sock > BANDS[0]) & (xs_sock < BANDS[1])) | (shank & (xs_shank > WRAP[0]) & (xs_shank < WRAP[1]))
    rgb = np.where(leather[..., None], c_lea, c_iron)
    # The socket's open back: dark inside, a lighter rim.
    hole = inside(HOLE_P, 0.0)
    hs, ht = (HOLE_P[0] + HOLE_P[2]) / 2, (HOLE_P[1] + HOLE_P[3]) / 2
    rim = np.maximum(np.abs(s - hs), np.abs(t - ht)) > (HOLE_P[2] - HOLE_P[0]) / 2 - 2.5
    rgb = np.where((hole & ~rim)[..., None], ramp(IRON, 0.05 + 0.05 * md.vnoise(q(1.0), 5)), rgb)

    # The blade: forged steel; the edge (ring point 2) ground bright and worn, the spine (between points 4 and 0) dark,
    # a fuller sunk in each flat, the forging's scale in the hollows, a few nicks along the edge.
    ring = [(0.085, 0.0), (0.085, -RIDGE), (0.0, -1.0), (-0.085, -RIDGE), (-0.085, 0.0)]
    t_pts = [ring_t(ring, j, *T_BLADE) for j in range(6)]
    d_edge = np.abs(t - t_pts[2])
    ground = 1 - md.smoothstep(d_edge, 1.0, 5.0)
    near_spine = np.minimum(np.abs(t - t_pts[0]), np.abs(t - t_pts[5]))
    spine = 1 - md.smoothstep(near_spine, 0.5, 2.5)
    # The flats: rows between points 0-1 (left) and 3-4 (right); the fuller a share down from the spine.
    left = (t - t_pts[0]) / (t_pts[1] - t_pts[0])
    right = (t_pts[4] - t) / (t_pts[4] - t_pts[3])
    share = np.where(t < t_pts[2], left, right) * RIDGE
    fuller = ((share > FULLER[0]) & (share < FULLER[1]) & (xb > FULLER[2]) & (xb < FULLER[3])).astype(float)
    fuller_edge = fuller * (1 - md.smoothstep(np.minimum(np.abs(share - FULLER[0]), np.abs(share - FULLER[1])), 0.0, 0.05))
    scale = md.smoothstep(md.fbm(q(4.0, 2.0), 31, 3), 0.56, 0.7) * (1 - ground)
    nicks = md.smoothstep(md.vnoise(np.stack([s / 3.0, s * 0.0 + 7.0, s * 0.0], -1), 41), 0.8, 0.9) * ground
    lit_steel = (0.46 + 0.2 * (md.fbm(q(2.0, 6.0), 33, 3) - 0.5) + 0.4 * ground - 0.15 * spine - 0.14 * fuller
                 + 0.1 * fuller_edge - 0.22 * scale - 0.35 * nicks)
    c_blade = ramp(STEEL, lit_steel)
    rgb = np.where(blade_rows[..., None], c_blade, rgb)
    h_iron = -0.5 * pits
    h_lea = np.where(turn < 0.8, -0.4, 0.15 * (md.fbm(q(1.0), 17, 2) - 0.5))
    h = np.where(leather, h_lea, h_iron)
    h = np.where(blade_rows, -0.6 * fuller - 0.2 * scale - 0.3 * nicks, h) * DEPTH
    return np.clip(rgb, 0, 1), h


_relief = {}


def relief(name):
    """The bayonet's relief for its normal map (normaltiles.py): (heights in skin texels, 1, FULL)."""
    if name not in _relief:
        _relief[name] = (paint_full()[1], 1.0, FULL)
    return _relief[name]


# ----------------------------------------------------------------------------

def rotate(p, angles):
    """Quake's angles (pitch, yaw, roll; pitch up positive as an entity's model is drawn) applied to a point."""
    pitch, yaw, roll = (math.radians(a) for a in angles)
    x, y, z = p
    # roll about x, then pitch about y (nose up), then yaw about z
    y, z = y * math.cos(roll) - z * math.sin(roll), y * math.sin(roll) + z * math.cos(roll)
    x, z = x * math.cos(pitch) - z * math.sin(pitch), x * math.sin(pitch) + z * math.cos(pitch)
    x, y = x * math.cos(yaw) - y * math.sin(yaw), x * math.sin(yaw) + y * math.cos(yaw)
    return (x, y, z)


def placed(mesh, pos, angles, scale):
    out = mdlgen.Mesh(mesh.skin_w, mesh.skin_h, {})
    out.verts = [(add(pos, rotate(mul(p, scale), angles)), rotate(n, angles), uv) for p, n, uv in mesh.verts]
    out.tris = list(mesh.tris)
    return out


def main():
    args = sys.argv[1:]
    placed_dir = None
    if "--placed" in args:
        i = args.index("--placed")
        placed_dir = args[i + 1]
        del args[i:i + 2]
    args = [a for a in args if not a.startswith("--")]
    out_dir = args[0] if args else os.path.join(HERE, "..", "..", "quakevr", "progs")
    path = os.path.join(out_dir, "bayonet.mdl")
    mpath = os.path.join(out_dir, "bayonet_mount.mdl")
    guard = genguard.Guard("make_bayonet.py", [path, path + "_0.png", mpath, mpath + "_0.png"])
    mesh = build()
    mount_mesh = build_mount()
    bad = mesh.check_winding() + mount_mesh.check_winding()
    if bad:
        raise SystemExit("%d triangles wound the wrong way" % bad)
    import numpy as np
    import make_debris as md
    from PIL import Image
    rgb, _ = paint_full()
    skin = md.to_palette(rgb, SKIN_W, LEATHER + IRON + STEEL).tobytes()
    if path not in guard.kept:
        mdlgen.write_mdl(path, mesh, [skin], "bayonet")
    if path + "_0.png" not in guard.kept:
        Image.fromarray(np.round(rgb * 255).astype(np.uint8)).save(path + "_0.png", optimize=True)
    if mpath not in guard.kept:
        mdlgen.write_mdl(mpath, mount_mesh, [skin], "bayonet_mount")
    if mpath + "_0.png" not in guard.kept:
        Image.fromarray(np.round(rgb * 255).astype(np.uint8)).save(mpath + "_0.png", optimize=True)
    guard.finish()
    print("bayonet_mount.mdl: %d vertices, %d triangles -> %s" % (len(mount_mesh.verts), len(mount_mesh.tris),
                                                                   os.path.normpath(mpath)))
    lo = [min(v[0][k] for v in mesh.verts) for k in range(3)]
    hi = [max(v[0][k] for v in mesh.verts) for k in range(3)]
    print("bayonet.mdl: %d vertices, %d triangles -> %s" % (len(mesh.verts), len(mesh.tris), os.path.normpath(path)))
    print("  bounds %.2f %.2f %.2f .. %.2f %.2f %.2f (box middle %.2f %.2f %.2f)" % tuple(
        lo + hi + [(a + b) / 2 for a, b in zip(lo, hi)]))
    root = (BLADE[0][0], 0.0, (BLADE[0][1] + BLADE[0][2]) / 2)
    hand = (0.6, 0.0, SHANK[-1][1])
    print("  tip %.2f %.2f %.2f, blade root %.2f %.2f %.2f, butt %.2f 0 %.2f, handle x %.2f..%.2f, grip middle %.2f 0 %.2f"
          % (TIP + root + (SOCKET_X[0], SOCKET_Z, SOCKET_X[0], WRAP[1], hand[0], hand[2])))
    line = sub(TIP, hand)
    along = dot(sub(root, hand), line) / dot(line, line)
    print("  the blade from %.3f of the grip-to-tip line (bl_from); blade %.2f units (%.1f cm)" %
          (along, TIP[0] - BLADE[0][0], (TIP[0] - BLADE[0][0]) / 26.25 * 100))
    for gun, pos, angles, scale in MOUNTS:
        tip = add(pos, rotate(mul(TIP, scale), angles))
        print("  on %-12s at %.2f %.2f %.2f, angles %.0f %.0f %.0f, scale %.4f: its tip %.2f %.2f %.2f" %
              ((gun,) + pos + angles + (scale,) + tip))
        if placed_dir:
            mdlgen.write_mdl(os.path.join(placed_dir, "bayonet_on_" + gun), placed(mesh, pos, angles, scale), [skin],
                             "bayonet")


if __name__ == "__main__":
    main()
