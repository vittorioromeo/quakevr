#!/usr/bin/env python3
# make_ammo_pouch.py -- generates quakevr/progs/vrpouch_ammo.mdl, the ammo pouch on the front of the belt (immersive
# reloading, vr_reload_mode 3: vr_view.cpp setupAmmoPouch, QC vr_reload.qc; docs/vr-port/RELOAD.md).
#
# make_pouch.py's leather pouch (the grenade pouch at the small of the back), made wider and boxier for the front of the
# belt, with no strap over its open top (a hand dips in and out of it all through a fight). The leather alone: what it
# holds is drawn by the engine (vr_view.cpp setupAmmoPouchItems: the rounds' own held models, at their held sizes, piled
# or standing in it and wobbling as the pouch moves; ROUND22.md, "Ammo pouch contents"). One frame a kind of round, the
# leather stretched round what it holds (the engine lerps between them as the pouch's kind changes), and the empty pouch:
#   0 empty (the front fallen in), 1 shells (a loose pile), 2 nailgun and super nailgun magazines (two rows of three),
#   3 cells (thicker: deeper), 4 grenades and multi-grenades (three at the back, two in front: deeper still),
#   5 proximity grenades, 6 rockets (lying along it in a zig-zag stack: wider, its floor lower).
# vr_view.cpp ammoPouchShapes has the same table (with the inside's outline: INNER, BULGE_P, BACK_X).
#
# Usage: python Misc/quakevr/make_ammo_pouch.py [output progs folder]
#
# Model space as make_pouch.py's: +x out of the body (the back against it at x 0.1), +y to its left, +z up, world units
# at scale 1 (vr_ammo_pouch_scale scales it as drawn). The skin is make_pouch.py's (its leather, iron and stitching).

import os
import sys

import genguard
import make_pouch as pouch
import mdlgen

# The front pouch's shape (make_pouch.py's globals, set before building: its functions read them).
pouch.Z_BOTTOM, pouch.Z_TOP = -2.3, 1.5
pouch.BULGE_P = 6.0  # boxy, its corners rounded (the magazines' rows and the rockets reach into them)
pouch.INNER = 0.93   # the leather's thickness (a share of the outline)
pouch.RING = 22      # (its sides smooth enough for the rows in the corners)
WRAP_DEPTH = 3.0     # the skin's wrap (every frame shares its skin coordinates: the shells' frame's)

# The frames: (half its width, its depth out of the back at the middle, the inside's floor, full).
FRAMES = [
    (4.6, 1.3, 0.6, False),  # 0 empty
    (4.6, 3.0, 0.8, True),   # 1 shells
    (4.6, 3.0, 0.6, True),   # 2 nailgun and super nailgun magazines
    (4.6, 3.5, 0.6, True),   # 3 cells
    (4.8, 4.4, 0.6, True),   # 4 grenades, multi-grenades
    (4.6, 3.6, 0.6, True),   # 5 proximity grenades
    (5.2, 3.5, -1.2, True),  # 6 rockets
]


def build(frame):
    half_w, depth, floor, full = FRAMES[frame]
    pouch.HALF_W = half_w
    pouch.Z_FLOOR = floor
    pouch.DEPTH = {0: WRAP_DEPTH, 1: depth}
    m = pouch.Mesh(pouch.SKIN_W, pouch.SKIN_H, pouch.REGIONS)
    mid = pouch.body(m, depth, full)
    # Rivets at the front's top corners and a pair on its middle, where a belt loop is sewn on behind.
    for y in (-half_w * 0.82, half_w * 0.82):
        px = pouch.front_x(y, depth)
        pouch.stud(m, (px, y, pouch.Z_TOP - 0.4), (px - mid[0], y, 0.0), 0.2, 0.12)
    fx = pouch.front_x(0.0, depth) * (1.03 if full else 1.0)
    for z in (-0.2, -1.4):
        pouch.stud(m, (fx, 0.0, z), (1.0, 0.0, 0.0), 0.17, 0.1)
    return m


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "progs")
    frames = [build(f) for f in range(len(FRAMES))]
    for f in frames[1:]:
        assert len(f.verts) == len(frames[0].verts) and f.tris == frames[0].tris, "frames of the same mesh"
    path = os.path.join(out, "vrpouch_ammo.mdl")
    guard = genguard.Guard("make_ammo_pouch.py", [path])
    mdlgen.write_mdl(path, frames[0], [pouch.paint_skin()], "pouch", frames=frames[1:])
    print("vrpouch_ammo.mdl: %d vertices, %d triangles, %d frames -> %s" % (len(frames[0].verts), len(frames[0].tris),
                                                                           len(FRAMES), os.path.normpath(path)))
    guard.finish()


if __name__ == "__main__":
    main()
