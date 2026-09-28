# make_vrclimb_map.py <out.map>: writes quakevr/maps/vrclimb.map, the climbing test map (a rung wall, a long ledge over
# a trench); compile as MAPPING.md says ("Full").
import sys
out = sys.argv[1]

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

B = []
X0, X1, Y0, Y1, Z0, Z1 = -512, 512, -384, 768, -208, 512
# the hull
B.append(box(X0 - 16, Y0 - 16, Z1, X1 + 16, Y1 + 16, Z1 + 16, "qvr_ceiling"))
B.append(box(X0 - 16, Y0 - 16, Z0 - 16, X1 + 16, Y1 + 16, Z0, "qvr_floor"))
B.append(box(X0 - 16, Y0 - 16, Z0, X0, Y1 + 16, Z1, "qvr_wall"))
B.append(box(X1, Y0 - 16, Z0, X1 + 16, Y1 + 16, Z1, "qvr_wall"))
B.append(box(X0, Y0 - 16, Z0, X1, Y0, Z1, "qvr_wall"))
B.append(box(X0, Y1, Z0, X1, Y1 + 16, Z1, "qvr_wall"))
# the main floor (z 0) in front of the rung wall, and a walkway along the west wall past the trench
B.append(box(X0, Y0, Z0, 96, 192, 0, "qvr_wall", "qvr_floor"))
B.append(box(X0, 192, Z0, -400, Y1, 0, "qvr_wall", "qvr_floor"))
# the rung tower: x 96..512, y -384..128, top at 280 (mantle onto it)
B.append(box(96, Y0, 0, X1, 128, 280, "qvr_wall", "qvr_floor"))
# rungs: 8 deep, 4 tall, 192 wide, tops every 20 units from 36 to 256
for top in range(36, 257, 20):
    B.append(box(88, -96, top - 4, 96, 96, top, "qvr_trim"))
# the long ledge: x 96..512, y 128..768, top at 48, above the trench (floor at -192)
B.append(box(96, 128, Z0, X1, Y1, 48, "qvr_wall", "qvr_floor"))
# stairs out of the trench, up to the walkway (along -x), y 640..768
for i in range(12):
    B.append(box(-400, 640, Z0, -112 - 24 * i, Y1, -176 + 16 * i, "qvr_wall", "qvr_floor"))

# the grab-leniency corner (ROUND21.md, "Climbing: hand placement and grab leniency"): a thin wall (4 thick, 96 high)
# facing the main floor, and a ledge (top 48) just behind it, its lip 6 units past the wall (not to be grabbed through it)
B.append(box(-304, -320, 0, -300, -200, 96, "qvr_wall"))
B.append(box(-420, -320, 0, -310, -200, 48, "qvr_wall", "qvr_floor"))

# ROUND21.md, "Climbing: hand orientation, staying attached, small ledges": a low narrow wall (4 thick, top 48: mantle
# onto its top; `setpos -218 -280 24 0 0 0` facing it), and a ledge with no room on top (top 48, 60 deep, a slab 40
# above it: `setpos -138 -280 24 0 0 0`)
B.append(box(-200, -330, 0, -196, -230, 48, "qvr_wall", "qvr_floor"))
B.append(box(-120, -330, 0, -60, -230, 48, "qvr_wall", "qvr_floor"))
B.append(box(-120, -330, 88, -60, -230, 104, "qvr_wall", "qvr_floor"))

# ROUND21.md, "Ledge map": moving ledges, each started by a trigger where the player stands to test it (facing +x, its
# face 18 units ahead, as the long ledge's is from `setpos 78 176 24`):
# - a lift (func_train "lift1"): a block 40 high (x -300..-236, y 40..136) rising at 8 units a second to 80 up and back,
#   waiting 1 s at the bottom and 2 at the top, started as soon as the player is at `setpos -318 88 24 0 0 0`;
# - a plat (func_plat "plat1", x -200..-136, y 40..104, top 48): up at the start, lowered 40 at 24 units a second 3 s
#   after the player is at `setpos -218 72 24 0 0 0` (then a plat as usual: it rises when stood on).
BE = []  # brush entities: (keys, brushes)
BE.append(({"classname": "func_train", "targetname": "lift1", "target": "lift1_a", "speed": "8"},
           [box(-300, 40, 0, -236, 136, 40, "qvr_trim", "qvr_floor")]))
BE.append(({"classname": "trigger_once", "target": "lift1"}, [box(-340, 60, 0, -306, 116, 64, "qvr_trim")]))
BE.append(({"classname": "func_plat", "targetname": "plat1", "speed": "24", "height": "40"},
           [box(-200, 40, 0, -136, 104, 48, "qvr_trim", "qvr_floor")]))
BE.append(({"classname": "trigger_once", "target": "plat1", "delay": "3"}, [box(-240, 50, 0, -206, 94, 64, "qvr_trim")]))

ents = []
ents.append('{\n"classname" "info_player_start"\n"origin" "64 0 24"\n"angle" "0"\n}')
for (x, y, z, l) in [(0, 0, 440, 300), (0, 400, 440, 300), (-300, 400, 200, 250), (300, -200, 440, 250),
                     (24, -60, 70, 220), (24, 60, 170, 220), (24, -60, 250, 220), (300, 0, 330, 250),
                     (24, 180, 100, 220), (24, 380, 100, 220), (24, 580, 100, 220), (-200, 450, -100, 250),
                     (-200, 0, 100, 250), (-60, -40, 60, 300), (-60, 40, 160, 300), (-60, -40, 260, 300), (-60, 220, 100, 300), (-60, 420, 100, 300), (-250, -260, 110, 250),
                     (-230, -280, 110, 250), (-150, -280, 80, 250), (-280, 88, 170, 250), (-170, 72, 110, 250)]:
    ents.append(f'{{\n"classname" "light"\n"origin" "{x} {y} {z}"\n"light" "{l}"\n"_color" "1 0.95 0.85"\n}}')
for (name, x, y, z, target, wait) in [("lift1_a", -300, 40, 0, "lift1_b", 1), ("lift1_b", -300, 40, 80, "lift1_a", 2)]:
    ents.append(f'{{\n"classname" "path_corner"\n"targetname" "{name}"\n"target" "{target}"\n"origin" "{x} {y} {z}"\n"wait" "{wait}"\n}}')
for keys, brushes in BE:
    ents.append("{\n" + "".join(f'"{k}" "{v}"\n' for k, v in keys.items()) + "\n".join(brushes) + "\n}")

with open(out, "w", newline="\n") as f:
    f.write('// Game: Quake VR\n// Format: Valve\n// entity 0\n{\n"classname" "worldspawn"\n"mapversion" "220"\n'
            '"wad" "quakevr/wads/quakevr_dev.wad"\n"_tb_mod" "hipnotic;rogue;quakevr"\n'
            '"message" "Quake VR climbing test"\n"worldtype" "2"\n"light" "80"\n"_dirtscale" "1.5"\n"_dirtdepth" "96"\n')
    f.write("\n".join(B) + "\n}\n")
    for i, e in enumerate(ents):
        f.write(f"// entity {i + 1}\n{e}\n")
