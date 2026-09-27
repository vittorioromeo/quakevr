#!/usr/bin/env python3
# make_gadget.py -- generates quakevr/progs/vrgadget.mdl, the wrist gadget (vr_gadget.cpp): an
# olive-drab device strapped over the back of the off hand's forearm, whose screen shows the
# HUD (vr_hud_mode 1); and quakevr/progs/vrgadget_strap.mdl, one of the two straps that hold it round the forearm.
#
# Usage: python Misc/quakevr/make_gadget.py [output progs folder]
#
# The gadget. Model space: +x the screen's right, +y its up, +z out of it; the device's centre at the origin.
# Units: Quake units at vr_world_scale 1.25 (1 m = 32.8 units): about 12 x 8 cm. The screen
# (vr_gadget.cpp's screenRect, which must match) spans x -1.5..1.5, y -0.93..0.93 just above the
# top face (z 0.41). The casing is 3.8 x 2.6 x 0.7 round the origin (vr_gadget.cpp's gadgetTop). Palette indices from
# gfx/palette.lmp: olive 55..59, metal 3..7, black 0..1, leather 170..173, red 250.
#
# The casing (round 21, the author's note: "more interesting, the same art style and dimensions"): a lower shell and
# a lid parted by a dark seam, the bezel and screen as before, four screws in the lid, two dials on the right end,
# two buttons on the lower side, grip grooves on the ends, the hologram's emitter (a slot of dark glass in a metal
# frame along the top edge, over the screen, where the hologram rises), a short antenna, and under it two lugs where
# the straps pass, riveted.
#
# The strap (vrgadget_strap.mdl): an olive webbing band round the forearm with a buckle and two rivets on the little
# finger's side. It is drawn twice, under the lugs, in the forearm's own frame (not the gadget's: vr_view.cpp
# setupGadget), scaled per axis to the bracer there (vr_body_build): model x along the forearm (the band from -1 to
# 1), y and z across it, inner radius 1 (the bracer's ring axes: z its hint, the little finger's side). Frame 1 is
# the band as a cone (its elbow edge, x -1, STRAP_TAPER wider); the engine blends it from frame 0 (a cylinder) by
# the bracer's own taper there (the zero blend).

import math
import os
import sys

import genguard
import mdlgen

REGIONS = {"casing": (0, 0, 32, 32), "metal": (32, 0, 64, 32), "screen": (0, 32, 32, 64),
           "strap": (32, 32, 56, 64), "led": (56, 32, 64, 64)}
RAMPS = {"casing": [55, 56, 57, 58, 59], "metal": [3, 4, 5, 6, 7], "screen": [0, 1],
         "strap": [170, 171, 172, 173], "led": [250, 251]}
# The straps: olive webbing, darker than the casing, against the brown leather bracer under them.
STRAP_RAMPS = dict(RAMPS, strap=[51, 52, 53, 54])

STRAP_SIDES = 24
STRAP_THICK = 0.07  # of the inner radius (about 3 mm on a forearm)
STRAP_TAPER = 0.6   # frame 1: the elbow edge's radius, 1 + this


def build():
    m = mdlgen.Mesh(64, 64, REGIONS)
    # The casing: a lower shell and a lid, bevelled, parted by a dark seam (inset: the outline stays 3.8 x 2.6 x 0.7).
    m.box((0.0, 0.0, -0.13), (1.9, 1.3, 0.22), "casing", bevel=0.35)
    m.box((0.0, 0.0, 0.11), (1.87, 1.27, 0.02), "screen", bevel=0.34)
    m.box((0.0, 0.0, 0.24), (1.9, 1.3, 0.11), "casing", bevel=0.35)
    # The bezel and the screen inset in it (as before: the screen's image is drawn over it).
    m.box((0.0, 0.0, 0.37), (1.7, 1.12, 0.03), "metal", bevel=0.2)
    m.box((0.0, 0.0, 0.40), (1.52, 0.95, 0.01), "screen")
    # Four screws in the lid, at the ends beside the bezel.
    for x in (-1.8, 1.8):
        for y in (-0.8, 0.8):
            m.box((x, y, 0.345), (0.055, 0.055, 0.03), "metal", bevel=0.02)
    # Two dials on the right end, and a status light.
    for y in (0.55, -0.45):
        m.box((2.02, y, 0.05), (0.14, 0.22, 0.22), "metal", bevel=0.08)
    m.box((-1.62, 1.06, 0.41), (0.1, 0.1, 0.02), "led")
    # The hologram's emitter: a slot of dark glass in a metal frame, on the lid along the top edge over the screen.
    m.box((0.0, 1.21, 0.345), (0.62, 0.075, 0.03), "metal")
    m.box((0.0, 1.21, 0.37), (0.52, 0.04, 0.015), "screen")
    # A short antenna on the left end, between the screws: a base and a whip with a knob.
    m.box((-1.8, 0.0, 0.37), (0.07, 0.07, 0.03), "metal", bevel=0.025)
    m.box((-1.8, 0.0, 0.5), (0.022, 0.022, 0.1), "metal")
    m.box((-1.8, 0.0, 0.62), (0.04, 0.04, 0.025), "metal", bevel=0.015)
    # Two buttons on the lower side (the screen's bottom edge), and grip grooves across the left end.
    for x in (-0.55, 0.25):
        m.box((x, -1.33, -0.05), (0.2, 0.05, 0.08), "metal", bevel=0.06)
    for y in (-0.6, -0.3, 0.3, 0.6):
        m.box((-1.91, y, -0.08), (0.035, 0.06, 0.16), "screen")
    # The lugs the straps pass under, riveted.
    for x in (-1.25, 1.25):
        m.box((x, 0.0, -0.47), (0.3, 0.95, 0.12), "casing", bevel=0.1)
        for y in (-0.6, 0.6):  # on the face away from the middle
            m.box((x + math.copysign(0.3, x), y, -0.47), (0.035, 0.06, 0.06), "metal", bevel=0.02)
    return m


def strap_ring(m, radius_of, rings):
    """A closed band: `rings` is a list of (x, inner radius scale) and the outer surface is STRAP_THICK further
    out; `radius_of(x)` scales every point's distance from the axis (frame 1's taper)."""
    n = STRAP_SIDES

    def pt(x, a, r):
        k = radius_of(x) * r
        return (x, math.cos(a) * k, math.sin(a) * k)

    xs = [x for x, _ in rings]
    for i in range(n):
        a0 = 2 * math.pi * i / n
        a1 = 2 * math.pi * (i + 1) / n
        x0, x1 = xs[0], xs[-1]
        ro, ri = 1.0 + STRAP_THICK, 1.0
        # outer, inner, and the two edges (counter-clockwise seen from outside)
        m.quad(pt(x0, a0, ro), pt(x0, a1, ro), pt(x1, a1, ro), pt(x1, a0, ro), "strap")
        m.quad(pt(x0, a1, ri), pt(x0, a0, ri), pt(x1, a0, ri), pt(x1, a1, ri), "strap")
        m.quad(pt(x0, a0, ri), pt(x0, a1, ri), pt(x0, a1, ro), pt(x0, a0, ro), "strap")
        m.quad(pt(x1, a1, ri), pt(x1, a0, ri), pt(x1, a0, ro), pt(x1, a1, ro), "strap")


def build_strap(taper):
    """The strap, frame 0 (taper 0) or 1 (STRAP_TAPER)."""
    m = mdlgen.Mesh(64, 64, REGIONS)

    def radius_of(x):
        return 1.0 + taper * (1.0 - x) * 0.5

    strap_ring(m, radius_of, [(-1.0, 1.0), (1.0, 1.0)])
    # The buckle on the little finger's side (+z): a metal frame on the band, the tongue through it, two rivets.
    base = []
    top = 1.0 + STRAP_THICK
    base.append(((0.0, 0.0, top + 0.035), (1.08, 0.34, 0.035), "metal", 0.1))
    base.append(((0.0, 0.0, top + 0.08), (0.95, 0.2, 0.012), "strap", 0.0))
    for y in (-0.5, 0.5):
        base.append(((0.0, y, top + 0.012), (0.18, 0.06, 0.012), "metal", 0.0))
    for c, h, region, bevel in base:
        start = len(m.verts)
        m.box(c, h, region, bevel=bevel)
        # onto the cone: the box's points pushed out with the band (every point's distance from the axis)
        for i in range(start, len(m.verts)):
            p, nrm, uv = m.verts[i]
            k = radius_of(p[0])
            m.verts[i] = ((p[0], p[1] * k, p[2] * k), nrm, uv)
    return m


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "progs")
    # The files edited in Blender since this wrote them are not overwritten (genguard.py: --keep-edited, --force).
    guard = genguard.Guard("make_gadget.py", [os.path.join(out, n) for n in ("vrgadget.mdl", "vrgadget_strap.mdl")])
    m = build()
    skin = mdlgen.dithered_skin(64, 64, REGIONS, RAMPS, 777)
    path = os.path.join(out, "vrgadget.mdl")
    mdlgen.write_mdl(path, m, [skin], "gadget")
    print("vrgadget.mdl: %d vertices, %d triangles -> %s" % (len(m.verts), len(m.tris), os.path.normpath(path)))

    s0, s1 = build_strap(0.0), build_strap(STRAP_TAPER)
    path = os.path.join(out, "vrgadget_strap.mdl")
    mdlgen.write_mdl(path, s0, [mdlgen.dithered_skin(64, 64, REGIONS, STRAP_RAMPS, 778)], "strap", frames=[s1])
    print("vrgadget_strap.mdl: %d vertices, %d triangles, 2 frames -> %s" % (len(s0.verts), len(s0.tris),
                                                                             os.path.normpath(path)))
    guard.finish()


if __name__ == "__main__":
    main()
