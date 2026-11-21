# make_vralert_map.py -- writes quakevr/maps/vralert.map, the test map of the stealth AI's spread of an alarm (monster to
# monster: docs/vr-port/STEALTH.md, "Hostile and Alert propagation"; `vr_stealth_test 130`, QC vr_stealth_test5.qc), and
# with --compile builds it (qbsp, vis, light) and copies the .bsp/.lit/.lux beside it. quakevr_dev.wad textures only.
#
#   python Misc/quakevr/make_vralert_map.py [--compile] [--tools DIR] [--work DIR]
#
# Grunts placed round one, V (the one shot), each with what it should do when V is hurt or killed by the player:
#   N  176 units from V (west), in the open, facing away:     Hostile (near, in line)
#   S  640 units from V, facing it:                           Hostile (sees it)
#   A  192 units from V, facing it, placed in ambush (deaf):  stays Idle (Quake's ambush: no relay from other monsters)
#   W  340 units from V behind a wall, by a doorway:          Alert at V (hears its cry round the wall: in its PVS)
#   F  in a sealed room 1150 units off:                       stays Idle (out of earshot)
# The hall is lit; the player starts at its west end behind S. The test puts him in a sealed booth (BOOTH) first.
# Keep the positions in step with vr_stealth_test5.qc's VR_STT5_Home.
import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
DEFAULT_TOOLS = "C:/OHWorkspace/ericw-tools-2.0.0-alpha11-win64"
NAME = "vralert"
OUT = os.path.join(ROOT, "quakevr", "maps", NAME + ".map")


def box(x0, y0, z0, x1, y1, z1, tex, top=None):
    top = top or tex
    return "\n".join([
        "{",
        f"( {x0} {y0} {z0} ) ( {x0} {y0+1} {z0} ) ( {x0} {y0} {z0+1} ) {tex} [ 0 -1 0 0 ] [ 0 0 -1 0 ] 0 1 1",
        f"( {x0} {y0} {z0} ) ( {x0} {y0} {z0+1} ) ( {x0+1} {y0} {z0} ) {tex} [ 1 0 0 0 ] [ 0 0 -1 0 ] 0 1 1",
        f"( {x0} {y0} {z0} ) ( {x0+1} {y0} {z0} ) ( {x0} {y0+1} {z0} ) {tex} [ -1 0 0 0 ] [ 0 -1 0 0 ] 0 1 1",
        f"( {x1} {y1} {z1} ) ( {x1} {y1+1} {z1} ) ( {x1+1} {y1} {z1} ) {top} [ 1 0 0 0 ] [ 0 -1 0 0 ] 0 1 1",
        f"( {x1} {y1} {z1} ) ( {x1+1} {y1} {z1} ) ( {x1} {y1} {z1+1} ) {tex} [ -1 0 0 0 ] [ 0 0 -1 0 ] 0 1 1",
        f"( {x1} {y1} {z1} ) ( {x1} {y1} {z1+1} ) ( {x1} {y1+1} {z1} ) {tex} [ 0 1 0 0 ] [ 0 0 -1 0 ] 0 1 1",
        "}"])


def entity(keys, brushes=()):
    return "{\n" + "".join(f'"{k}" "{v}"\n' for k, v in keys.items()) + "".join(b + "\n" for b in brushes) + "}"


B = []
E = []
WALL, FLOOR, TRIM, PANEL = "qvr_wall", "qvr_floor", "qvr_trim", "qvr_panel"


def room(x0, y0, x1, y1, z1):
    """A sealed room: its floor (z 0), ceiling and four walls, 16 thick, outside the interior x0..x1, y0..y1."""
    B.append(box(x0 - 16, y0 - 16, z1, x1 + 16, y1 + 16, z1 + 16, "qvr_ceiling"))
    B.append(box(x0 - 16, y0 - 16, -16, x1 + 16, y1 + 16, 0, WALL, FLOOR))
    B.append(box(x0 - 16, y0 - 16, 0, x0, y1 + 16, z1, WALL))
    B.append(box(x1, y0 - 16, 0, x1 + 16, y1 + 16, z1, WALL))
    B.append(box(x0, y0 - 16, 0, x1, y0, z1, WALL))
    B.append(box(x0, y1, 0, x1, y1 + 16, z1, WALL))


def board(text, x, y, z, angle, scale="0.35"):
    E.append(entity({"classname": "func_worldtext_banner", "worldtext": text.replace("\n", "\\n"), "worldtext_halign": "1",
                     "worldtext_scale": scale, "angle": str(angle), "origin": f"{x} {y} {z}"}))


def grunt(x, y, yaw, flags=0):
    keys = {"classname": "monster_army", "origin": f"{x} {y} 24", "angle": str(yaw)}
    if flags:
        keys["spawnflags"] = str(flags)
    E.append(entity(keys))


Z1 = 256
# ---- the hall (north, y 16..448) and room B (south, y -320..0): one room split by a wall at y 0..16, a doorway in it
room(-768, -320, 768, 448, Z1)
B.append(box(-768, 0, 0, 192, 16, Z1, WALL))
B.append(box(320, 0, 0, 768, 16, Z1, WALL))
B.append(box(192, 0, 128, 320, 16, Z1, WALL))
# ---- the far room (east, sealed) and the booth (west, sealed: the test puts the player there, out of everyone's sight)
room(1024, 16, 1280, 448, Z1)
room(-1408, 128, -1216, 320, 160)

grunt(0, 128, 0)          # V
grunt(-176, 128, 180)     # N
grunt(-640, 128, 0)       # S
grunt(0, 320, 270, 1)     # A (ambush)
grunt(256, -96, 90)       # W
grunt(1152, 232, 180)     # F
for (x, y) in ((0, 128), (-176, 128), (-640, 128), (0, 320), (256, -96)):
    B.append(box(x - 24, y - 24, 0, x + 24, y + 24, 1, TRIM, PANEL))

board("ALARM TEST: shoot V (the middle grunt)\n"
      "N (near) and S (sees V): attack you", -300, 440, 180, 270)
board("A (ambush, deaf): stays put\n"
      "W (behind the wall): hears V, comes to look\n"
      "F (sealed room east): never hears", -300, 440, 130, 270)
board("V", 0, 160, 96, 270, "0.5")
board("N", -176, 160, 96, 270, "0.5")
board("S", -640, 160, 96, 270, "0.5")
board("A (ambush)", 0, 352, 96, 270, "0.5")
board("W (behind the wall)", 256, -64, 96, 270, "0.5")

E.append(entity({"classname": "info_player_start", "origin": "-736 40 24", "angle": "0"}))
for (x, y) in [(x, y) for x in range(-640, 641, 320) for y in (-160, 232)] + [(1152, 232)]:
    B.append(box(x - 48, y - 8, Z1 - 4, x + 48, y + 8, Z1, "qvr_light"))
    E.append(entity({"classname": "light", "origin": f"{x} {y} {Z1 - 40}", "light": "240", "wait": "0.5", "_color": "1 0.96 0.9"}))
E.append(entity({"classname": "light", "origin": "-1312 224 120", "light": "150"}))


def write(path):
    with open(path, "w", newline="\n") as f:
        f.write('// Game: Quake VR\n// Format: Valve\n// Written by Misc/quakevr/make_vralert_map.py: edit that, not this.\n'
                '// entity 0\n{\n"classname" "worldspawn"\n"mapversion" "220"\n'
                '"wad" "quakevr/wads/quakevr_dev.wad"\n"_tb_mod" "hipnotic;rogue;quakevr"\n'
                '"message" "Alarm Test"\n"worldtype" "2"\n"light" "50"\n"_vr_debris" "0"\n"_qvr_prelit" "1"\n')
        f.write("\n".join(B) + "\n}\n")
        for i, e in enumerate(E):
            f.write(f"// entity {i + 1}\n{e}\n")


def compile_map(tools, work):
    os.makedirs(work, exist_ok=True)
    src = os.path.join(work, NAME + ".map")
    bsp = os.path.join(work, NAME + ".bsp")
    shutil.copyfile(OUT, src)
    for cmd in ([os.path.join(tools, "qbsp.exe"), "-nolog", "-nopercent", "-wadpath", ROOT, src, bsp],
                [os.path.join(tools, "vis.exe"), "-nolog", "-nopercent", bsp],
                [os.path.join(tools, "light.exe"), "-nolog", "-nopercent", "-extra4", "-dirt", "-dirtscale", "1.5",
                 "-dirtdepth", "96", "-bounce", "-lit", "-lux", "-lightgrid", "-lightgrid_dist", "32", "32", "32", bsp]):
        result = subprocess.run(cmd, capture_output=True, text=True)
        lines = (result.stdout + result.stderr).splitlines()
        warnings = [l for l in lines if "WARNING" in l.upper() or "ERROR" in l.upper() or "LEAK" in l.upper()]
        print(f"{os.path.basename(cmd[0])}: exit {result.returncode}" + "".join("\n  " + w for w in warnings[:12]))
        if result.returncode:
            sys.exit(1)
    for ext in (".bsp", ".lit", ".lux"):
        shutil.copyfile(os.path.join(work, NAME + ext), os.path.join(os.path.dirname(OUT), NAME + ext))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compile", action="store_true")
    parser.add_argument("--tools", default=DEFAULT_TOOLS)
    parser.add_argument("--work", default=os.path.join(ROOT, "scratch", NAME + "_build"))
    args = parser.parse_args()
    write(OUT)
    print(f"wrote {OUT}: {len(B)} brushes, {len(E)} entities")
    if args.compile:
        compile_map(args.tools, args.work)


if __name__ == "__main__":
    main()
