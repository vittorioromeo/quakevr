# make_vrropes_map.py -- writes quakevr/maps/vrropes.map, the climbable ropes' test hall (QC vr_ropes.qc,
# Quake/vr/vr_climbrope.cpp; docs/vr-port/ROPES.md), and with --compile builds it (qbsp, vis, light) and copies the
# .bsp/.lit/.lux beside it. quakevr_dev.wad textures only.
#
#   python Misc/quakevr/make_vrropes_map.py [--compile] [--tools DIR] [--work DIR]
#
# One hall (1536 x 1536, 512 high; floor z 0; north = +y). The player starts at the south wall facing north, open floor
# ahead of him (vr_rope_test makes its own rope and chain 128 units ahead).
#   - the middle: four swinging ropes in a row west to east, 192 apart, 384 long (rope, chain, rope, chain), and a ledge
#     128 high at the west wall (steps up its south side) to jump off towards the first;
#   - north-west: a rope pinned at both ends (its target an info_notnull on the floor);
#   - north: a heavy chain (3 thick, 40 kg), 192 long;
#   - north-east: a rope longer than the drop (600: the rest lies on the floor);
#   - (ropes7) south-west: a wooden and a metal climbing pole (vr_pole), ceiling to floor; far north: a rope strung level
#     100 high (2% slack) and east: a diagonal one (230 down to 90): monkey traverses;
#   - (ropes8a) south: a level metal pole 88 high on two posts (a monkey traverse); south-east: a slanted wooden pole
#     from the floor up to a beam (27 degrees off upright: climbed).
import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
DEFAULT_TOOLS = "C:/OHWorkspace/ericw-tools-2.0.0-alpha11-win64"
NAME = "vrropes"
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


def entity(keys):
    return "{\n" + "".join(f'"{k}" "{v}"\n' for k, v in keys.items()) + "}"


B = []
E = []
WALL, FLOOR, TRIM, PANEL = "qvr_wall", "qvr_floor", "qvr_trim", "qvr_panel"


def board(text, x, y, z, angle, scale="0.4"):
    E.append(entity({"classname": "func_worldtext_banner", "worldtext": text.replace("\n", "\\n"), "worldtext_halign": "1",
                     "worldtext_scale": scale, "angle": str(angle), "origin": f"{x} {y} {z}"}))


def rope(x, y, length, chain=False, **keys):
    k = {"classname": "vr_chain" if chain else "vr_rope", "origin": f"{x} {y} {Z1 - 8}", "rope_length": str(length)}
    k.update({key: str(v) for key, v in keys.items()})
    E.append(entity(k))
    B.append(box(x - 24, y - 24, Z1 - 4, x + 24, y + 24, Z1, TRIM))  # a plate on the ceiling where it hangs


X0, X1, Y0, Y1, Z1 = -768, 768, -768, 768, 512
B.append(box(X0 - 16, Y0 - 16, Z1, X1 + 16, Y1 + 16, Z1 + 16, "qvr_ceiling"))
B.append(box(X0 - 16, Y0 - 16, -16, X1 + 16, Y1 + 16, 0, WALL, FLOOR))
B.append(box(X0 - 16, Y0 - 16, 0, X0, Y1 + 16, Z1, WALL))
B.append(box(X1, Y0 - 16, 0, X1 + 16, Y1 + 16, Z1, WALL))
B.append(box(X0, Y0 - 16, 0, X1, Y0, Z1, WALL))
B.append(box(X0, Y1, 0, X1, Y1 + 16, Z1, WALL))

# ---- the swinging row (y 0) and the ledge at the west wall to jump off from
for i, x in enumerate((-288, -96, 96, 288)):
    rope(x, 0, 384, chain=(i % 2 == 1))
B.append(box(X0, -96, 0, -480, 96, 128, WALL, FLOOR))
for k in range(1, 8):  # steps up the ledge's south side, 16 high each, rising north
    B.append(box(X0, -96 - 24 * (8 - k), 0, -480, -96 - 24 * (7 - k), 16 * k, WALL, FLOOR))
board("SWING: four ropes 192 apart\nrope, chain, rope, chain\njump from the ledge", 0, -160, 96, 90, "0.4")

# ---- pinned at both ends (north-west), a heavy chain (north), longer than the drop (north-east)
rope(-400, 400, 520, target="vrropes_pin")
E.append(entity({"classname": "info_notnull", "targetname": "vrropes_pin", "origin": "-400 400 8"}))
board("PINNED: its bottom held", -400, 330, 72, 90, "0.35")
rope(0, 400, 192, chain=True, rope_thickness=3, rope_mass=40)
board("HEAVY CHAIN: 3 thick, 40 kg", 0, 330, 72, 90, "0.35")
rope(400, 400, 600)
board("LONG ROPE: longer than the drop", 400, 330, 72, 90, "0.35")

# ---- (ropes7) climbing poles (south-west): wood and metal, ceiling to floor
for x, metal in ((-560, 0), (-440, 1)):
    E.append(entity({"classname": "vr_pole", "origin": f"{x} -450 {Z1 - 8}", "rope_style": str(metal)}))
    B.append(box(x - 24, -474, Z1 - 4, x + 24, -426, Z1, TRIM))
board("POLES: wood, metal", -500, -520, 72, 90, "0.35")


def strung(a, b, slack):
    # (ropes7) a rope strung from a to b, slack longer than the line between; a post just past each end
    E.append(entity({"classname": "vr_rope", "origin": f"{a[0]} {a[1]} {a[2]}", "rope_end": f"{b[0]} {b[1]} {b[2]}",
                     "rope_slack": str(slack)}))
    dx, dy = b[0] - a[0], b[1] - a[1]
    n = (dx * dx + dy * dy) ** 0.5
    for p, s in ((a, -1), (b, 1)):
        x, y = round(p[0] + s * 12 * dx / n), round(p[1] + s * 12 * dy / n)
        B.append(box(x - 4, y - 4, 0, x + 4, y + 4, p[2] + 6, TRIM))


# ---- (ropes7) a level rope overhead (north), a diagonal one (east): monkey traverses (hang under, hand over hand)
strung((-300, 600, 100), (300, 600, 100), 0.02)
board("LEVEL ROPE: hang under it, hand over hand", 0, 540, 40, 90, "0.35")
strung((600, -300, 230), (600, 300, 90), 0.02)
board("DIAGONAL ROPE: a monkey traverse", 560, 0, 40, 180, "0.35")

# ---- (ropes8a) a level metal pole (south: a monkey traverse, a jump up to it) on two posts, and a slanted wooden one
# (south-east: from the floor up to a beam, 27 degrees off upright: climbed as the upright ones)
E.append(entity({"classname": "vr_pole", "origin": "-320 -250 88", "rope_end": "-80 -250 88", "rope_style": "1"}))
for x in (-332, -68):
    B.append(box(x - 4, -254, 0, x + 4, -246, 94, TRIM))
board("LEVEL POLE: hang under it, hand over hand", -200, -300, 40, 90, "0.35")
E.append(entity({"classname": "vr_pole", "origin": "520 -440 224", "rope_end": "520 -552 1", "rope_style": "0"}))
B.append(box(504, -456, 224, 536, -424, Z1, TRIM))
board("SLANTED POLE: climb it", 520, -620, 72, 90, "0.35")

# ---- player, lights
E.append(entity({"classname": "info_player_start", "origin": "0 -600 24", "angle": "90"}))
for x in range(-576, 577, 384):
    for y in range(-576, 577, 384):
        E.append(entity({"classname": "light", "origin": f"{x} {y} {Z1 - 48}", "light": "260", "wait": "0.5",
                         "_color": "1 0.96 0.9"}))


def write(path):
    with open(path, "w", newline="\n") as f:
        f.write('// Game: Quake VR\n// Format: Valve\n// Written by Misc/quakevr/make_vrropes_map.py: edit that, not this.\n'
                '// entity 0\n{\n"classname" "worldspawn"\n"mapversion" "220"\n'
                '"wad" "quakevr/wads/quakevr_dev.wad"\n"_tb_mod" "hipnotic;rogue;quakevr"\n'
                '"message" "Climbable Ropes"\n"worldtype" "2"\n"light" "60"\n"_vr_debris" "0"\n"_qvr_prelit" "1"\n')
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
                [os.path.join(tools, "light.exe"), "-nolog", "-nopercent", "-extra4", "-bounce", "-lit", "-lux", bsp]):
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
