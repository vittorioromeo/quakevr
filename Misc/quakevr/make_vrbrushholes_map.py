# make_vrbrushholes_map.py -- writes quakevr/maps/vrbrushholes.map, a test hall for the hole blockers (vr_hull_holes,
# Quake/vr/vr_hull_holes.cpp; HULLS.md "Holes"): gaps Quake's 32 box never fit but a 16-wide one does. With --compile
# builds it (qbsp, vis, light) and copies the .bsp/.lit/.lux beside it. quakevr_dev.wad textures only.
#
#   python Misc/quakevr/make_vrbrushholes_map.py [--compile] [--tools DIR] [--work DIR]
#
# One hall (2048 x 1536, 320 high; floor z 0; north = +y). The player starts at (0, -600) facing north. The places
# (vr_hull_holes_test's coordinates: keep them in step):
#   - west: a pit (x -768..-568, y -64..136, 128 deep) under a grate (bars 8 wide every 32 both ways, z -8..0: holes 24);
#   - north-west: a wall (y 256..272) with a doorway (x -352..-200, 128 high) closed by a bar gate (a func_door, up,
#     targetname "gate" so it stays shut: four bars 8 wide at y 260..268, gaps 24, the first by the wall);
#   - east: a raised block (z 128; x 256..640, y -320..96) with a lift shaft (x 448..588, y -96..44) and a func_plat
#     (x 448..568, y -96..24, top z 128, targetname "lift" so it stays up): 20-unit slots east and north of it;
#     stairs up the block's south side;
#   - north-east: a row of pillars (16 wide, 24 apart, y 448..464, x 160..576, 192 high);
#   - south-west: a pit (x -768..-568, y -560..-360) under a diagonal grate (bars 8 thick turned 45 degrees, 30 apart
#     across them), and one (y -300..-100) under a louvred grate (bars at z -8..0 with 48-unit gaps, under each gap one
#     at z -16..-8: 20-unit gaps seen from above);
#   - south-east: two func_walls 24 apart (x 700..764 and 788..852, y -500..-436, 128 high);
#   - south: a drain (Quoth start's ground decoration copied: a frame round a 32 x 32 hole at x -316..-284,
#     y -316..-284, z 16..32, over a 16-high pit: exactly Quake's box's width).
import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
DEFAULT_TOOLS = "C:/OHWorkspace/ericw-tools-2.0.0-alpha11-win64"
NAME = "vrbrushholes"
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


def prism(pts, z0, z1, tex):
    """A brush standing on the convex polygon pts (counter-clockwise seen from above), z0..z1: its sides square to the
    floor, any way turned."""
    f = lambda v: f"{v:.3f}".rstrip("0").rstrip(".")
    faces = []
    for i in range(len(pts)):
        (ax, ay), (bx, by) = pts[i], pts[(i + 1) % len(pts)]
        n = ((bx - ax) ** 2 + (by - ay) ** 2) ** 0.5
        ux, uy = (bx - ax) / n, (by - ay) / n
        faces.append(f"( {f(bx)} {f(by)} {z0} ) ( {f(ax)} {f(ay)} {z0} ) ( {f(ax)} {f(ay)} {z0 + 64} ) {tex} "
                     f"[ {f(ux)} {f(uy)} 0 0 ] [ 0 0 -1 0 ] 0 1 1")
    faces.append(f"( 0 0 {z1} ) ( 0 1 {z1} ) ( 1 0 {z1} ) {tex} [ 1 0 0 0 ] [ 0 -1 0 0 ] 0 1 1")
    faces.append(f"( 0 0 {z0} ) ( 1 0 {z0} ) ( 0 1 {z0} ) {tex} [ -1 0 0 0 ] [ 0 -1 0 0 ] 0 1 1")
    return "\n".join(["{"] + faces + ["}"])


def entity(keys, brushes=()):
    return "{\n" + "".join(f'"{k}" "{v}"\n' for k, v in keys.items()) + "".join(b + "\n" for b in brushes) + "}"


B = []
E = []
WALL, FLOOR, TRIM, PANEL = "qvr_wall", "qvr_floor", "qvr_trim", "qvr_panel"


def board(text, x, y, z, angle, scale="0.35"):
    E.append(entity({"classname": "func_worldtext_banner", "worldtext": text.replace("\n", "\\n"), "worldtext_halign": "1",
                     "worldtext_scale": scale, "angle": str(angle), "origin": f"{x} {y} {z}"}))


X0, X1, Y0, Y1, Z1, ZB = -1024, 1024, -768, 768, 320, -144
B.append(box(X0 - 16, Y0 - 16, Z1, X1 + 16, Y1 + 16, Z1 + 16, "qvr_ceiling"))
B.append(box(X0 - 16, Y0 - 16, ZB, X0, Y1 + 16, Z1, WALL))
B.append(box(X1, Y0 - 16, ZB, X1 + 16, Y1 + 16, Z1, WALL))
B.append(box(X0, Y0 - 16, ZB, X1, Y0, Z1, WALL))
B.append(box(X0, Y1, ZB, X1, Y1 + 16, Z1, WALL))

# ---- the floor, thick, with the grate's pit cut out (x -768..-568, y -64..136, floor z -128)
PX0, PX1, PY0, PY1 = -768, -568, -64, 136
B.append(box(X0, Y0, ZB, PX0, Y1, 0, WALL, FLOOR))
B.append(box(PX1, Y0, ZB, X1, Y1, 0, WALL, FLOOR))
# (south of it, two more pits: the diagonal grate's, y -560..-360, and the louvred grate's, y -300..-100)
DY0, DY1, LVY0, LVY1 = -560, -360, -300, -100
B.append(box(PX0, Y0, ZB, PX1, DY0, 0, WALL, FLOOR))
B.append(box(PX0, DY0, ZB, PX1, DY1, -128, WALL, FLOOR))
B.append(box(PX0, DY1, ZB, PX1, LVY0, 0, WALL, FLOOR))
B.append(box(PX0, LVY0, ZB, PX1, LVY1, -128, WALL, FLOOR))
B.append(box(PX0, LVY1, ZB, PX1, PY0, 0, WALL, FLOOR))
B.append(box(PX0, PY1, ZB, PX1, Y1, 0, WALL, FLOOR))
B.append(box(PX0, PY0, ZB, PX1, PY1, -128, WALL, FLOOR))
for k in range(7):  # bars 8 wide every 32 both ways: holes 24
    B.append(box(PX0 + 32 * k, PY0, -8, PX0 + 32 * k + 8, PY1, 0, TRIM))
    B.append(box(PX0, PY0 + 32 * k, -8, PX1, PY0 + 32 * k + 8, 0, TRIM))
board("GRATE: 24-UNIT HOLES\nyou walk over it; nothing falls through", -668, -100, 64, 270)

# ---- the diagonal grate: bars 8 thick turned 45 degrees, 30 apart across them (42 along x and y, where the line test
# looks; Quake's box needs 45.25 across), over the pit x -768..-568, y -560..-360 (vr_hull_holes_diag)
DC = (-668.0, -460.0)
R2 = 0.5 ** 0.5
for o in (-133, -95, -57, -19, 19, 57, 95, 133):
    hl = 100.0 / R2 - abs(o) + 16.0  # (the pit's chord along the bar, and 16 more onto its rim each end)
    cx, cy = DC[0] + o * R2, DC[1] + o * R2  # d1 = (1, 1) across the bars, d2 = (1, -1) along them
    pts = [(cx + s * hl * R2 + t * 4 * R2, cy - s * hl * R2 + t * 4 * R2) for (s, t) in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    B.append(prism(pts, -8, 0, TRIM))
board("DIAGONAL GRATE: 30 ACROSS\nnothing falls between the slanted bars", -668, -570, 64, 270)

# ---- the louvred grate: bars along x at z -8..0 with 48-unit gaps, under each gap a bar at z -16..-8 (20-unit gaps seen
# from above, none along a line), over the pit x -768..-568, y -300..-100 (vr_hull_holes_vert)
for k in range(4):
    y0 = LVY0 + 56 * k
    B.append(box(PX0, y0, -8, PX1, y0 + 8, 0, TRIM))
    if k < 3:
        B.append(box(PX0, y0 + 28, -16, PX1, y0 + 36, -8, TRIM))
board("LOUVRED GRATE: 20 FROM ABOVE\nnothing falls past the lower bars", -668, -310, 64, 270)

# ---- the bar gate: a wall at y 256..272 with a doorway x -352..-200 (128 high); the gate's bars at y 260..268
GW0, GW1, GY0, GY1 = -352, -200, 256, 272
B.append(box(X0, GY0, 0, GW0, GY1, Z1, WALL))
B.append(box(GW1, GY0, 0, -64, GY1, Z1, WALL))
B.append(box(GW0, GY0, 128, GW1, GY1, Z1, WALL))
bars = [box(GW0 + 24 + 32 * k, 260, 0, GW0 + 32 + 32 * k, 268, 128, TRIM) for k in range(4)]
E.append(entity({"classname": "func_door", "angle": "-1", "targetname": "gate", "speed": "100", "lip": "8"}, bars))
board("BAR GATE: 24-UNIT GAPS\nnothing slips between the bars", -276, 240, 150, 270)

# ---- the lift: a raised block (z 128) with a shaft; the plat leaves 20-unit slots east and north
LX0, LX1, LY0, LY1 = 256, 640, -320, 96
SX0, SX1, SY0, SY1 = 448, 588, -96, 44
B.append(box(LX0, LY0, 0, SX0, LY1, 128, WALL, FLOOR))
B.append(box(SX1, LY0, 0, LX1, LY1, 128, WALL, FLOOR))
B.append(box(SX0, LY0, 0, SX1, SY0, 128, WALL, FLOOR))
B.append(box(SX0, SY1, 0, SX1, LY1, 128, WALL, FLOOR))
for k in range(1, 8):  # stairs up the block's south side (16-unit risers, 32 treads), x 256..384
    B.append(box(LX0, LY0 - 32 * (8 - k), 0, LX0 + 128, LY0 - 32 * (7 - k), 16 * k, WALL, FLOOR))
E.append(entity({"classname": "func_plat", "targetname": "lift", "height": "120", "speed": "150"},
                [box(SX0, SY0, 120, 568, 24, 128, TRIM, PANEL)]))
board("LIFT EDGE: 20-UNIT SLOTS\nyou cannot drop down beside the lift", 512, -330, 190, 270)

# ---- the pillars: 16 wide, 24 apart
for k in range(11):
    B.append(box(160 + 40 * k, 448, 0, 176 + 40 * k, 464, 192, WALL))
board("PILLARS: 24-UNIT GAPS\nnothing slips between them", 368, 440, 220, 270)

# ---- two func_walls 24 apart (x 700..764 and 788..852, y -500..-436, 128 high): the gap between two brush models
# (vr_hull_holes_models)
for (x0, x1) in ((700, 764), (788, 852)):
    E.append(entity({"classname": "func_wall"}, [box(x0, -500, 0, x1, -436, 128, PANEL)]))
board("TWO FUNC_WALLS: 24-UNIT GAP\nnothing slips between them", 776, -510, 170, 270)

# ---- the drain (backlog1122): Quoth start's ground decoration at (0, 2272, -416) copied here (moved to (-300, -300,
# 0): its pieces' bounds from vr_hull_holes_mark): a frame of 8-wide bars at z 16..32 round a 32 x 32 hole (x -316..-284,
# y -316..-284) over a 16-high pit and a drain channel. Quake's box, exactly as wide as the hole, stands on the frame
# (hull 1 is solid there and its clip brush recovery doesn't fill it: the samples meet the frame); a 16 box fell through.
DRAIN = [(-56, 2224, -392, 56, 2256, -384), (24, 2256, -416, 56, 2328, -384), (24, 2252, -392, 56, 2256, -384),
         (24, 2240, -416, 56, 2256, -392), (16, 2288, -400, 24, 2328, -384), (-16, 2288, -400, 16, 2296, -384),
         (-24, 2288, -400, -16, 2328, -384), (16, 2248, -400, 24, 2288, -384), (-24, 2256, -400, -16, 2288, -384),
         (-16, 2252, -392, 16, 2256, -384), (-24, 2248, -400, -16, 2256, -384), (16, 2240, -392, 56, 2256, -384),
         (32, 2240, -416, 56, 2252, -392), (16, 2224, -412, 32, 2252, -392), (24, 2240, -416, 32, 2244, -408),
         (-16, 2240, -392, 16, 2256, -384), (-24, 2224, -412, -16, 2256, -384), (-64, 2256, -448, -24, 2336, -376),
         (-64, 2240, -392, -24, 2256, -384), (-32, 2224, -412, -24, 2252, -392), (-64, 2240, -448, -24, 2256, -392),
         (-96, 2272, -448, -40, 2336, -392)]
for (x0, y0, z0, x1, y1, z1) in DRAIN:
    B.append(box(x0 - 300, y0 - 2572, max(z0 + 416, 0), x1 - 300, y1 - 2572, z1 + 416, TRIM))
B.append(box(-360, -368, 0, -240, -348, 16, WALL, FLOOR))  # (a step up onto it from the south)
board("DRAIN: A 32-UNIT HOLE\nQuake's box stands over it; so do you", -300, -370, 80, 270)

# ---- player, lights
E.append(entity({"classname": "info_player_start", "origin": "0 -600 24", "angle": "90"}))
LAMPS = [(x, y) for x in range(-896, 897, 256) for y in range(-640, 641, 256)]
for (x, y) in LAMPS:
    B.append(box(x - 48, y - 8, Z1 - 4, x + 48, y + 8, Z1, "qvr_light"))
    E.append(entity({"classname": "light", "origin": f"{x} {y} {Z1 - 40}", "light": "220", "wait": "0.6", "_color": "1 0.96 0.9"}))
for (x, y, z) in ((-668, 36, -60), (520, -30, 60), (-276, 300, 64), (-276, 200, 64), (-668, -460, -60),
                  (-668, -200, -60), (776, -560, 64)):
    E.append(entity({"classname": "light", "origin": f"{x} {y} {z}", "light": "150", "wait": "0.7"}))


def write(path):
    with open(path, "w", newline="\n") as f:
        f.write('// Game: Quake VR\n// Format: Valve\n// Written by Misc/quakevr/make_vrbrushholes_map.py: edit that, not this.\n'
                '// entity 0\n{\n"classname" "worldspawn"\n"mapversion" "220"\n'
                '"wad" "quakevr/wads/quakevr_dev.wad"\n"_tb_mod" "hipnotic;rogue;quakevr"\n'
                '"message" "Hole Blockers"\n"worldtype" "2"\n"light" "60"\n"_vr_debris" "0"\n"_qvr_prelit" "1"\n')
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
                [os.path.join(tools, "light.exe"), "-nolog", "-nopercent", "-extra4", "-dirt", "-bounce", "-lit", "-lux",
                 "-lightgrid", "-lightgrid_dist", "32", "32", "32", bsp]):
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
