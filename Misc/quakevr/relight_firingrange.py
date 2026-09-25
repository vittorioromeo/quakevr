#!/usr/bin/env python3
# relight_firingrange.py -- re-lights Quake VR's own firing range (quakevr/maps/vrfiringrange.bsp)
# with ericw-tools' `light`, in place.
#
# The map was lit with seventeen "light" 1200 point lights floating above an open-air platform: their
# ranges overlapped everywhere, so almost the whole lightmap sat at its 255 ceiling (twice normal
# brightness with overbright lighting), which washed the yellow-brown textures out into a glaring,
# oversaturated yellow and left no shading to read shapes, targets or the training dummy by. The
# platform is outdoors under a sky box, so it is now lit as outdoors: a sun at an angle (hard-ish
# shadows from the railings, the tower and the stands; lit and unlit sides of things differ), the
# sky dome filling the shade, ambient occlusion in corners (-dirt), no bounced light and no point
# lights (so models are shaded from Quake's fixed direction: vr_model_lighting has no map lights
# here). White light: no .lit is needed.
#
# The sun keys go into the worldspawn, and the old light entities out, of both the map's entity file
# (vrfiringrange.ent, which the engine loads in place of the .bsp's entities) and the .bsp's own
# entity lump, so both describe the lighting the lightmap was made with; the other entities (targets, dispensers, the dummy, world texts, items) are kept as
# they are. Geometry is untouched; running it again gives the same lighting (`light` is multi-threaded,
# so where each face's lightmap sits in the file may differ).
#
# Usage:
#   python Misc/quakevr/relight_firingrange.py [--light path/to/light.exe] [--maps quakevr/maps]
# ericw-tools v0.18.1 was used (https://github.com/ericwa/ericw-tools/releases); `light` may also be
# given by the ERICW_LIGHT environment variable or found on PATH.

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

from relight_maps import entities_text, with_entities

# Smooth shadows, gentle ambient occlusion (an open space: corners only), no bounce.
LIGHT_ARGS = ["-extra4", "-dirt", "-dirtscale", "1.0", "-dirtdepth", "96"]

# Worldspawn keys for `light` (keys starting with "_" are ignored by the game).
SUN = [
    # `light` halves light values (its default -range 0.5). About 150 of 255 on the sunlit floor
    # and 90..100 in its shade, 100..115 on walls facing the sun and 50..60 on the others: with the
    # port's lightmap contrast (vr_light_contrast 2, full light at 128) that is bright daylight
    # without clipping, and models lit on a par with the floor (vr_model_light_parity), such as the
    # training dummy, do not glare.
    ("_sunlight", "150"),
    ("_sun_mangle", "200 -40 0"),      # from the east-north-east, behind the player at the start:
                                       # the target signs and the dummy's front are sunlit
    ("_sunlight_penumbra", "3"),       # soft shadow edges
    ("_sunlight_dirt", "-1"),          # direct sun has its shadows: no ambient occlusion on it
    ("_sunlight2", "280"),             # the sky dome: shade stays readable, not black
]

# Point lights to add (origin, value); none: the sun and the sky light the platform.
LIGHTS = []

ENTITY = re.compile(r"\{[^{}]*\}", re.S)
KEY = re.compile(r'"([^"]*)"\s+"([^"]*)"')


def relit_entities(text):
    """The entity file's text with its light entities replaced by LIGHTS and the sun in worldspawn."""
    out = []
    for block in ENTITY.findall(text):
        keys = dict(KEY.findall(block))
        if keys.get("classname") == "light":
            continue
        if keys.get("classname") == "worldspawn":
            lines = [l for l in block.strip("{}").strip().split("\n")
                     if not any(l.startswith('"%s"' % k) for k, _ in SUN)]
            block = "{\n" + "\n".join(lines + ['"%s" "%s"' % kv for kv in SUN]) + "\n}"
        out.append(block)
        if keys.get("classname") == "worldspawn":
            for origin, value in LIGHTS:
                out.append('{\n"classname" "light"\n"origin" "%s"\n"light" "%s"\n}' % (origin, value))
    return "\n".join(out) + "\n"


def find_light(given):
    for candidate in (given, os.environ.get("ERICW_LIGHT"), shutil.which("light")):
        if candidate and os.path.isfile(candidate):
            return candidate
    sys.exit("light.exe not found: pass --light or set ERICW_LIGHT")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--light")
    parser.add_argument("--maps", default=os.path.join(here, "..", "..", "quakevr", "maps"))
    args = parser.parse_args()
    light = find_light(args.light)
    bsp_path = os.path.join(args.maps, "vrfiringrange.bsp")
    ent_path = os.path.join(args.maps, "vrfiringrange.ent")

    with open(ent_path, newline="") as f:
        ents = relit_entities(f.read())
    with open(bsp_path, "rb") as f:
        data = f.read()

    with tempfile.TemporaryDirectory() as tmp:
        work = os.path.join(tmp, "vrfiringrange.bsp")
        with open(work, "wb") as f:
            f.write(with_entities(data, ents))
        result = subprocess.run([light] + LIGHT_ARGS + [work], cwd=tmp, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, errors="replace")
        if result.returncode != 0:
            print(result.stdout[-3000:])
            sys.exit("light failed")
        with open(work, "rb") as f:
            relit = f.read()
        if entities_text(relit) != ents:
            relit = with_entities(relit, ents)
        lit = os.path.join(tmp, "vrfiringrange.lit")
        if os.path.isfile(lit):
            sys.exit("light made a .lit: the lights are meant to be white")

    with open(bsp_path, "wb") as f:
        f.write(relit)
    with open(ent_path, "w", newline="\n") as f:
        f.write(ents)
    print("relit", os.path.normpath(bsp_path), "and", os.path.normpath(ent_path))


if __name__ == "__main__":
    main()
