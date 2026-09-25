#!/usr/bin/env python3
# make_gadget.py -- generates quakevr/progs/vrgadget.mdl, the wrist gadget (vr_gadget.cpp): an
# olive-drab device strapped over the back of the off hand's forearm, whose screen shows the
# HUD (vr_hud_mode 1).
#
# Usage: python Misc/quakevr/make_gadget.py [output progs folder]
#
# Model space: +x the screen's right, +y its up, +z out of it; the device's centre at the origin.
# Units: Quake units at vr_world_scale 1.25 (1 m = 32.8 units): about 12 x 8 cm. The screen
# (vr_gadget.cpp's screenRect, which must match) spans x -1.5..1.5, y -0.93..0.93 just above the
# top face (z 0.41). Palette indices from gfx/palette.lmp: olive 55..59, metal 3..7, black 0..1,
# leather 170..173, red 250.

import os
import sys

import mdlgen

REGIONS = {"casing": (0, 0, 32, 32), "metal": (32, 0, 64, 32), "screen": (0, 32, 32, 64),
           "strap": (32, 32, 56, 64), "led": (56, 32, 64, 64)}
RAMPS = {"casing": [55, 56, 57, 58, 59], "metal": [3, 4, 5, 6, 7], "screen": [0, 1],
         "strap": [170, 171, 172, 173], "led": [250, 251]}


def build():
    m = mdlgen.Mesh(64, 64, REGIONS)
    # The casing, bevelled, with a darker bezel on top and the screen inset in it.
    m.box((0.0, 0.0, 0.0), (1.9, 1.3, 0.35), "casing", bevel=0.35)
    m.box((0.0, 0.0, 0.37), (1.7, 1.12, 0.03), "metal", bevel=0.2)
    m.box((0.0, 0.0, 0.40), (1.52, 0.95, 0.01), "screen")
    # Two dials on the right end, and a status light.
    for y in (0.55, -0.45):
        m.box((2.02, y, 0.05), (0.14, 0.22, 0.22), "metal", bevel=0.08)
    m.box((-1.62, 1.06, 0.41), (0.1, 0.1, 0.02), "led")
    # Straps round the forearm, under the casing.
    for x in (-1.25, 1.25):
        m.box((x, 0.0, -0.42), (0.3, 1.55, 0.1), "strap")
        m.box((x, 1.55, -0.75), (0.3, 0.1, 0.4), "strap")
        m.box((x, -1.55, -0.75), (0.3, 0.1, 0.4), "strap")
    return m


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "progs")
    m = build()
    skin = mdlgen.dithered_skin(64, 64, REGIONS, RAMPS, 777)
    path = os.path.join(out, "vrgadget.mdl")
    mdlgen.write_mdl(path, m, [skin], "gadget")
    print("vrgadget.mdl: %d vertices, %d triangles -> %s" % (len(m.verts), len(m.tris), os.path.normpath(path)))


if __name__ == "__main__":
    main()
