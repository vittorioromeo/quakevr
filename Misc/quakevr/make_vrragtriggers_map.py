# make_vrragtriggers_map.py -- writes quakevr/maps/vrragtriggers.map, a bright test hall for the situational active-ragdoll
# triggers (QC/vr_kdtrig.qc; Combat > Ragdoll Triggers), and with --compile builds it (qbsp, vis, light) and copies the
# .bsp/.lit/.lux beside it. No monsters: spawn them with the toolgun. quakevr_dev.wad textures only.
#
#   python Misc/quakevr/make_vrragtriggers_map.py [--compile] [--tools DIR] [--work DIR]
#
# One hall (2560 x 1792, 448 high; floor z 0; north = +y). The player starts in the middle facing north.
#   - north: four ledge platforms (drops 32, 48, 96, 160) with a stair up each one's east side, standing room at the edge;
#   - north-west: three staircases (16-unit risers; wide with 24 and 16 treads, one narrow) up to landings, 7 steps;
#   - north-east: four ramps (12, 20, 30, 40 degrees, rising north), a flat run-out south of each, a plateau on top;
#   - south: the flat arena: a standing mark and strips every 64 units south of it out to 320 (shotgun range 160, ...);
#   - south-east: explosive boxes at 100, 150, 200 and 300 units from a standing mark, open floor round them.
import argparse
import math
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
DEFAULT_TOOLS = "C:/OHWorkspace/ericw-tools-2.0.0-alpha11-win64"
NAME = "vrragtriggers"
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


def ramp(x0, y0, x1, y1, h, tex, top):
    """A wedge on the floor (z 0) rising along +y from nothing at y0 to h at y1."""
    L = y1 - y0
    return "\n".join([
        "{",
        f"( {x0} {y0} 0 ) ( {x0} {y0+1} 0 ) ( {x0} {y0} 1 ) {tex} [ 0 -1 0 0 ] [ 0 0 -1 0 ] 0 1 1",
        f"( {x0} {y0} 0 ) ( {x0+1} {y0} 0 ) ( {x0} {y0+1} 0 ) {tex} [ -1 0 0 0 ] [ 0 -1 0 0 ] 0 1 1",
        f"( {x0} {y0} 0 ) ( {x0} {y0+L} {h} ) ( {x0+1} {y0} 0 ) {top} [ 1 0 0 0 ] [ 0 -1 0 0 ] 0 1 1",
        f"( {x1} {y1} {h} ) ( {x1+1} {y1} {h} ) ( {x1} {y1} {h+1} ) {tex} [ -1 0 0 0 ] [ 0 0 -1 0 ] 0 1 1",
        f"( {x1} {y1} {h} ) ( {x1} {y1} {h+1} ) ( {x1} {y1+1} {h} ) {tex} [ 0 1 0 0 ] [ 0 0 -1 0 ] 0 1 1",
        "}"])


def entity(keys, brushes=()):
    return "{\n" + "".join(f'"{k}" "{v}"\n' for k, v in keys.items()) + "".join(b + "\n" for b in brushes) + "}"


B = []
E = []
WALL, FLOOR, TRIM, PANEL = "qvr_wall", "qvr_floor", "qvr_trim", "qvr_panel"


def board(text, x, y, z, angle, scale="0.4"):
    E.append(entity({"classname": "func_worldtext_banner", "worldtext": text.replace("\n", "\\n"), "worldtext_halign": "1",
                     "worldtext_scale": scale, "angle": str(angle), "origin": f"{x} {y} {z}"}))


X0, X1, Y0, Y1, Z1 = -1280, 1280, -896, 896, 448
B.append(box(X0 - 16, Y0 - 16, Z1, X1 + 16, Y1 + 16, Z1 + 16, "qvr_ceiling"))
B.append(box(X0 - 16, Y0 - 16, -16, X1 + 16, Y1 + 16, 0, WALL, FLOOR))
B.append(box(X0 - 16, Y0 - 16, 0, X0, Y1 + 16, Z1, WALL))
B.append(box(X1, Y0 - 16, 0, X1 + 16, Y1 + 16, Z1, WALL))
B.append(box(X0, Y0 - 16, 0, X1, Y0, Z1, WALL))
B.append(box(X0, Y1, 0, X1, Y1 + 16, Z1, WALL))

# ---- ledges (north): platform tops 32, 48, 96, 160; a stair up the east side of each, standing room at the south edge
for i, h in enumerate((32, 48, 96, 160)):
    a = -640 + 320 * i
    B.append(box(a, 512, 0, a + 224, Y1, h, WALL, FLOOR))
    n = h // 16
    for k in range(1, n + 1):  # step k (height 16k) climbs north; the top step meets the platform's side
        B.append(box(a + 224, Y1 - 16 * (n - k + 1), 0, a + 320, Y1 - 16 * (n - k), 16 * k, WALL, FLOOR))
    board(f"LEDGE: DROP {h}", a + 112, 510, h + 56, 270, "0.35")

# ---- stairs (north-west): 16-unit risers, 6 steps and a landing (112 high); treads 24 (wide), 16 (wide), 24 (narrow)
for (y0, y1, tread, label) in ((704, 864, 24, "STAIRS: WIDE, TREAD 24"), (504, 664, 16, "STAIRS: WIDE, TREAD 16"),
                               (400, 464, 24, "STAIRS: NARROW, TREAD 24")):
    B.append(box(X0, y0, 0, -1152, y1, 112, WALL, FLOOR))  # the landing
    for k in range(1, 7):
        B.append(box(-1152 + tread * (6 - k), y0, 0, -1152 + tread * (7 - k), y1, 16 * k, WALL, FLOOR))
    board(label, -1152 + tread * 6 + 16, (y0 + y1) // 2 - 40, 90, 0, "0.3")

# ---- slopes (north-east): wedges rising north from y 120 over 320 units, a plateau on top, the floor south is the run-out
for i, deg in enumerate((12, 20, 30, 40)):
    x = 700 + 140 * i
    h = int(round(math.tan(math.radians(deg)) * 320))
    B.append(ramp(x, 120, x + 128, 440, h, WALL, FLOOR))
    B.append(box(x, 440, 0, x + 128, 540, h, WALL, FLOOR))
    board(f"{deg} DEG", x + 32, 100, 60, 270, "0.4")

# ---- flat arena (south): a standing mark at (0, -300) and a strip every 64 units south of it out to 320
B.append(box(-32, -332, 0, 32, -268, 1, TRIM, PANEL))
for d in range(64, 321, 64):
    B.append(box(-256, -300 - d - 2, 0, 256, -300 - d + 2, 1, TRIM, PANEL))
    board(str(d), 262, -300 - d + 8, 2, 0, "0.3")
board("FLAT ARENA: stand on the mark\nstrips every 64 units south\nshotgun 160, wounded, strong blows", 0, -230, 70, 270, "0.4")

# ---- explosions (south-east): a mark at (800, -450); boxes at 100 (east), 150 (south), 200 (north), 300 (west) units
MX, MY = 800, -450
B.append(box(MX - 32, MY - 32, 0, MX + 32, MY + 32, 1, TRIM, PANEL))
BOXES = [(MX + 100, MY), (MX, MY - 150), (MX, MY + 200), (MX - 300, MY)]
for (x, y) in BOXES:
    E.append(entity({"classname": "misc_explobox", "origin": f"{x} {y} 8"}))
board("EXPLOSIONS: stand on the mark\nboxes at 100 E, 150 S, 200 N, 300 W", MX, MY + 300, 90, 270, "0.4")

# ---- player, lights (a grid of ceiling lamps, fill lights over the raised areas), the minimum light
E.append(entity({"classname": "info_player_start", "origin": "0 0 24", "angle": "90"}))
LAMPS = [(x, y) for x in range(-1120, 1121, 320) for y in range(-768, 769, 256)]
for (x, y) in LAMPS:
    B.append(box(x - 48, y - 8, Z1 - 4, x + 48, y + 8, Z1, "qvr_light"))
    E.append(entity({"classname": "light", "origin": f"{x} {y} {Z1 - 40}", "light": "240", "wait": "0.5", "_color": "1 0.96 0.9"}))
for (x, y, z) in ((-1100, 560, 200), (-1100, 780, 200), (0, 700, 300), (-480, 700, 300), (480, 700, 300), (840, 400, 380),
                  (840, 500, 380), (980, 400, 380), (1180, 400, 380)):
    E.append(entity({"classname": "light", "origin": f"{x} {y} {z}", "light": "130", "wait": "0.7", "_color": "1 0.97 0.92"}))


def write(path):
    with open(path, "w", newline="\n") as f:
        f.write('// Game: Quake VR\n// Format: Valve\n// Written by Misc/quakevr/make_vrragtriggers_map.py: edit that, not this.\n'
                '// entity 0\n{\n"classname" "worldspawn"\n"mapversion" "220"\n'
                '"wad" "quakevr/wads/quakevr_dev.wad"\n"_tb_mod" "hipnotic;rogue;quakevr"\n'
                '"message" "Ragdoll Triggers"\n"worldtype" "2"\n"light" "50"\n"_vr_debris" "0"\n"_qvr_prelit" "1"\n')
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
