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

ents = []
ents.append('{\n"classname" "info_player_start"\n"origin" "64 0 24"\n"angle" "0"\n}')
for (x, y, z, l) in [(0, 0, 440, 300), (0, 400, 440, 300), (-300, 400, 200, 250), (300, -200, 440, 250),
                     (24, -60, 70, 220), (24, 60, 170, 220), (24, -60, 250, 220), (300, 0, 330, 250),
                     (24, 180, 100, 220), (24, 380, 100, 220), (24, 580, 100, 220), (-200, 450, -100, 250),
                     (-200, 0, 100, 250), (-60, -40, 60, 300), (-60, 40, 160, 300), (-60, -40, 260, 300), (-60, 220, 100, 300), (-60, 420, 100, 300), (-250, -260, 110, 250)]:
    ents.append(f'{{\n"classname" "light"\n"origin" "{x} {y} {z}"\n"light" "{l}"\n"_color" "1 0.95 0.85"\n}}')

with open(out, "w", newline="\n") as f:
    f.write('// Game: Quake VR\n// Format: Valve\n// entity 0\n{\n"classname" "worldspawn"\n"mapversion" "220"\n'
            '"wad" "quakevr/wads/quakevr_dev.wad"\n"_tb_mod" "hipnotic;rogue;quakevr"\n'
            '"message" "Quake VR climbing test"\n"worldtype" "2"\n"light" "80"\n"_dirtscale" "1.5"\n"_dirtdepth" "96"\n')
    f.write("\n".join(B) + "\n}\n")
    for i, e in enumerate(ents):
        f.write(f"// entity {i + 1}\n{e}\n")
