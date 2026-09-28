# make_spawn_buttons.py -- builds the brush models of vrfiringrange's second row of monster buttons.
#
# vrfiringrange's first five monster buttons (soldier to scrag) are brush entities of the map itself: a
# tech10_1 panel against the west railing, facing east, with a door02_1 / +0basebtn button on it. The
# other monsters' buttons stand on the other side, along the platform's east edge, facing west; they
# are added by the entity file (quakevr/maps/vrfiringrange.ent), which cannot add brushes, so each is
# two entities drawing a small external brush model, as the ammo boxes do (maps/b_*.bsp):
#
#   maps/vr_spawnpanel.bsp   the panel: 16 x 64 x 80 units, its front face at x 0 facing -x (west),
#                            centred on y 0, standing on z 0 (a func_wall);
#   maps/vr_spawnbutton.bsp  the button: 8 x 32 x 32 units, its back at x 0, its face (+0basebtn) at
#                            x -8 facing -x, centred on y 0, from z 0 to 32 (a func_button with "angle"
#                            0: pushed into the panel), placed 40 units up the panel as on the map.
#
# The brushes and their texture alignment are the map's own (the soldier button and its panel, moved
# to the origin), turned half a turn about z. Being separate models they cannot share the map's
# lightmap: they are lit here by a flat minimum light and one light above and in front, to about the
# level of the map's buttons (vrfiringrange's sun, relight_quakevr_maps.py, lights the first row's
# faces from the east).
#
#   python Misc/quakevr/make_spawn_buttons.py [--tools DIR] [--wad PATH] [--out quakevr/maps]
#
# Needs ericw-tools (qbsp, light) and a Quake texture wad with tech10_1, door02_1 and the basebtn
# frames (+0basebtn, +1basebtn, +abasebtn).

import argparse
import os
import subprocess
import sys
import tempfile

import genguard

DEFAULT_TOOLS = "C:/OHWorkspace/ericw-tools-2.0.0-alpha11-win64"
DEFAULT_WAD = "C:/OHWorkspace/TrenchBroom/Q.wad"

PANEL = """\
( -16 32 0 ) ( -16 32 80 ) ( -16 -32 80 ) tech10_1 4 56.351 0 1 1
( 0 -32 80 ) ( 0 -32 0 ) ( -16 -32 0 ) tech10_1 44 56.351 0 1 1
( -16 -32 0 ) ( 0 -32 0 ) ( 0 32 0 ) tech10_1 44 12 0 1 1
( 0 32 80 ) ( 0 -32 80 ) ( -16 -32 80 ) tech10_1 44 12 0 1 1
( 0 32 0 ) ( 0 32 80 ) ( -16 32 80 ) tech10_1 44 56.351 0 1 1
( 0 32 0 ) ( 0 -32 0 ) ( 0 -32 80 ) tech10_1 32 60.697 0 1 1.26556
"""

BUTTON = """\
( 0 16 0 ) ( 0 16 32 ) ( 0 -16 32 ) door02_1 32 32.351 0 1 1
( 8 -16 32 ) ( 8 -16 0 ) ( 0 -16 0 ) door02_1 24 32.351 0 1 1
( 0 -16 0 ) ( 8 -16 0 ) ( 8 16 0 ) door02_1 24 20 0 1 1
( 8 16 32 ) ( 8 -16 32 ) ( 0 -16 32 ) door02_1 24 20 0 1 1
( 8 16 0 ) ( 8 16 32 ) ( 0 16 32 ) door02_1 24 32.351 0 1 1
( 8 16 0 ) ( 8 -16 0 ) ( 8 -16 32 ) +0basebtn 48 0.351 0 1 1
"""

# The map's brushes face +x (east); the second row faces -x (west).
def turned(brush):
    """A brush turned half a turn about z (x and y negated; the planes' point order is kept)."""
    lines = []
    for line in brush.strip().splitlines():
        points, rest = line.rsplit(")", 1)
        turned_points = []
        for point in points.split(")"):
            x, y, z = (float(v) for v in point.strip().lstrip("(").split())
            turned_points.append("( %g %g %g )" % (-x or 0.0, -y or 0.0, z))
        lines.append(" ".join(turned_points) + rest)
    return "".join(line + "\n" for line in lines)


# name: (brush, light origin): the light in front of the face (-x) and above its top.
MODELS = {
    "vr_spawnpanel": (turned(PANEL), "-96 0 128"),
    "vr_spawnbutton": (turned(BUTTON), "-104 0 88"),
}

MINLIGHT = "70"
LIGHT = "160"


def map_text(brush, light_origin, wad):
    return (
        '{\n"classname" "worldspawn"\n"wad" "%s"\n"_minlight" "%s"\n{\n%s}\n}\n'
        '{\n"classname" "light"\n"origin" "%s"\n"light" "%s"\n"delay" "2"\n}\n'
        % (wad, MINLIGHT, brush, light_origin, LIGHT)
    )


def main():
    parser = argparse.ArgumentParser(description="Builds vrfiringrange's monster button brush models.")
    parser.add_argument("--tools", default=DEFAULT_TOOLS, help="ericw-tools folder (qbsp.exe, light.exe)")
    parser.add_argument("--wad", default=DEFAULT_WAD, help="texture wad")
    parser.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "..", "quakevr", "maps"))
    args = parser.parse_args()

    qbsp = os.path.join(args.tools, "qbsp.exe")
    light = os.path.join(args.tools, "light.exe")
    # The files edited by hand since this wrote them are not overwritten (genguard.py: --keep-edited, --force).
    guard = genguard.Guard("make_spawn_buttons.py", [os.path.join(args.out, n + ".bsp") for n in MODELS])
    with tempfile.TemporaryDirectory() as tmp:
        for name, (brush, light_origin) in MODELS.items():
            src = os.path.join(tmp, name + ".map")
            bsp = os.path.join(tmp, name + ".bsp")
            with open(src, "w", newline="\n") as f:
                f.write(map_text(brush, light_origin, args.wad.replace("\\", "/")))
            # -nofill: a lone brush in the void, like the ammo boxes' models (no outside to fill). -threads 1: the
            # same file every time (with threads, light.exe lays the lightmaps out in the order they finish).
            for cmd in ([qbsp, "-nofill", src, bsp], [light, "-threads", "1", "-extra4", bsp]):
                result = subprocess.run(cmd, capture_output=True, text=True)
                if result.returncode:
                    sys.exit("%s failed:\n%s%s" % (os.path.basename(cmd[0]), result.stdout, result.stderr))
            dest = os.path.join(args.out, name + ".bsp")
            with open(bsp, "rb") as f, open(dest, "wb") as g:
                g.write(f.read())
            print("wrote", os.path.normpath(dest))
    guard.finish()


if __name__ == "__main__":
    main()
