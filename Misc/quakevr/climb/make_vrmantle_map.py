# make_vrmantle_map.py [--compile]: writes quakevr/maps/vrmantle.map, the flatscreen mantle's test map (vr_flatmantle_test,
# ROUND24.md "Flatscreen mantle"), and with --compile builds it (qbsp, vis, light; C:\OHWorkspace\ericw-tools-2.0.0-alpha11-win64).
# A row of level blocks along x, each 96 wide and deep with a 64 gap, the lip along y 0 (facing -y), their tops at the
# heights in LIPS over the floor (z 0). Stand in front of block i: `setpos <i * 160 + 48> -18 24 0 90 0`.
import os, shutil, subprocess, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
OUT = os.path.join(ROOT, "quakevr", "maps", "vrmantle.map")
TOOLS = r"C:\OHWorkspace\ericw-tools-2.0.0-alpha11-win64"

LIPS = [48, 64, 80, 96, 128, 160]  # (vr_flatmove.cpp mantleCases: the same order)
WIDTH, DEPTH, GAP = 96, 96, 64


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
X0, X1 = -64, len(LIPS) * (WIDTH + GAP) + 64
Y0, Y1, Z0, Z1 = -160, DEPTH + 64, -16, 320
B.append(box(X0 - 16, Y0 - 16, Z1, X1 + 16, Y1 + 16, Z1 + 16, "qvr_ceiling"))
B.append(box(X0 - 16, Y0 - 16, Z0, X1 + 16, Y1 + 16, 0, "qvr_floor"))
B.append(box(X0 - 16, Y0 - 16, Z0, X0, Y1 + 16, Z1, "qvr_wall"))
B.append(box(X1, Y0 - 16, Z0, X1 + 16, Y1 + 16, Z1, "qvr_wall"))
B.append(box(X0, Y0 - 16, Z0, X1, Y0, Z1, "qvr_wall"))
B.append(box(X0, Y1, Z0, X1, Y1 + 16, Z1, "qvr_wall"))
for i, lip in enumerate(LIPS):
    x0 = i * (WIDTH + GAP)
    B.append(box(x0, 0, 0, x0 + WIDTH, DEPTH, lip, "qvr_wall", "qvr_floor"))

ents = ['{\n"classname" "info_player_start"\n"origin" "48 -100 24"\n"angle" "90"\n}']
for i in range(len(LIPS) + 1):
    ents.append(f'{{\n"classname" "light"\n"origin" "{i * (WIDTH + GAP) - GAP / 2} -60 240"\n"light" "300"\n}}')

with open(OUT, "w", newline="\n") as f:
    f.write('// Game: Quake VR\n// Format: Valve\n// entity 0\n{\n"classname" "worldspawn"\n"mapversion" "220"\n'
            '"wad" "quakevr/wads/quakevr_dev.wad"\n"_tb_mod" "hipnotic;rogue;quakevr"\n'
            '"message" "Quake VR flatscreen mantle test"\n"worldtype" "2"\n"light" "120"\n')
    f.write("\n".join(B) + "\n}\n")
    for i, e in enumerate(ents):
        f.write(f"// entity {i + 1}\n{e}\n")
for i, lip in enumerate(LIPS):
    print(f"lip {lip}: setpos {i * (WIDTH + GAP) + WIDTH // 2} -18 24 0 90 0")

if "--compile" in sys.argv:
    work = os.path.join(ROOT, "scratch", "vrmantle_build")
    os.makedirs(work, exist_ok=True)
    src, bsp = os.path.join(work, "vrmantle.map"), os.path.join(work, "vrmantle.bsp")
    shutil.copyfile(OUT, src)
    for cmd in ([os.path.join(TOOLS, "qbsp.exe"), "-nolog", "-nopercent", "-wadpath", ROOT, src, bsp],
                [os.path.join(TOOLS, "vis.exe"), "-nolog", "-nopercent", bsp],
                [os.path.join(TOOLS, "light.exe"), "-nolog", "-nopercent", "-lit", bsp]):
        r = subprocess.run(cmd, capture_output=True, text=True)
        lines = (r.stdout + r.stderr).splitlines()
        bad = [l for l in lines if "WARNING" in l.upper() or "ERROR" in l.upper()]
        print(f"{os.path.basename(cmd[0])}: exit {r.returncode}" + "".join("\n  " + w for w in bad[:8]))
        if r.returncode:
            sys.exit(1)
    for ext in (".bsp", ".lit"):
        shutil.copyfile(os.path.join(work, "vrmantle" + ext), os.path.join(os.path.dirname(OUT), "vrmantle" + ext))
