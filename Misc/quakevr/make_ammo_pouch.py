#!/usr/bin/env python3
# make_ammo_pouch.py -- generates quakevr/progs/vrpouch_ammo.mdl, the ammo pouch on the front of the belt (immersive
# reloading, vr_reload_mode 3: vr_view.cpp setupAmmoPouch, QC vr_reload.qc; docs/vr-port/RELOAD_PLAN.md).
#
# make_pouch.py's leather pouch (the grenade pouch at the small of the back), made wider and shallower for the front of
# the belt, with no strap over its open top (a hand dips in and out of it all through a fight), and shotgun shells
# standing in it brass up in a loose row, their heads and a hand's width of red hull out of the rim, so that it reads as
# the shotgun's ammo when you look down. Two frames: 0 full (you have ammo for the gun in your other hand), 1 empty (the
# shells gone down out of sight, the front fallen in flat); the engine picks the frame. Since: frames by ammo and count
# (vr_view.cpp ammoPouchFrame): shells, magazines, cells, and the launchers' rounds (make_rounds.py's rockets, standing
# nose up, as many as the reserve has up to 3, spaced evenly about the middle; skin 1 the multi-rockets). The grenades'
# and proximity grenades' frames (17-24) are the full pouch with none in it: the engine draws Quake's own grenades
# standing in it (vr_view.cpp setupAmmoPouchGrenades: progs/grenade.mdl, mervup.mdl, proxbomb.mdl, the user's game's
# models, at vr_grenade_scale; ROUND21.md, "Grenades back to the original models"). --generated-rounds: make_rounds.py's
# grenades in those frames instead, as before 2026-10-08 (to switch back; the engine's then drawn too: its
# pouchGrenades table).
#
# Usage: python Misc/quakevr/make_ammo_pouch.py [output progs folder] [--generated-rounds]
#
# Model space as make_pouch.py's: +x out of the body (the back against it at x 0), +y to its left, +z up, world units at
# scale 1 (vr_ammo_pouch_scale scales it as drawn). The skin is make_pouch.py's (its leather, iron and stitching), its
# grenade regions repainted as the shells' hull and brass.

import math
import os
import sys

import genguard
import make_pouch as pouch
import mdlgen

# The front pouch's shape (make_pouch.py's globals, set before building: its functions read them).
pouch.HALF_W = 4.4
pouch.DEPTH = {0: 2.5, 1: 1.3}
pouch.Z_BOTTOM, pouch.Z_TOP = -2.3, 1.5
pouch.Z_FLOOR = 0.6
pouch.BULGE_P = 3.0

# The shells standing in it: their middles (y), their lean (degrees, about x), how far their tops stand out. A tidy row,
# mirror-symmetric about the middle (the author: they looked overly big and not symmetric; they were 0.52 across the
# hull, leaning and standing out at random): evenly spaced, fanning out a little to the sides, the middle one highest.
SHELLS = [(-1.9, -6.0, 1.0), (-0.95, -3.0, 1.12), (0.0, 0.0, 1.2), (0.95, 3.0, 1.12), (1.9, 6.0, 1.0)]
SHELL_R = 0.33   # the hull's radius, world units: a held shell's as drawn (make_shell.py's 0.99 cm at its 1.25 Size)
RIM_R = 0.37     # its rim's (1.12 cm)
BRASS = 0.55     # how much of the top is brass


def shell_upright(m, x, y, lean, top, bottom, full):
    """A shell standing brass up from `bottom` to `top`, leaning `lean` degrees sideways: an octagonal hull (the
    "gren" region: red below, brass at the top) and its head (the "grentop" region: the brass base, the primer)."""
    a = math.radians(lean)
    r = SHELL_R if full else 0.12

    def at(z, rr, k):
        c = (math.cos((k + 0.5) * math.pi / 4), math.sin((k + 0.5) * math.pi / 4))
        dz = z - bottom
        return (x + rr * c[0], y + rr * c[1] * math.cos(a) - dz * math.sin(a), bottom + dz * math.cos(a) + rr * c[1] * math.sin(a))

    rings = [[at(bottom, r, k) for k in range(8)], [at(top - BRASS * 0.15, r, k) for k in range(8)],
             [at(top - BRASS * 0.15, r * RIM_R / SHELL_R, k) for k in range(8)], [at(top, r * RIM_R / SHELL_R, k) for k in range(8)]]
    vs = (1.0, 0.08, 0.04, 0.0)
    axis_top = at(top, 0.0, 0)
    for i in range(3):
        for k in range(8):
            j = (k + 1) % 8
            p = [rings[i][k], rings[i][j], rings[i + 1][j], rings[i + 1][k]]
            c = mdlgen.mul(mdlgen.add(p[0], p[2]), 0.5)
            ax = at((c[2] - bottom) / max(1e-6, math.cos(a)) + bottom, 0.0, 0)
            outward = mdlgen.sub(c, ax) if i != 1 else mdlgen.sub(axis_top, at(bottom, 0.0, 0))
            uv = [pouch.region_uv("gren", u, v) for u, v in ((k / 8, vs[i]), ((k + 1) / 8, vs[i]),
                                                               ((k + 1) / 8, vs[i + 1]), (k / 8, vs[i + 1]))]
            m.face(p, uv, outward)
    m.face(rings[3], [pouch.region_uv("grentop", 0.5 + 0.45 * math.cos((k + 0.5) * math.pi / 4),
                                      0.5 + 0.45 * math.sin((k + 0.5) * math.pi / 4)) for k in range(8)],
           mdlgen.sub(axis_top, at(bottom, 0.0, 0)))


# frames, one more round in sight each (a shell, or a magazine: how many the reserve fills, part-filled counting).
KINDS = [  # (kind, its frames' first, how many it shows at most)
    (1, 1, 5),   # shells
    (2, 6, 3),   # nailgun magazines
    (3, 9, 2),   # super nailgun magazines
    (4, 11, 3),  # thunderbolt cells
    (5, 14, 3),  # rockets
    (6, 17, 4),  # grenades
    (7, 21, 4),  # proximity grenades
]
FRAMES = 25
SHELL_ORDER = [2, 1, 3, 0, 4]  # which of SHELLS show first (the middle out)
# The magazines standing in it, feed end up, from make_mags.py (their skins below the pouch's, 64 x 64 each).
MAGS = {2: ("nail", 0.66, (-2.45, 0.0, 2.45)), 3: ("snail", 0.5, (-1.75, 1.75)), 4: ("cell", 0.56, (-2.6, 0.0, 2.6))}
MAG_TOP = 2.9       # their tops (the pouch's rim at 1.5; lower, the nailgun's base plates show through its rounded bottom)
MAG_SKIN_T = 128    # the row the magazines' skins start at (each 64 wide: nail, snail, cell)


# The launchers' rounds standing in it, nose up (make_rounds.py's meshes; their skins in the skin's last rows, 64 x 32
# each): kind: (name, scale, its top, the gap between their middles, its skin's column).
ROUNDS = {5: ("rocket", 0.55, 3.5, 2.2, 0), 6: ("grenade", 0.75, 3.0, 1.95, 64), 7: ("prox", 0.7, 2.85, 2.0, 128)}
ROUND_SKIN_T = 192  # the row their skins start at
GENERATED_ROUNDS = "--generated-rounds" in sys.argv  # make_rounds.py's grenades baked in (as before 2026-10-08)
if GENERATED_ROUNDS:
    sys.argv.remove("--generated-rounds")


def round_meshes():
    import make_rounds
    return {"rocket": make_rounds.build_rocket(), "grenade": make_rounds.build_grenade(), "prox": make_rounds.build_prox()}


def frame_spec(frame):
    """(kind, count) a frame shows; (0, 0) the empty pouch."""
    for kind, first, most in KINDS:
        if first <= frame < first + most:
            return kind, frame - first + 1
    return 0, 0


def add_part(m, part, rot, at, s_off, t_off=MAG_SKIN_T):
    """`part`'s triangles (a make_mags.py or make_rounds.py mesh) into `m`: turned (rows: where its x, y, z go), scaled
    into place at `at`, its skin coordinates moved to its own square of the skin. The new vertices' range."""
    first = len(m.verts)
    for p, n, (s, t) in part.verts:
        q = mdlgen.add(mdlgen.add(mdlgen.add(mdlgen.mul(rot[0], p[0]), mdlgen.mul(rot[1], p[1])), mdlgen.mul(rot[2], p[2])), at)
        nn = mdlgen.norm(mdlgen.add(mdlgen.add(mdlgen.mul(rot[0], n[0]), mdlgen.mul(rot[1], n[1])), mdlgen.mul(rot[2], n[2])))
        m.verts.append((q, nn, (s + s_off, t + t_off)))
    m.tris.extend((a + first, b + first, c + first) for a, b, c in part.tris)
    return first, len(m.verts)


def collapse(m, rng, centre):
    """The vertices in `rng` drawn to a speck at `centre` (out of sight under the floor): a round not in the pouch."""
    for i in range(*rng):
        p, n, st = m.verts[i]
        m.verts[i] = (mdlgen.add(centre, mdlgen.mul(mdlgen.sub(p, centre), 0.05)), n, st)


def build(frame):
    kind, count = frame_spec(frame)
    full = count > 0
    depth = pouch.DEPTH[0 if full else 1]
    m = pouch.Mesh(pouch.SKIN_W, pouch.SKIN_H * 2, pouch.REGIONS)
    mid = pouch.body(m, depth, full)
    sx = pouch.BACK_X + pouch.DEPTH[0] * 0.5
    hidden = (sx, 0.0, pouch.Z_BOTTOM + 0.5)
    # The shells (as the first pouch had them).
    shown = set(SHELL_ORDER[:count]) if kind == 1 else set()
    for k, (y, lean, out) in enumerate(SHELLS):
        first = len(m.verts)
        shell_upright(m, sx, y, lean, pouch.Z_TOP + out, pouch.Z_FLOOR - 0.2, True)
        if k not in shown:
            collapse(m, (first, len(m.verts)), hidden)
    # The magazines: along the pouch's width (their x), their thickness across its depth.
    import make_mags
    parts = {2: make_mags.nail_mag(), 3: make_mags.snail_mag(), 4: make_mags.cell_mag()}
    for mk, (name, scale, ys) in MAGS.items():
        part, top = parts[mk]
        rot = ((0.0, scale, 0.0), (-scale, 0.0, 0.0), (0.0, 0.0, scale))
        for j, y in enumerate(ys):
            at = (sx, y, MAG_TOP - top * scale)
            rng = add_part(m, part, rot, at, {2: 0, 3: 64, 4: 128}[mk])
            if not (kind == mk and j < count):
                collapse(m, rng, hidden)
    # The launchers' rounds: nose up (their +x up), as many as this frame shows spaced evenly about the middle.
    meshes = round_meshes()
    for rk, (name, scale, top, gap, s_off) in ROUNDS.items():
        if rk != 5 and not GENERATED_ROUNDS:
            continue  # (the grenades: Quake's own, drawn by the engine)
        part = meshes[name]
        hi = max(p[0] for p, _, _ in part.verts)
        rot = ((0.0, 0.0, scale), (0.0, scale, 0.0), (-scale, 0.0, 0.0))
        most = next(k[2] for k in KINDS if k[0] == rk)
        n = count if kind == rk else 0
        for j in range(most):
            y = (j - (n - 1) / 2.0) * gap if j < n else 0.0
            rng = add_part(m, part, rot, (sx, y, top - hi * scale), s_off, ROUND_SKIN_T)
            if j >= n:
                collapse(m, rng, hidden)
    # Rivets at the front's top corners and a pair on its middle, where a belt loop is sewn on behind.
    for y in (-pouch.HALF_W * 0.82, pouch.HALF_W * 0.82):
        px = pouch.front_x(y, depth)
        pouch.stud(m, (px, y, pouch.Z_TOP - 0.4), (px - mid[0], y, 0.0), 0.2, 0.12)
    fx = pouch.front_x(0.0, depth) * (1.03 if full else 1.0)
    for z in (-0.2, -1.4):
        pouch.stud(m, (fx, 0.0, z), (1.0, 0.0, 0.0), 0.17, 0.1)
    return m


def paint_skin(variant=False):
    """make_pouch.py's skin, its grenade regions repainted: the hull's red (dark at the bottom, ribbed), the brass head
    (dull, a darker rim line) above it; the head's top brass with a grey primer in a dark ring."""
    px = bytearray(pouch.paint_skin())
    w = pouch.SKIN_W
    s0, t0, s1, t1 = pouch.REGIONS["gren"]
    for t in range(t0, t1):
        for s in range(s0, s1):
            v = (t - t0 + 0.5) / (t1 - t0)  # 0 at the top
            h = ((s * 7919 + t * 104729) % 97) / 97.0
            if v < 0.14:
                c = 30 if h < 0.5 else 31 if h < 0.8 else 199
                if 0.1 <= v:
                    c = 28
            else:
                c = 77 if h < 0.55 else 78 if h < 0.8 else 76
                if (s - s0) % 6 == 0:
                    c -= 2  # the hull's ribs
                if v > 0.8:
                    c -= 3  # down in the pouch's shadow
            px[t * w + s] = c
    s0, t0, s1, t1 = pouch.REGIONS["grentop"]
    for t in range(t0, t1):
        for s in range(s0, s1):
            u, v = (s - s0 + 0.5) / (s1 - s0) - 0.5, (t - t0 + 0.5) / (t1 - t0) - 0.5
            d = math.hypot(u, v) * 2
            h = ((s * 7919 + t * 104729) % 97) / 97.0
            px[t * w + s] = (10 if h < 0.6 else 9) if d < 0.26 else 4 if d < 0.36 else (30 if h < 0.5 else 31 if h < 0.88 else 28)
    import make_mags
    px += bytes(pouch.SKIN_W * pouch.SKIN_H)
    for k, name in enumerate(("vr_mag_nail", "vr_mag_snail", "vr_mag_light")):
        mag = make_mags.paint(name, variant)
        for t in range(make_mags.SKIN_H):
            row = (MAG_SKIN_T + t) * w + k * 64
            px[row:row + 64] = mag[t * make_mags.SKIN_W:(t + 1) * make_mags.SKIN_W]
    import make_rounds
    for name, (s_off, special) in {"rocket": (0, variant), "grenade": (64, variant), "prox": (128, False)}.items():
        rp = make_rounds.paint(name, 1 if special else 0)
        for t in range(make_rounds.SKIN_H):
            row = (ROUND_SKIN_T + t) * w + s_off
            px[row:row + make_rounds.SKIN_W] = rp[t * make_rounds.SKIN_W:(t + 1) * make_rounds.SKIN_W]
    for c in px:
        assert c < 224 or variant, "no fullbright texels (but the lava nails' and plasma's glow)"
    return bytes(px)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "progs")
    frames = [build(f) for f in range(FRAMES)]
    for f in frames[1:]:
        assert len(f.verts) == len(frames[0].verts) and f.tris == frames[0].tris, "frames of the same mesh"
    path = os.path.join(out, "vrpouch_ammo.mdl")
    guard = genguard.Guard("make_ammo_pouch.py", [path])
    # Skin 1: the lava nails' magazines and the plasma cells, the multi-rockets and multi-grenades (STAT_QVR_POUCHKIND's
    # 8: vr_view.cpp ammoPouchFrame).
    mdlgen.write_mdl(path, frames[0], [paint_skin(), paint_skin(True)], "pouch", frames=frames[1:])
    print("vrpouch_ammo.mdl: %d vertices, %d triangles, %d frames -> %s" % (len(frames[0].verts), len(frames[0].tris),
                                                                           FRAMES, os.path.normpath(path)))
    guard.finish()


if __name__ == "__main__":
    main()
