# make_vrcalibration_map.py -- writes quakevr/maps/vrcalibration.map, the VR Calibration room (the main menu's first row:
# Quake/vr/vr_setup.hpp), and with --compile builds it (qbsp, vis, light: MAPPING.md's "Full" profile, with bounced light).
#
#   python Misc/trenchbroom/make_id_wad.py                       (once: id's textures from your own paks)
#   python Misc/quakevr/make_vrcalibration_map.py [--compile] [--tools DIR] [--work DIR]
#
# Calibration only: an octagonal chamber (9.75 m across, 5.4 m to the ceiling), the player on the calibration spot in its
# middle facing a board (north) that says what happens and where each calibration is in the menus, and a doorway (south,
# behind the player) whose teleporter glow takes him to the vrstart hub (trigger_changelevel). No buttons, items or
# props: the old room's setting buttons, pool, climbing and pickups are the test hall's (make_vrtesthall_map.py).
#
# Looks: id's base textures (the author's decision: the committed .bsp embeds them; the WAD is made from the player's
# own paks by make_id_wad.py and never committed), and quakevr_dev.wad's qvrc_pad for the spot. Eight wall panels, one
# per side (tech14_1: each side is exactly one panel; the four cut corners fit theirs), pilasters over their joints with
# a small lamp each, a skirting, a metal wainscot and its rail, a bevelled cornice, a stepped ceiling coffer with a ring
# of light tiles and a light panel, a framed board, a cased doorway. Every face's texture is placed by rule (`Tex`):
# world-aligned so that coplanar faces continue each other (floor and ceiling tiles meet the walls and the coffer at
# their joints; a side's panel runs on above the doorway), or fitted to the face (trims fill their band's height
# exactly; panels, lamps, the spot and the board's frame fill their face).
#
# The board names menu pages as {menu:<page title>} (or {menu:<title>><row>}): the engine writes each one's path from the
# main menu as the menus are when the map loads (menu::expandPaths), and `vr_menu_path_check maps/vrcalibration.map`
# lists them and fails on one that no longer exists. When menus change, run that check.
import argparse
import itertools
import math
import os
import re
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
DEFAULT_TOOLS = "C:/OHWorkspace/ericw-tools-2.0.0-alpha11-win64"
OUT = os.path.join(ROOT, "quakevr", "maps", "vrcalibration.map")
WADS = ["quakevr/wads/id_textures.wad", "quakevr/wads/quakevr_dev.wad"]  # relative to the checkout (qbsp -wadpath)
EPS = 1e-6

# ---- the textures


def wad_sizes():
    """{texture name (lower case): (width, height)} of the WADs; the id one made by make_id_wad.py."""
    sizes = {}
    for rel in WADS:
        path = os.path.join(ROOT, rel)
        if not os.path.exists(path):
            sys.exit(f"{rel} is missing: python Misc/trenchbroom/make_id_wad.py makes it from your id1 paks")
        with open(path, "rb") as f:
            d = f.read()
        n, ofs = struct.unpack("<ii", d[4:12])
        for i in range(n):
            pos, _, _, _, _, _, name = struct.unpack("<iiibbh16s", d[ofs + 32 * i: ofs + 32 * i + 32])
            w, h = struct.unpack("<II", d[pos + 16:pos + 24])
            sizes.setdefault(name.split(b"\0")[0].decode("latin1").lower(), (w, h))
    return sizes


TEXSIZE = wad_sizes()

PANEL = "tech14_1"     # 128 x 128: a wall panel, one a side
FLOOR = "sfloor4_2"    # 64: floor plates
CEILING = "sfloor4_1"  # 64
WAINSCOT = "metal4_4"  # 64
STRIP_H = "tech04_1"   # 128 x 16: a riveted strip (skirting, rail, cornice, frames, the doorway's header)
STRIP_V = "tech04_3"   # 16 x 128: the same, upright (pilasters, the doorway's jambs, the board's frame)
LAMP = "tlight01"      # 32: a small round lamp (the pilasters')
RING = "ceil1_1"       # 16: a glowing tile (the coffer's ring)
PANEL_LIGHT = "light3_3"  # 64: the coffer's light panel
COFFER = "metal1_1"    # 64: the coffer's sides
BOARD = "black"
PORTAL = "*teleport"
PASSAGE = "tech08_2"   # 128: the passage's walls
PAD = "qvrc_pad"       # quakevr_dev.wad: the calibration spot


# ---- vectors

def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def unit(a):
    n = math.sqrt(dot(a, a))
    return (a[0] / n, a[1] / n, a[2] / n)


# ---- textures on faces

class Tex:
    """How a face is textured. mode:
    world -- world-aligned at `scale`, the texture's corner at `anchor` (coplanar faces continue each other);
    fit   -- fitted to the face's extent, `reps` (u, v) times (0: as many as come nearest at `scale`);
    fitu  -- the width (U) fitted reps[0] times, V world-aligned at `scale` from `anchor` (a panel on a cut corner);
    fitv  -- the height (V) fitted reps[1] times, U world-aligned at the same scale (a strip along its band);
    rot   -- the texture turned 90 degrees on the face."""

    def __init__(self, name, mode="world", scale=1.0, anchor=(32, 32, 0), reps=(0, 1), rot=False):
        self.name, self.mode, self.scale, self.anchor, self.reps, self.rot = name, mode, scale, anchor, reps, rot


def axes(n, rot):
    """The face's texture axes: in its plane, U to the right and V down as seen from in front (floors: world x, -y)."""
    if abs(n[2]) > 0.75:
        u, v = (1.0, 0.0, 0.0), (0.0, -1.0, 0.0)
    else:
        u = unit((-n[1], n[0], 0.0))
        v = cross(u, n)
    if rot:
        u, v = v, (-u[0], -u[1], -u[2])
    return u, v


def fmt(x):
    r = round(x)
    return str(int(r)) if abs(x - r) < 1e-4 else f"{x:.4f}".rstrip("0").rstrip(".")


def fitted(extent, size, reps, scale):
    reps = reps or max(1, round(extent / (size * scale)))
    return extent / (size * reps)


def face_line(pts, n, tex, on_plane):
    """A Valve 220 face line: three points (outward winding), the texture and its axes, offsets and scales."""
    u, v = axes(n, tex.rot)
    tw, th = TEXSIZE[tex.name.lower()]
    us = [dot(p, u) for p in on_plane]
    vs = [dot(p, v) for p in on_plane]
    su = sv = tex.scale
    ou, ov = -dot(tex.anchor, u) / su, -dot(tex.anchor, v) / sv
    if tex.mode in ("fit", "fitu"):
        su = fitted(max(us) - min(us), tw, tex.reps[0], tex.scale)
        ou = -min(us) / su
    if tex.mode in ("fit", "fitv"):
        sv = fitted(max(vs) - min(vs), th, tex.reps[1], tex.scale)
        ov = -min(vs) / sv
        if tex.mode == "fitv":
            su = sv
            ou = -dot(tex.anchor, u) / su
    ou, ov = ou % tw, ov % th
    p = " ".join("( " + " ".join(fmt(c) for c in q) + " )" for q in pts)
    return (f"{p} {tex.name} [ {fmt(u[0])} {fmt(u[1])} {fmt(u[2])} {fmt(ou)} ] "
            f"[ {fmt(v[0])} {fmt(v[1])} {fmt(v[2])} {fmt(ov)} ] 0 {fmt(su)} {fmt(sv)}")


def brush(verts, side, top=None, bottom=None):
    """A convex brush, the hull of `verts`; its faces textured by `side` (walls), `top` and `bottom` (normals within
    about 40 degrees of up or down)."""
    top, bottom = top or side, bottom or side
    centre = tuple(sum(v[k] for v in verts) / len(verts) for k in range(3))
    planes = []
    for a, b, c in itertools.combinations(verts, 3):
        nn = cross(sub(b, a), sub(c, a))
        if dot(nn, nn) < EPS:
            continue
        n = unit(nn)
        d = dot(n, a)
        if dot(n, centre) > d:
            n, d = (-n[0], -n[1], -n[2]), -d
        if any(dot(n, q) - d > 1e-4 for q in verts):
            continue
        if any(dot(n, m) > 1 - 1e-6 and abs(d - e) < 1e-4 for m, e in planes):
            continue
        planes.append((n, d))
    lines = ["{"]
    for n, d in planes:
        on = [q for q in verts if abs(dot(n, q) - d) < 1e-4]
        p1 = on[0]
        p2 = max(on, key=lambda q: dot(sub(q, p1), sub(q, p1)))
        p3 = max(on, key=lambda q: dot(cross(sub(p2, p1), sub(q, p1)), cross(sub(p2, p1), sub(q, p1))))
        if dot(cross(sub(p3, p1), sub(p2, p1)), n) < 0:
            p2, p3 = p3, p2
        tex = top if n[2] > 0.75 else bottom if n[2] < -0.75 else side
        lines.append(face_line((p1, p2, p3), n, tex, on))
    lines.append("}")
    return "\n".join(lines)


def prism(poly, z0, z1, side, top=None, bottom=None):
    """A brush from a convex polygon in x, y (either winding) between heights z0 and z1."""
    return brush([(x, y, z0) for x, y in poly] + [(x, y, z1) for x, y in poly], side, top, bottom)


def box(x0, y0, z0, x1, y1, z1, side, top=None, bottom=None):
    return prism([(x0, y0), (x1, y0), (x1, y1), (x0, y1)], z0, z1, side, top, bottom)


def entity(keys, brushes=()):
    return "{\n" + "".join(f'"{k}" "{v}"\n' for k, v in keys.items()) + "".join(b + "\n" for b in brushes) + "}"


# ---- the room: an octagon (a square with its corners cut at 45 degrees), the floor at z 0

H, C = 160, 96           # half the width (the straight walls' faces at x, y = +-H); the corners cut C back
Z = 176                  # the ceiling
T = 16                   # the walls' thickness
TOP = 232                # above everything (the coffer's top)
DOOR = 32                # the doorway: x -32..32 in the south wall, 96 high, a passage 64 deep to the hub's glow
DOOR_Z = 96
CASE = 16                # the doorway's casing, each side and above
PASS_Y = -H - 64         # the passage's end
RAIL_Z, PANEL_Z = 32, 40  # the wainscot's top (under its rail), the panels' bottom (128 high: up to the cornice)
CORNICE_Z = PANEL_Z + 128

# The octagon's corners, counter-clockwise from the south wall's east end: side i runs from OCT[i] to OCT[i + 1];
# side 7 is the south wall (the doorway's), 3 the north wall (the board's). Every side is about 128 long (the cut
# corners 136): one panel each.
OCT = [(H - C, -H), (H, -(H - C)), (H, H - C), (H - C, H), (-(H - C), H), (-H, H - C), (-H, -(H - C)), (-(H - C), -H)]
SOUTH = 7


def inset(poly, d):
    """The polygon moved in by d (out for d < 0), each side parallel to its own, the corners mitred."""
    out = []
    k = len(poly)
    for i in range(k):
        a, b, c = poly[i - 1], poly[i], poly[(i + 1) % k]
        lines = []
        for p, q in ((a, b), (b, c)):
            t = unit((q[0] - p[0], q[1] - p[1], 0.0))
            nin = (-t[1], t[0])
            lines.append(((p[0] + nin[0] * d, p[1] + nin[1] * d), t))
        (p1, t1), (p2, t2) = lines
        den = t1[0] * t2[1] - t1[1] * t2[0]
        s = ((p2[0] - p1[0]) * t2[1] - (p2[1] - p1[1]) * t2[0]) / den
        out.append((p1[0] + t1[0] * s, p1[1] + t1[1] * s))
    return out


def side_pieces(i, outer, inner, gap):
    """Side i's piece of a band between the polygons `outer` and `inner`: on the south wall two pieces, x outside
    -gap..gap (the doorway)."""
    a, b, ia, ib = outer[i], outer[(i + 1) % 8], inner[i], inner[(i + 1) % 8]
    if i != SOUTH:
        return [[a, b, ib, ia]]
    return [[a, (-gap, a[1]), (-gap, ia[1]), ia], [(gap, b[1]), b, ib, (gap, ib[1])]]


def straight(i):
    return i % 2 == 1


B = []   # the world's brushes
E = []   # entities (after the worldspawn)

# ---- the floor, the walls (a slab behind each side, one panel on it), the ceiling's slabs (below)
B.append(box(-H - T, PASS_Y - T, -T, H + T, H + T, 0, Tex(FLOOR)))
outer = inset(OCT, -T)
for i in range(8):
    # a straight side's panel world-aligned (128 wide from x or y -64: it runs on above the doorway); a cut corner's
    # fitted to its face
    t = Tex(PANEL, anchor=(64, 64, CORNICE_Z)) if straight(i) else Tex(PANEL, "fitu", reps=(1, 0), anchor=(0, 0, CORNICE_Z))
    for piece in side_pieces(i, OCT, outer, DOOR):
        B.append(prism(piece, -T, TOP, t))
B.append(box(-DOOR, -H - T, DOOR_Z, DOOR, -H, TOP, Tex(PANEL, anchor=(64, 64, CORNICE_Z))))
for i in range(8):  # behind the cut corners: the room's square corners filled
    a, b = OCT[i], OCT[(i + 1) % 8]
    if not straight(i):
        corner = (b[0] if abs(b[0]) == H else a[0], b[1] if abs(b[1]) == H else a[1])
        far = (corner[0] + math.copysign(T, corner[0]), corner[1] + math.copysign(T, corner[1]))
        B.append(prism([outer[i], outer[(i + 1) % 8], far], -T, TOP, Tex(PANEL)))


def band(z0, z1, depth, tex_of, gap=DOOR + CASE):
    """A moulding round the room's walls from z0 to z1, standing `depth` out from them, its corners mitred, the doorway
    left open. tex_of(i) -> the Tex of side i's faces."""
    inner = inset(OCT, depth)
    return [prism(piece, z0, z1, tex_of(i)) for i in range(8) for piece in side_pieces(i, OCT, inner, gap)]


STRIP = Tex(STRIP_H, "fitv")
B += band(0, 8, 4, lambda i: STRIP)
B += band(8, RAIL_Z, 2, lambda i: Tex(WAINSCOT, "fitu", reps=(2, 0), anchor=(0, 0, RAIL_Z)))
B += band(RAIL_Z, PANEL_Z, 4, lambda i: STRIP)
inner8 = inset(OCT, 8)
for i in range(8):  # the cornice: a 45-degree bevel from the wall's panels' top out to 8 under the ceiling
    a, b, ia, ib = OCT[i], OCT[(i + 1) % 8], inner8[i], inner8[(i + 1) % 8]
    B.append(brush([(a[0], a[1], CORNICE_Z), (b[0], b[1], CORNICE_Z), (a[0], a[1], Z), (b[0], b[1], Z),
                    (ia[0], ia[1], Z), (ib[0], ib[1], Z)], STRIP))

# ---- pilasters over the panels' joints (16 wide, 4 out), a small lamp on each
LAMPS = []
for i in range(8):
    v = OCT[i]
    bis = unit((-v[0], -v[1], 0.0))      # the corner's inward bisector (the room is centred on the origin)
    perp = (-bis[1], bis[0])

    def at(s, d, v=v, bis=bis, perp=perp):
        return (v[0] + perp[0] * s + bis[0] * d, v[1] + perp[1] * s + bis[1] * d)

    B.append(prism([at(-8, -4), at(8, -4), at(8, 5), at(-8, 5)], RAIL_Z, CORNICE_Z, Tex(STRIP_V, "fit", reps=(1, 1)),
                   STRIP, STRIP))
    B.append(prism([at(-6, 5), at(6, 5), at(6, 9), at(-6, 9)], 126, 138, Tex(STRIP_H, "fit", reps=(1, 1)),
                   STRIP, STRIP))
    B.append(prism([at(-5, 9), at(5, 9), at(5, 10), at(-5, 10)], 127, 137, Tex(LAMP, "fit", reps=(1, 1))))
    LAMPS.append(at(0, 24))

# ---- the ceiling: a stepped coffer (z 176 to 216 over x, y -88..88), a ring of light tiles under its step, a light panel
STEP, INNER, CZ = 96, 88, 216
for x0, y0, x1, y1 in ((-H - T, STEP, H + T, H + T), (-H - T, -H - T, H + T, -STEP),
                       (-H - T, -STEP, -STEP, STEP), (STEP, -STEP, H + T, STEP)):
    B.append(box(x0, y0, Z, x1, y1, Z + 8, STRIP, bottom=Tex(CEILING)))
for x0, y0, x1, y1 in ((-H - T, INNER, H + T, H + T), (-H - T, -H - T, H + T, -INNER),
                       (-H - T, -INNER, -INNER, INNER), (INNER, -INNER, H + T, INNER)):
    B.append(box(x0, y0, Z + 8, x1, y1, TOP, Tex(COFFER, anchor=(32, 32, CZ)), bottom=Tex(RING, scale=0.5, anchor=(0, 0, 0))))
B.append(box(-INNER, -INNER, CZ, INNER, INNER, TOP, Tex(COFFER), bottom=Tex(CEILING, anchor=(32, 32, 0))))
B.append(box(-32, -32, CZ - 4, 32, 32, CZ, Tex(STRIP_H, "fit", reps=(1, 1)), bottom=Tex(PANEL_LIGHT, "fit", reps=(1, 1))))

# ---- the calibration spot: an octagonal pad 2 high, its edge bevelled, the ring on its top
PADPOLY = [(x * 48, y * 48) for x, y in ((0.5, -1), (1, -0.5), (1, 0.5), (0.5, 1), (-0.5, 1), (-1, 0.5), (-1, -0.5), (-0.5, -1))]
B.append(brush([(x, y, 0) for x, y in PADPOLY] + [(x, y, 2) for x, y in inset(PADPOLY, 2)], STRIP,
               top=Tex(PAD, "fit", reps=(1, 1))))

# ---- the board: a black panel in a frame on the north wall (its face 3 out), the texts 1 in front of it
BX, BZ0, BZ1, F = 52, 48, 152, 8
B.append(box(-BX, H - 3, BZ0, BX, H, BZ1, Tex(BOARD)))
B.append(box(-BX - F, H - 5, BZ0 - F, BX + F, H, BZ0, Tex(STRIP_H, "fit", reps=(1, 1))))
B.append(box(-BX - F, H - 5, BZ1, BX + F, H, BZ1 + F, Tex(STRIP_H, "fit", reps=(1, 1))))
B.append(box(-BX - F, H - 5, BZ0, -BX, H, BZ1, Tex(STRIP_V, "fit", reps=(1, 1))))
B.append(box(BX, H - 5, BZ0, BX + F, H, BZ1, Tex(STRIP_V, "fit", reps=(1, 1))))

# ---- the doorway: its casing on the room's side, the passage, the hub's teleporter glow at its end
e = DOOR + CASE
B.append(box(-e, -H, 0, -DOOR, -H + 6, DOOR_Z, Tex(STRIP_V, "fit", reps=(1, 1))))
B.append(box(DOOR, -H, 0, e, -H + 6, DOOR_Z, Tex(STRIP_V, "fit", reps=(1, 1))))
B.append(box(-e, -H, DOOR_Z, e, -H + 6, DOOR_Z + CASE, Tex(STRIP_H, "fit", reps=(1, 1))))
WALLP = Tex(PASSAGE, anchor=(0, -H, DOOR_Z))
B.append(box(-DOOR - T, PASS_Y - T, -T, -DOOR, -H - T, DOOR_Z, WALLP))
B.append(box(DOOR, PASS_Y - T, -T, DOOR + T, -H - T, DOOR_Z, WALLP))
B.append(box(-DOOR - T, PASS_Y - T, -T, DOOR + T, PASS_Y, DOOR_Z, WALLP))
B.append(box(-DOOR - T, PASS_Y - T, DOOR_Z, DOOR + T, -H - T, DOOR_Z + T, WALLP, bottom=Tex(CEILING)))
GLOW_Y = PASS_Y + 8
E.append(entity({"classname": "func_illusionary"},
                [box(-DOOR, PASS_Y, 0, DOOR, GLOW_Y, DOOR_Z, Tex(PORTAL, "fit", reps=(1, 0)))]))
E.append(entity({"classname": "trigger_changelevel", "map": "vrstart", "spawnflags": "1"},
                [box(-DOOR, GLOW_Y, 0, DOOR, GLOW_Y + 24, DOOR_Z, Tex("trigger"))]))

# ---- the boards' texts


def board(text, x, y, z, angle, scale, halign="1"):
    """A text board (func_worldtext_banner) facing `angle`; its lines split with \\n."""
    E.append(entity({"classname": "func_worldtext_banner", "worldtext": text, "worldtext_halign": halign,
                     "worldtext_scale": scale, "angle": str(angle), "origin": f"{x} {y} {z}"}))


N = "\\n"
TEXT_Y = H - 4
board("VR CALIBRATION", 0, TEXT_Y, 140, 270, "0.7")
board(N.join(["Stand in the middle of your play space", "and follow the words in front of you:",
              "your height, then your body."]), 0, TEXT_Y, 122, 270, "0.33")
board(N.join(["Again later, from the menus:", "Height: {menu:VR Settings>Set Height Now}",
              "Body: {menu:Body Calibration}"]), 0, TEXT_Y, 100, 270, "0.28")
board(N.join(["Hands: {menu:Hand/Gun Calibration}", "Seated? {menu:Body Calibration>Position}"]),
      0, TEXT_Y, 78, 270, "0.28")
board(N.join(["VR HUB", "Walk through when you are done."]), 0, -H + 7, DOOR_Z + CASE + 20, 90, "0.33")

# ---- the player, the lights
E.append(entity({"classname": "info_player_start", "origin": "0 0 26", "angle": "90"}))
WARM = "1 0.9 0.75"
# the coffer's ring and panel give light (ericw-tools' _surface: copied over every face of the texture)
for tex, value in ((RING, "40"), (PANEL_LIGHT, "160")):
    E.append(entity({"classname": "light", "_surface": tex, "light": value, "wait": "1", "_color": "1 0.97 0.92",
                     "_surface_offset": "2", "origin": "0 0 160"}))
# the pilasters' lamps: each washes its two panels
for x, y in LAMPS:
    E.append(entity({"classname": "light", "light": "200", "wait": "1.2", "_color": WARM, "origin": f"{fmt(x)} {fmt(y)} 132"}))
# the room's fill, from the coffer
E.append(entity({"classname": "light", "light": "280", "wait": "0.6", "_color": "1 0.97 0.92", "origin": "0 0 196"}))
# the hub's blue glow out of the doorway
E.append(entity({"classname": "light", "light": "260", "wait": "1.2", "_color": "0.5 0.6 1", "origin": f"0 {PASS_Y + 24} 48"}))


# The room's music: a CD track of Quake's own (id1's, Nine Inch Nails' soundtrack; only a reference: it plays from the
# player's own Quake, at his music volume; none there: silence). Calm and otherworldly for setting up (the author,
# 2026-10-10): 6, "Parallel Dimensions", its slow synth drones.
MUSIC = 6


def write(path):
    with open(path, "w", newline="\n") as f:
        f.write('// Game: Quake VR\n// Format: Valve\n// Written by Misc/quakevr/make_vrcalibration_map.py: edit that, not this.\n'
                '// entity 0\n{\n"classname" "worldspawn"\n"mapversion" "220"\n'
                f'"wad" "{";".join(WADS)}"\n"_tb_mod" "hipnotic;rogue;quakevr"\n'
                f'"message" "VR Calibration"\n"worldtype" "2"\n"sounds" "{MUSIC}"\n"light" "20"\n"_vr_debris" "0"\n"_vr_crates" "0"\n'
                '"_qvr_prelit" "1"\n')  # (lit here: the game's relight batches pass it over)
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
                 "-dirtdepth", "96", "-bounce", "-lit", "-lux", "-lightgrid", "-lightgrid_dist", "32", "32", "32", bsp]):
        result = subprocess.run(cmd, capture_output=True, text=True)
        lines = (result.stdout + result.stderr).splitlines()
        warnings = [l for l in lines if "WARNING" in l.upper() or "ERROR" in l.upper() or "LEAK" in l.upper()]
        print(f"{os.path.basename(cmd[0])}: exit {result.returncode}" + "".join("\n  " + w for w in warnings[:12]))
        if result.returncode:
            sys.exit(1)
    for ext in (".bsp", ".lit", ".lux"):
        shutil.copyfile(os.path.join(work, "vrcalibration" + ext), os.path.join(os.path.dirname(OUT), "vrcalibration" + ext))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compile", action="store_true", help="also build the .bsp, .lit and .lux (ericw-tools 2.0)")
    parser.add_argument("--tools", default=DEFAULT_TOOLS)
    parser.add_argument("--work", default=os.path.join(ROOT, "scratch", "vrcalibration_build"),
                        help="the compile's folder (its .prt and copies stay there)")
    args = parser.parse_args()
    write(OUT)
    print(f"wrote {OUT}: {len(B)} brushes, {len(E)} entities; boards name {', '.join(menu_specs())}")
    if args.compile:
        compile_map(args.tools, args.work)


if __name__ == "__main__":
    main()
