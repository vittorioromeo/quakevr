#!/usr/bin/env python3
# make_grenade_skins.py -- adds skin 1 to quakevr/progs/grenade.mdl, progs/mervup.mdl (the mission pack's
# multi-grenade) and progs/proxbomb.mdl (the other mission pack's proximity grenade; since 2026-10-08, ROUND21.md,
# "Grenades back to the original models": the pouches' grenades are these again): the hand grenade with its pin still in (vr_grenade.qc VR_HGREN_SKIN_UNARMED; docs/vr-port/ROUND21.md,
# "Hand grenades: unarmed look", "Multi-grenades from the pouch"). Skin 0, the live grenade's (dark brown iron, a glowing
# red band; the multi-grenade's brown shell and glowing amber caps), is kept; skin 1 is it muted: the iron its grey (the
# palette's grey ramp: a nearest colour over the whole palette turned the dark browns teal), the band a dull, unlit brick
# red (no fullbright texels: the band glows only once the fuse runs). The engine leaves skin 1 no smoke trail
# (vr_particles.cpp VR_GrenadeTrail).
#
# Usage: python Misc/quakevr/make_grenade_skins.py [progs folder] [--saturation S] [--band-light L]
#
# Idempotent: skin 0 is kept, any other skins are replaced. It stops if skin 1 was painted by hand since it wrote it
# (genguard.py: --keep-edited, --force).

import os
import struct
import sys

import genguard
from mdlgen import HEADER, read_skins

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "blender", "addons", "quakevr_models"))
from qpal import PALETTE  # noqa: E402

SATURATION = 0.3   # of the band's own colour, the rest its grey (Rec. 601 luma)
BAND_LIGHT = 0.9   # the fullbright band's lightness kept (unlit, it would otherwise read brighter than the iron)
FULLBRIGHT = 224   # palette indices from here glow
GREYS = range(16)  # the palette's grey ramp, black to white
GREY_GAIN, GREY_LIFT = 1.4, 16  # the iron's grey a little lighter than its brown (dull steel, not black)


def arg(name, default):
    if name in sys.argv:
        i = sys.argv.index(name)
        value = float(sys.argv[i + 1])
        del sys.argv[i:i + 2]
        return value
    return default


def nearest(rgb, among):
    """The nearest palette index of `among`."""
    return min(among, key=lambda i: sum((PALETTE[i][k] - rgb[k]) ** 2 for k in range(3)))


def muted_index(index, saturation, band_light, gain, lift):
    r, g, b = PALETTE[index]
    grey = 0.299 * r + 0.587 * g + 0.114 * b
    if index < FULLBRIGHT:
        grey = min(255.0, grey * gain + lift)
        return nearest((grey, grey, grey), GREYS)
    r, g, b, grey = r * band_light, g * band_light, b * band_light, grey * band_light
    return nearest(tuple(grey + saturation * (c - grey) for c in (r, g, b)), range(FULLBRIGHT))


# Each model and its iron's grey (gain, lift): the multi-grenade's shell is a pale lavender, grey a little darker than
# it (lighter, it read brighter than the live one); the proximity grenade's dark brown iron as the grenade's.
MODELS = (("grenade.mdl", GREY_GAIN, GREY_LIFT), ("mervup.mdl", 0.75, 0), ("proxbomb.mdl", GREY_GAIN, GREY_LIFT))


def mute(path, saturation, band_light, gain, lift):
    guard = genguard.Guard("make_grenade_skins.py", [path], part=genguard.MDL_SKINS_AFTER_0)

    data = open(path, "rb").read()
    h = list(HEADER.unpack_from(data, 0))
    num_skins, w, hgt = h[12], h[13], h[14]
    skins, off = read_skins(data, HEADER.size, num_skins, w, hgt)
    base = skins[0]
    (group,) = struct.unpack_from("<i", base, 0)
    if group != 0:
        raise SystemExit("%s: skin 0 is a group; not supported" % path)
    table = [muted_index(i, saturation, band_light, gain, lift) for i in range(256)]
    muted = bytes(table[p] for p in base[4:4 + w * hgt])

    h[12] = 2
    with open(path, "wb") as f:
        f.write(HEADER.pack(*h))
        f.write(base)
        f.write(struct.pack("<i", 0) + muted)
        f.write(data[off:])
    guard.finish()
    band = sorted({table[i] for i in set(base[4:4 + w * hgt]) if i >= FULLBRIGHT})
    print("%s: 2 skins (%dx%d); skin 1 grey, the band's fullbrights (saturation %.2f) to %s" % (
        os.path.basename(path), w, hgt, saturation, ", ".join("%d %s" % (i, PALETTE[i]) for i in band)))


def main():
    saturation = arg("--saturation", SATURATION)
    band_light = arg("--band-light", BAND_LIGHT)
    folder = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", "quakevr", "progs")
    for name, gain, lift in MODELS:
        mute(os.path.join(folder, name), saturation, band_light, gain, lift)


if __name__ == "__main__":
    main()
