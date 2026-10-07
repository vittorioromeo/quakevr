#!/usr/bin/env python3
# make_mags.py -- the magazines of immersive reloading's phase 2 (QC vr_reload.qc; docs/vr-port/RELOAD_PLAN.md):
#   quakevr/progs/vr_mag_nail.mdl     the nailgun's: a box magazine as the old nailgun pickup's (g_nail.mdl) leg under
#                                     the receiver: a brown body with stamped ribs, a steel base plate, its feed lips
#                                     with the nails' heads in them; 3.4 x 1.7 x 5.8 units.
#   quakevr/progs/vr_mag_snail.mdl    the super nailgun's: a longer, wider box of the same make (36 nails), fed from the
#                                     gun's outer side; a window down its side shows the nails.
#   quakevr/progs/vr_mag_light.mdl    the thunderbolt's: a power cell hung under it: an octagonal steel can with bronze
#                                     bands, a copper contact on top, a dark insulated foot.
#   quakevr/progs/vr_magwell_on_<gun>.mdl its well (its receiver: a steel collar round the magazine's top, flush with the
#                                     gun at the seat), drawn on the gun whether a magazine is in or not.
#   quakevr/progs/vr_mag_on_<gun>.mdl each as it sits in its gun (v_nail, v_lava, v_nail2, v_lava2, v_light, v_plasma),
#                                     in that gun's model space: drawn with the gun's own place, turn and mirroring
#                                     (vr_view.cpp setupMagazines), its Scale and offsets the gun's (vr_weapons.cpp
#                                     makeModelTransform: a file per gun, as each gun has its own settings): the nailgun's
#                                     under the receiver ahead of the trigger guard, raked forward a little; the super
#                                     nailgun's out of the outer (right, -y) side of its body, square to its upper face
#                                     (22 degrees up: the author's note, 37 was too steep); the
#                                     thunderbolt's under its body, ahead of the grip.
# The props (the first three) have their origin at their middle (they tumble about it), the magazine's insertion axis
# along +z (its top, the feed end, up). vr_view.cpp's magMounts and loadPorts tables take the `well` points printed here
# (where a held magazine's middle seats). Quake palette indices (gfx/palette.lmp: browns 16..31 and 96..111, greys 0..15,
# the blued steel 32..39, coppers 112..121), no fullbrights.
#
# Usage: python Misc/quakevr/make_mags.py [output progs folder]

import math
import os
import random
import sys

import genguard
import mdlgen
from mdlgen import add, cross, mul, norm, sub

SKIN_W, SKIN_H = 64, 64
REGIONS = {
    "body": (0, 0, 32, 32),     # the box's (the can's) sides
    "steel": (32, 0, 64, 16),   # base plates, lips, caps' rims
    "top": (32, 16, 64, 32),    # the feed end: the nails' heads (the cell's contact)
    "band": (0, 32, 32, 48),    # bronze bands (the cell), dark ribs
    "window": (32, 32, 64, 48), # a window down the side: nails' shanks in it (the super nailgun's)
    "foot": (0, 48, 32, 64),    # the bottom
    "spare": (32, 48, 64, 64),
}


class Mesh(mdlgen.Mesh):
    def __init__(self):
        super().__init__(SKIN_W, SKIN_H, REGIONS)


def transformed(mesh, rot, at):
    """A copy of `mesh` turned by `rot` (rows: where local x, y, z go) and moved to `at`: as it sits in its gun."""
    out = Mesh()

    def tr(p):
        return add(add(add(mul(rot[0], p[0]), mul(rot[1], p[1])), mul(rot[2], p[2])), at)

    def trn(n):
        return norm(add(add(mul(rot[0], n[0]), mul(rot[1], n[1])), mul(rot[2], n[2])))

    out.verts = [(tr(p), trn(n), st) for p, n, st in mesh.verts]
    out.tris = list(mesh.tris)
    return out


def box_mag(m, hx, hy, hz, bevel, lips, ribs=True):
    """A box magazine about the origin (x along its gun, y across it, z its length, the feed end up): its body, a base
    plate a little wider, the feed lips on top (narrower), and a raised rib round it below the lips."""
    m.box((0.0, 0.0, 0.0), (hx, hy, hz), "body", bevel=bevel, cap_region="foot")
    m.box((0.0, 0.0, -hz - 0.22), (hx + 0.18, hy + 0.16, 0.22), "steel", bevel=bevel, cap_region="foot")
    m.box((0.0, 0.0, hz + 0.2), (hx * lips, hy * 0.72, 0.2), "steel", bevel=bevel * 0.5, cap_region="top")
    if ribs:
        m.box((0.0, 0.0, hz * 0.62), (hx + 0.08, hy + 0.08, 0.14), "band", bevel=bevel, cap_region="band")
    return hz + 0.4  # its top


def nail_mag():
    m = Mesh()
    top = box_mag(m, 1.7, 0.85, 2.9, 0.28, 0.82)
    return m, top


def snail_mag():
    m = Mesh()
    top = box_mag(m, 2.3, 1.05, 4.3, 0.32, 0.84)
    # The window down its side (the outer face, -y as it sits): the nails' shanks behind a slot, a steel frame round it.
    m.box((0.0, -1.05 - 0.06, -0.3), (1.5, 0.06, 3.0), "window", cap_region="window")
    return m, top


def cell_mag():
    """The thunderbolt's cell: an octagonal can (a box with deep bevels) with two bronze bands, a dark insulated foot,
    a steel rim and a copper contact at its top."""
    m = Mesh()
    hx, hy, hz = 1.75, 1.5, 2.9
    m.box((0.0, 0.0, 0.0), (hx, hy, hz), "body", bevel=0.6, cap_region="foot")
    for z in (-1.4, 1.2):
        m.box((0.0, 0.0, z), (hx + 0.1, hy + 0.1, 0.32), "band", bevel=0.63, cap_region="band")
    m.box((0.0, 0.0, -hz - 0.3), (hx * 0.92, hy * 0.92, 0.3), "foot", bevel=0.5, cap_region="foot")
    m.box((0.0, 0.0, hz + 0.18), (hx * 0.8, hy * 0.8, 0.18), "steel", bevel=0.45, cap_region="steel")
    m.box((0.0, 0.0, hz + 0.52), (0.5, 0.5, 0.18), "top", bevel=0.18, cap_region="top")
    return m, hz + 0.7


def rot_x(deg):
    """Rows: where local x, y, z go, turned about x by `deg` (y towards z)."""
    a = math.radians(deg)
    return ((1.0, 0.0, 0.0), (0.0, math.cos(a), math.sin(a)), (0.0, -math.sin(a), math.cos(a)))


def rot_y(deg):
    """About y by `deg` (x towards -z: a positive angle rakes the bottom forward)."""
    a = math.radians(deg)
    return ((math.cos(a), 0.0, -math.sin(a)), (0.0, 1.0, 0.0), (math.sin(a), 0.0, math.cos(a)))


# Where each sits in its gun: (prop, attached name, its turn, the seat in gun space (where its top meets the gun),
# the guns it fits).
def mounts():
    nail, nail_top = nail_mag()
    snail, snail_top = snail_mag()
    cell, cell_top = cell_mag()
    # The super nailgun's: square to the face it goes into (the author: 37 degrees was too steep), the upper of the body's
    # two right faces (normal (0, -0.93, 0.38): 22 degrees up): its length along that face's inward normal, local -y (the
    # face with the window) up towards the eyes, local x along the gun.
    n = norm((0.0, 0.926, -0.378))
    snail_rot = ((1.0, 0.0, 0.0), (0.0, n[2], -n[1]), n)
    return [
        ("vr_mag_nail", nail, nail_top, rot_y(-8.0), (6.9, 0.0, -1.45), ("v_nail.mdl", "v_lava.mdl")),
        ("vr_mag_snail", snail, snail_top, snail_rot, (7.2, -5.44, 1.2), ("v_nail2.mdl", "v_lava2.mdl")),
        ("vr_mag_light", cell, cell_top, ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0)), (11.6, 0.0, 3.0),
         ("v_light.mdl", "v_plasma.mdl")),
    ]


def paint(kind, variant=False):
    """Each magazine's skin: the body in its gun's colours (the nailguns' brown boxes, stamped ribs; the cell's blued
    steel), worn steel, the feed end (nail heads in rows, or the copper contact), bronze bands, the window, the foot.
    `variant` (skin 1; the lava and plasma guns' own): the lava nails' magazines a dark red body, their nails glowing
    (fullbright orange heads, red-hot shanks in the window); the plasma cell a deep blue can, its bands and contact a
    glowing pale blue: never taken for the plain ones."""
    rng = random.Random({"vr_mag_nail": 701, "vr_mag_snail": 702, "vr_mag_light": 703}[kind] + (50 if variant else 0))
    body_ramp = [17, 18, 19, 20, 21, 22, 23] if kind != "vr_mag_light" else [0, 32, 33, 34, 35, 36, 37]
    steel = [3, 4, 5, 6, 7, 8, 9]
    band = [96, 97, 98, 99, 100, 101] if kind == "vr_mag_light" else [16, 17, 18]
    if variant:
        body_ramp = [70, 72, 73, 74, 75, 76, 77] if kind != "vr_mag_light" else [220, 216, 215, 214, 213, 212, 211]
        band = [70, 72, 74] if kind != "vr_mag_light" else [244, 245, 244, 246]
    px = bytearray(SKIN_W * SKIN_H)

    def pick(ramp, v):
        v = max(0.0, min(0.999, v + rng.uniform(-0.12, 0.12)))
        return ramp[int(v * len(ramp))]

    for t in range(SKIN_H):
        for s in range(SKIN_W):
            region = next(r for r, (s0, t0, s1, t1) in REGIONS.items() if s0 <= s < s1 and t0 <= t < t1)
            s0, t0, s1, t1 = REGIONS[region]
            u, v = (s - s0 + 0.5) / (s1 - s0), (t - t0 + 0.5) / (t1 - t0)
            if region == "body":
                val = 0.5
                if kind != "vr_mag_light":
                    if int(v * 10) % 3 == 0:
                        val -= 0.3   # stamped ribs across it
                    if u < 0.06 or u > 0.94:
                        val += 0.25  # worn edges
                else:
                    if abs(u - 0.5) < 0.08:
                        val += 0.3   # a seam down the can
                    if v < 0.08 or v > 0.92:
                        val -= 0.25
                c = pick(body_ramp, val)
            elif region == "steel":
                c = pick(steel, 0.45 + (0.3 if (u < 0.08 or u > 0.92 or v < 0.1) else 0.0))
            elif region == "top":
                if kind == "vr_mag_light":
                    d = math.hypot(u - 0.5, v - 0.5)
                    c = pick([244, 245, 246] if variant else [112, 113, 114, 115, 116, 117],
                             0.6 if d < 0.3 else 0.3)  # copper (plasma: a pale blue glow)
                else:
                    # Two rows of nail heads (grey discs) in the dark lips.
                    cx = (u * 6) % 1.0
                    cy = (v * 2) % 1.0
                    head = [233, 234, 235, 236] if variant else steel
                    c = pick(head, 0.75) if math.hypot(cx - 0.5, cy - 0.5) < 0.32 else (226 if variant else 1)
            elif region == "band":
                c = pick(band, 0.55 + (0.25 if v < 0.25 else -0.2 if v > 0.75 else 0.0))
            elif region == "window":
                # A dark slot with the nails' shanks along it (steel lines), a steel frame round it.
                if u < 0.08 or u > 0.92 or v < 0.05 or v > 0.95:
                    c = pick(steel, 0.5)
                elif int(v * 16) % 2 == 0:
                    c = pick([231, 232, 233] if variant else steel, 0.65)
                else:
                    c = 1
            elif region == "foot":
                c = pick([1, 2, 3, 4], 0.4) if kind == "vr_mag_light" else pick(steel, 0.35)
            else:
                c = 2
            assert c < 224 or variant  # (the variants' glow: fullbright on purpose)
            px[t * SKIN_W + s] = c
    return bytes(px)


def magwell(hx, hy, top, wall=0.32, height=1.1):
    """A magazine's well (its receiver, drawn on the gun: vr_view.cpp setupMagazines): a steel collar round the magazine's
    top end, flush with the gun at the seat (the magazine's top, local z `top`) and down its length `height`, its walls
    `wall` thick round the magazine's section (half sizes hx, hy, a little gap), a lighter lip at its mouth."""
    m = Mesh()
    gx, gy = hx + 0.08, hy + 0.08
    zc = top - height / 2
    for sx in (-1.0, 1.0):
        m.box((sx * (gx + wall / 2), 0.0, zc), (wall / 2, gy + wall, height / 2), "steel", cap_region="foot")
    for sy in (-1.0, 1.0):
        m.box((0.0, sy * (gy + wall / 2), zc), (gx, wall / 2, height / 2), "steel", cap_region="foot")
    m.box((0.0, 0.0, top - height + 0.08), (gx + wall + 0.06, gy + wall + 0.06, 0.08), "band", cap_region="band")
    return m


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "quakevr", "progs")
    paths = []
    for name, *_ in mounts():
        paths += [os.path.join(out, name + ".mdl")] + [os.path.join(out, pre + g) for g in _[-1]
                                                        for pre in ("vr_mag_on_", "vr_magwell_on_")]
    guard = genguard.Guard("make_mags.py", paths)
    sections = {"vr_mag_nail": (1.7, 0.85), "vr_mag_snail": (2.3, 1.05), "vr_mag_light": (1.75, 1.5)}
    for name, mesh, top, rot, seat, guns in mounts():
        skin, fiery = paint(name), paint(name, True)
        # The prop: skin 0 plain, 1 the lava nails' or plasma's (QC sets it by the ammo).
        mdlgen.write_mdl(os.path.join(out, name + ".mdl"), mesh, [skin, fiery], name)
        # As it sits: its top at the seat, turned; its well round it.
        top_at = add(add(mul(rot[0], 0.0), mul(rot[1], 0.0)), mul(rot[2], top))
        at = sub(seat, top_at)
        well = magwell(sections[name][0], sections[name][1], top)
        for k, g in enumerate(guns):
            own = fiery if k == 1 else skin  # (the second gun of each pair is the lava or plasma one)
            mdlgen.write_mdl(os.path.join(out, "vr_mag_on_" + g), transformed(mesh, rot, at), [own], name + "_on")
            mdlgen.write_mdl(os.path.join(out, "vr_magwell_on_" + g), transformed(well, rot, at), [skin], name + "_well")
        print("%s: %d vertices, %d triangles; in %s its middle (the well) at (%.2f %.2f %.2f)" % (
            name, len(mesh.verts), len(mesh.tris), "/".join(guns), at[0], at[1], at[2]))
    guard.finish()


if __name__ == "__main__":
    main()
