# make_vrslipgates_map.py [--compile | --tests]: writes quakevr/maps/vrslipgates.map, the slipgate test map (ROUND21.md,
# "Slipgate test map"), and with --compile builds it as MAPPING.md's "Full" profile does, without -dirt (even, bright
# light to debug by): qbsp, vis, light (C:\OHWorkspace\ericw-tools-2.0.0-alpha11-win64) into quakevr/maps (.bsp, .lit, .lux).
# --tests prints where to stand at each gate (slipgates_test.sh) and writes nothing.
#
# id's textures (T_* below; the gates' faces are id's *teleport): run Misc/trenchbroom/make_id_wad.py first. The .bsp
# embeds the ones it uses (the author's decision, as for the other test maps built from id_textures.wad).
#
# A slipgate here is what Quake VR builds one from (Quake/vr/vr_portals.hpp): world faces textured *tele... in a
# wall, and a trigger_teleport over them reaching 8 units out in front of them, whose target is an
# info_teleport_destination. Every gate is two-way: each side's destination stands DEST_OFF units out from the other
# side's face (clear of that side's trigger and frame for a shambler-sized monster: Quake's teleport puts monsters
# there), facing out of it. The rooms (interiors; walls 16 thick outside them):
#   hub     the start; corridors north (flush gallery) and south (heights and water room)
#   FA, FB  flush galleries: FA's north wall and FB's south wall, face to face 272 apart, have four gates each,
#           their bottoms at the floor: crate (48x48), player (64x96), large (128x160: shambler, vore), wide (256x128).
#           FB lies behind FA's gates, so a monster chasing a player who went through walks straight into them.
#   GA, GB  framed galleries (east of FA/FB): the same pairs in protruding frames with sills: a crate hatch (sill 24),
#           player (sill 16: a step, for players and monsters), player (sill 32: a jump; monsters can't), large and
#           wide (sill 16).
#   T, U    turns: T's west and east gates loop into each other (walk west for ever); T's north gate comes out of U's
#           east wall (a 90-degree turn); T's south-west corner is a 45-degree wall whose gate comes out of U's south
#           wall (135 degrees).
#   LV      heights and water: a gate at the floor comes out of a gate over a 128-high platform (stairs back down);
#           two gates beside a sunken pool come out facing it (east wall <-> south wall, 90 degrees).
# Each room has a panel of buttons that spawn a monster (func_enemy_dispenser) by its far wall: grunt, dog, ogre,
# shambler, scrag. Crates, ammo boxes and weapons lie by the gates; a ramp in FA rolls crates into the crate gate.
import math, os, shutil, subprocess, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
OUT = os.path.join(ROOT, "quakevr", "maps", "vrslipgates.map")
TOOLS = r"C:\OHWorkspace\ericw-tools-2.0.0-alpha11-win64"
WALL, TELE_DEPTH, TRIG_OUT, FRAME = 16, 8, 8, 16
DEST_OFF = {False: 48, True: 56}  # destination's distance out from the exit's face: flush, framed
# id's textures (the author's decision: the committed .bsp embeds the ones it uses), from quakevr/wads/id_textures.wad,
# which Misc/trenchbroom/make_id_wad.py makes from the player's own id1 paks (git-ignored, never committed). The
# compiler's own (trigger) come from either WAD.
TELE, T_WATER, T_WALL, T_FLOOR, T_CEIL = "*teleport", "*water0", "tech06_1", "sfloor4_2", "ceiling1_3"
T_FRAME, T_PANEL, T_BUTTON = "slipside", "comp1_1", "+0basebtn"
WADS = "quakevr/wads/id_textures.wad;quakevr/wads/quakevr_dev.wad"


def cross(a, b): return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def face(p0, n, tex):
    """A face through the integer point p0 facing out along the integer vector n (Valve 220). Its three points are
    integers, so faces sharing a plane are exactly coplanar (no cracks at the 45-degree gate)."""
    ln = math.sqrt(sum(c * c for c in n))
    if abs(n[2]) >= 0.7 * ln:
        U = [64, 0, 0]
        tu, tv = [1, 0, 0], [0, -1, 0]
    else:
        U = [n[1] * 64, -n[0] * 64, 0]
        lu = math.sqrt(U[0] ** 2 + U[1] ** 2)
        tu, tv = [U[0] / lu, U[1] / lu, 0], [0, 0, -1]
    V = cross(U, n)  # (p2 - p0) x (p1 - p0) = V x U = n |U|^2: outward along n, as qbsp reads it
    p1 = [p0[i] + U[i] for i in range(3)]
    p2 = [p0[i] + V[i] for i in range(3)]
    f = lambda p: " ".join(str(int(c)) for c in p)
    g = lambda v: " ".join(f"{c:.6g}" for c in v)
    return f"( {f(p0)} ) ( {f(p1)} ) ( {f(p2)} ) {tex} [ {g(tu)} 0 ] [ {g(tv)} 0 ] 0 1 1"


def brush(planes):
    return "{\n" + "\n".join(face(p, n, t) for p, n, t in planes) + "\n}"


def box(x0, y0, z0, x1, y1, z1, tex, top=None, front=None):
    """An axis-aligned box; `top` textures its top face; `front` = (normal, texture) for one side face."""
    x0, x1 = sorted((x0, x1))
    y0, y1 = sorted((y0, y1))
    z0, z1 = sorted((z0, z1))
    t = lambda n: front[1] if front and list(front[0]) == n else tex
    return brush([([x0, y0, z0], [-1, 0, 0], t([-1, 0, 0])), ([x1, y1, z1], [1, 0, 0], t([1, 0, 0])),
                  ([x0, y0, z0], [0, -1, 0], t([0, -1, 0])), ([x1, y1, z1], [0, 1, 0], t([0, 1, 0])),
                  ([x0, y0, z0], [0, 0, -1], tex), ([x1, y1, z1], [0, 0, 1], top or tex)])


WORLD = []     # world brushes
ENTS = []      # (key-value dict, [brushes])
TESTS = []     # (name, stand(back) of each side): setpos strings for the test scripts (--tests)


def esc(v):
    """A key's value as the .map file has it: a new line as a backslash and an n (Quake's entity strings)."""
    return str(v).replace(chr(10), chr(92) + "n")


def ent(classname, brushes=None, **kv):
    d = {"classname": classname}
    for k, v in kv.items():
        d[k] = v
    ENTS.append((d, brushes or []))


# Sides of a room: their outward normal (into the wall) and the yaw out of the wall (into the room).
SIDES = {"n": ((0, 1), 270), "s": ((0, -1), 90), "e": ((1, 0), 180), "w": ((-1, 0), 0)}
YAW_IN = {"n": 90, "s": 270, "e": 0, "w": 180}  # the yaw of walking into a wall (a button's travel)


class Room:
    def __init__(self, name, x0, y0, x1, y1, z0=0, z1=256, pool=None):
        self.name, self.x0, self.y0, self.x1, self.y1, self.z0, self.z1 = name, x0, y0, x1, y1, z0, z1
        self.ops = {s: [] for s in SIDES}
        self.pool = pool
        ROOMS.append(self)

    def at(self, side, a, d):
        """The point `a` along a side's wall, `d` units into the wall from its face (negative: into the room)."""
        if side == "n": return (a, self.y1 + d)
        if side == "s": return (a, self.y0 - d)
        if side == "e": return (self.x1 + d, a)
        return (self.x0 - d, a)

    def boxat(self, side, a0, a1, d0, d1, z0, z1, tex, top=None, front=None):
        p, q = self.at(side, a0, d0), self.at(side, a1, d1)
        return box(p[0], p[1], z0, q[0], q[1], z1, tex, top, front)

    def door(self, side, a0, a1, zt=128):
        self.ops[side].append(dict(a0=a0, a1=a1, zb=self.z0, zt=zt, gate=False))

    def build(self):
        x0, y0, x1, y1, z0, z1, W = self.x0, self.y0, self.x1, self.y1, self.z0, self.z1, WALL
        WORLD.append(box(x0 - W, y0 - W, z1, x1 + W, y1 + W, z1 + W, T_CEIL))
        if self.pool:
            px0, py0, px1, py1, depth = self.pool
            WORLD.append(box(x0 - W, y0 - W, z0 - depth - W, x1 + W, y1 + W, z0 - depth, T_WALL))
            for b in ((x0 - W, y0 - W, px0, y1 + W), (px1, y0 - W, x1 + W, y1 + W), (px0, y0 - W, px1, py0),
                      (px0, py1, px1, y1 + W)):
                WORLD.append(box(b[0], b[1], z0 - depth, b[2], b[3], z0, T_WALL, T_FLOOR))
            WORLD.append(box(px0, py0, z0 - depth, px1, py1, z0 - 8, T_WATER))
        else:
            WORLD.append(box(x0 - W, y0 - W, z0 - W, x1 + W, y1 + W, z0, T_FLOOR))
        for side in SIDES:
            lo, hi = (x0 - W, x1 + W) if side in "ns" else (y0, y1)
            cur = lo
            for op in sorted(self.ops[side], key=lambda o: o["a0"]):
                if op["a0"] > cur:
                    WORLD.append(self.boxat(side, cur, op["a0"], 0, W, z0, z1, T_WALL))
                a0, a1 = op["a0"], op["a1"]
                if op["zb"] > z0:
                    WORLD.append(self.boxat(side, a0, a1, 0, W, z0, op["zb"], T_WALL))
                if op["zt"] < z1:
                    WORLD.append(self.boxat(side, a0, a1, 0, W, op["zt"], z1, T_WALL))
                if op["gate"]:
                    # the gate's sheet (id's are about 8 deep, a wall right behind), and its backing; a deeper sheet
                    # reaches out past the wall into a pocket of its own
                    dp, zb, zt = op["depth"], op["zb"], op["zt"]
                    WORLD.append(self.boxat(side, a0, a1, 0, dp, zb, zt, TELE))
                    WORLD.append(self.boxat(side, a0, a1, dp, max(W, dp + 8), zb, zt, T_WALL))
                    if dp + 8 > W:
                        for b in ((a0 - 8, a0, zb - 8, zt + 8), (a1, a1 + 8, zb - 8, zt + 8), (a0, a1, zb - 8, zb),
                                  (a0, a1, zt, zt + 8)):
                            WORLD.append(self.boxat(side, b[0], b[1], W, dp + 8, b[2], b[3], T_WALL))
                cur = a1
            if cur < hi:
                WORLD.append(self.boxat(side, cur, hi, 0, W, z0, z1, T_WALL))
        n = max(1, round((x1 - x0) / 224)), max(1, round((y1 - y0) / 224))
        for i in range(n[0]):
            for j in range(n[1]):
                ent("light", origin=f"{x0 + (2 * i + 1) * (x1 - x0) // (2 * n[0])} "
                                    f"{y0 + (2 * j + 1) * (y1 - y0) // (2 * n[1])} {z1 - 40}", light="340")

    def panel(self, side, a, tag, monsters, spawn):
        """A row of buttons on a wall, each spawning a monster at `spawn` (a function of its index: x, y, z)."""
        for i, (label, kind) in enumerate(monsters):
            c = a + i * 40
            name = f"{tag}_{label.lower()}"
            br = self.boxat(side, c - 12, c + 12, -8, 0, 32, 56, T_PANEL,
                            front=(tuple(-v for v in SIDES[side][0]) + (0,), T_BUTTON))
            ent("func_button", [br], angle=str(YAW_IN[side]), wait="1", worldtext=label, worldtext_halign="1",
                worldtext_scale="0.3", target=name)
            sx, sy, sz = spawn(i)
            ent("func_enemy_dispenser", origin=f"{sx} {sy} {sz}", targetname=name, weapon=str(kind))
        p = self.at(side, a + 80, -4)
        ent("func_worldtext_banner", origin=f"{p[0]} {p[1]} 84", angle=str(SIDES[side][1]), worldtext_halign="1",
            worldtext_scale="0.35", worldtext="Spawn a monster\n(it appears by the far wall)")


ROOMS = []
MONSTERS = [("Grunt", 0), ("Dog", 7), ("Ogre", 1), ("Shambler", 3), ("Scrag", 4)]


class Gate:
    def __init__(self, room, side, center, width, height, sill=0, framed=False, floor=None, label="", depth=TELE_DEPTH):
        self.room, self.side, self.framed, self.label = room, side, framed, label
        self.floor = room.z0 if floor is None else floor
        self.a0, self.a1 = center - width // 2, center + width // 2
        self.zb, self.zt = self.floor + sill, self.floor + sill + height
        self.center, self.name = center, f"{room.name}_{side}{center}"
        room.ops[side].append(dict(a0=self.a0, a1=self.a1, zb=self.zb, zt=self.zt, gate=True, depth=depth))
        self.out_yaw = SIDES[side][1]

    def dest(self):
        p = self.room.at(self.side, self.center, -DEST_OFF[self.framed])
        return p[0], p[1], self.floor + 1

    def stand(self, back=64):
        """Where a player stands facing into the gate, `back` units out from it (a setpos)."""
        p = self.room.at(self.side, self.center, -back)
        return f"{p[0]} {p[1]} {self.floor + 24} 0 {YAW_IN[self.side]} 0"

    def build(self, target):
        r, s = self.room, self.side
        if self.framed:
            F = FRAME
            WORLD.append(r.boxat(s, self.a0 - F, self.a0, -F, 0, self.floor, self.zt + F, T_FRAME))
            WORLD.append(r.boxat(s, self.a1, self.a1 + F, -F, 0, self.floor, self.zt + F, T_FRAME))
            WORLD.append(r.boxat(s, self.a0 - F, self.a1 + F, -F, 0, self.zt, self.zt + F, T_FRAME))
            if self.zb > self.floor:
                WORLD.append(r.boxat(s, self.a0, self.a1, -F, 0, self.floor, self.zb, T_FRAME))
        ent("trigger_teleport", [r.boxat(s, self.a0, self.a1, -TRIG_OUT, 4, self.zb, self.zt, "trigger")],
            target=target)
        p = r.at(s, self.center, -(FRAME if self.framed else 0) - 4)
        ent("func_worldtext_banner", origin=f"{p[0]} {p[1]} {self.zt + (FRAME if self.framed else 0) + 20}",
            angle=str(self.out_yaw), worldtext_halign="1", worldtext_scale="0.4", worldtext=self.label)


def link(a, b):
    """Two-way: walking into a comes out of b, and into b out of a."""
    for g, other in ((a, b), (b, a)):
        name = "to_" + other.name
        g.build(name)
        x, y, z = other.dest()
        ent("info_teleport_destination", origin=f"{x} {y} {z}", angle=str(other.out_yaw), targetname=name)


# ---------------------------------------------------------------------------------------------------------------------
# The rooms
hub = Room("hub", -256, -256, 256, 128, z1=192)
FA = Room("fa", -640, 256, 640, 640)
FB = Room("fb", -640, 928, 640, 1312)
GA = Room("ga", 832, 256, 2112, 640)
GB = Room("gb", 832, 928, 2112, 1312)
T = Room("t", -1600, 256, -960, 768)
U = Room("u", -1600, 1056, -960, 1504)
LV = Room("lv", -512, -1088, 512, -448, z1=352, pool=(64, -1024, 448, -576, 64))

# Corridors (rooms too), and the doors cut into both walls they pass through.
corridors = [Room("c_hub_fa", -48, 144, 48, 240, z1=160), Room("c_fa_fb", -624, 656, -560, 912, z1=160),
             Room("c_fa_ga", 656, 400, 816, 496, z1=160), Room("c_fb_gb", 656, 1088, 816, 1184, z1=160),
             Room("c_ga_gb", 2032, 656, 2096, 912, z1=160), Room("c_t_fa", -944, 288, -656, 384, z1=160),
             Room("c_u_fb", -944, 1088, -656, 1184, z1=160), Room("c_hub_lv", -48, -432, 48, -272, z1=160)]
hub.door("n", -48, 48); corridors[0].door("s", -48, 48); corridors[0].door("n", -48, 48); FA.door("s", -48, 48)
FA.door("n", -624, -560); corridors[1].door("s", -624, -560); corridors[1].door("n", -624, -560); FB.door("s", -624, -560)
FA.door("e", 400, 496); corridors[2].door("w", 400, 496); corridors[2].door("e", 400, 496); GA.door("w", 400, 496)
FB.door("e", 1088, 1184); corridors[3].door("w", 1088, 1184); corridors[3].door("e", 1088, 1184); GB.door("w", 1088, 1184)
GA.door("n", 2032, 2096); corridors[4].door("s", 2032, 2096); corridors[4].door("n", 2032, 2096); GB.door("s", 2032, 2096)
T.door("e", 288, 384); corridors[5].door("w", 288, 384); corridors[5].door("e", 288, 384); FA.door("w", 288, 384)
U.door("e", 1088, 1184); corridors[6].door("w", 1088, 1184); corridors[6].door("e", 1088, 1184); FB.door("w", 1088, 1184)
hub.door("s", -48, 48); corridors[7].door("n", -48, 48); corridors[7].door("s", -48, 48); LV.door("n", -48, 48)

# Flush galleries: FA's north wall <-> FB's south wall, the gates at the same x.
# The large pair's sheets are 48 deep (the rest 8, as id's): a prop's box stops against the wall behind a sheet
# shallower than its half-width before its middle reaches the gate (ROUND21.md, "Slipgate test map").
FLUSH = [("crate", -448, 48, 48, 8), ("player", -256, 64, 96, 8), ("large", 0, 128, 160, 48), ("wide", 352, 256, 128, 8)]
for kind, x, w, h, dp in FLUSH:
    deep = f", {dp} deep" if dp > TELE_DEPTH else ""
    a = Gate(FA, "n", x, w, h, depth=dp, label=f"FLUSH {kind} {w}x{h}{deep}\nto the north gallery")
    b = Gate(FB, "s", x, w, h, depth=dp, label=f"FLUSH {kind} {w}x{h}{deep}\nback to the south gallery")
    link(a, b)
    TESTS.append((f"flush_{kind}", a.stand, b.stand))

# Framed galleries: GA's north wall <-> GB's south wall.
FRAMED = [("crate", 1000, 48, 48, 24), ("player", 1180, 64, 96, 16), ("highsill", 1360, 64, 96, 32),
          ("large", 1580, 128, 160, 16), ("wide", 1860, 256, 128, 16)]
for kind, x, w, h, sill in FRAMED:
    a = Gate(GA, "n", x, w, h, sill, True, label=f"FRAMED {kind} {w}x{h}\nsill {sill}: to the north")
    b = Gate(GB, "s", x, w, h, sill, True, label=f"FRAMED {kind} {w}x{h}\nsill {sill}: back south")
    link(a, b)
    TESTS.append((f"framed_{kind}", a.stand, b.stand))

# Turns. The loop: T's west gate comes out of T's east gate walking west, and the east one out of the west one.
tw = Gate(T, "w", 560, 64, 96, label="LOOP: walk west\nfor ever")
te = Gate(T, "e", 560, 64, 96, label="LOOP: walk east\nfor ever")
link(tw, te)
TESTS.append(("loop", tw.stand, te.stand))
# 90 degrees: T's north gate (walking north) comes out of U's east wall walking west.
tn = Gate(T, "n", -1280, 64, 96, label="90 DEGREES\nout of the next room's east wall")
ue = Gate(U, "e", 1408, 64, 96, label="90 DEGREES\nback to the turns room")
link(tn, ue)
TESTS.append(("turn90", tn.stand, ue.stand))
# 45 degrees: T's south-west corner is a wall along x + y = C; its gate (walking south-west, yaw 225) comes out of U's
# south wall walking north (a 135-degree turn). Planes through integer points with integer normals only.
C, D, K0, K1 = -1152, 12, -1902, -1810  # the face; the sheet's depth (in x + y); the aperture's ends (x - y)
DZ = 96
us = Gate(U, "s", -1280, 64, 96, label="45-DEGREE WALL\nback to the turns room")
for planes in (
        # the corner's fill: north-west of the opening, south-east of it, behind it, and over it
        [([-1600, 0, 0], [-1, 0, 0]), ([C, 0, 0], [1, 1, 0]), ([K0, 0, 0], [1, -1, 0])],
        [([0, 256, 0], [0, -1, 0]), ([C, 0, 0], [1, 1, 0]), ([K1, 0, 0], [-1, 1, 0])],
        [([-1600, 0, 0], [-1, 0, 0]), ([0, 256, 0], [0, -1, 0]), ([C - D, 0, 0], [1, 1, 0]), ([K0, 0, 0], [-1, 1, 0]),
         ([K1, 0, 0], [1, -1, 0])]):
    WORLD.append(brush([(p, n, T_WALL) for p, n in planes] + [([0, 0, 0], [0, 0, -1], T_WALL),
                                                                    ([0, 0, 256], [0, 0, 1], T_WALL)]))
gatecut = [([K0, 0, 0], [-1, 1, 0]), ([K1, 0, 0], [1, -1, 0])]
WORLD.append(brush([(p, n, T_WALL) for p, n in gatecut + [([C, 0, 0], [1, 1, 0]), ([C - D, 0, 0], [-1, -1, 0]),
                                                               ([0, 0, DZ], [0, 0, -1]), ([0, 0, 256], [0, 0, 1])]]))
WORLD.append(brush([(p, n, TELE) for p, n in gatecut + [([C, 0, 0], [1, 1, 0]), ([C - D, 0, 0], [-1, -1, 0]),
                                                         ([0, 0, 0], [0, 0, -1]), ([0, 0, DZ], [0, 0, 1])]]))
ent("trigger_teleport", [brush([(p, n, "trigger") for p, n in gatecut + [
    ([C + 11, 0, 0], [1, 1, 0]), ([C - 6, 0, 0], [-1, -1, 0]), ([0, 0, 0], [0, 0, -1]), ([0, 0, DZ], [0, 0, 1])]])],
    target="to_" + us.name)
mid = ((K0 + K1) // 2 + C) // 2, (C - (K0 + K1) // 2) // 2  # (-1504, 352)
ent("info_teleport_destination", origin=f"{mid[0] + 34} {mid[1] + 34} 1", angle="45", targetname="to_t_diag")
us.build("to_t_diag")
x, y, z = us.dest()
ent("info_teleport_destination", origin=f"{x} {y} {z}", angle=str(us.out_yaw), targetname="to_" + us.name)
ent("func_worldtext_banner", origin=f"{mid[0] + 3} {mid[1] + 3} {DZ + 20}", angle="45", worldtext_halign="1",
    worldtext_scale="0.4", worldtext="45-DEGREE WALL\nout of the next room's south wall")
TESTS.append(("turn45", lambda back=64: f"{mid[0] + round(back / 1.4142)} {mid[1] + round(back / 1.4142)} 24 0 225 0", us.stand))

# Heights and water. A platform (top 128) in LV's north-west, stairs up its south side; the west wall's floor-level
# gate comes out of the gate over the platform, and back.
WORLD.append(box(-512, -768, 0, -192, -448, 128, T_WALL, T_FLOOR))
for i in range(8):
    WORLD.append(box(-320, -896 + 16 * i, 0, -224, -768, 16 * (i + 1), T_WALL, T_FLOOR))
low = Gate(LV, "w", -960, 64, 96, label="UP 128\nto the platform")
high = Gate(LV, "w", -608, 64, 96, floor=128, label="DOWN 128\nto the floor")
link(low, high)
TESTS.append(("heights", low.stand, high.stand))
# The pool (x 64..448, y -1024..-576, 64 deep, its water 8 under the floor): gates in the east and south walls 64 from
# its edges, each coming out of the other facing it.
pe = Gate(LV, "e", -800, 64, 96, label="WATER: out of the\nsouth wall, facing the pool")
ps = Gate(LV, "s", 256, 64, 96, label="WATER: out of the\neast wall, facing the pool")
link(pe, ps)
TESTS.append(("water", pe.stand, ps.stand))

# FA's ramp: a wedge (its top from z 64 at y 400 down to the floor at y 540) in front of the crate gate, 100 units
# short of it (clear of the destination there): a crate let go at its top slides at the gate.
WORLD.append(brush([([-488, 0, 0], [-1, 0, 0], T_WALL), ([-408, 0, 0], [1, 0, 0], T_WALL),
                    ([0, 400, 0], [0, -1, 0], T_WALL), ([0, 0, 0], [0, 0, -1], T_WALL),
                    ([-488, 540, 0], [0, 16, 35], T_FLOOR)]))

# Monster panels (the dispensers by the far wall; the scrag in the air).
FA.panel("s", -600, "fa", MONSTERS, lambda i: (-480 + 200 * i, 336, 96 if i == 4 else 32))
FB.panel("n", -600, "fb", MONSTERS, lambda i: (-480 + 200 * i, 1232, 96 if i == 4 else 32))
GA.panel("s", 880, "ga", MONSTERS, lambda i: (1060 + 200 * i, 336, 96 if i == 4 else 32))
GB.panel("n", 880, "gb", MONSTERS, lambda i: (1060 + 200 * i, 1232, 96 if i == 4 else 32))
T.panel("s", -1200, "t", MONSTERS, lambda i: (-1400 + 100 * i, 420, 96 if i == 4 else 32))
U.panel("n", -1500, "u", MONSTERS, lambda i: (-1400 + 100 * i, 1300, 96 if i == 4 else 32))
LV.panel("n", 100, "lv", MONSTERS, lambda i: (16 + 96 * i, -512, 96 if i == 4 else 32))

# Props: crates, ammo boxes and weapons by the gates.
for kind, x, w, h, dp in FLUSH:
    ent("vr_crate", origin=f"{x - w // 2 - 48} 560 0", angle="10")
    ent("item_shells", origin=f"{x + w // 2 + 40} 470 0")
ent("vr_crate", origin="-448 416 64", angle="0")
ent("vr_crate", origin="-448 1100 0", spawnflags="1", angle="30")
for kind, x, w, h, sill in FRAMED:
    ent("vr_crate", origin=f"{x - w // 2 - 64} 560 0", spawnflags="1" if kind in ("large", "wide") else "0")
    ent("item_spikes", origin=f"{x + w // 2 + 48} 470 0")
for x, y in ((-1300, 640), (-1460, 460), (-1100, 1300), (-320, -980), (-256, -620), (380, -900)):
    ent("vr_crate", origin=f"{x} {y} {128 if y == -620 else 0}")
ent("item_rockets", origin="400 -700 0")
ent("item_health", origin="-1100 600 0")
for i, (w, flags) in enumerate(((2, 0), (4, 0), (17, 0), (13, 0), (14, 0))):  # axe, shotgun, crowbar, sword, chainsaw
    ent("func_weapon_grabbable", origin=f"{-160 + 80 * i} -32 24", weapon=str(w), weaponflags=str(flags))
ent("item_cells", origin="160 -200 0")
ent("item_shells", origin="200 -200 0")
ent("item_rockets", origin="-200 -200 0")

# The start, its signs and a tip.
ent("info_player_start", origin="0 -160 24", angle="90")
ent("func_worldtext_banner", origin="0 124 132", angle="270", worldtext_halign="1", worldtext_scale="0.45",
    speed="0.085", worldtext="Slipgate test map$North: flush gates,\ngates in frames east$West of them: turns, a loop$"
                             "South: heights and water")
ent("func_worldtext_banner", origin="-252 -64 72", angle="0", worldtext_halign="1", worldtext_scale="0.35",
    worldtext="Every gate goes both ways.\nButtons in each room spawn monsters.")
ent("func_worldtext_banner", origin="252 -64 72", angle="180", worldtext_halign="1", worldtext_scale="0.35",
    worldtext="Grab, throw, push crates\nand boxes through the gates.")
ent("func_vr_tip", origin="0 -96 64", spawnflags="4", tipname="vrslipgates_welcome", distance="200",
    message="Slipgate test map: every gate goes both ways.\nEach room's buttons spawn monsters.")

for r in ROOMS:
    r.build()

if "--tests" in sys.argv:  # the gates' standing places only (slipgates_test.sh), nothing written
    for name, a, b in TESTS:  # name|A side|B side|A from 200 back|B from 200 back
        print(f"{name}|{a()}|{b()}|{a(200)}|{b(200)}")
    sys.exit(0)

with open(OUT, "w", newline="\n") as f:
    f.write('// Game: Quake VR\n// Format: Valve\n// entity 0\n{\n"classname" "worldspawn"\n"mapversion" "220"\n'
            '"wad" "' + WADS + '"\n"_tb_mod" "hipnotic;rogue;quakevr"\n'
            '"message" "Quake VR slipgate test"\n"worldtype" "2"\n"light" "80"\n"_vr_crates" "0"\n"_vr_debris" "0"\n')
    f.write("\n".join(WORLD) + "\n}\n")
    for i, (kv, brushes) in enumerate(ENTS):
        f.write(f"// entity {i + 1}\n{{\n" + "".join(f'"{k}" "{esc(v)}"\n' for k, v in kv.items()))
        f.write("".join(b + "\n" for b in brushes) + "}\n")
print(f"wrote {os.path.relpath(OUT, ROOT)}: {len(WORLD)} world brushes, {len(ENTS)} entities")
for name, a, b in TESTS:
    print(f"  {name}: setpos {a()}  |  {b()}")

if "--compile" in sys.argv:
    work = os.path.join(ROOT, "scratch", "vrslipgates_build")
    os.makedirs(work, exist_ok=True)
    src, bsp = os.path.join(work, "vrslipgates.map"), os.path.join(work, "vrslipgates.bsp")
    if not os.path.exists(os.path.join(ROOT, "quakevr", "wads", "id_textures.wad")):
        sys.exit("no quakevr/wads/id_textures.wad: run python Misc/trenchbroom/make_id_wad.py first")
    shutil.copyfile(OUT, src)
    for cmd in ([os.path.join(TOOLS, "qbsp.exe"), "-nolog", "-nopercent", "-wadpath", ROOT, src, bsp],
                [os.path.join(TOOLS, "vis.exe"), "-nolog", "-nopercent", bsp],
                [os.path.join(TOOLS, "light.exe"), "-nolog", "-nopercent", "-extra4",
                 "-lit", "-lux", "-lightgrid", "-lightgrid_dist", "64", "64", "64", bsp]):
        r = subprocess.run(cmd, capture_output=True, text=True)
        lines = (r.stdout + r.stderr).splitlines()
        bad = [l for l in lines if "WARNING" in l.upper() or "ERROR" in l.upper() or "LEAK" in l.upper()]
        print(f"{os.path.basename(cmd[0])}: exit {r.returncode}" + "".join("\n  " + w for w in bad[:12]))
        if r.returncode:
            sys.exit(1)
    for ext in (".bsp", ".lit", ".lux"):
        shutil.copyfile(os.path.join(work, "vrslipgates" + ext), os.path.join(os.path.dirname(OUT), "vrslipgates" + ext))
    print("copied vrslipgates.bsp, .lit, .lux to quakevr/maps")
