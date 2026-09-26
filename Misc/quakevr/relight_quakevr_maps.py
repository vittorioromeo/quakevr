#!/usr/bin/env python3
# relight_quakevr_maps.py -- re-lights Quake VR's own maps (quakevr/maps/*.bsp) with ericw-tools'
# `light`, in place, each with its own settings (MAPS below).
#
# vrfiringrange: lit with seventeen "light" 1200 point lights floating above an open-air platform,
# whose ranges overlapped everywhere, so almost the whole lightmap sat at its 255 ceiling (twice normal
# brightness with overbright lighting): a glaring, oversaturated yellow with no shading to read shapes,
# targets or the training dummy by. The platform is outdoors under a sky box, so it is lit as outdoors:
# a sun at an angle (hard-ish shadows from the railings, the tower and the stands), the sky dome filling
# the shade, ambient occlusion in corners (-dirt), no bounced light and no point lights (so models are
# shaded from Quake's fixed direction: vr_model_lighting has no map lights there). White light: no .lit.
#
# vrtutorial: was never lit at all (an empty lightmap lump), so the engine drew every surface at full
# brightness, whatever its 79 light entities said. It is now lit by its own lamps, given a longer reach
# (tutorial_lamp): the strip lights over the tutorial boards, the lamp posts and the ceiling lamps, plus
# the glowing textures (the strip lights, the lamp posts, buttons and the slipgate;
# relight_maps.glow_lights, coloured, .lit), strong ambient occlusion and no bounced light, so that the
# lamps make pools of light and the corners and the space between them stay dark. The sixteen "light"
# 1200 lamps floating 300 units up, over the whole map (a flat fill), are dropped; a faint night sky and
# moon keep the open courtyard readable. The glowing textures' lights are only given to `light` (as in
# relight_maps.py): the map keeps its own entities.
#
# The worldspawn keys go into, and the dropped light entities out of, both the map's entity file
# (<map>.ent, which the engine loads in place of the .bsp's entities, if the map has one) and the .bsp's
# own entity lump, so both describe the lighting the lightmap was made with; every other entity is kept
# as it is. Geometry is untouched. Running it again gives the same lighting (the dropped lights stay
# dropped, the keys are set, not added; `light` is multi-threaded, so where each face's lightmap sits in
# the file may differ).
#
# Usage:
#   python Misc/quakevr/relight_quakevr_maps.py [--light path/to/light.exe] [--maps quakevr/maps]
#       [--quake <Quake folder>] [--only vrtutorial ...] [--out <folder>]
# --quake (or the QUAKE_DIR environment variable) is the folder containing id1, for id1's palette
# (gfx/palette.lmp): the glowing textures' light colours need it. --out writes the results elsewhere
# (for trying settings); the default is in place.
# ericw-tools 2.0.0-alpha11 is used (https://github.com/ericwa/ericw-tools/releases/tag/2.0.0-alpha11; v0.18.1
# before round 17, which gives the same light); `light` may also be given by the ERICW_LIGHT environment
# variable or found on PATH, else relight_maps.DEFAULT_LIGHT. Besides the .lit, `light` writes each map's
# light directions (<map>.lux, deluxemaps: vr_deluxemap) and a grid of the light in the air (the
# LIGHTGRID_OCTREE BSPX lump in the .bsp): relight_maps.OUTPUT_ARGS.

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

import vis_maps
from relight_maps import (OUTPUT_ARGS, entities_text, find_light, glow_lights, id_light_values, light_command, pak_file,
                          remapped_lux, with_entities)

LIGHTGRID_ARGS = ["-lightgrid_dist", "64", "64", "64"]


def tutorial_lamp(keys):
    """vrtutorial's lamps ("light" 200 or 250, linear falloff: `light` - distance * `wait`)."""
    x, y, z = (float(v) for v in keys["origin"].split())
    if z < 60:
        # The four around each lamp post (one fixture): dimmer, a little further, so that the post
        # makes one pool of light instead of a blown-out wall beside it.
        return [("light", "60"), ("wait", "0.5")]
    if x < -270:
        # The strip lights over the boards in the open courtyard: two thirds further, to light
        # the courtyard's floor between them.
        return [("wait", "0.6")]
    # The strip lights over the boards indoors: a third further (the corridors between the boards
    # are not black); the ceiling lamps ("light" 250) as they were (every lamp's keys are set, also
    # to Quake's default, so that a change here replaces what an earlier run of this script set).
    return [("wait", "1")] if keys.get("light") == "250" else [("wait", "0.75")]


MAPS = {
    "vrfiringrange": {
        # Smooth shadows, gentle ambient occlusion (an open space: corners only), no bounce.
        "args": ["-extra4", "-dirt", "-dirtscale", "1.0", "-dirtdepth", "96"],
        # Worldspawn keys for `light` (keys starting with "_" are ignored by the game).
        "worldspawn": [
            # `light` halves light values (its default -range 0.5). About 150 of 255 on the sunlit floor
            # and 90..100 in its shade, 100..115 on walls facing the sun and 50..60 on the others: with
            # the port's lightmap contrast (vr_light_contrast 2, full light at 128) that is bright
            # daylight without clipping, and models lit on a par with the floor (vr_model_light_parity),
            # such as the training dummy, do not glare.
            ("_sunlight", "150"),
            ("_sun_mangle", "200 -40 0"),   # from the east-north-east, behind the player at the start:
                                            # the target signs and the dummy's front are sunlit
            ("_sunlight_penumbra", "3"),    # soft shadow edges
            ("_sunlight_dirt", "-1"),       # direct sun has its shadows: no ambient occlusion on it
            ("_sunlight2", "280"),          # the sky dome: shade stays readable, not black
        ],
        # Light entities dropped: all of them (the sun and the sky light the platform).
        "drop": lambda keys: True,
        "lamp_keys": lambda keys: [],
        "glow": False,
        "lit": False,
    },
    "vrtutorial": {
        # Smooth shadows, strong ambient occlusion (the corridors' corners and the space under the boards
        # go dark), no bounced light (it would fill the shade), coloured light.
        "args": ["-extra4", "-dirt", "-dirtscale", "1.5", "-dirtdepth", "96", "-lit"],
        "worldspawn": [
            # A faint, cool night sky and moon over the open courtyard (indoors, under the roof, they do
            # not reach): about 20..45 of 255 there away from the lamps, which the lightmap contrast
            # (vr_light_contrast 2) shows as dim; the moon from the north-east gives the railings,
            # crates and walls a lit and an unlit side and shadows on the floor.
            ("_sunlight2", "80"),
            ("_sunlight2_color", "150 170 255"),
            ("_sunlight2_dirt", "1"),
            ("_sunlight", "50"),
            ("_sun_mangle", "210 -55 0"),
            ("_sunlight_color", "170 190 255"),
            ("_sunlight_penumbra", "3"),
            ("_sunlight_dirt", "-1"),
        ],
        # Keys set on the lamps kept (chosen by where they are, so that running it again sets the same).
        "lamp_keys": tutorial_lamp,
        # The sixteen "light" 1200 lamps 300 units up over the whole map: a flat fill, no lamp there.
        "drop": lambda keys: float(keys.get("light", "300") or 300) >= 1000,
        "glow": True,
        "lit": True,
        # Its light from ericw-tools v0.18.1 (--legacy-light), the directions and the light grid from 2.0: the map is
        # made of thin trims, and 2.0 samples a strip thinner than a luxel (the 4 to 8 units of wall between each
        # board and the strip light over it) at its edges, where the brush beside it shades them: those strips came
        # out half as bright or black, 0.6% of the map's luxels (0.02% of id's maps'). 0.18 kept them lit.
        "legacy": True,
    },
}

# ericw-tools v0.18.1, for the maps with "legacy" (--legacy-light, or ERICW_LIGHT_LEGACY): without it they are lit
# by 2.0 alone (with a warning).
LEGACY_LIGHT = "C:/OHWorkspace/ericw-tools-v0.18.1-32-g6660c5f-win64/bin/light.exe"

ENTITY = re.compile(r"\{[^{}]*\}", re.S)
KEY = re.compile(r'"([^"]*)"\s+"([^"]*)"')


def with_keys(block, pairs):
    """An entity's text with these keys set (replaced or added at the end)."""
    if not pairs:
        return block
    lines = [l for l in block.strip("{}").strip().split("\n")
             if not any(l.startswith('"%s"' % k) for k, _ in pairs)]
    return "{\n" + "\n".join(lines + ['"%s" "%s"' % kv for kv in pairs]) + "\n}"


def relit_entities(text, settings):
    """The entity text with the map's dropped lights left out, its lamps' and worldspawn keys set."""
    out = []
    for block in ENTITY.findall(text):
        keys = dict(KEY.findall(block))
        if keys.get("classname") == "light":
            if settings["drop"](keys):
                continue
            block = with_keys(block, settings["lamp_keys"](keys))
        elif keys.get("classname") == "worldspawn":
            block = with_keys(block, settings["worldspawn"])
        out.append(block)
    return "\n".join(out) + "\n"


def run_light(command, data, name, tmp):
    """`light` (command: the executable and options) on the map `data` in the folder `tmp`: (.bsp, .lit or None,
    .lux or None)."""
    work = os.path.join(tmp, name + ".bsp")
    with open(work, "wb") as f:
        f.write(data)
    result = subprocess.run(command + [work], cwd=tmp, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                            errors="replace")
    if result.returncode != 0:
        print(result.stdout[-3000:])
        sys.exit("%s: light failed" % name)
    out = []
    for ext in (".bsp", ".lit", ".lux"):
        path = os.path.join(tmp, name + ext)
        if os.path.isfile(path):
            with open(path, "rb") as f:
                out.append(f.read())
        else:
            out.append(None)
    return out


def relight(name, settings, light, maps, out_dir, palette, legacy=None):
    bsp_path = os.path.join(maps, name + ".bsp")
    ent_path = os.path.join(maps, name + ".ent")
    with open(bsp_path, "rb") as f:
        data = f.read()
    has_ent = os.path.isfile(ent_path)
    if has_ent:
        with open(ent_path, newline="") as f:
            source = f.read()
    else:
        source = entities_text(data)
    ents = relit_entities(source, settings)
    # The .bsp's own entities with the same changes (the same as the .ent's, if it has one).
    bsp_ents = ents if has_ent else relit_entities(entities_text(data), settings)
    extra = ""
    if settings["glow"]:
        if palette is None:
            sys.exit("%s: the glowing textures need id1's palette: pass --quake or set QUAKE_DIR" % name)
        extra = glow_lights(data, palette, 1.0, 300.0)

    source_bsp = with_entities(data, id_light_values(ents) + extra)
    with tempfile.TemporaryDirectory() as tmp:
        # (light: `light` and the outputs' options, relight_maps.light_command)
        lit_bsp, lit_data, lux_data = run_light(light[:1] + settings["args"] + light[1:], source_bsp, name, tmp)
    if settings.get("legacy") and legacy:
        # The light and lightmap of the legacy `light`; 2.0's light grid (BSPX) and directions, moved to its layout.
        with tempfile.TemporaryDirectory() as tmp:
            old_bsp, old_lit, _ = run_light([legacy] + settings["args"], source_bsp, name, tmp)
        if lux_data is not None:
            lux_data = remapped_lux(lux_data, lit_bsp, old_bsp)
        lit_bsp, lit_data = vis_maps.packed(old_bsp, bspx=vis_maps.bspx_lumps(lit_bsp)), old_lit
    elif settings.get("legacy"):
        print("%s: no legacy light (v0.18.1, --legacy-light): lit by 2.0 alone, whose light on the thin strips over "
              "the boards is darker" % name)
    relit = with_entities(lit_bsp, bsp_ents)
    if lit_data is not None and not settings["lit"]:
        sys.exit("%s: light made a .lit: the lights are meant to be white" % name)
    if lit_data is None and settings["lit"]:
        sys.exit("%s: light made no .lit" % name)

    os.makedirs(out_dir, exist_ok=True)
    written = [os.path.join(out_dir, name + ".bsp")]
    with open(written[0], "wb") as f:
        f.write(relit)
    if has_ent:
        written.append(os.path.join(out_dir, name + ".ent"))
        with open(written[-1], "w", newline="\n") as f:
            f.write(ents)
    if lit_data is not None:
        written.append(os.path.join(out_dir, name + ".lit"))
        with open(written[-1], "wb") as f:
            f.write(lit_data)
    if lux_data is not None:
        written.append(os.path.join(out_dir, name + ".lux"))
        with open(written[-1], "wb") as f:
            f.write(lux_data)
    print("relit", ", ".join(os.path.normpath(p) for p in written))


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--light", help="ericw-tools light executable")
    parser.add_argument("--maps", default=os.path.join(here, "..", "..", "quakevr", "maps"))
    parser.add_argument("--out", help="where to write the relit maps (default: in place, --maps)")
    parser.add_argument("--quake", default=os.environ.get("QUAKE_DIR"),
                        help="Quake folder (containing id1), for the palette of the glowing textures")
    parser.add_argument("--only", nargs="*", choices=sorted(MAPS), help="maps to relight (default: all)")
    parser.add_argument("--legacy-light", default=os.environ.get("ERICW_LIGHT_LEGACY", LEGACY_LIGHT),
                        help="ericw-tools v0.18.1's light, for the maps whose light it makes (\"legacy\" in MAPS; '' none)")
    args = parser.parse_args()
    # The light grid every 64 units, not 32: these maps are in the repository, and the firing range's open
    # air made a 1.6 MB grid at 32 (0.2 MB at 64; models' ambient light changes slowly in the open).
    light, version = light_command(find_light(args.light), OUTPUT_ARGS.split() + LIGHTGRID_ARGS)
    print("relighting with %s" % version)
    palette = pak_file(os.path.join(args.quake, "id1"), "gfx/palette.lmp") if args.quake else None
    legacy = args.legacy_light if args.legacy_light and os.path.isfile(args.legacy_light) else None
    for name in args.only or sorted(MAPS):
        relight(name, MAPS[name], light, args.maps, args.out or args.maps, palette, legacy)


if __name__ == "__main__":
    main()
