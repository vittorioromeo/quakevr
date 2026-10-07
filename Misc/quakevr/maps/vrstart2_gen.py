# vrstart2_gen.py -- writes quakevr/maps/vrstart2.map, the new VR hub (an island at night), and with --compile builds
# it (qbsp 0.18.1, ericw-tools 2.0's vis and light; presets "fast" and "final": compile_map, PRESETS, MAPPING.md).
#
#   python Misc/quakevr/maps/vrstart2_gen.py [--compile [--preset fast|final] [--check RAYS]] [--tools DIR] [--qbsp EXE]
#   (first: python Misc/trenchbroom/make_id_wad.py, the id textures' WAD)
#
# Everything in the map is made here (reproducible; the .map stays editable in TrenchBroom: the generated parts are
# TrenchBroom groups, the props func_detail): edit this script, not the .map. Layout (x east, y north, the water's
# surface at z 0, 32.8 units a metre):
#
#   - a lake 8000 units across (244 m) ringed by cliffs and mountains (the map's border), sky above, at night;
#   - the island in its middle (about 3100 x 2200 units, 95 x 67 m): a rocky beach round a grassy upland, a ravine with
#     the sea in it cutting in from the north;
#   - the path, from the south-west corner: the pier where the player starts -> the arrival beach (welcome, the
#     tutorial and calibration buttons) -> a wooden staircase up the bank -> the campaign terrace (the campaign
#     buttons and the slipgate) -> a wooden bridge over the ravine -> the settings pavilion (setting buttons) -> a
#     staircase down -> the firing range (a few basic guns, ammunition, targets) -> a path along the shore -> the
#     lookout tower (a ladder up; a diving board over deep water).
#
# Textures: id's (see TEXN).
import argparse
import math
import os
import random
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import mapgeom
from mapgeom import (Tex, MapWriter, Perlin, add, box, beam, catmull_rom, cross, cylinder, delaunay, dot, hull, length,
                     lerp, ngon, norm, point_in_poly, polyline_dist, prism, seg_dist, smoothstep, sub, mul, terrain_mesh, simplify_points, unbend)

ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
DEFAULT_TOOLS = "C:/OHWorkspace/ericw-tools-2.0.0-alpha11-win64"  # vis, light
# qbsp: ericw-tools 0.18.1's (the author's decision, 2026-10-07). 2.0-alpha11's makes faces from its BSP's portals and
# fills through them; on this map, even with the slivers it loses faces at taken out (terrain_mesh, unbend, hull...),
# a few portals still failed: missing faces and air made solid (0-9 holes in 600,000 rays per build, chaotic: any edit
# moved them), and its fix (an unfilled hull 0 spliced with filled clipping hulls) made loads slower. 0.18.1's makes
# faces by CSG: 0 holes in a million rays (bsp_holes.py). ROUND21.md, "vrstart2 on ericw-tools 2.0 again".
DEFAULT_QBSP = "C:/OHWorkspace/ericw-tools-v0.18.1-32-g6660c5f-win64/bin/qbsp.exe"
MAPNAME = "vrstart2"
OUT = os.path.join(ROOT, "quakevr", "maps", MAPNAME + ".map")

# ---------------------------------------------------------------------------------------------------------------------
# Textures: id's own (quakevr/wads/id_textures.wad, extracted from the player's paks by
# Misc/trenchbroom/make_id_wad.py, never committed: the author's decision is that the compiled hub may embed them),
# and Quake VR's (quakevr_dev.wad, Misc/trenchbroom/make_assets.py) for what id has none of.
TEXN = {
    "grass": "grass1_1", "grass2": "ground1_2", "path": "ground1_8", "sand": "rock3_2", "seabed": "ground1_5",
    "cliff": "rock5_2", "cliff2": "rock3_8", "cliffwet": "rock5_1", "moss": "rock4_2", "plank": "woodflr1_2",
    "beam": "wood1_3", "log": "cliff2_1", "logend": "wood1_7", "board": "wood1_1", "rope": "rock3_8",
    "iron": "metal1_1", "flag": "azfloor1_1", "block": "wswamp2_1", "trim": "wall14_5", "water": "*04awater1",
    "portal": "*teleport", "sky": "sky1", "crystal": "tlight03", "button": "+0basebtn", "target": "qvr_target",
    "bark": "cliff2_1", "needles": "wgrass1_1", "lamp": "qvr_lantern", "roof": "wizwood1_2", "rune": "sliplite",
    "clip": "clip", "trigger": "trigger", "skip": "skip",
}
WADS = "quakevr/wads/id_textures.wad;quakevr/wads/quakevr_dev.wad"


def T(key, **kw):
    return Tex(TEXN[key], **kw)


# ---------------------------------------------------------------------------------------------------------------------
# The world's dimensions
BOX = 4000            # the inside of the sealing box: x, y in -BOX..BOX
SKY_TOP = 2304
FLOOR_Z = -1024       # the terrain prisms' bottoms
WATER_Z = 0
SEA_DEPTH = 300       # the lake's floor, far from the shores
GROUND_STEP = 8       # the island's ground heights are multiples of this (height(): Quake's collision)

PN = Perlin(7)        # terrain
PN2 = Perlin(11)      # the cliffs' line and heights
PN3 = Perlin(23)      # texture patches

# The island's coast (control points, anticlockwise from the south-west beach), smoothed
COAST_CTRL = [(-1250, -1000), (-700, -1060), (-100, -990), (500, -1060), (980, -1010), (1250, -960), (1430, -760),
              (1570, -320), (1620, 150), (1470, 620), (1060, 960), (500, 1110), (-100, 1060), (-500, 1170),
              (-820, 1170), (-1220, 960), (-1520, 520), (-1630, 0), (-1560, -520), (-1440, -860)]
COAST = catmull_rom(COAST_CTRL, 10)

# Islets in the lake: (x, y, radius, peak height)
ISLETS = [(2350, 1550, 230, 70), (-2550, 1650, 170, 40), (2050, -2150, 200, 95), (-2400, -1900, 150, 26)]

# Rolling hills on the island: (x, y, radius, height added)
HILLS = [(450, 650, 420, 170), (-150, -560, 330, 90), (900, 380, 300, 60), (-1250, 520, 380, 70), (150, 900, 300, 50)]

# Flattened places: (kind, shape, target z, blend); kind 'set' (to the height), 'cut' (no higher than it);
# shape ('circle', x, y, r) or ('rect', x0, y0, x1, y1)
ZONES = []

# Paths: polylines of (x, y, z) (z: the walking height), their half-width
PATHS = []

# The ravine (the sea in a cleft from the north coast): its centre line, the water channel's and the banks' half-widths
RAVINE = [(-640, 1400), (-630, 950), (-665, 560), (-640, 200), (-650, -130), (-630, -330), (-610, -430)]
RAVINE_IN, RAVINE_OUT, RAVINE_BED = 64, 175, -56

# Deep water close by the shore (the diving board): (x, y, r)
DEEP = [(1160, -1110, 260)]

# ---- the places along the path (see the header)
PIER = dict(x=-1200, y0=-850, y1=-1370, z=24, w=104)
ARRIVAL = (-1170, -760)
STAIR1 = dict(cx=-1200, cy=-500, dx=0, dy=-1, zb=16, zt=112, run=12, w=96)   # (its top edge, the way down)
TERRACE = dict(x=-1150, y=-150, r=232, z=112)
GATE = dict(x=-1150, y=46)                                                  # the slipgate (faces south)
BRIDGE = dict(x0=-868, x1=-436, y=-150, z=112, arch=14, w=96)
PAVILION = dict(x0=-300, x1=84, y0=-232, y1=40, z=128)
STAIR2 = dict(cx=84, cy=-96, dx=1, dy=0, zb=56, zt=128, run=16, w=80)
RANGE = dict(x0=300, x1=1500, y0=-250, y1=170, z=56, line=470)
TOWER = dict(x=1160, y=-770, z=56, half=88, deck=260)


def stair_info(st):
    """A flight's numbers: risers, its bottom edge's centre, its footprint (x0, y0, x1, y1)."""
    n = (st["zt"] - st["zb"]) // 8
    L = (n - 1) * st["run"]
    bx, by = st["cx"] + st["dx"] * L, st["cy"] + st["dy"] * L
    hw = st["w"] / 2 + 36
    if st["dx"]:
        fp = (min(st["cx"], bx), st["cy"] - hw, max(st["cx"], bx), st["cy"] + hw)
    else:
        fp = (st["cx"] - hw, min(st["cy"], by), st["cx"] + hw, max(st["cy"], by))
    return n, (bx, by), fp


def build_layout():
    """The zones and paths from the places above."""
    t, p, b, pv, rg, tw = TERRACE, PIER, BRIDGE, PAVILION, RANGE, TOWER
    s1, s2 = STAIR1, STAIR2
    n1, s1_bot, fp1 = stair_info(s1)
    n2, s2_bot, fp2 = stair_info(s2)
    # the arrival beach, and the upland above the bank
    ZONES.append(("set", ("rect", -1520, -930, -900, s1_bot[1] - 6), s1["zb"] - 2, 60))
    ZONES.append(("set", ("rect", -1480, s1["cy"] - 4, -960, -300), s1["zt"] - 6, 60))
    ZONES.append(("set", ("circle", p["x"], p["y0"] + 30, 90), 12, 60))
    # under the staircases: a ramp below their treads
    ZONES.append(("ramp", ("rect",) + fp1, (s1["cx"], s1["cy"], s1["zt"] - 22, s1_bot[0], s1_bot[1], s1["zb"] - 6), 8))
    ZONES.append(("ramp", ("rect",) + fp2, (s2["cx"], s2["cy"], s2["zt"] - 24, s2_bot[0], s2_bot[1], s2["zb"] - 6), 8))
    # the campaign terrace and the path to it
    ZONES.append(("set", ("circle", t["x"], t["y"], t["r"] + 24), t["z"] - 8, 90))
    PATHS.append(([(s1["cx"], s1["cy"] + 52, s1["zt"] - 6), (s1["cx"] + 20, s1["cy"] + 100, s1["zt"] - 6),
                   (t["x"] - 10, t["y"] - t["r"] - 10, t["z"] - 6)], 52))
    # the bridge's ends
    ZONES.append(("set", ("rect", b["x0"] - 60, b["y"] - 90, b["x0"] + 20, b["y"] + 90), b["z"] - 6, 50))
    ZONES.append(("set", ("rect", b["x1"] - 20, b["y"] - 90, b["x1"] + 70, b["y"] + 90), b["z"] - 6, 50))
    # the pavilion and the path to it, the range
    ZONES.append(("set", ("rect", pv["x0"] - 30, pv["y0"] - 30, pv["x1"] + 4, pv["y1"] + 30), pv["z"] - 16, 70))
    PATHS.append(([(b["x1"] + 50, b["y"], b["z"] - 6), (b["x1"] + 110, b["y"] + 20, b["z"] - 4),
                   (pv["x0"] - 40, (pv["y0"] + pv["y1"]) / 2, pv["z"] - 16)], 50))
    ZONES.append(("set", ("rect", rg["x0"], rg["y0"], rg["x1"], rg["y1"]), rg["z"], 80))
    PATHS.append(([(s2_bot[0] + 20, s2["cy"], s2["zb"]), (rg["line"] - 60, -60, rg["z"])], 50))
    # the backstop: a bank of earth behind the targets
    ZONES.append(("set", ("rect", rg["x1"] - 10, rg["y0"] - 40, rg["x1"] + 90, rg["y1"] + 40), rg["z"] + 120, 50))
    # along the shore to the tower
    ZONES.append(("set", ("circle", tw["x"], tw["y"], tw["half"] + 50), tw["z"], 60))
    PATHS.append(([(rg["line"] - 40, rg["y0"] + 10, rg["z"]), (520, -470, 60), (760, -640, 58), (980, -676, 56),
                   (tw["x"] - 20, tw["y"] + tw["half"] + 40, tw["z"])], 50))


# ---------------------------------------------------------------------------------------------------------------------
# The height field
class Grid:
    """A function sampled on a grid, read back bilinearly (the coast's distance: costly to compute at every point)."""

    def __init__(self, fn, x0, y0, x1, y1, step):
        self.x0, self.y0, self.step = x0, y0, step
        self.nx, self.ny = int((x1 - x0) / step) + 2, int((y1 - y0) / step) + 2
        self.v = [[fn(x0 + i * step, y0 + j * step) for i in range(self.nx)] for j in range(self.ny)]
        self.x1, self.y1 = x0 + (self.nx - 1) * step, y0 + (self.ny - 1) * step

    def inside(self, x, y):
        return self.x0 <= x < self.x1 and self.y0 <= y < self.y1

    def __call__(self, x, y):
        fx, fy = (x - self.x0) / self.step, (y - self.y0) / self.step
        i, j = int(fx), int(fy)
        tx, ty = fx - i, fy - j
        v = self.v
        a = lerp(v[j][i], v[j][i + 1], tx)
        b = lerp(v[j + 1][i], v[j + 1][i + 1], tx)
        return lerp(a, b, ty)


def coast_distance_exact(x, y):
    best = 1e18
    n = len(COAST)
    for i in range(n):
        a, b = COAST[i], COAST[(i + 1) % n]
        d, _ = seg_dist(x, y, a[0], a[1], b[0], b[1])
        best = min(best, d)
    return best if point_in_poly(x, y, COAST) else -best


COAST_GRID = None


def coast_distance(x, y):
    if COAST_GRID.inside(x, y):
        return COAST_GRID(x, y)
    return coast_distance_exact(x, y)


def cliff_radius(theta):
    """Where the cliffs rise from the lake: further out towards the corners of the square map."""
    c, s = math.cos(theta), math.sin(theta)
    return (3200 + 380 * (2 * c * s) ** 2 + 230 * PN2.fbm(c * 1.7 + 5, s * 1.7 + 5, 3)
            + 110 * PN2(c * 6 + 1, s * 6 + 2) + 45 * PN2(c * 15 + 9, s * 15 - 4))


def cliff_params(theta):
    c, s = math.cos(theta), math.sin(theta)
    top = 500 + 240 * PN2.fbm(c * 2.3 + 11, s * 2.3, 3) + 130 * PN2(c * 8 - 5, s * 8 + 3)
    ridge = 1300 + 420 * PN2.fbm(c * 1.3 - 3, s * 1.3 + 7, 3) + 160 * PN2(c * 5 + 2, s * 5 - 6)
    return top, ridge


# Ledges at the cliffs' feet, a swimmer's way out of the water: (angle in degrees, half-width in degrees)
CLIFF_LEDGES = [(20, 4), (95, 3), (150, 5), (215, 4), (290, 3), (340, 4)]


def cliff_height(x, y):
    r = math.hypot(x, y)
    theta = math.atan2(y, x)
    e = r - cliff_radius(theta)
    if e < -480:
        return None
    top, ridge = cliff_params(theta)
    if e < -40:
        h = -SEA_DEPTH + (SEA_DEPTH - 90) * smoothstep(-480, -40, e) + 18 * PN.fbm(x / 300, y / 300, 3)
        deg = math.degrees(theta) % 360
        for a, w in CLIFF_LEDGES:
            da = abs((deg - a + 180) % 360 - 180)
            if da < w and e > -150:
                h = max(h, 12 + 5 * PN.fbm(x / 60, y / 60, 2))
        return h
    if e < 60:
        t = (e + 40) / 100
        return lerp(-90, top, t ** 0.85) + 70 * PN.fbm(x / 110, y / 110, 3) * math.sin(math.pi * t) + 25 * PN(x / 50, y / 50)
    k = smoothstep(60, 750, e)
    rough = 130 * PN.fbm(x / 240 + 3, y / 240, 5) * (0.4 + k)
    return top + (ridge - top) * k + rough


def sea_height(x, y, d):
    dd = -d
    if dd < 100:
        h = -dd * 0.16
    else:
        h = -16 - (SEA_DEPTH - 16) * (1 - math.exp(-(dd - 100) / 230))
    h += 14 * PN.fbm(x / 420, y / 420, 3) * smoothstep(60, 400, dd)
    for (ix, iy, r, peak) in ISLETS:
        dist = math.hypot(x - ix, y - iy)
        if dist < r * 3:
            k = math.exp(-(dist / r) ** 2 * 1.3)
            bump = (peak + SEA_DEPTH) * k - SEA_DEPTH + 30 * PN.fbm(x / 90, y / 90, 3) * k
            h = max(h, bump)
    for (dx, dy, r) in DEEP:
        dist = math.hypot(x - dx, y - dy)
        if dist < r:
            h = min(h, lerp(h, -SEA_DEPTH, smoothstep(r, r * 0.45, dist)))
    return h


def land_height(x, y, d):
    """The island above the water (d: the distance in from its coast)."""
    beach = min(d, 150) * 0.12
    inland = 96 + 26 * PN.fbm(x / 700, y / 700, 3)
    for (hx, hy, r, ht) in HILLS:
        dist2 = (x - hx) ** 2 + (y - hy) ** 2
        inland += ht * math.exp(-dist2 / (r * r))
    h = lerp(beach, inland, smoothstep(110, 430, d))
    h += 5 * PN.fbm(x / 120, y / 120, 2) * smoothstep(20, 120, d)
    return h


def zone_weight(shape, x, y, blend):
    if shape[0] == "circle":
        _, cx, cy, r = shape
        dist = math.hypot(x - cx, y - cy) - r
    else:
        _, x0, y0, x1, y1 = shape
        dx = max(x0 - x, 0, x - x1)
        dy = max(y0 - y, 0, y - y1)
        dist = math.hypot(dx, dy)
    if dist <= 0:
        return 1.0
    return 1.0 - smoothstep(0, blend, dist)


def path_height(x, y):
    """(weight, height) of the nearest path at (x, y)."""
    best = (0.0, 0.0)
    for pts, hw in PATHS:
        d, i, t = polyline_dist(x, y, pts)
        if d < hw + 130:
            z = lerp(pts[i][2], pts[i + 1][2], t)
            w = 1.0 - smoothstep(hw - 10, hw + 130, d)  # (wide banks: gentle enough to walk off the path)
            if w > best[0]:
                best = (w, z - 3)
    return best


def on_path(x, y, margin=0):
    for pts, hw in PATHS:
        d, _, _ = polyline_dist(x, y, pts)
        if d < hw - margin:
            return True
    return False


def height(x, y):
    d = coast_distance(x, y)
    if d >= 0:
        h = land_height(x, y, d)
    else:
        h = sea_height(x, y, d)
    # the flattened places
    for kind, shape, target, blend in ZONES:
        w = zone_weight(shape, x, y, blend)
        if w <= 0 or target is None:
            continue
        if kind == "set":
            h = lerp(h, target, w)

    w, pz = path_height(x, y)
    if w > 0:
        h = lerp(h, pz, w)
    for kind, shape, target, blend in ZONES:  # (the staircases' ramps over the paths' ends)
        if kind == "ramp":
            w = zone_weight(shape, x, y, blend)
            if w > 0:
                ax, ay, za, bx, by, zb = target
                L2 = (bx - ax) ** 2 + (by - ay) ** 2
                tt = max(0.0, min(1.0, ((x - ax) * (bx - ax) + (y - ay) * (by - ay)) / L2))
                h = lerp(h, lerp(za, zb, tt), w)
    # the ravine
    rd, _, _ = polyline_dist(x, y, RAVINE)
    if rd < RAVINE_OUT:
        bed = RAVINE_BED + 10 * PN.fbm(x / 80, y / 80, 2)
        if rd < RAVINE_IN:
            h = min(h, bed)
        else:
            k = (rd - RAVINE_IN) / (RAVINE_OUT - RAVINE_IN)
            h = min(h, lerp(bed, h, k ** 1.6))
    ch = cliff_height(x, y)
    if ch is not None:
        return max(h, ch)
    # The island's walkable ground in steps of GROUND_STEP units: triangles nearly but not quite coplanar (the noise, the
    # blends, rounding) make Quake's collision snag the player on flat ground (the hull traces start "solid" at their
    # seams); with heights in steps most neighbours are exactly coplanar, the rest at clear angles. (A walk test of 36
    # legs over gentle ground: 24 stopped short before, 7 after; ROUND21.md, "vrstart2".)
    if h > 6 and d > -100:
        h = max(8, GROUND_STEP * round(h / GROUND_STEP))
    return h


# ---------------------------------------------------------------------------------------------------------------------
# The terrain: points (a jittered lattice, denser where it matters, and the features' outlines), a Delaunay
# triangulation, a prism under each triangle.
def spacing_class(x, y):
    """1, 2, 4 or 8: the lattice step (64 units) there."""
    d = coast_distance(x, y)
    if d > -200:
        return 1
    r = math.hypot(x, y)
    e = r - cliff_radius(math.atan2(y, x))
    if -260 < e < 320:
        return 2
    for (ix, iy, rr, _) in ISLETS:
        if math.hypot(x - ix, y - iy) < rr * 1.8:
            return 1
    if d > -650:
        return 2
    if e > 900 or d < -1100:
        return 8  # far up the mountains, or deep in the lake (under the water's fog): 512 units
    return 4


def terrain_points():
    rnd = random.Random(5)
    pts = {}
    feat = []

    def add_feat(x, y):
        feat.append((int(round(x)), int(round(y))))

    # the cliffs' foot, face and top: rings of points so the face is crisp
    jr = random.Random(77)
    for ring, e in enumerate((-40, -15, 10, 35, 60, 180)):
        n = 260
        for i in range(n):
            th = 2 * math.pi * (i + 0.5 * (ring % 2) + jr.uniform(-0.2, 0.2)) / n
            r = cliff_radius(th) + e + (jr.uniform(-7, 7) if -40 < e < 60 else 0)
            x, y = r * math.cos(th), r * math.sin(th)
            if abs(x) < BOX - 40 and abs(y) < BOX - 40:
                add_feat(x, y)
    # the flattened places' outlines
    for kind, shape, target, blend in ZONES:
        if shape[0] == "circle":
            _, cx, cy, r = shape
            n = max(8, int(2 * math.pi * r / 56))
            for i in range(n):
                a = 2 * math.pi * i / n
                add_feat(cx + r * math.cos(a), cy + r * math.sin(a))
        elif kind in ("set", "ramp"):
            _, x0, y0, x1, y1 = shape
            for (ax, ay, bx, by) in ((x0, y0, x1, y0), (x1, y0, x1, y1), (x1, y1, x0, y1), (x0, y1, x0, y0)):
                n = max(1, int(math.hypot(bx - ax, by - ay) / 56))
                for i in range(n):
                    add_feat(lerp(ax, bx, i / n), lerp(ay, by, i / n))
    # the paths' edges
    for pts_, hw in PATHS:
        for i in range(len(pts_) - 1):
            ax, ay, _ = pts_[i]
            bx, by, _ = pts_[i + 1]
            L = math.hypot(bx - ax, by - ay)
            nx, ny = -(by - ay) / L, (bx - ax) / L
            n = max(1, int(L / 44))
            for k in range(n + 1):
                t = k / n
                cx, cy = lerp(ax, bx, t), lerp(ay, by, t)
                for off in (-hw, hw):
                    add_feat(cx + nx * off, cy + ny * off)
    # the ravine's channel
    for i in range(len(RAVINE) - 1):
        ax, ay = RAVINE[i]
        bx, by = RAVINE[i + 1]
        L = math.hypot(bx - ax, by - ay)
        nx, ny = -(by - ay) / L, (bx - ax) / L
        n = max(1, int(L / 60))
        for k in range(n):
            t = k / n
            cx, cy = lerp(ax, bx, t), lerp(ay, by, t)
            for off in (-RAVINE_IN, 0, RAVINE_IN):
                add_feat(cx + nx * off, cy + ny * off)
    featset = {}
    for f in feat:
        featset[(f[0] // 32, f[1] // 32)] = f
    feat = list(featset.values())
    bucket = {}
    for f in feat:
        bucket.setdefault((f[0] // 32, f[1] // 32), []).append(f)

    def near_feature(x, y, r=26):
        cx, cy = int(x) // 32, int(y) // 32
        for i in (-1, 0, 1):
            for j in (-1, 0, 1):
                for f in bucket.get((cx + i, cy + j), ()):
                    if (f[0] - x) ** 2 + (f[1] - y) ** 2 < r * r:
                        return True
        return False

    # the lattice
    n = BOX // 64
    for i in range(-n, n + 1):
        for j in range(-n, n + 1):
            x, y = i * 64, j * 64
            jx, jy = rnd.uniform(-15, 15), rnd.uniform(-15, 15)
            edge = abs(i) == n or abs(j) == n
            k = spacing_class(x, y)
            if not edge and (i % k or j % k):
                continue
            if edge:
                corner = abs(i) == n and abs(j) == n
                if not corner and (i % 4 if abs(j) == n else j % 4):
                    continue
                px = BOX if i == n else -BOX if i == -n else x
                py = BOX if j == n else -BOX if j == -n else y
            else:
                px, py = x + jx * (1 if k == 1 else 2), y + jy * (1 if k == 1 else 2)
                if near_feature(px, py):
                    continue
            pts[(int(round(px)), int(round(py)))] = 1
    for f in feat:
        if abs(f[0]) < BOX and abs(f[1]) < BOX:
            pts[f] = 1
    return list(pts.keys())


def terrain_texture(c, n, x, y):
    cz = c[2]
    d = coast_distance(x, y)
    if n[2] < 0.72:
        if cz < -24:
            return T("cliffwet", mode="face", scale=2)
        return T("cliff", mode="face", scale=2) if PN3(x / 500, y / 500) > -0.15 else T("cliff2", mode="face", scale=2)
    if cz < -10:
        return T("seabed", scale=2)
    if d >= 0 and on_path(x, y, 6):
        return T("path")
    if d >= 0:
        if d < 170 and cz < 30:
            return T("sand")
        return T("grass", scale=1.5) if PN3.fbm(x / 420, y / 420, 2) > -0.1 else T("grass2", scale=1.5)
    # not the island: the cliffs' gentle parts, islets' tops, ledges
    if cz < 30:
        return T("sand") if n[2] > 0.95 else T("cliff2", mode="face", scale=2)
    if n[2] > 0.88 and cz < 1100:
        return T("moss", scale=2)
    return T("cliff2", mode="face", scale=2)


# The terrain is simplified away from the island (mapgeom.simplify_points): a point is left out where the surface
# without it is within this many units of it. The island, its shore and the islets keep every point.
TERRAIN_TOL_LAKE = 3      # the lake's floor (under 16 to 300 units of dark water)
TERRAIN_TOL_CLIFF = 2     # the cliffs, from their feet to their tops (the ledges a swimmer climbs out on)
TERRAIN_TOL_FAR = 6       # the mountains behind the cliffs' tops


def terrain_tolerance(x, y):
    if abs(x) >= BOX or abs(y) >= BOX or coast_distance(x, y) > -300:
        return 0
    for (ix, iy, rr, _) in ISLETS:
        if math.hypot(x - ix, y - iy) < rr * 1.8:
            return 0
    e = math.hypot(x, y) - cliff_radius(math.atan2(y, x))
    if e > 320:
        return TERRAIN_TOL_FAR
    if e > -260:
        return TERRAIN_TOL_CLIFF
    return TERRAIN_TOL_LAKE


def build_terrain(mw):
    t0 = time.time()
    pts = terrain_points()
    H = [int(round(height(p[0], p[1]))) for p in pts]
    tol = [terrain_tolerance(p[0], p[1]) for p in pts]
    kept = simplify_points(pts, H, tol, [i for i in range(len(pts)) if tol[i] == 0])
    print("terrain: %d of %d points kept (%.1f s)" % (len(kept), len(pts), time.time() - t0))
    pts = [pts[i] for i in kept]
    pts, moves, left = unbend(pts, set(i for i, p in enumerate(pts) if abs(p[0]) >= BOX or abs(p[1]) >= BOX), rounds=12)
    print("terrain: %d points moved off nearly straight lines of edges (%d left)" % (moves, left))
    H = [int(round(height(p[0], p[1]))) for p in pts]
    tris = delaunay(pts)
    area = sum(abs((pts[b][0] - pts[a][0]) * (pts[c][1] - pts[a][1]) - (pts[b][1] - pts[a][1]) * (pts[c][0] - pts[a][0]))
               for a, b, c in tris) / 2
    if abs(area - (2 * BOX) ** 2) > 1:
        print("WARNING: the triangulation covers %.0f, not %.0f" % (area, (2 * BOX) ** 2))
    texcache = {}

    def tri_tex(ti, H):
        if ti not in texcache:
            a, b, c = tris[ti]
            tri = [(pts[i][0], pts[i][1], H[i]) for i in (a, b, c)]
            cx, cy, cz = [sum(q[k] for q in tri) / 3 for k in range(3)]
            nrm = norm(cross(sub(tri[1], tri[0]), sub(tri[2], tri[0])))
            if nrm[2] < 0:
                nrm = mul(nrm, -1)
            texcache[ti] = terrain_texture((cx, cy, cz), nrm, cx, cy)
        t = texcache[ti]
        return (t.name, t.mode, t.scale)

    # watertight, no nearly coplanar neighbours (ericw-tools 2.0's qbsp loses faces at those: mapgeom.terrain_mesh),
    # exactly coplanar neighbours of one texture merged into convex prisms
    # the island's walkable ground keeps its heights (steps of GROUND_STEP: exactly level or clearly sloped, which is
    # what keeps the player from snagging on it; height())
    walk = [i for i, p in enumerate(pts) if coast_distance(p[0], p[1]) > -100 and H[i] % GROUND_STEP == 0 and H[i] >= 8]
    H, polys, st = terrain_mesh(pts, tris, H, tri_tex, pinned=walk, levels=(WATER_Z,))
    print("terrain: %d corners moved (at most %.2f units, %d moves): %d nearly coplanar pairs and %d nearly level tops left; "
          "%d triangles -> %d prisms" % (st["moved"], st["drift"], st["moves"], st["left"], st["tilted"], st["tris"], st["polys"]))
    # detail: the structural world is only the sealing box (an open lake: vis has nothing to cull, and a structural
    # height field of ten thousand prisms makes qbsp's tree enormous)
    groups = {"terrain island": mw.detail("terrain: island"), "terrain lake": mw.detail("terrain: lake floor"),
              "terrain cliffs": mw.detail("terrain: cliffs and mountains")}
    side = T("cliff2")
    for ti in range(len(tris)):
        tri_tex(ti, H)
    for cyc, members in polys:
        poly = [(pts[i][0], pts[i][1], H[i]) for i in cyc]
        cx = sum(q[0] for q in poly) / len(poly)
        cy = sum(q[1] for q in poly) / len(poly)
        br = prism(poly, FLOOR_Z, texcache[members[0]], side)
        d = coast_distance(cx, cy)
        r = math.hypot(cx, cy)
        if d > -300:
            groups["terrain island"].append(br)
        elif r > cliff_radius(math.atan2(cy, cx)) - 300:
            groups["terrain cliffs"].append(br)
        else:
            groups["terrain lake"].append(br)
    print("terrain: %d points, %d triangles (%.1f s)" % (len(pts), len(tris), time.time() - t0))
    # the things built on it keep their corners clear of the ground and the water's surface (mapgeom.SURFACES)
    mapgeom.SURFACES[:] = [(mapgeom.MeshSurface(pts, tris, H), True),
                           (lambda x, y: WATER_Z if abs(x) < BOX and abs(y) < BOX else None, False)]
    return H


def ground(x, y):
    return height(x, y)


# ---------------------------------------------------------------------------------------------------------------------
# Building blocks
def part(mw, name):
    """A structure's brushes: a func_detail (a TrenchBroom group). (As func_walls of their own the faces were as many,
    and the dynamic lights' shadow casters' search cost more.)"""
    return mw.detail(name)


PIECE_CLASS = "func_detail"


class Pieces:
    """Things spread over the map (pines, boulders, torches' posts, crystals), split() after each: gathered by
    768-unit tile of the map, a func_detail each, all in one TrenchBroom group (a group per tile in the editor). As
    func_walls (brush entities) the faces were no fewer, and hundreds of them made the dynamic lights' shadow caster
    search dearer."""

    TILE = 768

    def __init__(self, mw, name):
        mw.groups.append((name, []))
        self.mw, self.gid, self.cur, self.tiles = mw, len(mw.groups), [], {}

    def split(self):
        bs = [b for b in self.cur if b is not None]
        self.cur = []
        if not bs:
            return
        p0 = bs[0].faces[0][0]
        key = (int(p0[0] // self.TILE), int(p0[1] // self.TILE))
        if key not in self.tiles:
            self.tiles[key] = []
            self.mw.add({"classname": PIECE_CLASS, "_tb_group": str(self.gid), "_shadow": "1"}, self.tiles[key])
        self.tiles[key].extend(bs)

    def append(self, b):
        self.cur.append(b)


def split(out):
    if isinstance(out, Pieces):
        out.split()



RND = random.Random(1234)


def wood(key, axis, rnd=RND):
    """Wood with its grain along `axis`, at a random place on the texture (no two planks alike)."""
    return T(key, mode="grain", axis=axis, uoff=rnd.randrange(0, 64), voff=rnd.randrange(0, 64), scale=0.75,
             end=T("logend", scale=0.75))


def chamfer_box(x0, y0, z0, x1, y1, z1, c, tex):
    pts = []
    for x, sx in ((x0, 1), (x1, -1)):
        for y, sy in ((y0, 1), (y1, -1)):
            for z, sz in ((z0, 1), (z1, -1)):
                pts += [(x + sx * c, y, z), (x, y + sy * c, z), (x, y, z + sz * c)]
    return hull(pts, tex)


def post(out, x, y, z0, z1, r=4, square=True, tex=None):
    """An upright: a square timber (chamfered) or a round log."""
    if square:
        out.append(chamfer_box(x - r, y - r, z0, x + r, y + r, z1, min(1.0, r / 4), tex or wood("beam", (0, 0, 1))))
    else:
        out.append(cylinder((x, y, z0), (x, y, z1), r, 8, tex or wood("log", (0, 0, 1))))


def rail(out, p, q, w=4, h=3, key="beam"):
    out.append(beam(p, q, w, h, wood(key, sub(q, p))))


# Thin things nobody should bump into (ropes, torch brackets, lantern wires): no collision (func_detail_illusionary).
# Their clip hulls were the trouble: a thin slanted brush grown by the player's box can reach out as an invisible wall.
NONSOLID = []


def rope(out, p, q, sag=4, segs=2):
    """A rope from p to q, sagging in the middle (thin beams, not solid): two pieces (with more, the pieces' faces
    meet at a degree or two, slivers ericw-tools 2.0's qbsp loses faces at; and they are more brushes)."""
    out = NONSOLID
    pts = []
    for i in range(segs + 1):
        t = i / segs
        pts.append((lerp(p[0], q[0], t), lerp(p[1], q[1], t), lerp(p[2], q[2], t) - sag * 4 * t * (1 - t)))
    for i in range(segs):
        out.append(beam(pts[i], pts[i + 1], 1.5, 1.5, T("rope", mode="grain", axis=sub(pts[i + 1], pts[i]), scale=0.5)))


def railing(out, pts, top=36, mid=20, every=56, sq=2.5, rope_mid=False):
    """Posts along the polyline `pts` ((x, y, z): the walking height), a top rail and a middle rail (or a rope)."""
    for i in range(len(pts) - 1):
        a, b = pts[i], pts[i + 1]
        L = math.hypot(b[0] - a[0], b[1] - a[1])
        n = max(1, int(round(L / every)))
        for k in range(n + (1 if i == len(pts) - 2 else 0)):
            t = k / n
            x, y, z = lerp(a[0], b[0], t), lerp(a[1], b[1], t), lerp(a[2], b[2], t)
            post(out, x, y, z - 2, z + top + 2, sq)
        rail(out, (a[0], a[1], a[2] + top), (b[0], b[1], b[2] + top), 4, 3)
        if rope_mid:
            for k in range(n):
                t0, t1 = k / n, (k + 1) / n
                rope(out, (lerp(a[0], b[0], t0), lerp(a[1], b[1], t0), lerp(a[2], b[2], t0) + mid),
                     (lerp(a[0], b[0], t1), lerp(a[1], b[1], t1), lerp(a[2], b[2], t1) + mid), 3)
        elif mid:
            rail(out, (a[0], a[1], a[2] + mid), (b[0], b[1], b[2] + mid), 3, 3)


def deck(out, x0, y0, x1, y1, ztop, along, width=11, gap=1, thick=3, zfn=None, rnd=RND):
    """Planks covering x0..x1, y0..y1, their long axis `along` ('x' or 'y'), tops at ztop (or zfn(x, y))."""
    if along == "x":
        n = int((y1 - y0) // (width + gap))
        pitch = (y1 - y0) / n
        for k in range(n):
            ya, yb = y0 + k * pitch, y0 + k * pitch + pitch - gap
            z = zfn(0.5 * (x0 + x1), 0.5 * (ya + yb)) if zfn else ztop
            j0, j1 = rnd.uniform(-1.5, 1.5), rnd.uniform(-1.5, 1.5)
            sc = (yb - ya) / 15  # one of the texture's 16-texel boards on each plank
            tex = T("plank", mode="grain", axis=(1, 0, 0), scale=sc, uoff=yb / sc + 16 * rnd.randrange(4) + 0.5,
                    voff=rnd.randrange(64), end=T("logend", scale=0.75))
            out.append(box(x0 + j0, ya, z - thick, x1 + j1, yb, z, tex))
    else:
        n = int((x1 - x0) // (width + gap))
        pitch = (x1 - x0) / n
        for k in range(n):
            xa, xb = x0 + k * pitch, x0 + k * pitch + pitch - gap
            z = zfn(0.5 * (xa + xb), 0.5 * (y0 + y1)) if zfn else ztop
            j0, j1 = rnd.uniform(-1.5, 1.5), rnd.uniform(-1.5, 1.5)
            sc = (xb - xa) / 15
            tex = T("plank", mode="grain", axis=(0, 1, 0), scale=sc, uoff=-xa / sc + 16 * rnd.randrange(4) + 0.5,
                    voff=rnd.randrange(64), end=T("logend", scale=0.75))
            out.append(box(xa, y0 + j0, z - thick, xb, y1 + j1, z, tex))


def rock(out, cx, cy, cz, rx, ry, rz, seed, tex=None, flat=0.35):
    """A boulder: the hull of points on a squashed ellipsoid, its base sunk in the ground."""
    rnd = random.Random(seed)
    pts = []
    yaw = rnd.uniform(0, 2 * math.pi)
    c, s = math.cos(yaw), math.sin(yaw)
    for i in range(16):
        u = rnd.uniform(-1, 1)
        a = rnd.uniform(0, 2 * math.pi)
        rr = math.sqrt(1 - u * u)
        k = rnd.uniform(0.82, 1.0)
        px, py, pz = rr * math.cos(a) * rx * k, rr * math.sin(a) * ry * k, u * rz * k
        bottom = pz < -rz * flat
        if bottom:
            pz = -rz * flat - rnd.uniform(0, rz * 0.4)
        x, y, z = cx + px * c - py * s, cy + px * s + py * c, cz + pz
        if bottom and mapgeom.SURFACES:
            # the flattened underside well under the ground (its faces nearly level with the ground just under or
            # over it would be slivers; ericw-tools 2.0's qbsp loses faces at those)
            g = mapgeom.SURFACES[0][0](x, y)
            if g is not None:
                z = min(z, g - 4)
        pts.append((x, y, z))
    b = hull(pts, tex or T("cliff", mode="face", uoff=rnd.randrange(256), voff=rnd.randrange(256)))
    if b:
        out.append(b)
    split(out)


def staircase(out, st, rails=True):
    """A wooden flight: treads on two stringers, handrails on posts. The treads' tops step 8 down from st's top edge
    (z zt) to the ground (zb) at its bottom edge."""
    n, bot, _ = stair_info(st)
    dx, dy, run, w = st["dx"], st["dy"], st["run"], st["w"]
    px, py = -dy, dx  # across the flight
    cx, cy = st["cx"], st["cy"]

    def at(dist, across, z):
        return (cx + dx * dist + px * across, cy + dy * dist + py * across, z)

    axis = (px, py, 0)
    for i in range(1, n):
        z = st["zt"] - 8 * i
        a, b = at((i - 1) * run - 1, -w / 2 + 6, z - 3), at(i * run + 1, w / 2 - 6, z)
        out.append(box(a[0], a[1], a[2], b[0], b[1], b[2], wood("plank", axis)))
        # the riser board under the tread's back edge
        a, b = at((i - 1) * run - 1, -w / 2 + 8, z - 9), at((i - 1) * run + 1, w / 2 - 8, z - 3)
        out.append(box(a[0], a[1], a[2], b[0], b[1], b[2], wood("board", axis)))
    L = (n - 1) * run
    for side in (-1, 1):
        across = side * (w / 2 - 3)
        top, low = at(-4, across, st["zt"] - 4), at(L + 6, across, st["zb"] - 6)
        out.append(beam(add(top, (0, 0, -4)), add(low, (0, 0, -4)), 6, 14, wood("beam", sub(low, top))))
        if rails:
            pts = [at(-8, side * (w / 2 + 2), st["zt"]), at(L, side * (w / 2 + 2), st["zb"] + 8)]
            railing(out, pts, top=36, mid=0, every=48, sq=2.5)
            # newel posts at both ends
            for p_ in pts:
                post(out, p_[0], p_[1], p_[2] - 10, p_[2] + 44, 4)


# ---------------------------------------------------------------------------------------------------------------------
# The places
def build_pier(mw):
    out = part(mw, "pier")
    p = PIER
    x, y0, y1, z, w = p["x"], p["y0"], p["y1"], p["z"], p["w"]
    deck(out, x - w / 2, y1, x + w / 2, y0, z, "x")
    for sx in (-36, 36):
        out.append(box(x + sx - 4, y1 - 4, z - 11, x + sx + 4, y0, z - 3, wood("beam", (0, 1, 0))))
    for yy in range(int(y1) + 8, int(y0) - 20, 96):
        out.append(box(x - 60, yy - 4, z - 19, x + 60, yy + 4, z - 11, wood("beam", (1, 0, 0))))
        for sx in (-52, 52):
            top = z + 32 if yy == int(y1) + 8 else z + 20 if yy == int(y1) + 104 else z - 11
            out.append(cylinder((x + sx, yy, -340), (x + sx, yy, top), 7, 8, wood("log", (0, 0, 1))))
    # ropes from the end's bollards to the short ones behind them
    rope(out, (x - 52, y1 + 8, z + 26), (x - 52, y1 + 104, z + 15), 3)
    rope(out, (x + 52, y1 + 8, z + 26), (x + 52, y1 + 104, z + 15), 3)


def build_arrival(mw):
    out = part(mw, "arrival")
    ax, ay = ARRIVAL
    g = 14
    # the welcome board: two posts and a board facing the pier (south)
    bx, by = WELCOME
    for sx in (-70, 70):
        post(out, bx + sx, by, g - 10, g + 118, 5, square=False)
    out.append(box(bx - 76, by - 3, g + 54, bx + 76, by + 3, g + 112, wood("board", (1, 0, 0))))
    out.append(box(bx - 80, by - 5, g + 112, bx + 80, by + 5, g + 118, wood("beam", (1, 0, 0))))
    # the quick-start stand: a panel on two posts, facing south
    qx, qy = QUICK
    out.append(box(qx - 40, qy - 4, g + 6, qx + 40, qy + 4, g + 70, wood("board", (1, 0, 0))))
    for sx in (-44, 44):
        post(out, qx + sx, qy, g - 10, g + 76, 4)
    # a campfire's ring of stones
    fx, fy = CAMPFIRE
    for i in range(9):
        a = 2 * math.pi * i / 9
        rock(out, fx + 26 * math.cos(a), fy + 26 * math.sin(a), g + 2, 9, 7, 6, 900 + i)
    for a in (0.3, 1.9):
        out.append(cylinder((fx - 18 * math.cos(a), fy - 18 * math.sin(a), g + 2),
                            (fx + 18 * math.cos(a), fy + 18 * math.sin(a), g + 6), 3, 6,
                            wood("log", (math.cos(a), math.sin(a), 0))))
    # logs to sit on
    for (lx, ly, a) in ((fx - 70, fy + 30, 1.2), (fx + 10, fy - 70, 0.1)):
        d = (36 * math.cos(a), 36 * math.sin(a))
        out.append(cylinder((lx - d[0], ly - d[1], g + 8), (lx + d[0], ly + d[1], g + 8), 8, 8,
                            wood("log", (d[0], d[1], 0))))


WELCOME = (-1350, -690)                  # the welcome board (faces south, left of the way up)
QUICK = (-1040, -730)                    # the quick-start stand (tutorial, calibration)
CAMPFIRE = (-1400, -830)


def build_stairs(mw):
    staircase(part(mw, "staircase up the bank"), STAIR1)
    out = part(mw, "staircase landing")
    s = STAIR1
    deck(out, s["cx"] - s["w"] / 2, s["cy"], s["cx"] + s["w"] / 2, s["cy"] + 40, s["zt"], "x")
    for sx in (-s["w"] / 2 + 4, s["w"] / 2 - 4):
        out.append(box(s["cx"] + sx - 4, s["cy"], s["zt"] - 14, s["cx"] + sx + 4, s["cy"] + 40, s["zt"] - 3,
                       wood("beam", (0, 1, 0))))
    staircase(part(mw, "staircase down to the range"), STAIR2)


LECTERNS_X = (-165, -55, 55, 165)       # from the gate's centre
LECTERN_Y = -60                          # their south faces (the buttons on them)


def build_terrace(mw):
    out = part(mw, "campaign terrace")
    t = TERRACE
    cx, cy, r, z = t["x"], t["y"], t["r"], t["z"]
    pts = []
    for (x, y) in ngon(cx, cy, r, 24, math.pi / 24):
        pts += [(x, y, z), (x, y, z - 14)]
    out.append(hull(pts, T("flag", scale=0.5), lambda n, c: None if n[2] > 0.5 else T("block", mode="face", scale=0.5)))
    # the curb round the rim, open to the south (the stairs), east (the bridge) and north (the gate)
    gaps = [(270, 22), (0, 18), (90, 30)]
    for i in range(24):
        a0, a1 = 360 * i / 24, 360 * (i + 1) / 24
        mid = (a0 + a1) / 2
        if any(abs((mid - g + 180) % 360 - 180) < w for g, w in gaps):
            continue
        q = []
        for a in (a0 + 0.6, a1 - 0.6):
            ar = math.radians(a)
            for rr in (r - 14, r + 1):
                q += [(cx + rr * math.cos(ar), cy + rr * math.sin(ar), z),
                      (cx + rr * math.cos(ar), cy + rr * math.sin(ar), z + 10)]
        out.append(hull(q, T("block", mode="face", scale=0.5)))
    # the slipgate: a dais, two pillars of stacked blocks, an arch of voussoirs
    gx, gy = GATE["x"], GATE["y"]
    out.append(chamfer_box(gx - 104, gy - 64, z - 8, gx + 104, gy + 44, z + 8, 2, T("block", scale=0.5)))
    out.append(chamfer_box(gx - 80, gy - 40, z + 8, gx + 80, gy + 32, z + 16, 2, T("block", scale=0.5)))
    zb = z + 16
    spring = zb + 104
    for sx in (-1, 1):
        x0, x1 = gx + sx * 44, gx + sx * 72
        for k in range(4):
            out.append(chamfer_box(min(x0, x1), gy - 14, zb + 26 * k, max(x0, x1), gy + 14, zb + 26 * (k + 1), 2,
                                   T("block", scale=0.5)))
    segs = 7
    for k in range(segs):
        a0, a1 = math.pi * k / segs, math.pi * (k + 1) / segs
        q = []
        for a in (a0, a1):
            for rr in (44, 74 if k == segs // 2 else 72):
                for yy in (gy - 15, gy + 15):
                    q.append((gx + rr * math.cos(a), yy, spring + rr * math.sin(a)))
        out.append(hull(q, T("block", mode="face", scale=0.5)))
    # the portal's surface: the opening (a rectangle with a half circle on top), not solid
    q = []
    for yy in (gy - 1, gy + 1):
        q += [(gx - 44, yy, zb), (gx + 44, yy, zb)]
        for k in range(13):
            a = math.pi * k / 12
            q.append((gx + 44 * math.cos(a), yy, spring + 44 * math.sin(a)))
    mw.add({"classname": "func_illusionary"}, [hull(q, T("portal", scale=0.5))])
    mw.add({"classname": "trigger_changelevel", "map": "start", "spawnflags": "1"},  # (no intermission)
           [box(gx - 40, gy - 10, zb, gx + 40, gy + 10, spring + 30, "trigger")])
    # the campaign lecterns (their buttons are entities: build_entities)
    for lx in LECTERNS_X:
        out.append(chamfer_box(gx + lx - 26, LECTERN_Y, z, gx + lx + 26, LECTERN_Y + 22, z + 62, 2, T("block", scale=0.5)))
        out.append(chamfer_box(gx + lx - 30, LECTERN_Y - 3, z + 62, gx + lx + 30, LECTERN_Y + 25, z + 68, 2,
                               T("trim", scale=0.5)))


def build_bridge(mw):
    out = part(mw, "bridge")
    b = BRIDGE
    x0, x1, y, z, arch, w = b["x0"], b["x1"], b["y"], b["z"], b["arch"], b["w"]

    def zf(x, _y=0):
        return z + arch * math.sin(math.pi * max(0.0, min(1.0, (x - x0) / (x1 - x0))))

    deck(out, x0, y - w / 2, x1, y + w / 2, z, "y", width=10, gap=1.5, zfn=zf)
    xs = [lerp(x0 - 20, x1 + 20, i / 10) for i in range(11)]
    for sy in (-38, 38):
        for i in range(10):
            pa = (xs[i], y + sy, zf(xs[i]) - 9)
            pb = (xs[i + 1], y + sy, zf(xs[i + 1]) - 9)
            out.append(beam(pa, pb, 10, 12, wood("log", sub(pb, pa))))
    # the abutments
    for xe in (x0, x1):
        out.append(chamfer_box(xe - 44, y - 64, z - 60, xe + 44, y + 64, z - 15, 3, T("block", scale=0.5)))
    # the trestle in the middle
    xm = 0.5 * (x0 + x1)
    zt = zf(xm) - 15
    for sy in (-46, 46):
        out.append(cylinder((xm, y + sy, RAVINE_BED - 40), (xm, y + sy, zt), 7, 8, wood("log", (0, 0, 1))))
    out.append(beam((xm, y - 56, zt - 4), (xm, y + 56, zt - 4), 10, 8, wood("beam", (0, 1, 0))))
    for zz in (-20, 40):
        rail(out, (xm, y - 46, zz), (xm, y + 46, zz + 50), 5, 5)
        rail(out, (xm, y + 46, zz), (xm, y - 46, zz + 50), 5, 5)
    # railings: posts, a top rail along the arch, a rope between them
    for sy in (-w / 2 - 2, w / 2 + 2):
        pts = [(xx, y + sy, zf(xx)) for xx in [lerp(x0, x1, i / 8) for i in range(9)]]
        railing(out, pts, top=38, mid=20, every=54, sq=2.5, rope_mid=True)
        for xe in (x0, x1):
            post(out, xe, y + sy, zf(xe) - 30, z + 70, 5, square=False)


def build_pavilion(mw):
    out = part(mw, "settings pavilion")
    pv = PAVILION
    x0, x1, y0, y1, z = pv["x0"], pv["x1"], pv["y0"], pv["y1"], pv["z"]
    g = z - 16
    ym = 0.5 * (y0 + y1)
    deck(out, x0, y0, x1, ym - 1, z, "y", width=11)
    deck(out, x0, ym + 1, x1, y1, z, "y", width=11)
    for yy in (y0 + 6, ym, y1 - 6):
        out.append(box(x0, yy - 4, g, x1, yy + 4, z - 3, wood("beam", (1, 0, 0))))
    # the rim boards round the deck's edge
    for (a, b_) in (((x0 - 4, y0 - 4), (x1 + 4, y0)), ((x0 - 4, y1), (x1 + 4, y1 + 4)), ((x0 - 4, y0), (x0, y1)),
                    ((x1, y0), (x1 + 4, y1))):
        ax_ = (1, 0, 0) if b_[0] - a[0] > b_[1] - a[1] else (0, 1, 0)
        out.append(box(a[0], a[1], g - 4, b_[0], b_[1], z - 1, wood("board", ax_)))
    # the step up on the west side
    out.append(box(x0 - 20, ym - 64, g - 4, x0 - 4, ym + 64, g + 8, wood("plank", (0, 1, 0))))
    # the posts, the plates on them, the roof
    eave = z + 128
    cols = (x0 + 12, 0.5 * (x0 + x1), x1 - 12)
    for xx in cols:
        for yy in (y0 + 12, y1 - 12):
            post(out, xx, yy, z - 3, eave, 6)
            for sx in (-1, 1):  # knee braces
                if (xx == cols[0] and sx < 0) or (xx == cols[-1] and sx > 0):
                    continue
                rail(out, (xx, yy, eave - 30), (xx + sx * 28, yy, eave - 2), 4, 4)
    for yy in (y0 + 12, y1 - 12):
        out.append(box(x0 - 8, yy - 7, eave, x1 + 8, yy + 7, eave + 12, wood("beam", (1, 0, 0))))
    ridge = eave + 76
    for xx in cols:
        out.append(box(xx - 5, y0, eave + 12, xx + 5, y1, eave + 20, wood("beam", (0, 1, 0))))
        out.append(box(xx - 4, ym - 4, eave + 20, xx + 4, ym + 4, ridge - 6, wood("beam", (0, 0, 1))))
    out.append(box(x0 - 40, ym - 6, ridge - 10, x1 + 40, ym + 6, ridge + 2, wood("beam", (1, 0, 0))))

    def roof_tex(n, c):
        return T("roof", mode="grain", axis=(1, 0, 0), scale=0.5) if n[2] > 0.3 else wood("plank", (1, 0, 0))

    slope = 76 / (ym - y0 - 12)
    for (ya, yb) in ((y0 - 40, ym), (y1 + 40, ym)):
        za = eave + 6 - 52 * slope
        zb = ridge + 2
        q = []
        for xx in (x0 - 40, x1 + 40):
            q += [(xx, ya, za), (xx, yb, zb), (xx, ya, za + 7), (xx, yb, zb + 7)]
        out.append(hull(q, T("plank"), roof_tex))
    # the setting boards: north and south walls, their faces inside
    for (ya, yb) in ((y1 - 26, y1 - 18), (y0 + 18, y0 + 26)):
        out.append(box(x0 + 30, ya, z, x1 - 30, yb, z + 120, wood("board", (1, 0, 0))))
        out.append(box(x0 + 26, ya - 1, z + 120, x1 - 26, yb + 1, z + 126, wood("beam", (1, 0, 0))))
    # railings along the open sides (west and east: the ways in and out stay open)
    for xx in (x0 + 2, x1 - 2):
        for (ya, yb) in ((y0 + 12, ym - 52), (ym + 52, y1 - 12)):
            railing(out, [(xx, ya, z), (xx, yb, z)], top=36, mid=18, every=48)


TARGET_BOARDS = [(800, -160), (1150, 80), (1380, -160), (1020, -40)]


def build_range(mw):
    out = part(mw, "firing range")
    rg = RANGE
    z, line = rg["z"], rg["line"]
    # the benches at the firing line (the guns and the ammunition on them)
    for (ya, yb) in ((-200, -60), (-20, 120)):
        out.append(box(line - 34, ya, z + 31, line, yb, z + 35, wood("plank", (0, 1, 0))))
        for xx in (line - 30, line - 4):
            for yy in (ya + 4, yb - 4):
                post(out, xx, yy, z - 2, z + 31, 2.5)
        out.append(box(line - 30, ya + 4, z + 10, line - 4, yb - 4, z + 13, wood("plank", (0, 1, 0))))
    # the side fences and the lane dividers
    y0, y1 = rg["y0"] + 20, rg["y1"] - 20
    for yy in (y0, y1):
        railing(out, [(line + 30, yy, z), (rg["x1"] - 40, yy, z)], top=34, mid=16, every=96, sq=3)
    for yy in (-100, 20):
        railing(out, [(line + 40, yy, z), (rg["x1"] - 80, yy, z)], top=22, mid=0, every=120, sq=2.5)
    # the backstop: a wall of logs before the bank
    xb = rg["x1"] - 40
    for k in range(6):
        zz = z + 10 + 20 * k
        out.append(cylinder((xb, y0 - 20, zz), (xb, y1 + 20, zz), 10, 8, wood("log", (0, 1, 0))))
    for yy in (y0 - 10, -40, y1 + 10):
        post(out, xb + 14, yy, z - 20, z + 130, 6, square=False)
    # the target boards on their stands
    for (tx, ty) in TARGET_BOARDS:
        for sy in (-20, 20):
            post(out, tx + 4, ty + sy, z - 4, z + 64, 2.5)
        sc = 52 / 64  # one copy of the 64-texel bullseye on the board's 52-unit face (u along +y, v down)
        face = T("target", scale=sc, uoff=(-(ty - 26) / sc) % 64, voff=((z + 82) / sc) % 64)
        out.append(box(tx, ty - 26, z + 30, tx + 3, ty + 26, z + 82, T("board"),
                       lambda n, c, face=face: face if n[0] < -0.9 else None))
    # a shelf for things to knock off (rocks and bricks)
    sx, sy = SHELF
    out.append(box(sx - 8, sy - 40, z + 32, sx + 8, sy + 40, z + 35, wood("plank", (0, 1, 0))))
    for yy in (sy - 34, sy + 34):
        post(out, sx, yy, z - 2, z + 32, 2.5)


SHELF = (700, 80)


def build_tower(mw):
    out = part(mw, "lookout tower")
    tw = TOWER
    cx, cy, z, h = tw["x"], tw["y"], tw["z"], tw["half"]
    top = z + tw["deck"]
    pr = h - 8
    corners = [(cx + sx * pr, cy + sy * pr) for sx in (-1, 1) for sy in (-1, 1)]
    for (x, y) in corners:
        out.append(cylinder((x, y, z - 24), (x, y, top + 44), 9, 8, wood("log", (0, 0, 1))))
    # the deck: joists and planks
    for xx in (cx - pr, cx, cx + pr):
        out.append(box(xx - 5, cy - h, top - 13, xx + 5, cy + h, top - 3, wood("beam", (0, 1, 0))))
    deck(out, cx - h, cy - h, cx + h, cy + h, top, "x")
    # ring beams and cross braces on each side, at two heights
    mid = z + 0.5 * tw["deck"] - 10
    for (a, b_) in ((corners[0], corners[1]), (corners[2], corners[3]), (corners[0], corners[2]), (corners[1], corners[3])):
        for zz in (mid, top - 14):
            rail(out, (a[0], a[1], zz), (b_[0], b_[1], zz), 7, 7)
        for (za, zb_) in ((z + 14, mid), (mid, top - 14)):
            rail(out, (a[0], a[1], za), (b_[0], b_[1], zb_), 5, 5)
            rail(out, (b_[0], b_[1], za), (a[0], a[1], zb_), 5, 5)
    # railings: gaps for the ladder (north) and the diving board (south)
    gx = 28
    for y in (cy + pr, cy - pr):
        railing(out, [(cx - pr, y, top), (cx - gx, y, top)], top=40, mid=20, every=60, sq=2.5)
        railing(out, [(cx + gx, y, top), (cx + pr, y, top)], top=40, mid=20, every=60, sq=2.5)
    for x in (cx - pr, cx + pr):
        railing(out, [(x, cy - pr, top), (x, cy + pr, top)], top=40, mid=20, every=60, sq=2.5)
    # the ladder (north face): two rails, rungs every 20 units (climbing: each rung a ledge)
    ly = cy + h
    for sx in (-25, 21):
        out.append(box(cx + sx, ly, z - 8, cx + sx + 4, ly + 10, top + 40, wood("beam", (0, 0, 1))))
    for k in range(tw["deck"] // 20 + 1):
        rz = z + 16 + 20 * k  # (16, 36, 56... over the ground: vrclimb's rung wall's heights, where the climbing plays work)
        if rz > top - 16:
            break
        out.append(box(cx - 21, ly + 2, rz - 4, cx + 21, ly + 7, rz, wood("log", (1, 0, 0))))
    # the diving board (south), on two braces
    out.append(box(cx - 16, cy - h - 170, top - 4, cx + 16, cy - h, top, wood("plank", (0, 1, 0))))
    out.append(box(cx - 14, cy - h - 170, top - 10, cx + 14, cy - h, top - 4, wood("beam", (0, 1, 0))))
    for sx in (-10, 10):
        rail(out, (cx + sx, cy - pr, top - 90), (cx + sx, cy - h - 80, top - 10), 5, 6)


def build_world(mw):
    """The sealing box (sky round and above, rock under) and the lake's water."""
    w = mw.world
    S = BOX + 32

    def outside(n, c):
        # the floor's faces towards the void: skip (never drawn; nothing for qbsp or light to keep)
        return T("skip") if dot(n, (-c[0], -c[1], -c[2])) <= 0 else None

    w.append(box(-S, -S, FLOOR_Z - 32, S, S, FLOOR_Z, T("cliff2"), outside))
    w.append(box(-S, -S, SKY_TOP, S, S, SKY_TOP + 32, T("sky")))  # (sky: unlit; skip on it would make it solid)
    w.append(box(-S, -S, FLOOR_Z, -BOX, S, SKY_TOP, T("sky")))
    w.append(box(BOX, -S, FLOOR_Z, S, S, SKY_TOP, T("sky")))
    w.append(box(-BOX, -S, FLOOR_Z, BOX, -BOX, SKY_TOP, T("sky")))
    w.append(box(-BOX, BOX, FLOOR_Z, BOX, S, SKY_TOP, T("sky")))
    # The lake's water: one brush (ericw-tools 2.0's qbsp cuts its surface into faces small enough for a lightmap: the
    # water lit, the torches' light on it)
    w.append(box(-BOX, -BOX, FLOOR_Z, BOX, BOX, WATER_Z, T("water", scale=1.0)))


# ---------------------------------------------------------------------------------------------------------------------
# Lights: torches along the path (Quake VR's wall torches: the player can take one off its post), braziers, lanterns,
# the campfire, the slipgate's glow, and crystals glowing on the lake's floor.
TORCH = {"light": "230", "_color": "1 0.6 0.28", "wait": "1.1"}


def ent(mw, cls, x, y, z, **keys):
    k = {"classname": cls, "origin": "%d %d %d" % (round(x), round(y), round(z))}
    if cls.startswith("light"):
        keys.setdefault("_dirt", -1)  # no ambient occlusion on the fires and lamps (it ate the pavilion's light)
    k.update({a: str(b) for a, b in keys.items()})
    mw.add(k)


def torch_post(mw, out, x, y, g, yaw, h=72):
    """A log post with an iron bracket and a wall torch on it, facing `yaw` (degrees)."""
    post(out, x, y, g - 12, g + h, 4.5, square=False)
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    NONSOLID.append(beam((x + c * 3, y + s * 3, g + h - 22), (x + c * 9, y + s * 9, g + h - 22), 3, 3, T("iron", scale=0.5)))
    ent(mw, "light_torch_small_walltorch", x + c * 10, y + s * 10, g + h - 18, angle=yaw, **TORCH)
    split(out)


def wall_torch(mw, out, x, y, z, yaw):
    """A torch on a structure's upright (a bracket from its face)."""
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    NONSOLID.append(beam((x - c * 2, y - s * 2, z - 4), (x + c * 5, y + s * 5, z - 4), 3, 3, T("iron", scale=0.5)))
    ent(mw, "light_torch_small_walltorch", x + c * 6, y + s * 6, z, angle=yaw, **TORCH)
    split(out)


def brazier(mw, out, x, y, g, h=40, base="block", sink=8):
    """A stone pillar with an iron bowl and a large flame; its foot `sink` under the ground's height g."""
    out.append(chamfer_box(x - 10, y - 10, g - sink, x + 10, y + 10, g + h, 2, T(base, scale=0.5)))
    q = []
    for (px, py) in ngon(x, y, 15, 8, math.pi / 8):
        q.append((px, py, g + h + 10))
    for (px, py) in ngon(x, y, 9, 8, math.pi / 8):
        q.append((px, py, g + h))
    out.append(hull(q, T("iron", scale=0.5)))
    ent(mw, "light_flame_large_yellow", x, y, g + h + 18, light=300, _color="1 0.55 0.25", wait=0.9)
    split(out)


def lantern(mw, out, x, y, z, light=200, hang=0, side=None):
    """A hanging lantern: an iron frame round a glowing glass (qvr_lantern: fullbright, it shines at night), and its
    light under it, or beside it towards `side` ((dx, dy): one standing on a post; over it, the lantern's own shadow hid
    the floor round the post). (Not in the glass: light counts every face as a shadow caster, func_detail_illusionary's
    too, and the glass kept it in.)"""
    out.append(box(x - 5, y - 5, z - 7, x + 5, y + 5, z + 7, T("lamp", scale=0.5)))
    out.append(box(x - 6, y - 6, z + 7, x + 6, y + 6, z + 10, T("iron", scale=0.5)))
    out.append(box(x - 6, y - 6, z - 10, x + 6, y + 6, z - 7, T("iron", scale=0.5)))
    if hang:
        NONSOLID.append(box(x - 0.75, y - 0.75, z + 10, x + 0.75, y + 0.75, z + 10 + hang, T("iron", scale=0.5)))
    lx, ly, lz = (x + side[0] * 10, y + side[1] * 10, z) if side else (x, y, z - 16)
    ent(mw, "light", lx, ly, lz, light=light, _color="1 0.75 0.45", wait=0.8)
    split(out)


def crystal(out, x, y, g, rnd):
    """A cluster of glowing crystals on the lake's floor."""
    n = rnd.randint(4, 7)
    for i in range(n):
        a = rnd.uniform(0, 2 * math.pi)
        tilt = rnd.uniform(0.0, 0.5)
        L = rnd.uniform(24, 64)
        r = rnd.uniform(4, 8)
        bx, by = x + math.cos(a) * rnd.uniform(0, 9), y + math.sin(a) * rnd.uniform(0, 9)
        tip = (bx + math.cos(a) * L * tilt, by + math.sin(a) * L * tilt, g + L)
        pts = []
        for (px, py) in ngon(bx, by, r, 5, rnd.uniform(0, 1)):
            pts += [(px, py, g - 6), (px + (tip[0] - bx) * 0.8, py + (tip[1] - by) * 0.8, g + L * 0.8)]
        pts.append(tip)
        b = hull(pts, T("crystal", scale=0.5))
        if b:
            out.append(b)
    split(out)


def build_lights(mw):
    out = Pieces(mw, "lights: posts, braziers, lanterns, crystals")
    p, s1, s2, t, b, pv, rg, tw = PIER, STAIR1, STAIR2, TERRACE, BRIDGE, PAVILION, RANGE, TOWER
    # the pier's end: torches on the two tall bollards, facing the deck
    for sx in (-1, 1):
        wall_torch(mw, out, p["x"] + sx * 52 - sx * 7, p["y1"] + 8, p["z"] + 26, 0 if sx < 0 else 180)
    # half way along the pier: a lantern on a post
    post(out, p["x"] + 56, -1120, -10, p["z"] + 64, 4, square=False)
    out.append(box(p["x"] + 40, -1123, p["z"] + 58, p["x"] + 58, -1117, p["z"] + 62, wood("beam", (1, 0, 0))))
    lantern(mw, out, p["x"] + 42, -1120, p["z"] + 44, 240, hang=4)
    # the arrival: the campfire, torches by the boards
    fx, fy = CAMPFIRE
    ent(mw, "light_flame_large_yellow", fx, fy, 14 + 14, light=320, _color="1 0.5 0.2", wait=0.8)
    wx, wy = WELCOME
    torch_post(mw, out, wx - 96, wy - 10, height(wx - 96, wy - 10), 300)
    torch_post(mw, out, -1270, -880, height(-1270, -880), 30)
    torch_post(mw, out, -1130, -880, height(-1130, -880), 150)
    qx, qy = QUICK
    torch_post(mw, out, qx + 64, qy - 8, height(qx + 64, qy - 8), 225)
    # the staircase: torches on the newel posts at its foot and at its top
    n1, bot1, _ = stair_info(s1)
    for sx in (-1, 1):
        wall_torch(mw, out, s1["cx"] + sx * (s1["w"] / 2 + 2), bot1[1] - 4, s1["zb"] + 46, 270)
        torch_post(mw, out, s1["cx"] + sx * 64, s1["cy"] + 60, s1["zt"] - 6, 90 if sx < 0 else 90)
    # the terrace: braziers round its rim, torches on the gate's pillars, the portal's glow
    for a in (225, 315, 160, 20):
        ar = math.radians(a)
        brazier(mw, out, t["x"] + (t["r"] - 30) * math.cos(ar), t["y"] + (t["r"] - 30) * math.sin(ar), t["z"])
    gx, gy = GATE["x"], GATE["y"]
    for sx in (-1, 1):
        wall_torch(mw, out, gx + sx * 58, gy - 15, t["z"] + 16 + 70, 270)
    ent(mw, "light", gx, gy - 40, t["z"] + 70, light=260, _color="0.55 0.45 1", wait=1.4)
    ent(mw, "light", gx, gy + 30, t["z"] + 70, light=200, _color="0.55 0.45 1", wait=1.4)
    # the bridge's ends: torches on the tall posts
    for xe, yaw in ((b["x0"], 0), (b["x1"], 180)):
        for sy in (-1, 1):
            wall_torch(mw, out, xe, b["y"] + sy * (b["w"] / 2 + 2), b["z"] + 56, yaw)
    # the pavilion: lanterns under the roof, torches at the ways in and out
    ym = 0.5 * (pv["y0"] + pv["y1"])
    for xx in (pv["x0"] + 12, 0.5 * (pv["x0"] + pv["x1"]), pv["x1"] - 12):
        lantern(mw, out, xx, ym, pv["z"] + 104, 220, hang=40)
    for xx, yaw in ((pv["x0"] - 26, 180), (pv["x1"] + 26, 0)):
        for sy in (-1, 1):
            torch_post(mw, out, xx, ym + sy * 70, pv["z"] - 16, yaw)
    # the range: torches along its fences, braziers by the targets' bank, a lantern over the benches
    for xx in range(rg["line"] + 160, rg["x1"] - 60, 260):
        for yy, yaw in ((rg["y0"] + 8, 90), (rg["y1"] - 8, 270)):
            torch_post(mw, out, xx, yy, rg["z"], yaw, h=64)
    for yy in (rg["y0"] + 40, rg["y1"] - 40):
        brazier(mw, out, rg["x1"] - 90, yy, rg["z"])
    for (ya, yb) in ((-200, -60), (-20, 120)):  # lantern posts at the benches' outer ends, arms over them
        yy, sy = (ya - 6, 1) if ya < -100 else (yb + 6, -1)
        post(out, rg["line"] - 17, yy, rg["z"] - 10, rg["z"] + 96, 3.5)
        out.append(box(rg["line"] - 19, min(yy, yy + sy * 40), rg["z"] + 90, rg["line"] - 15, max(yy, yy + sy * 40),
                       rg["z"] + 94, wood("beam", (0, 1, 0))))
        lantern(mw, out, rg["line"] - 17, yy + sy * 36, rg["z"] + 76, 170, hang=4)
    # the path along the shore: torches on posts
    for (x, y, yaw) in ((600, -400, 225), (900, -590, 250)):  # (beside the path, inland)
        torch_post(mw, out, x, y, height(x, y), yaw)
    # the tower: torches at the ladder's foot, a lantern on the deck
    for sx in (-1, 1):
        torch_post(mw, out, tw["x"] + sx * 56, tw["y"] + tw["half"] + 40, tw["z"], 90)
    lantern(mw, out, tw["x"] - tw["half"] + 20, tw["y"] - tw["half"] + 20, tw["z"] + tw["deck"] + 40, 220, side=(0.7, 0.7))
    post(out, tw["x"] - tw["half"] + 20, tw["y"] - tw["half"] + 20, tw["z"] + tw["deck"] - 2, tw["z"] + tw["deck"] + 30, 2)
    # crystals glowing on the lake's floor (a ring round the island, a few by the cliffs and the islets)
    rnd = random.Random(31)
    spots = []
    tries = 0
    while len(spots) < 34 and tries < 5000:
        tries += 1
        x, y = rnd.uniform(-BOX + 300, BOX - 300), rnd.uniform(-BOX + 300, BOX - 300)
        g = height(x, y)
        d = coast_distance(x, y)
        if not (-300 < g < -40) or d > -60:
            continue
        if any(math.hypot(x - a, y - b_) < 420 for a, b_ in spots):
            continue
        spots.append((x, y))
        crystal(out, x, y, g, rnd)
        ent(mw, "light", x, y, g + 72, light=380, _color="0.3 0.85 1", wait=0.8)
    out.split()
    print("lights: %d crystals" % len(spots))


# ---------------------------------------------------------------------------------------------------------------------
# Decoration: pines, boulders along the beach, barrels, a rowboat by the pier, the islets' braziers
def keep_clear(x, y, margin=0):
    """False where things must not stand: the paths, the places (zones), the structures, the start."""
    if on_path(x, y, -60 - margin):
        return False
    for kind, shape, target, blend in ZONES:
        if zone_weight(shape, x, y, 30 + margin) > 0:
            return False
    p = PIER
    if abs(x - p["x"]) < 120 + margin and p["y1"] - 60 < y < p["y0"] + 140:
        return False
    if math.hypot(x - CAMPFIRE[0], y - CAMPFIRE[1]) < 120 + margin:
        return False
    rd, _, _ = polyline_dist(x, y, RAVINE)
    if rd < RAVINE_OUT + 20:
        return False
    return True


def pine(out, x, y, g, h, rnd):
    """A pine: a bark trunk and three or four cones of needles. Each cone's tip is well inside the cone above it (were
    the tips at one point, the cones' faces would meet there nearly coplanar: slivers ericw-tools 2.0's qbsp loses
    faces at)."""
    trunk = rnd.uniform(5, 8)
    out.append(cylinder((x, y, g - 16), (x, y, g + h * 0.8), trunk, 7, T("bark", mode="face", scale=0.5)))
    layers = rnd.randint(3, 4)
    base = g + max(64, h * rnd.uniform(0.26, 0.34))
    tiers = []
    for i in range(layers):
        f = i / layers
        r = h * (0.36 - 0.22 * f) * rnd.uniform(0.9, 1.1)
        z0 = base + (h - (base - g)) * f * 0.9
        ring = ngon(x + rnd.uniform(-3, 3), y + rnd.uniform(-3, 3), r, 8, rnd.uniform(0, 1))
        under = ngon(x, y, r * 0.55, 8, rnd.uniform(0, 1))
        tip = (x + rnd.uniform(-2, 2), y + rnd.uniform(-2, 2), z0 + r * 1.5)
        tex = T("needles", mode="face", scale=0.5, uoff=rnd.randrange(64), voff=rnd.randrange(64))
        tiers.append([r, z0, ring, under, tip, tex])
    top = g + h
    for t in reversed(tiers):
        r, z0, ring, under, tip, tex = t
        t[4] = (tip[0], tip[1], min(tip[2], top))
        top = z0 + (t[4][2] - z0) * 0.75  # the next cone down ends three quarters of the way up this one
    for r, z0, ring, under, tip, tex in tiers:
        pts = [(px, py, z0) for (px, py) in ring] + [(px, py, z0 - r * 0.18) for (px, py) in under] + [tip]
        b = hull(pts, tex)
        if b:
            out.append(b)
    split(out)


BARRELS = [(-1270, -900, 0), (-1255, -925, 0), (-1290, -915, 1), (-1120, -905, 0), (130, 60, 0), (150, 40, 0),
           (360, 150, 0), (380, 165, 1), (-280, -270, 0)]  # (x, y, lying)


def rowboat(out, x, y, z):
    """A rowboat tied by the pier: its hull of planks (thin slanted brushes), a keel, two seats, oars."""
    L, W = 76, 22          # half-length, half-width at the middle
    def side(s):
        q = []
        for (px, k) in ((-L, 0.05), (-L * 0.6, 0.75), (0, 1.0), (L * 0.6, 0.75), (L, 0.05)):
            q.append((x + px, y + s * W * k, z + 12))
            q.append((x + px, y + s * W * k * 0.7, z - 4))
        # each side as three convex pieces
        for i in range(0, 8, 2):
            pts = q[i:i + 4]
            inner = [(p[0], y + (p[1] - y) * 0.82, p[2]) for p in pts]
            b = hull(pts + inner, T("plank", mode="grain", axis=(1, 0, 0), scale=0.75))
            if b:
                out.append(b)
    side(1)
    side(-1)
    out.append(hull([(x - L * 0.8, y - 4, z - 6), (x + L * 0.8, y - 4, z - 6), (x - L * 0.8, y + 4, z - 6), (x + L * 0.8, y + 4, z - 6),
                     (x - L * 0.6, y - W * 0.55, z - 2), (x + L * 0.6, y - W * 0.55, z - 2), (x - L * 0.6, y + W * 0.55, z - 2),
                     (x + L * 0.6, y + W * 0.55, z - 2)], T("plank", mode="grain", axis=(1, 0, 0), scale=0.75)))
    for px in (-26, 22):
        out.append(box(x + px - 5, y - W * 0.82, z + 5, x + px + 5, y + W * 0.82, z + 8, wood("plank", (0, 1, 0))))
    for s in (-1, 1):
        out.append(beam((x - 40, y + s * 6, z + 9), (x + 30, y + s * 14, z + 9), 2.5, 1.5, wood("beam", (1, 0, 0))))


def build_decor(mw):
    rnd = random.Random(808)
    trees = Pieces(mw, "decor: pines")
    rocks = Pieces(mw, "decor: boulders")
    props = Pieces(mw, "decor: barrels, boat, islets")
    # pines: groves on the hills and along the island's edge
    placed = []
    tries = 0
    while len(placed) < 46 and tries < 20000:
        tries += 1
        x, y = rnd.uniform(-1500, 1550), rnd.uniform(-950, 1100)
        d = coast_distance(x, y)
        if d < 230 or not keep_clear(x, y, 110):  # (their branches at head height: keep them off the paths)
            continue
        if any(math.hypot(x - a, y - b) < 110 for a, b in placed):
            continue
        # groves: denser near the hills
        near = max(math.exp(-((x - hx) ** 2 + (y - hy) ** 2) / (r * r * 2.2)) for (hx, hy, r, _) in HILLS)
        if rnd.random() > 0.25 + 0.75 * near:
            continue
        placed.append((x, y))
        pine(trees, x, y, height(x, y), rnd.uniform(150, 280), rnd)
    # boulders along the beach and at the cliffs' feet
    stones = []
    tries = 0
    while len(stones) < 90 and tries < 30000:
        tries += 1
        x, y = rnd.uniform(-1800, 1800), rnd.uniform(-1300, 1400)
        d = coast_distance(x, y)
        if not (-40 < d < 150) or not keep_clear(x, y):
            continue
        if any(math.hypot(x - a, y - b) < 90 for a, b in stones):
            continue
        stones.append((x, y))
        s = rnd.uniform(10, 34)
        rock(rocks, x, y, height(x, y) + s * 0.2, s * rnd.uniform(1.0, 1.6), s * rnd.uniform(0.9, 1.3), s * rnd.uniform(0.6, 1.0),
             rnd.randrange(1 << 20))
    for i in range(70):  # at the cliffs' feet, half in the water
        th = rnd.uniform(0, 2 * math.pi)
        r = cliff_radius(th) - rnd.uniform(30, 120)
        x, y = r * math.cos(th), r * math.sin(th)
        if abs(x) > BOX - 80 or abs(y) > BOX - 80:
            continue
        s = rnd.uniform(30, 90)
        rock(rocks, x, y, height(x, y) + s * 0.1, s * 1.4, s, s * 0.8, rnd.randrange(1 << 20), T("cliff", mode="face", scale=2))
    # barrels by the pier, the pavilion, the range: physics props (QC vr_barrel: a crate's, make_crates.py's model),
    # turned and skinned at random
    brnd = random.Random(909)
    for (x, y, lying) in BARRELS:
        rnd.randrange(64), rnd.uniform(0, 1)  # (what the brush barrels drew: the islets' boulders keep their places)
        k = {"classname": "vr_barrel", "origin": "%d %d %d" % (x, y, height(x, y) + (12 if lying else 16) + 4),
             "angle": str(brnd.randrange(0, 360)), "skin": str(brnd.randrange(3))}
        if lying:
            k["spawnflags"] = "1"
        mw.add(k)
    # the rowboat, tied to the pier's end
    p = PIER
    rowboat(props, p["x"] - p["w"] / 2 - 40, p["y1"] + 150, 0)
    rope(props, (p["x"] - p["w"] / 2 - 4, p["y1"] + 104, p["z"] - 6), (p["x"] - p["w"] / 2 - 104, p["y1"] + 150, 12), 6)
    # a brazier and boulders on each islet: places to swim to
    for (ix, iy, r, peak) in ISLETS:
        if peak < 40:
            continue
        g = height(ix, iy)
        brazier(mw, props, ix, iy, g, 36, base="cliff", sink=64)
        for k in range(5):
            a = 2 * math.pi * k / 5 + rnd.uniform(-0.3, 0.3)
            dd = r * rnd.uniform(0.35, 0.6)
            s = rnd.uniform(18, 40)
            rock(rocks, ix + dd * math.cos(a), iy + dd * math.sin(a), height(ix + dd * math.cos(a), iy + dd * math.sin(a)) + s * 0.2,
                 s * 1.3, s, s * 0.8, rnd.randrange(1 << 20))
    for pc in (trees, rocks, props):
        pc.split()
    print("decor: %d pines, %d boulders" % (len(placed), len(stones)))


# ---------------------------------------------------------------------------------------------------------------------
# Entities
BUTTON_SIZE = 18     # a button's face, square (e1m1's are 32: in VR a hand-sized 18 is plenty); its label shows above it
BUTTON_TEX = 32      # +0basebtn's (and +abasebtn's, its pressed frame) size in texels
BUTTON_BAND = 4      # the texels of the texture's outer frame the button's sides, top and bottom show


def button_tex(lo, hi, d, key="button"):
    """A TexFn laying one copy of the button texture on a button's box (`lo`, `hi`) pushed along `d` (horizontal,
    axis-aligned): fitted to the front face (its frame on the face's edges, never tiled), and on the sides, top and
    bottom the texture's outer frame band (BUTTON_BAND texels across the depth from the front edge, the face's own
    fit along the other way), as if the front were folded round them."""
    right = (d[1], -d[0], 0.0)                   # seen from the front (looking along d)
    corners = [(x, y, z) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
    r0, r1 = min(dot(c, right) for c in corners), max(dot(c, right) for c in corners)
    d0, d1 = min(dot(c, d) for c in corners), max(dot(c, d) for c in corners)
    z1 = hi[2]
    width, height, depth = r1 - r0, hi[2] - lo[2], d1 - d0
    su, sv, sd = width / BUTTON_TEX, height / BUTTON_TEX, depth / BUTTON_BAND
    down = (0.0, 0.0, -1.0)
    name = TEXN[key]

    def off(o, sc):  # the shift that puts texel 0 at the coordinate o along an axis scaled by sc (mod one copy)
        return (-o / sc) % BUTTON_TEX

    def spec(n, c):
        if abs(dot(n, d)) > 0.9:   # the front (and the back, in the panel)
            return (name, right, down, off(r0, su), off(-z1, sv), su, sv)
        if abs(n[2]) > 0.9:        # the top and the bottom: across, the face's fit; back from the front edge, the band
            return (name, right, d, off(r0, su), off(d0, sd), su, sd)
        return (name, d, down, off(d0, sd), off(-z1, sv), sd, sv)  # the sides

    return spec


def button(mw, label, command, x, y, z, angle, depth=6, size=BUTTON_SIZE, scale="0.2", key="button"):
    """A func_button running `command` (buttonEffect 3), `label` above it (QC's buttons.qc: 12 over its centre, 6 in
    front); pushed towards `angle` (90 or 270 here). (x, y): the middle of its back, on the panel it is set in; z: its
    centre's height; it stands `depth` out of the panel, `size` square."""
    a = math.radians(angle)
    d = (round(math.cos(a)), round(math.sin(a)), 0.0)
    h = size / 2
    rx, ry = abs(d[1]) * h, abs(d[0]) * h
    fx, fy = x - d[0] * depth, y - d[1] * depth
    lo = (min(x, fx) - rx, min(y, fy) - ry, z - h)
    hi = (max(x, fx) + rx, max(y, fy) + ry, z + h)
    mw.add({"classname": "func_button", "buttonEffect": "3", "targetname": command + "\\n", "worldtext": label,
            "worldtext_halign": "1", "worldtext_scale": scale, "angle": str(angle), "wait": "1", "speed": "50",
            "lip": "4", "sounds": "1"}, [box(lo[0], lo[1], lo[2], hi[0], hi[1], hi[2], button_tex(lo, hi, d, key))])


def banner(mw, text, x, y, z, angle, scale="0.3", speed=None):
    """A text board (func_worldtext_banner) facing `angle`; lines split with \\n, pages with $."""
    k = {"classname": "func_worldtext_banner", "worldtext": text, "worldtext_halign": "1", "worldtext_scale": scale,
         "angle": str(angle), "origin": "%d %d %d" % (x, y, z)}
    if speed:
        k["speed"] = str(speed)
    mw.add(k)


def tip(mw, name, message, x, y, z, distance=200, target=None, size=None):
    k = {"classname": "func_vr_tip", "tipname": name, "message": message, "distance": str(distance),
         "origin": "%d %d %d" % (x, y, z)}
    if target:
        k["target"] = target
    if size:
        k["tip_size"] = str(size)
    mw.add(k)


N = "\\n"

# the campaign lecterns' buttons: (label, command); the first three choose what the slipgate starts (the old hub's
# vr_activestartpaknameidx; QC's buttons.qc marks a mission pack that is not installed), the fourth starts at once
CAMPAIGNS = [("QUAKE", "vr_activestartpaknameidx 0; echo Quake selected: step into the slipgate"),
             ("SCOURGE OF" + N + "ARMAGON", "vr_activestartpaknameidx 1; echo Scourge of Armagon selected: step into the slipgate"),
             ("DISSOLUTION" + N + "OF ETERNITY", "vr_activestartpaknameidx 2; echo Dissolution of Eternity selected: step into the slipgate"),
             ("DIMENSION" + N + "OF THE PAST" + N + "(starts now)", "vr_campaign_select dopa")]

# the pavilion's setting buttons (vr_setup_option <key>: Quake/vr/vr_setup.cpp's table; each press steps the setting,
# shows it on a screen over the button and saves the config): the north board's rows, the south board's
SETTINGS_NORTH = [[("TURNING", "turning"), ("TURN SPEED", "turnspeed"), ("MOVE" + N + "TOWARDS", "movedir"),
                   ("SWAP STICKS", "sticks"), ("RUN OR WALK", "run")],
                  [("TELEPORT", "teleport"), ("CLIMBING", "climb"), ("WORLD SCALE", "scale"),
                   ("STANDING" + N + "OR SEATED", "position"), ("TIPS", "tips")]]
SETTINGS_SOUTH = [[("BODY", "body"), ("HUD", "hud"), ("CROSSHAIR", "crosshair"), ("WEAPON GRIP", "grip"),
                   ("GADGET ARM", "gadget")],
                  [("TORCH SIDE", "torch"), ("WEAPON MODE", "holsters"), ("RELOADING", "reload"),
                   ("TWO-HANDED" + N + "AIM", "twohand")]]


def build_entities(mw):
    p, t, pv, rg, tw = PIER, TERRACE, PAVILION, RANGE, TOWER
    mw.add({"classname": "info_player_start", "origin": "%d %d %d" % (p["x"], p["y1"] + 60, p["z"] + 24), "angle": "90"})
    # ---- the arrival
    wx, wy = WELCOME
    banner(mw, N.join(["WELCOME TO QUAKE VR", "", "Follow the torches up the steps:", "the campaigns, then the settings,",
                       "the firing range and the lookout."]), wx, wy - 5, 14 + 92, 270, "0.4")
    banner(mw, N.join(["Walk with the stick; turn with the other.", "In the water: stroke with your arms."]),
           wx, wy - 5, 14 + 64, 270, "0.3")
    qx, qy = QUICK
    banner(mw, N.join(["NEW TO VR?", "The tutorial teaches the basics;", "calibration fits the game to your body."]),
           qx, qy - 6, 14 + 96, 270, "0.3")
    button(mw, "VR" + N + "TUTORIAL", "map vrtutorial", qx - 19, qy - 4, 14 + 40, 90)
    button(mw, "VR" + N + "CALIBRATION", "map vrcalibration", qx + 19, qy - 4, 14 + 40, 90)
    tip(mw, "vs2_welcome", "Welcome! Walk with the stick and follow" + N + "the torches up to the campaigns.",
        p["x"], p["y1"] + 160, p["z"] + 40, 260)
    # ---- the campaign terrace
    gx, gy = GATE["x"], GATE["y"]
    zb = t["z"]
    for (label, cmd), lx in zip(CAMPAIGNS, LECTERNS_X):
        cx = gx + lx
        button(mw, label, cmd, cx, LECTERN_Y, zb + 42, 90,
               scale="0.17" if "starts" in label else "0.2")
    banner(mw, N.join(["CHOOSE A CAMPAIGN", "Press its stone, then step into the slipgate."]),
           gx, gy - 24, zb + 16 + 205, 270, "0.45")
    banner(mw, N.join(["Every campaign, and what", "its data needs:", "{menu:Official Campaigns}"]),
           gx - 250, LECTERN_Y - 20, zb + 60, 270, "0.25")
    tip(mw, "vs2_campaign", "Press a campaign's stone with your hand," + N + "then walk into the slipgate.",
        gx - 55, LECTERN_Y - 10, zb + 50, 220, target=CAMPAIGNS[0][1] + "\\n")
    # ---- the pavilion's settings
    x0, x1, y0, y1, z = pv["x0"], pv["x1"], pv["y0"], pv["y1"], pv["z"]
    cxs = [-228, -168, -108, -48, 12]
    for row, entries in enumerate(SETTINGS_NORTH):
        zz = z + 26 + 48 * row
        for (label, key), cx in zip(entries, cxs):
            button(mw, label, "vr_setup_option " + key, cx, y1 - 26, zz + 14, 90, depth=8)
    for row, entries in enumerate(SETTINGS_SOUTH):
        zz = z + 26 + 48 * row
        off = 30 if len(entries) < 5 else 0
        for (label, key), cx in zip(entries, cxs):
            button(mw, label, "vr_setup_option " + key, cx + off, y0 + 26, zz + 14, 270, depth=8)
    banner(mw, N.join(["MOVING AND TURNING", "More: {menu:Locomotion}"]), -108, y1 - 27, z + 138, 270, "0.3")
    banner(mw, N.join(["BODY AND HANDS", "More: {menu:Body and Display}"]), -108, y0 + 27, z + 138, 90, "0.3")
    # (over the way in, between the torches on either side of it: it stood in front of one of them)
    banner(mw, N.join(["SETTINGS", "Each button steps its setting and saves it;", "the screen above it shows the choice."]),
           x0 - 30, 0.5 * (y0 + y1), z + 92, 180, "0.3")
    tip(mw, "vs2_settings", "Press a button to change that setting:" + N + "the screen above it shows what it is now.",
        -168, y1 - 40, z + 50, 200, target="vr_setup_option turning\\n")
    # ---- the firing range: guns and ammunition on the benches, targets down the lanes
    lx = rg["line"] - 17
    top = rg["z"] + 44
    for (w, y) in ((4, -180), (5, -110), (6, 10)):
        mw.add({"classname": "func_weapon_grabbable", "weapon": str(w), "origin": "%d %d %d" % (lx, y, top), "angle": "90"})
    mw.add({"classname": "weapon_crowbar", "origin": "%d %d %d" % (lx, 100, top), "angles": "0 80 90"})
    for (cls, y) in (("item_shells", -145), ("item_shells", -80), ("item_spikes", 40), ("item_spikes", 70)):
        mw.add({"classname": cls, "origin": "%d %d %d" % (lx, y, top)})
    mw.add({"classname": "item_health", "origin": "%d %d %d" % (rg["line"] - 60, 140, rg["z"] + 8)})
    banner(mw, N.join(["FIRING RANGE", "Grip a gun from the bench; put", "ammunition in at a holster."]),
           rg["line"] - 70, -40, rg["z"] + 104, 180, "0.3")
    banner(mw, N.join(["Shoot the targets, the crates, the rocks", "on the shelf. Stronger weapons wait", "in the campaigns."]),
           rg["line"] - 70, -40, rg["z"] + 78, 180, "0.25")
    tip(mw, "vs2_range", "Grip a gun from the bench with either hand." + N + "Hold a box of ammunition to a holster to load.",
        lx, -40, top + 10, 200)
    zf = rg["z"]
    # lane 1 (y -160): a stack of crates
    for (x, y, zz, large) in ((1100, -178, zf, 0), (1100, -142, zf, 0), (1100, -160, zf + 32, 0), (1260, -170, zf, 1)):
        k = {"classname": "vr_crate", "origin": "%d %d %d" % (x, y, zz + 2), "angle": str(RND.randrange(0, 30)),
             "skin": str(RND.randrange(3))}
        if large:
            k["spawnflags"] = "1"
        mw.add(k)
    # lane 2 (y -40): a training dummy, an exploding box further on
    mw.add({"classname": "vr_dummy", "origin": "%d %d %d" % (780, -40, zf + 24), "angle": "180"})
    mw.add({"classname": "misc_explobox2", "origin": "%d %d %d" % (1300, -40, zf + 2)})
    # lane 3 (y 80): rocks and bricks on the shelf, crates further on
    sx, sy = SHELF
    for i, mdl in enumerate(("vr_rock1", "vr_brick1", "vr_rock3", "vr_brick3", "vr_rock5")):
        mw.add({"classname": "vr_debris_piece", "model": "progs/%s.mdl" % mdl, "origin": "%d %d %d" % (sx, sy - 30 + 15 * i, zf + 40),
                "angle": str(RND.randrange(0, 360)), "skin": str(RND.randrange(6))})
    for (x, y, zz) in ((1240, 70, zf), (1240, 102, zf), (1240, 86, zf + 32)):
        mw.add({"classname": "vr_crate", "origin": "%d %d %d" % (x, y, zz + 2), "angle": str(RND.randrange(0, 30)),
                "skin": str(RND.randrange(3))})
    # ---- the lookout tower
    banner(mw, N.join(["THE LOOKOUT", "Climb the ladder: grip a rung with", "an empty hand, pull yourself up."]),
           tw["x"] - 70, tw["y"] + tw["half"] + 60, tw["z"] + 80, 90, "0.28")
    banner(mw, N.join(["From the top: dive into deep water.", "Climbing: {menu:Climbing}"]),
           tw["x"] - 70, tw["y"] + tw["half"] + 60, tw["z"] + 56, 90, "0.25")
    tip(mw, "vs2_ladder", "Grip a rung with an empty hand and pull" + N + "down to climb. Let go to drop.",
        tw["x"], tw["y"] + tw["half"] + 8, tw["z"] + 60, 160)
    # ---- props: crates by the pier and the pavilion, night sounds
    for (x, y, large) in ((-1290, -830, 1), (-1285, -790, 0), (-1110, -840, 0), (110, -220, 0), (-330, 70, 1)):
        k = {"classname": "vr_crate", "origin": "%d %d %d" % (x, y, height(x, y) + 4), "angle": str(RND.randrange(0, 90)),
             "skin": str(RND.randrange(3))}
        if large:
            k["spawnflags"] = "1"
        mw.add(k)
    for (x, y) in ((-900, 400), (300, 500), (900, -300), (-700, -700)):
        mw.add({"classname": "ambient_swamp1", "origin": "%d %d %d" % (x, y, height(x, y) + 40)})


WORLD_KEYS = {
    "classname": "worldspawn", "mapversion": "220", "wad": WADS,
    "_tb_mod": "hipnotic;rogue;quakevr", "message": "Quake VR", "worldtype": "0", "sounds": "0",
    "light": "14", "_minlight_color": "0.55 0.62 1", "_sunlight": "230", "_sunlight_mangle": "240 -30 0", "_sunlight_color": "0.62 0.72 1.0",
    "_sunlight2": "75", "_sunlight2_color": "0.3 0.38 0.62", "_bounce": "1", "_vr_debris": "0", "_vr_crates": "0",
    "sky": "vs2night", "fog": "0.035 0.045 0.055 0.08",
}


ONLY_TERRAIN = "--only-terrain" in sys.argv


def write_map():
    global COAST_GRID
    t0 = time.time()
    COAST_GRID = Grid(coast_distance_exact, -2100, -1700, 2100, 1800, 24)
    build_layout()
    mw = MapWriter()
    build_world(mw)
    mw.add({"classname": "func_detail_illusionary"}, NONSOLID)
    build_terrain(mw)
    if ONLY_TERRAIN:
        mw.write(OUT.replace(".map", "_terrain.map"), WORLD_KEYS, "// terrain only (a debugging build)\n")
        return
    build_pier(mw)
    build_arrival(mw)
    build_stairs(mw)
    build_terrace(mw)
    build_bridge(mw)
    build_pavilion(mw)
    build_range(mw)
    build_tower(mw)
    build_lights(mw)
    build_decor(mw)
    build_entities(mw)
    header = "// Game: Quake VR\n// Format: Valve\n// Written by Misc/quakevr/maps/vrstart2_gen.py: edit that, not this.\n"
    mw.write(OUT, WORLD_KEYS, header)
    nb = len(mw.world) + sum(len(b) for _, b in mw.groups) + sum(len(b) for _, b in mw.entities)
    print("wrote %s: %d brushes, %d entities (%.1f s); %d nearly coplanar faces folded into their neighbours (%d kept), "
          "%d corners settled off the ground or the water, %d faces turned to an axis" % (OUT, nb, len(mw.entities),
          time.time() - t0, mapgeom.FOLD_STATS["dropped"], mapgeom.FOLD_STATS["kept"], mapgeom.SETTLE_STATS["moved"],
          mapgeom.AXIS_STATS["snapped"]))


# Compiling: qbsp 0.18.1 (DEFAULT_QBSP) -bsp2 -splitturb (the water's faces cut to lightmap size and lit, as 2.0 lit them:
# no lit_liquids patch needed), then 2.0's vis and light.
QBSP_ARGS = ["-bsp2", "-splitturb"]
# The two presets: "fast" for iterating (vis -fast, plain light: no ambient occlusion, bounce or extra samples), and
# "final", the one the shipped .bsp is built with (MAPPING.md's Full profile with -bounce).
PRESETS = {
    "fast": dict(vis=["-fast"], light=["-lit", "-lux", "-lightgrid", "-lightgrid_dist", "128", "128", "128"]),
    "final": dict(vis=[], light=["-extra4", "-dirt", "-dirtscale", "1.5", "-dirtdepth", "96", "-bounce", "-lit", "-lux",
                                 "-lightgrid", "-lightgrid_dist", "64", "64", "64"]),
}


def compile_map(tools, work, preset, check=0, qbsp=DEFAULT_QBSP):
    """qbsp (0.18.1's), vis and light (ericw-tools 2.0 in `tools`) in `work`, the .bsp, .lit and .lux copied next to
    the .map. Prints each stage's time and its warnings; `check`: rays of the hole test (bsp_holes.py) over the
    result."""
    os.makedirs(work, exist_ok=True)
    src = os.path.join(work, MAPNAME + ".map")
    bsp = os.path.join(work, MAPNAME + ".bsp")
    shutil.copyfile(OUT, src)
    pr = PRESETS[preset]
    cmds = [[qbsp, "-nopercent"] + QBSP_ARGS + ["-wadpath", ROOT, src, bsp],
            [os.path.join(tools, "vis.exe"), "-nolog", "-nopercent"] + pr["vis"] + [bsp],
            [os.path.join(tools, "light.exe"), "-nolog", "-nopercent"] + pr["light"] + [bsp]]
    total = time.time()
    for cmd in cmds:
        t0 = time.time()
        result = subprocess.run(cmd, capture_output=True, text=True)
        lines = (result.stdout + result.stderr).splitlines()
        name = os.path.basename(cmd[0])[:-4]
        warnings = [l for l in lines if ("WARNING" in l.upper() or "ERROR" in l.upper() or "LEAK" in l.upper())
                    and "info_player_deathmatch" not in l]
        print("%s: exit %d (%.0f s)%s" % (name, result.returncode, time.time() - t0,
                                         "".join("\n  " + w_ for w_ in warnings[:15])))
        with open(os.path.join(work, name + ".log"), "w") as f:
            f.write("\n".join(lines))
        if result.returncode:
            sys.exit(1)
    print("compiled (%s) in %.0f s" % (preset, time.time() - total))
    for ext in (".bsp", ".lit", ".lux"):
        if os.path.exists(os.path.join(work, MAPNAME + ext)):
            shutil.copyfile(os.path.join(work, MAPNAME + ext), os.path.join(os.path.dirname(OUT), MAPNAME + ext))
    if check:
        subprocess.run([sys.executable, os.path.join(HERE, "bsp_holes.py"), bsp, "--rays", str(check), "--show", "10"])


def main():
    ap = argparse.ArgumentParser(
        description="Writes quakevr/maps/vrstart2.map; with --compile also its .bsp, .lit and .lux (ericw-tools 2.0).",
        epilog="qbsp: ericw-tools 0.18.1's (%s; --qbsp), vis and light 2.0's (--tools). Presets (--preset): "
               "fast = vis -fast, light -lit -lux and a 128-unit light grid (no -extra4, -dirt or -bounce), for "
               "iterating; final = full vis, light %s: the shipped build. Check a build for holes: --check 1000000 "
               "(bsp_holes.py)." % (" ".join(QBSP_ARGS), " ".join(PRESETS["final"]["light"])))
    ap.add_argument("--compile", action="store_true", help="also build the .bsp, .lit and .lux")
    ap.add_argument("--preset", choices=sorted(PRESETS), default="final", help="the compile preset (default: final)")
    ap.add_argument("--fast", action="store_true", help="the same as --preset fast")
    ap.add_argument("--check", type=int, default=0, metavar="RAYS", help="after compiling, the hole test with RAYS rays")
    ap.add_argument("--tools", default=DEFAULT_TOOLS, help="ericw-tools 2.0's folder (vis, light)")
    ap.add_argument("--qbsp", default=DEFAULT_QBSP, help="the qbsp.exe (0.18.1's: see DEFAULT_QBSP)")
    ap.add_argument("--only-terrain", action="store_true", help="debugging: write <map>_terrain.map, the terrain alone")
    ap.add_argument("--work", default=os.path.join(tempfile.gettempdir(), MAPNAME + "_build"))
    args = ap.parse_args()
    write_map()
    if args.compile:
        compile_map(args.tools, args.work, "fast" if args.fast else args.preset, args.check, args.qbsp)


if __name__ == "__main__":
    main()
