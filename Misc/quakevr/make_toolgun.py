#!/usr/bin/env python3
# make_toolgun.py -- generates quakevr/progs/v_toolgun.mdl, the toolgun (QC WID_TOOLGUN, Quake/vr/vr_toolgun.cpp; docs/
# vr-port/TOOLGUN.md): a debug and sandbox tool held as a pistol. A dark iron body over a leather-wrapped pistol grip,
# brass bands round a short emitter barrel, a cyan crystal held in brass prongs on top, cyan rune lines along its sides
# and a crystal lens at the muzzle (the cyan is Quake's fullbright 244..246: it glows in the dark). A debug tool: a few
# bevelled boxes, as make_gadget.py's (mdlgen.py).
#
# Usage: python Misc/quakevr/make_toolgun.py [output progs folder]
#
# Model space as the enemy guns' (make_enemyguns.py), whose weapon settings it starts from (slot 26 copies slot 21's,
# Quake/vr/vr_weapons.inc): +x along the barrel (towards the muzzle), +z up, +y left; the origin in the middle of the
# pistol grip, which the main hand holds. Units: at the weapon settings' Scale 0.39 one model unit is 0.39 Quake units,
# about 1.2 cm: the gun is about 23 cm long. The muzzle anchor (MuzzleAnchorVertex) is the lens's front centre, whose
# vertex index this prints; the script also prints the bounds' corner (the MDL's scale_origin, about which the weapon
# Scale pivots: vr_weapons.inc's Offset follows it).

import math
import os
import sys

import genguard
import mdlgen

REGIONS = {"iron": (0, 0, 32, 32), "brass": (32, 0, 64, 32), "grip": (0, 32, 32, 64), "glow": (32, 32, 64, 64)}
RAMPS = {"iron": [2, 3, 4, 5, 6], "brass": [105, 106, 107, 108, 109], "grip": [170, 171, 172, 173], "glow": [244, 245, 246]}


def transform(m, start, rot, offset):
    """The vertices added since `start`: rotated by the 3x3 `rot` (rows) about the origin, then moved by `offset`."""
    def apply(v):
        return tuple(sum(rot[r][c] * v[c] for c in range(3)) for r in range(3))
    for i in range(start, len(m.verts)):
        p, n, uv = m.verts[i]
        m.verts[i] = (mdlgen.add(apply(p), offset), apply(n), uv)


def rot_x(deg):
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return ((1, 0, 0), (0, c, -s), (0, s, c))


def rot_y(deg):
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return ((c, 0, s), (0, 1, 0), (-s, 0, c))


# Along the barrel: a box built along z, turned so that its +z runs along +x (its top cap's centre the front's).
ALONG_X = rot_y(90.0)


def barrel_box(m, x, half_len, half_across, region, bevel=0.0):
    start = len(m.verts)
    m.box((0.0, 0.0, 0.0), (half_across, half_across, half_len), region, bevel=bevel)
    transform(m, start, ALONG_X, (x, 0.0, BORE_Z))
    return start


BORE_Z = 5.8   # the barrel's axis height above the grip's middle
LENS_X = 15.4  # the lens's front


def build():
    m = mdlgen.Mesh(64, 64, REGIONS)
    # The pistol grip, leaning back 12 degrees (its butt behind its top), a brass cap under it.
    start = len(m.verts)
    m.box((0.0, 0.0, 0.0), (1.7, 1.3, 3.8), "grip", bevel=0.6)
    m.box((0.0, 0.0, -4.0), (1.85, 1.4, 0.25), "brass", bevel=0.6)
    transform(m, start, rot_y(-12.0), (0.0, 0.0, 0.0))
    # The body over it: iron, bevelled, a darker top plate.
    m.box((3.6, 0.0, 5.6), (6.6, 1.55, 1.9), "iron", bevel=0.7)
    m.box((3.2, 0.0, 7.6), (5.4, 1.25, 0.15), "iron", bevel=0.5)
    # Cyan rune lines along both sides, and a brass trim under them.
    for y in (-1.57, 1.57):
        m.box((3.6, y, 6.0), (4.6, 0.04, 0.16), "glow")
        m.box((3.6, y, 4.6), (6.0, 0.05, 0.12), "brass")
    # The trigger guard (a bar ahead of the grip and its front post) and the trigger.
    m.box((2.6, 0.0, 2.55), (1.7, 0.32, 0.18), "brass")
    m.box((4.2, 0.0, 3.1), (0.18, 0.32, 0.6), "brass")
    m.box((1.9, 0.0, 3.0), (0.22, 0.25, 0.75), "iron")
    # The emitter barrel, three brass bands round it, the crystal lens at its end.
    barrel_box(m, 12.6, 2.6, 1.0, "iron", bevel=0.35)
    for x in (11.0, 12.6, 14.2):
        barrel_box(m, x, 0.28, 1.32, "brass", bevel=0.4)
    lens = barrel_box(m, LENS_X - 0.18, 0.18, 0.62, "glow", bevel=0.2)
    # The crystal on top, turned 45 degrees about the barrel, held by two brass prongs, and a brass knob at the back.
    start = len(m.verts)
    m.box((0.0, 0.0, 0.0), (2.0, 0.75, 0.75), "glow")
    transform(m, start, rot_x(45.0), (3.4, 0.0, 8.8))
    for x in (0.8, 6.0):
        m.box((x, 0.0, 8.5), (0.35, 0.55, 0.85), "brass", bevel=0.2)
    m.box((-3.4, 0.0, 5.8), (0.35, 0.9, 0.9), "brass", bevel=0.4)
    return m, lens


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "progs")
    # Files edited in Blender since this wrote them are not overwritten (genguard.py: --keep-edited, --force).
    guard = genguard.Guard("make_toolgun.py", [os.path.join(out, "v_toolgun.mdl")])
    m, lens = build()
    # The muzzle anchor: the lens's front centre (the top cap's centre of its box, along +x).
    muzzle = min(range(lens, len(m.verts)), key=lambda i: (m.verts[i][0][0] - LENS_X) ** 2 + m.verts[i][0][1] ** 2 +
                 (m.verts[i][0][2] - BORE_Z) ** 2)
    skin = mdlgen.dithered_skin(64, 64, REGIONS, RAMPS, 4242)
    path = os.path.join(out, "v_toolgun.mdl")
    mdlgen.write_mdl(path, m, [skin], "toolgun")
    lo = [min(v[0][k] for v in m.verts) for k in range(3)]
    print("v_toolgun.mdl: %d vertices, %d triangles -> %s" % (len(m.verts), len(m.tris), os.path.normpath(path)))
    print("muzzle anchor vertex %d at %s; bounds' corner (scale_origin) %.3f %.3f %.3f" %
          (muzzle, tuple(round(c, 3) for c in m.verts[muzzle][0]), lo[0], lo[1], lo[2]))
    guard.finish()


if __name__ == "__main__":
    main()
