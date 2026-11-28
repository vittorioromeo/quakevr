#!/usr/bin/env python3
# make_nailpistol_lava.py -- the nail pistol's lava-nail look (ROUND26.md "nplava"; his words 2026-11-28 item 19): its
# skin repainted in the lava nailguns' style, as indexed PNGs on the base skins' layouts, from the shipped models' skins:
#   src_models/blend/v_nailpistol/v_nailpistol_skin_v_nailpistol_lava.png       the gun (vr_gunvariants.inc's variant)
#   src_models/blend/v_nailpistol/vr_mag_on_v_nailpistol_skin_v_nailpistol_lava.png  its seated magazine (the part)
#   src_models/blend/vr_mag_npistol/vr_mag_npistol_skin1.png                    the loose magazine's skin 1 (QC
#                                                                                vr_reload.qc: a lava magazine's)
# The style (v_lava's and vr_mag_nail's skin 1 against their plain ones): the copper and tan paint and the brown wear
# turn a deep lava red, the gun's side vents (the slots' floors and walls, found by the model's geometry) glow molten,
# and the magazine's window shows glowing nails. The gun's glow keeps off the fire indices (224-239, 252, 253): its
# skin is a sighted one (vr_sights.cpp recolours those as sights, vr_sightalign.cpp finds its sight line by them), so it
# glows in the other fullbrights (the reds 247-251, the amber 240); the magazines' skins are not sighted: fire indices.
# The sight inserts (fire texels already there) and everything else stay byte for byte.
#
# Usage: python Misc/quakevr/make_nailpistol_lava.py [--preview out.png]
#   then: python Misc/quakevr/make_gun_variants.py build   (writes the models from the PNGs)
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "blender", "addons", "quakevr_models"))
import mdl as mdlmod  # noqa: E402
import qpal  # noqa: E402
import make_gun_variants as gv  # noqa: E402

BLEND = os.path.join(ROOT, "Misc", "quakevr", "src_models", "blend")
PROGS = os.path.join(ROOT, "quakevr", "progs")
OUT_GUN = os.path.join(BLEND, "v_nailpistol", "v_nailpistol_skin_v_nailpistol_lava.png")
OUT_MAGON = os.path.join(BLEND, "v_nailpistol", "vr_mag_on_v_nailpistol_skin_v_nailpistol_lava.png")
OUT_MAG = os.path.join(BLEND, "vr_mag_npistol", "vr_mag_npistol_skin1.png")

# The side vents (vr_gunfx.cpp ventSmoke: x 4.91..7.57, the side faces at y +-1.2, slots z 3.08..4.59): their floors at
# |y| 0.88, their walls between.
VENT_X, VENT_Z, VENT_FLOOR, VENT_RIM = (4.8, 7.7), (2.9, 4.8), 0.93, 1.16


def lum(c):
    return 0.30 * c[0] + 0.59 * c[1] + 0.11 * c[2]


def nearest(rgb, indices):
    return min(indices, key=lambda i: sum((qpal.PALETTE[i][k] - rgb[k]) ** 2 for k in range(3)))


PLAIN = range(0, 224)
LAVA_RED = {}


def lava_red(i):
    """A tan, copper or brown texel's lava red (non-fullbright): deep red, a little orange in the light ones."""
    if i not in LAVA_RED:
        L = lum(qpal.PALETTE[i])
        LAVA_RED[i] = nearest((min(255, L * 1.45 + 6), min(255, L * 0.34), min(255, L * 0.14)), PLAIN)
    return LAVA_RED[i]


def rasterize(model, keep):
    """The set of texels (s, t) covered by `model`'s triangles that keep((points, uv, normal)) accepts."""
    from PIL import Image, ImageDraw
    w, h = model.h[13], model.h[14]
    im = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(im)
    for tri in gv.triangles(model):
        if keep(tri):
            d.polygon([(u - 0.5, v - 0.5) for u, v in tri[1]], fill=255)
    px = im.load()
    return {(x, y) for y in range(h) for x in range(w) if px[x, y]}


def vent_parts(tri):
    p = tri[0]
    cx = sum(q[0] for q in p) / 3
    cy = abs(sum(q[1] for q in p) / 3)
    cz = sum(q[2] for q in p) / 3
    if not (VENT_X[0] <= cx <= VENT_X[1] and VENT_Z[0] <= cz <= VENT_Z[1] and 0.8 <= cy <= VENT_RIM):
        return None
    return "floor" if cy < VENT_FLOOR else "wall"


def gun_skin(m):
    w, h = m.h[13], m.h[14]
    src = m.skins[0][2][0]
    floors = rasterize(m, lambda t: vent_parts(t) == "floor")
    walls = rasterize(m, lambda t: vent_parts(t) == "wall")
    out = bytearray(src)
    for y in range(h):
        for x in range(w):
            k = y * w + x
            i = src[k]
            if i >= 224:
                continue  # (the sight inserts: as they are)
            if (x, y) in floors:
                out[k] = 240 if (x + y) % 3 else 251  # molten: amber, flecked red
            elif (x, y) in walls:
                out[k] = 250 if lum(qpal.PALETTE[i]) > 30 else 249
            elif i // 16 in (1, 10):
                out[k] = lava_red(i)  # (its red flecks, row 4, stay: glowing they speckled the whole gun)
    return bytes(out)


def mag_skin(mm, src):
    """The magazine's lava skin from the gun's top half (the magazines share it): its paint lava red, the nails in its
    window glowing (its red flecks and the light texels beside them: fire indices)."""
    w, h = mm.h[13], mm.h[14]
    used = rasterize(mm, lambda t: True)
    out = bytearray(src[:w * h])
    red = {(k % w, k // w) for k in range(w * h) if src[k] // 16 == 4}
    for k in range(w * h):
        x, y = k % w, k // w
        i = src[k]
        if i >= 224 or (x, y) not in used:
            continue
        L = lum(qpal.PALETTE[i])
        near_red = any((x + dx, y + dy) in red for dx in (-1, 0, 1) for dy in (-2, -1, 0, 1, 2))
        if i // 16 == 4:
            out[k] = 232 + min(3, int(L / 12))
        elif near_red and L > 60:
            out[k] = 236 if L > 120 else 234
        elif i // 16 in (1, 10):
            out[k] = lava_red(i)
    return bytes(out)


def main():
    m = mdlmod.Model.read(os.path.join(PROGS, "v_nailpistol.mdl"))
    mm = mdlmod.Model.read(os.path.join(PROGS, "vr_mag_npistol.mdl"))
    gun = gun_skin(m)
    mag = mag_skin(mm, m.skins[0][2][0])
    gv.write_png(OUT_GUN, m.h[13], m.h[14], gun)
    gv.write_png(OUT_MAGON, mm.h[13], mm.h[14], mag)
    gv.write_png(OUT_MAG, mm.h[13], mm.h[14], mag)
    changed = sum(1 for a, b in zip(gun, m.skins[0][2][0]) if a != b)
    print("nailpistol_lava: ok, the gun %d texels changed, the magazine %d" %
          (changed, sum(1 for a, b in zip(mag, m.skins[0][2][0]) if a != b)))
    if "--preview" in sys.argv:
        from PIL import Image
        path = sys.argv[sys.argv.index("--preview") + 1]
        w, h = m.h[13], m.h[14]
        o = Image.new("RGB", (w * 3 + 8, h))
        for n, data in enumerate((m.skins[0][2][0], gun)):
            im = Image.new("RGB", (w, h))
            im.putdata([qpal.PALETTE[i] for i in data])
            o.paste(im, (n * (w + 4), 0))
        im = Image.new("RGB", (mm.h[13], mm.h[14]))
        im.putdata([qpal.PALETTE[i] for i in mag])
        o.paste(im, (2 * (w + 4), 0))
        o.resize((o.width * 2, o.height * 2), Image.NEAREST).save(path)


if __name__ == "__main__":
    main()
