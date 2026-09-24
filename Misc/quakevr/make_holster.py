#!/usr/bin/env python3
# make_holster.py -- generates quakevr/progs/legholster.mdl, the holster drawn at the hip and upper
# holsters (vr_view.cpp, vr_leg_holster_model_*): a leather plate that rests against the body,
# with the weapon hanging on its outer side under two leather clips, and a strap up to the belt.
# Open on the outside, it suits small and large weapons alike.
#
# Usage: python Misc/quakevr/make_holster.py [output progs folder]
#
# Model space (as the model it replaces, whose placement cvars stay valid): +x forward, +z up,
# +y towards the body (the left-side holster is drawn mirrored); the holstered weapon hangs at
# the origin. The skin uses Quake palette indices (chosen from gfx/palette.lmp: leathers 168..174,
# reddish straps 98..101, metal 7..11), so no palette is needed to run this.

import os
import sys

import mdlgen

REGIONS = {"plate": (0, 0, 32, 32), "strap": (32, 0, 64, 32), "metal": (0, 32, 32, 64), "edge": (32, 32, 64, 64)}
RAMPS = {"plate": [168, 169, 170, 171], "strap": [98, 99, 100, 101], "metal": [7, 8, 9, 10, 11], "edge": [172, 173, 174]}


def build():
    m = mdlgen.Mesh(64, 64, REGIONS)
    # The plate against the body, tall and a little tapered at the bottom.
    m.box((0.0, 2.7, 0.5), (4.2, 0.5, 5.5), "plate", bevel=1.0)
    # Its stitched rim, a hair proud of it on the outer side.
    m.box((0.0, 2.1, 0.5), (4.4, 0.12, 5.7), "edge", bevel=1.1)
    # Two clips: from the plate's outer face around the weapon's side (open on the outside).
    for z in (3.3, -2.7):
        m.box((0.0, 1.2, z), (1.6, 1.0, 0.55), "strap")    # out from the plate
        m.box((0.0, -0.6, z), (1.6, 0.35, 0.55), "strap")  # across the weapon
        m.box((0.0, -0.6, z), (0.55, 0.42, 0.65), "metal") # a rivet on it
    # The strap up to the belt, with a buckle.
    m.box((0.0, 2.9, 7.6), (1.2, 0.35, 2.4), "strap")
    m.box((0.0, 2.5, 6.4), (1.5, 0.2, 0.7), "metal")
    return m


def stitching(region, s, t):
    if region == "plate" and (s % 32 in (2, 29) or t % 32 in (2, 29)) and (s + t) % 3 == 0:
        return RAMPS["edge"][0]
    return None


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "progs")
    m = build()
    skin = mdlgen.dithered_skin(64, 64, REGIONS, RAMPS, 4242, stitching)
    path = os.path.join(out, "legholster.mdl")
    mdlgen.write_mdl(path, m, skin, "holster")
    print("legholster.mdl: %d vertices, %d triangles -> %s" % (len(m.verts), len(m.tris), os.path.normpath(path)))


if __name__ == "__main__":
    main()
