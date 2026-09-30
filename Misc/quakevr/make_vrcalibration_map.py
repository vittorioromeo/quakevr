# make_vrcalibration_map.py -- writes quakevr/maps/vrcalibration.map, the VR Calibration room (the main menu's first row:
# Quake/vr/vr_setup.hpp), and with --compile builds it (qbsp, vis, light: MAPPING.md's "Full" profile, with bounced light).
#
#   python Misc/quakevr/make_vrcalibration_map.py [--compile] [--tools DIR]
#
# One hall (39 x 35 m, 11.7 m high), quakevr_dev.wad's textures only (no id textures: the .bsp is committed):
#   - the calibration spot where the player appears (south), facing the welcome board, a stand on the right with
#     START CALIBRATION and POSITION (standing or seated);
#   - the west wall: two panels of setting buttons (vr_setup_option <key>: the table in vr_setup.cpp), each steps its
#     setting through its presets, prints it and saves the config; the value shows on a small screen above it;
#   - the east: a pool (3.9 m deep) with steps down into it;
#   - the north-east: a platform (5.9 m) with rungs up its face, stairs up its side and three blocks to mantle onto;
#   - the north-west: a table with things to pick up, a weapon and a training dummy;
#   - the south wall: NEW GAME and VR HUB.
# The boards name menu pages as {menu:<page title>} (or {menu:<title>><row>}): the engine writes each one's path from the
# main menu as the menus are when the map loads (menu::expandPaths), and `vr_menu_path_check` (in the map, or with
# maps/vrcalibration.map) lists them and fails on one that no longer exists. When menus change, run that check.
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
DEFAULT_TOOLS = "C:/OHWorkspace/ericw-tools-2.0.0-alpha11-win64"
OUT = os.path.join(ROOT, "quakevr", "maps", "vrcalibration.map")


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


X0, X1, Y0, Y1, Z1 = -640, 640, -512, 640, 384  # the hall; the floor at z 0
BOTTOM = -176                                     # under the pool
POOL = (224, -416, 544, -96)                      # x0 y0 x1 y1
POOL_FLOOR, WATER_TOP = -128, -8

B = []   # the world's brushes
E = []   # entities (after the worldspawn)

# ---- the hall
B.append(box(X0 - 16, Y0 - 16, Z1, X1 + 16, Y1 + 16, Z1 + 16, "qvr_ceiling"))
B.append(box(X0 - 16, Y0 - 16, BOTTOM - 16, X1 + 16, Y1 + 16, BOTTOM, "qvr_wall"))
B.append(box(X0 - 16, Y0 - 16, BOTTOM, X0, Y1 + 16, Z1, "qvr_wall"))
B.append(box(X1, Y0 - 16, BOTTOM, X1 + 16, Y1 + 16, Z1, "qvr_wall"))
B.append(box(X0, Y0 - 16, BOTTOM, X1, Y0, Z1, "qvr_wall"))
B.append(box(X0, Y1, BOTTOM, X1, Y1 + 16, Z1, "qvr_wall"))
# the floor (z 0), round the pool's hole
px0, py0, px1, py1 = POOL
B.append(box(X0, Y0, BOTTOM, X1, py0, 0, "qvr_wall", "qvr_floor"))
B.append(box(X0, py1, BOTTOM, X1, Y1, 0, "qvr_wall", "qvr_floor"))
B.append(box(X0, py0, BOTTOM, px0, py1, 0, "qvr_wall", "qvr_floor"))
B.append(box(px1, py0, BOTTOM, X1, py1, 0, "qvr_wall", "qvr_floor"))
# the lights: the lamps' value and falloff ("wait": less reaches further), the fill lights' (low, by the walls and
# boards), the minimum light: soft, the room seen everywhere, not full bright (the bounced light adds much: small
# changes here go a long way; measured against vrfiringrange's look)
LIGHT = (245, 0.47, 135, 0.66, 46)
# ceiling lamps (a glowing strip over each light)
LAMPS = [(x, y) for x in (-448, -128, 192, 480) for y in (-320, 0, 320)]
for (x, y) in LAMPS:
    B.append(box(x - 48, y - 8, Z1 - 4, x + 48, y + 8, Z1, "qvr_light"))

# ---- the calibration spot and its stand (the player appears at 0 -288 facing north; the stand is on the right)
B.append(box(-48, -336, 0, 48, -240, 1, "qvr_trim", "qvr_panel"))  # the spot: a mat, flush enough to walk over
B.append(box(80, -344, 0, 96, -232, 64, "qvr_panel"))              # the stand; its buttons on its west face

# ---- the pool: its floor, steps down along its west side (north end), the water
B.append(box(px0, py0, BOTTOM, px1, py1, POOL_FLOOR, "qvr_wall", "qvr_panel"))
for k in range(7):
    B.append(box(px0 + 24 * k, py1 - 96, POOL_FLOOR, px0 + 24 * (k + 1), py1, -16 - 16 * k, "qvr_trim", "qvr_floor"))
B.append(box(px0, py0, POOL_FLOOR, px1, py1, WATER_TOP, "*qvr_water"))

# ---- climbing: the platform (x 288..640, y 320..640, top 192), rungs on its south face, stairs up its west side, and
# three blocks to mantle onto (tops 32, 48, 72) in front of it
PLAT = 192
B.append(box(288, 320, 0, X1, Y1, PLAT, "qvr_wall", "qvr_floor"))
for top in range(36, PLAT - 15, 20):
    B.append(box(400, 312, top - 4, 528, 320, top, "qvr_trim"))
for k in range(12):
    B.append(box(96 + 16 * k, 544, 0, 288, Y1, 16 * (k + 1), "qvr_wall", "qvr_floor"))
for i, h in enumerate((32, 48, 72)):
    B.append(box(-32 + 80 * i, 352, 0, 32 + 80 * i, 416, h, "qvr_wall", "qvr_floor"))

# ---- things to pick up: a table in the north-west
B.append(box(-448, 352, 0, -320, 416, 32, "qvr_trim", "qvr_panel"))

# ---- the setting panels on the west wall, and the south wall's panel
PANEL_X = X0 + 16  # the panels' face (facing +x)
B.append(box(X0, -416, 0, PANEL_X, -32, 96, "qvr_panel"))
B.append(box(X0, 32, 0, PANEL_X, 416, 96, "qvr_panel"))
B.append(box(-96, Y0, 0, 96, Y0 + 16, 96, "qvr_panel"))


def button(label, command, x0, y0, z0, x1, y1, z1, angle, scale="0.2"):
    """A func_button running `command` (buttonEffect 3), `label` on its face; pushed towards `angle`."""
    E.append(entity({
        "classname": "func_button", "buttonEffect": "3", "targetname": command + "\\n", "worldtext": label,
        "worldtext_halign": "1", "worldtext_scale": scale, "angle": str(angle), "wait": "1", "speed": "50",
        "lip": "4", "sounds": "1"}, [box(x0, y0, z0, x1, y1, z1, "qvr_button")]))


def west_buttons(entries, y_first):
    """Buttons along a west panel, 64 apart, their faces 8 out from the panel, pushed west (angle 180)."""
    for i, (label, key) in enumerate(entries):
        yc = y_first + 64 * i
        button(label, f"vr_setup_option {key}", PANEL_X, yc - 16, 24, PANEL_X + 8, yc + 16, 56, 180)


west_buttons([("TURNING", "turning"), ("TURN SPEED", "turnspeed"), ("MOVE TOWARDS", "movedir"),
              ("RUN OR WALK", "run"), ("TELEPORT", "teleport"), ("CLIMBING", "climb")], -384)
west_buttons([("MAIN HAND", "hand"), ("WORLD SCALE", "scale"), ("BODY", "body"), ("HUD", "hud"),
              ("CROSSHAIR", "crosshair"), ("WEAPON GRIP", "grip")], 64)
# the stand (west face at x 80, pushed east: angle 0)
button("START\\nCALIBRATION", "vr_setup here", 72, -328, 24, 80, -296, 56, 0)
button("POSITION", "vr_setup_option position", 72, -280, 24, 80, -248, 56, 0)
# the south wall's panel (its face at y Y0 + 16, facing north; pushed south: angle 270)
button("NEW GAME", "campaign 1; map start", -72, Y0 + 16, 24, -40, Y0 + 24, 56, 270)
button("VR HUB", "map vrstart", 40, Y0 + 16, 24, 72, Y0 + 24, 56, 270)


def board(text, x, y, z, angle, scale="0.3", halign="1"):
    """A text board (func_worldtext_banner) facing `angle`; its lines split with \\n, pages with $."""
    E.append(entity({"classname": "func_worldtext_banner", "worldtext": text, "worldtext_halign": halign,
                     "worldtext_scale": scale, "angle": str(angle), "origin": f"{x} {y} {z}"}))


N = "\\n"
board(N.join(["VR CALIBRATION ROOM", "", "The buttons on the walls change the main",
              "options: each press steps to the next", "choice, shows it on your wrist, and saves it."]),
      0, -144, 88, 270, "0.35")
board(N.join(["West: turning, moving, body, HUD.", "East: a pool. North-east: climbing.",
              "North-west: things to pick up."]), 0, -144, 62, 270, "0.3")
board(N.join(["CALIBRATE AGAIN", "START CALIBRATION: all of it.", "", "Body only:", "{menu:Body Calibration}", "",
              "Height only:", "{menu:Body and Display>Set Height Now}"]), 88, -288, 104, 180, "0.2")
board(N.join(["MOVING AND TURNING", "", "More: {menu:VR Settings>Comfort}", "and {menu:Locomotion}"]),
      PANEL_X + 1, -224, 136, 0, "0.3")
board(N.join(["BODY AND DISPLAY", "", "More: {menu:Body and Display}", "",
              "Hands not where your controllers are?", "{menu:Hand/Gun Calibration}"]),
      PANEL_X + 1, 224, 136, 0, "0.3")
board(N.join(["SWIMMING", "Stroke with your arms, or use the stick.", "Steps at the north end.", "",
              "{menu:Swimming}"]), 200, -256, 88, 180, "0.3")
board(N.join(["CLIMBING", "Grip a rung or a ledge with an empty", "hand and pull yourself up; let go to drop.",
              "Stairs on the left.", "", "{menu:Climbing}"]), 344, 318, 120, 270, "0.25")
board(N.join(["THINGS TO PICK UP", "Grip to take, open the hand to throw.", "", "Where weapons sit in your hand:",
              "{menu:Weapon Offsets}", "Throwing: {menu:Throwing and Physics}"]), -384, 336, 88, 270, "0.25")
board(N.join(["LOOKS AND SPEED", "Render scale, upscaling: {menu:Headset}", "", "Graphics: {menu:Graphics}", "",
              "Wrist gadget: {menu:Wrist Gadget}"]), -96, Y1 - 1, 96, 270, "0.35")
board(N.join(["START PLAYING", "NEW GAME starts Quake; VR HUB, the hub."]), 0, Y0 + 17, 88, 90, "0.3")

# ---- the player, the things, the lights
E.append(entity({"classname": "info_player_start", "origin": "0 -288 24", "angle": "90"}))
for cls, x, y in (("item_health", -432, 384), ("item_shells", -384, 384), ("item_spikes", -344, 384)):
    E.append(entity({"classname": cls, "origin": f"{x} {y} 40"}))
E.append(entity({"classname": "weapon_supershotgun", "origin": "-384 456 24"}))
E.append(entity({"classname": "vr_dummy", "origin": "-512 520 24", "angle": "300"}))
for (x, y) in LAMPS:
    E.append(entity({"classname": "light", "origin": f"{x} {y} {Z1 - 40}", "light": str(LIGHT[0]), "wait": str(LIGHT[1]), "_color": "1 0.96 0.9"}))
for (x, y, z) in ((-560, -224, 120), (-560, 224, 120), (0, -200, 140), (380, -256, 90), (464, 240, 140),
                  (-384, 300, 120), (0, 560, 160), (0, -440, 110)):
    E.append(entity({"classname": "light", "origin": f"{x} {y} {z}", "light": str(LIGHT[2]), "wait": str(LIGHT[3]), "_color": "1 0.97 0.92"}))


def write(path):
    with open(path, "w", newline="\n") as f:
        f.write('// Game: Quake VR\n// Format: Valve\n// Written by Misc/quakevr/make_vrcalibration_map.py: edit that, not this.\n'
                '// entity 0\n{\n"classname" "worldspawn"\n"mapversion" "220"\n'
                '"wad" "quakevr/wads/quakevr_dev.wad"\n"_tb_mod" "hipnotic;rogue;quakevr"\n'
                '"message" "VR Calibration"\n"worldtype" "2"\n"light" "' + str(LIGHT[4]) + '"\n"_vr_debris" "0"\n')
        f.write("\n".join(B) + "\n}\n")
        for i, e in enumerate(E):
            f.write(f"// entity {i + 1}\n{e}\n")


def menu_specs():
    return sorted(set(m for e in E for m in re.findall(r"\{menu:([^}]*)\}", e)))


def compile_map(tools, work):
    """Built in `work` (its files overwritten each time), then the .bsp, .lit and .lux copied beside the .map."""
    os.makedirs(work, exist_ok=True)
    src = os.path.join(work, "vrcalibration.map")
    bsp = os.path.join(work, "vrcalibration.bsp")
    shutil.copyfile(OUT, src)
    for cmd in ([os.path.join(tools, "qbsp.exe"), "-nolog", "-nopercent", "-wadpath", ROOT, src, bsp],
                [os.path.join(tools, "vis.exe"), "-nolog", "-nopercent", bsp],
                [os.path.join(tools, "light.exe"), "-nolog", "-nopercent", "-extra4", "-dirt", "-dirtscale", "1.5",
                 "-dirtdepth", "96", "-bounce", "-lit", "-lux", "-lightgrid", "-lightgrid_dist", "64", "64", "64", bsp]):
        result = subprocess.run(cmd, capture_output=True, text=True)
        lines = (result.stdout + result.stderr).splitlines()
        warnings = [l for l in lines if "WARNING" in l.upper() or "ERROR" in l.upper()]
        print(f"{os.path.basename(cmd[0])}: exit {result.returncode}" + "".join("\n  " + w for w in warnings[:12]))
        if result.returncode:
            sys.exit(1)
    for ext in (".bsp", ".lit", ".lux"):
        shutil.copyfile(os.path.join(work, "vrcalibration" + ext), os.path.join(os.path.dirname(OUT), "vrcalibration" + ext))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compile", action="store_true", help="also build the .bsp, .lit and .lux (ericw-tools 2.0)")
    parser.add_argument("--tools", default=DEFAULT_TOOLS)
    parser.add_argument("--work", default=os.path.join(tempfile.gettempdir(), "vrcalibration_build"),
                        help="the compile's folder (its .prt and copies stay there)")
    args = parser.parse_args()
    write(OUT)
    print(f"wrote {OUT}: {len(B)} brushes, {len(E)} entities; boards name {', '.join(menu_specs())}")
    if args.compile:
        compile_map(args.tools, args.work)


if __name__ == "__main__":
    main()
