# make_prop_area.py -- builds vrfiringrange's prop area: a table with every rock and brick on it, a wall with wall
# torches, and the two explosive boxes, for trying the held props and tuning them (Held Object Offsets) in the map
# where everything else is tested (NOTES.md start_2026-09-29_23-01-20; docs/vr-port/ROUND21.md, "Bricks in the palm;
# the grenade pouch's turn; the firing range's prop area").
#
# vrfiringrange.bsp has no source that matches it any more (its entities are quakevr/maps/vrfiringrange.ent, which the
# engine loads in place of the .bsp's), so, as make_spawn_buttons.py does for the second row of monster buttons, the
# area's brushes are small external brush models drawn by func_wall entities of the entity file:
#
#   maps/vr_proptable.bsp  the table: 296 x 40 units, 32 high (a 4-unit qvr_panel top over a qvr_wall body), centred
#                          on its origin in x and y, standing on z 0;
#   maps/vr_propwall.bsp   the torches' wall: 8 x 112 units, 96 high with a trim on top, its front face at x 0 facing
#                          +x (east), centred on y 0, standing on z 0.
#
# Their textures are quakevr_dev.wad's (distributable: a .bsp embeds the textures it uses). Being separate models they
# have their own lightmaps: a flat minimum light and lights above and in front, about as bright as the platform in the
# map's sun (relight_quakevr_maps.py).
#
# The entities go at the end of vrfiringrange.ent, after every other: no entity before them is renumbered (the motion
# takes find the training dummy as entity 137, docs/vr-port/MOTIONS.md). Each has a "_proparea" key (keys starting with
# "_" are ignored by the game), by which running this again replaces them rather than adding more. The area is in the
# platform's south-west corner, between the first row of monster buttons and the weapon pads, 500 units from the dummy:
# the table (x -564..-268, y -820..-780) runs east-west, reached from its north side, the rocks on its west half and the
# bricks on its east half; the wall stands against the west railing beside the table's west end, three torches on it
# at chest height; the explosive boxes stand past the table's east end.
#
#   python Misc/quakevr/make_prop_area.py [--tools DIR] [--wad PATH] [--maps quakevr/maps] [--ent-only]
#
# Needs ericw-tools (qbsp, light); --ent-only rewrites only the entities.

import argparse
import os
import re
import subprocess
import sys
import tempfile

import genguard

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_TOOLS = "C:/OHWorkspace/ericw-tools-2.0.0-alpha11-win64"
DEFAULT_WAD = os.path.normpath(os.path.join(HERE, "..", "..", "quakevr", "wads", "quakevr_dev.wad"))
DEFAULT_MAPS = os.path.normpath(os.path.join(HERE, "..", "..", "quakevr", "maps"))

FLOOR = 17  # the platform's floor (sfloor4_2's top)


def box(x0, y0, z0, x1, y1, z1, tex, top=None):
    """An axis-aligned brush (Quake's standard format): `tex` on its sides and bottom, `top` (or `tex`) on its top."""
    top = top or tex
    faces = [
        ((x0, y1, z0), (x0, y1, z1), (x0, y0, z1), tex),  # -x
        ((x1, y0, z1), (x1, y0, z0), (x0, y0, z0), tex),  # -y
        ((x0, y0, z0), (x1, y0, z0), (x1, y1, z0), tex),  # bottom
        ((x1, y1, z1), (x1, y0, z1), (x0, y0, z1), top),  # top
        ((x1, y1, z0), (x1, y1, z1), (x0, y1, z1), tex),  # +y
        ((x1, y1, z0), (x1, y0, z0), (x1, y0, z1), tex),  # +x
    ]
    return "{\n%s}\n" % "".join(
        "%s %s 0 0 0 1 1\n" % (" ".join("( %g %g %g )" % p for p in (a, b, c)), t) for a, b, c, t in faces)


# name: (brushes, [light origins], light). The wall's lights are dimmer: its torches' flames light it too.
MODELS = {
    "vr_proptable": (
        box(-148, -20, 28, 148, 20, 32, "qvr_trim", "qvr_panel") + box(-140, -14, 0, 140, 14, 28, "qvr_wall"),
        ["-80 72 112", "80 72 112"],
        160,
    ),
    "vr_propwall": (
        box(-8, -56, 0, 0, 56, 96, "qvr_wall") + box(-10, -58, 96, 2, 58, 100, "qvr_trim"),
        ["72 0 112"],
        60,
    ),
}

MINLIGHT = "70"


def map_text(brushes, lights, light, wad):
    text = '{\n"classname" "worldspawn"\n"wad" "%s"\n"_minlight" "%s"\n%s}\n' % (wad, MINLIGHT, brushes)
    for origin in lights:
        text += '{\n"classname" "light"\n"origin" "%s"\n"light" "%s"\n"delay" "2"\n}\n' % (origin, light)
    return text


TABLE = (-416, -800)  # its middle; its top at FLOOR + 32
WALL = (-600, -760)   # its front face's middle (against the west railing, whose posts end at x -608)

ROCKS = ["progs/vr_rock%d.mdl" % i for i in range(1, 6)]
BRICKS = ["progs/vr_brick%d.mdl" % i for i in range(1, 5)]


def entities():
    """The area's entities, in order: each a list of (key, value)."""
    out = []
    top = FLOOR + 32
    out.append([("classname", "func_wall"), ("model", "maps/vr_proptable.bsp"), ("origin", "%g %g %g" % (TABLE[0], TABLE[1], FLOOR))])
    out.append([("classname", "func_wall"), ("model", "maps/vr_propwall.bsp"), ("origin", "%g %g %g" % (WALL[0], WALL[1], FLOOR))])
    # The rocks from the table's west end, 30 units apart, each turned a little more; then the bricks, across the table.
    for i, model in enumerate(ROCKS):
        out.append([("classname", "vr_debris_piece"), ("model", model), ("origin", "%g %g %g" % (-536 + 30 * i, TABLE[1], top + 6)),
                    ("angle", "%d" % (40 * i))])
    for i, model in enumerate(BRICKS):
        out.append([("classname", "vr_debris_piece"), ("model", model), ("origin", "%g %g %g" % (-380 + 29 * i, TABLE[1], top + 6)),
                    ("angle", "90")])
    # The torches 10 units off the wall (a wall torch's box is 20 wide), at chest height, 32 apart.
    for dy in (32, 0, -32):
        out.append([("classname", "light_torch_small_walltorch"), ("origin", "%g %g %g" % (WALL[0] + 10, WALL[1] + dy, FLOOR + 58))])
    # The explosive boxes (their origin is their corner; they stand on the floor), past the table's east end.
    out.append([("classname", "misc_explobox"), ("origin", "-240 -808 %g" % FLOOR)])
    out.append([("classname", "misc_explobox2"), ("origin", "-240 -864 %g" % FLOOR)])
    # Labels: on the table's north face, facing north (the side it is reached from), and on the wall above the torches.
    for text, x in (("rocks", -476), ("bricks", -336)):
        out.append([("classname", "func_worldtext_banner"), ("origin", "%g %g %g" % (x, TABLE[1] + 21, FLOOR + 16)),
                    ("angle", "90"), ("worldtext", text), ("worldtext_halign", "1"), ("worldtext_scale", "0.2")])
    out.append([("classname", "func_worldtext_banner"), ("origin", "%g %g %g" % (WALL[0] + 1, WALL[1], FLOOR + 86)),
                ("angle", "0"), ("worldtext", "torches"), ("worldtext_halign", "1"), ("worldtext_scale", "0.25")])
    out.append([("classname", "func_worldtext_banner"), ("origin", "%g %g %g" % (TABLE[0], TABLE[1] + 48, FLOOR + 72)),
                ("angle", "90"), ("worldtext", "props: tune them in Held Object Offsets"), ("worldtext_halign", "1"),
                ("worldtext_scale", "0.3")])
    return out


def write_entities(path):
    with open(path, newline="") as f:
        text = f.read()
    blocks = re.findall(r"\{[^{}]*\}", text)
    kept = [b for b in blocks if '"_proparea"' not in b]
    added = ["{\n%s}" % "".join('"%s" "%s"\n' % kv for kv in keys + [("_proparea", "1")]) for keys in entities()]
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(kept + added) + "\n")
    print("wrote", os.path.normpath(path), "(%d entities, %d of the prop area)" % (len(kept) + len(added), len(added)))


def main():
    parser = argparse.ArgumentParser(description="Builds vrfiringrange's prop area.")
    parser.add_argument("--tools", default=DEFAULT_TOOLS, help="ericw-tools folder (qbsp.exe, light.exe)")
    parser.add_argument("--wad", default=DEFAULT_WAD, help="texture wad (quakevr_dev.wad)")
    parser.add_argument("--maps", default=DEFAULT_MAPS, help="the maps folder (quakevr/maps)")
    parser.add_argument("--ent-only", action="store_true", help="only rewrite vrfiringrange.ent's prop area")
    args = parser.parse_args()

    if not args.ent_only:
        qbsp = os.path.join(args.tools, "qbsp.exe")
        light = os.path.join(args.tools, "light.exe")
        guard = genguard.Guard("make_prop_area.py", [os.path.join(args.maps, n + ".bsp") for n in MODELS])
        with tempfile.TemporaryDirectory() as tmp:
            for name, (brushes, lights, light_value) in MODELS.items():
                src = os.path.join(tmp, name + ".map")
                bsp = os.path.join(tmp, name + ".bsp")
                with open(src, "w", newline="\n") as f:
                    f.write(map_text(brushes, lights, light_value, args.wad.replace("\\", "/")))
                # -nofill: brushes in the void (no outside to fill). -threads 1: the same file every time.
                for cmd in ([qbsp, "-nofill", src, bsp], [light, "-threads", "1", "-extra4", bsp]):
                    result = subprocess.run(cmd, capture_output=True, text=True)
                    if result.returncode:
                        sys.exit("%s failed:\n%s%s" % (os.path.basename(cmd[0]), result.stdout, result.stderr))
                dest = os.path.join(args.maps, name + ".bsp")
                with open(bsp, "rb") as f, open(dest, "wb") as g:
                    g.write(f.read())
                print("wrote", os.path.normpath(dest))
        guard.finish()
    write_entities(os.path.join(args.maps, "vrfiringrange.ent"))


if __name__ == "__main__":
    main()
