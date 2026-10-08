# vrtrailer_gen.py -- writes quakevr/maps/vrtrailer.map, the trailer's opening scene, and with --compile builds it as
# vrstart is built (qbsp 0.18.1, ericw-tools 2.0's vis and light; presets "fast" and "final"; MAPPING.md, "vrtrailer").
#
#   python Misc/quakevr/maps/vrtrailer_gen.py [--compile [--preset fast|final] [--check RAYS]] [--tools DIR] [--qbsp EXE]
#   (first: python Misc/trenchbroom/make_id_wad.py, the id textures' WAD)
#
# A much smaller vrstart, made of vrstart_gen.py's parts (its planks, logs, railings, ropes, boulders, pines, torches,
# braziers, materials, sky, moonlight and fog; its cliffs and mountains at 1/1.75 scale): a lake at night with a
# wooden bridge across its middle, from a small islet in the south to another in the north. Edit this script, not the
# .map. Layout (x east, y north, the water's surface at z 0, 32.8 units a metre):
#
#   - the player starts at the bridge's south end, facing north along it (the moon ahead, 30 degrees right and 30 up);
#   - torch pillars rise out of the water in pairs either side of the bridge, a flame in an iron bowl on each;
#   - half way, the bridge widens to a landing: on it a stone pedestal at torso height (33 units, 1.0 m) with Dawn of
#     the Machine's Super Axe lying on its top (a func_weapon_grabbable, weapon 18: spawnflags 1 lying as placed, 2 taken
#     by its handle, wherever the hand closes on it);
#   - the bridge goes on to the north islet, where a grunt stands looking out over the water, his back to the bridge:
#     oblivious until hurt (vr_oblivious 1: no sight, sound, footsteps or stealth AI wakes him);
#   - info_vr_trailer: the scene (QC vr_trailer.qc): recording mode on (vr_recording_clean_map: no tips, no wrist
#     messages), the player's hands empty, and vr_trailer_reset (Debug > Tests > Trailer Scene) for retakes.
import argparse
import math
import os
import random
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import mapgeom
from mapgeom import (MapWriter, beam, box, cylinder, cross, delaunay, hull, lerp, mul, ngon, norm, prism,
                     simplify_points, smoothstep, sub, terrain_mesh, unbend)
import vrstart_gen as vs
from vrstart_gen import (T, chamfer_box, post, rail, railing, rock, pine, wall_torch, torch_post, wood, ent, split,
                         Pieces, PN, PN3)

ROOT = vs.ROOT
MAPNAME = "vrtrailer"
OUT = os.path.join(ROOT, "quakevr", "maps", MAPNAME + ".map")

BOX = 2560            # the inside of the sealing box: x, y in -BOX..BOX
SKY_TOP = 1600
FLOOR_Z = -1024       # the terrain prisms' bottoms
WATER_Z = 0
LAKE_DEPTH = 210      # the lake's floor, far from the islets
GROUND_STEP = 8       # the islets' ground heights are multiples of this (vrstart's height(): Quake's collision)
SCALE = 1.75          # vrstart's cliffs and mountains, this much smaller (its cliff_height at x * SCALE, / SCALE)

DECK_Z = 40           # the bridge's walking height (1.2 m over the water)
BRIDGE_Y = 470        # the deck runs from -BRIDGE_Y to BRIDGE_Y along x = 0
BRIDGE_W = 96
LANDING = dict(x=104, y=72)   # the landing half way (half sizes): the pedestal in its middle
PEDESTAL_TOP = DECK_Z + 33    # 1.0 m: torso height
PILLARS_X = 168               # the torch pillars' pairs: x = +-PILLARS_X at these y
PILLARS_Y = (-300, 0, 300)
COAST_AT = 440                # where the islets' shores cross the bridge's line (y = +-COAST_AT)
GRUNT = (0, 760)              # the grunt (facing north, the water about 110 units ahead of him)
START = (0, -444)             # the player's start (facing north)

# The islets: (cx, cy, radius, seed): their centres put so that their shores cross x = 0 at y = +-COAST_AT.
ISLETS = []


def islet_radius(i, theta):
    _, _, r, seed = ISLETS[i]
    c, s = math.cos(theta), math.sin(theta)
    return r * (1 + 0.11 * PN(c * 1.3 + seed, s * 1.3 - seed) + 0.04 * PN(c * 4 + 3 * seed, s * 4))


def build_layout():
    ISLETS[:] = [(0, 0, 200, 3.7), (0, 0, 215, 8.1)]
    for i, sy in ((0, -1), (1, 1)):
        r = None
        cx, _, rr, seed = ISLETS[i]
        # the shore towards the bridge: theta 90 degrees (south islet) or 270 (north)
        th = math.pi / 2 if sy < 0 else -math.pi / 2
        ISLETS[i] = (cx, 0, rr, seed)
        r = islet_radius(i, th)
        ISLETS[i] = (cx, sy * (COAST_AT + r), rr, seed)


def coast_distance(x, y):
    """In from the nearest islet's shore (negative in the water)."""
    best = -1e9
    for i, (cx, cy, r, _) in enumerate(ISLETS):
        dd = math.hypot(x - cx, y - cy)
        if dd > r * 2.5 and best > -1e8:
            continue
        best = max(best, islet_radius(i, math.atan2(y - cy, x - cx)) - dd)
    return best


def zone(x, y):
    """(weight, height) of the flattened ground: the bridge's ends (the deck's height, less the planks)."""
    w = 0.0
    for sy in (-1, 1):
        y0, y1 = sorted((sy * (BRIDGE_Y - 40), sy * (BRIDGE_Y + 110)))
        dx = max(-72 - x, 0, x - 72)
        dy = max(y0 - y, 0, y - y1)
        dist = math.hypot(dx, dy)
        w = max(w, 1.0 if dist <= 0 else 1.0 - smoothstep(0, 70, dist))
    return w, DECK_Z


def cliffs(x, y):
    h = vs.cliff_height(x * SCALE, y * SCALE)
    return None if h is None else h / SCALE


def height(x, y):
    d = coast_distance(x, y)
    if d >= 0:
        h = lerp(0, DECK_Z, smoothstep(0, 75, d)) + 4 * PN.fbm(x / 90, y / 90, 2) * smoothstep(20, 80, d)
    else:
        dd = -d
        h = -dd * 0.2 if dd < 100 else -20 - (LAKE_DEPTH - 20) * (1 - math.exp(-(dd - 100) / 260))
        h += 12 * PN.fbm(x / 300, y / 300, 3) * smoothstep(60, 300, dd)
    w, z = zone(x, y)
    if w > 0:
        h = lerp(h, z, w)
    ch = cliffs(x, y)
    if ch is not None:
        h = max(h, ch)
    # the islets' walkable ground in steps of GROUND_STEP (vrstart's height(): no snagging on nearly level seams)
    if h > 6 and d > -60 and ch is None:
        h = max(8, GROUND_STEP * round(h / GROUND_STEP))
    return h


# ---------------------------------------------------------------------------------------------------------------------
# The terrain (vrstart's way: points, simplified away from the islets, unbent, triangulated, cleaned, prisms)
def cliff_e(x, y):
    """Out from the cliffs' foot line (this map's units)."""
    return math.hypot(x, y) - vs.cliff_radius(math.atan2(y, x)) / SCALE


def spacing(x, y):
    """The lattice step (32 units) there: 1, 2, 4 or 8."""
    if coast_distance(x, y) > -160:
        return 1
    e = cliff_e(x, y)
    if -150 < e < 190:
        return 2
    if e > 500:
        return 8
    return 4


TERRAIN_SEED = 6     # the lattice's jitter (6: no qbsp warnings; 5 and 7 had a clip hull's fill warning)


def terrain_points():
    rnd = random.Random(TERRAIN_SEED)
    pts = {}
    feat = []
    # the cliffs' foot, face and top (vrstart's rings, scaled)
    jr = random.Random(77)
    for ring, e in enumerate((-40, -15, 10, 35, 60, 180)):
        n = 200
        for i in range(n):
            th = 2 * math.pi * (i + 0.5 * (ring % 2) + jr.uniform(-0.2, 0.2)) / n
            r = (vs.cliff_radius(th) + e + (jr.uniform(-7, 7) if -40 < e < 60 else 0)) / SCALE
            x, y = r * math.cos(th), r * math.sin(th)
            if abs(x) < BOX - 30 and abs(y) < BOX - 30:
                feat.append((int(round(x)), int(round(y))))
    # the flattened ground's edges at the bridge's ends
    for sy in (-1, 1):
        for k in range(9):
            yy = sy * (BRIDGE_Y - 40) + sy * 150 * k / 8
            feat += [(-72, int(round(yy))), (72, int(round(yy)))]
        for xx in range(-72, 73, 24):
            feat += [(xx, sy * (BRIDGE_Y + 110))]
    featset = {(f[0] // 16, f[1] // 16): f for f in feat}
    feat = list(featset.values())

    def near_feature(x, y, r=14):
        for f in feat:
            if abs(f[0] - x) < r and abs(f[1] - y) < r:
                return True
        return False

    step = 32
    n = BOX // step
    for i in range(-n, n + 1):
        for j in range(-n, n + 1):
            x, y = i * step, j * step
            jx, jy = rnd.uniform(-7, 7), rnd.uniform(-7, 7)
            edge = abs(i) == n or abs(j) == n
            k = spacing(x, y)
            if not edge and (i % k or j % k):
                continue
            if edge:
                corner = abs(i) == n and abs(j) == n
                if not corner and (i % 8 if abs(j) == n else j % 8):
                    continue
                px = BOX if i == n else -BOX if i == -n else x
                py = BOX if j == n else -BOX if j == -n else y
            else:
                px, py = x + jx * k, y + jy * k
                if k <= 2 and near_feature(px, py):
                    continue
            pts[(int(round(px)), int(round(py)))] = 1
    for f in feat:
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
    if d >= -40:
        if cz < 26:
            return T("sand")
        if abs(x) < 64 and zone(x, y)[0] > 0.5:
            return T("path")
        return T("grass", scale=1.5) if PN3.fbm(x / 420, y / 420, 2) > -0.1 else T("grass2", scale=1.5)
    if cz < 30:
        return T("sand") if n[2] > 0.95 else T("cliff2", mode="face", scale=2)
    if n[2] > 0.88 and cz < 700:
        return T("moss", scale=2)
    return T("cliff2", mode="face", scale=2)


def terrain_tolerance(x, y):
    if abs(x) >= BOX or abs(y) >= BOX or coast_distance(x, y) > -200:
        return 0
    e = cliff_e(x, y)
    if e > 190:
        return 10     # the mountains behind the cliffs' tops
    if e > -150:
        return 2      # the cliffs
    return 6          # the lake's floor (under dark water and fog)


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

    walk = [i for i, p in enumerate(pts) if coast_distance(p[0], p[1]) > -60 and H[i] % GROUND_STEP == 0 and H[i] >= 8]
    H, polys, st = terrain_mesh(pts, tris, H, tri_tex, pinned=walk, levels=(WATER_Z,))
    print("terrain: %d corners moved (at most %.2f units, %d moves): %d nearly coplanar pairs and %d nearly level tops left; "
          "%d triangles -> %d prisms" % (st["moved"], st["drift"], st["moves"], st["left"], st["tilted"], st["tris"], st["polys"]))
    groups = {"islets": mw.detail("terrain: islets"), "lake": mw.detail("terrain: lake floor"),
              "cliffs": mw.detail("terrain: cliffs and mountains")}
    side = T("cliff2")
    for ti in range(len(tris)):
        tri_tex(ti, H)
    for cyc, members in polys:
        poly = [(pts[i][0], pts[i][1], H[i]) for i in cyc]
        cx = sum(q[0] for q in poly) / len(poly)
        cy = sum(q[1] for q in poly) / len(poly)
        br = prism(poly, FLOOR_Z, texcache[members[0]], side)
        if coast_distance(cx, cy) > -250:
            groups["islets"].append(br)
        elif cliff_e(cx, cy) > -250:
            groups["cliffs"].append(br)
        else:
            groups["lake"].append(br)
    print("terrain: %d points, %d triangles (%.1f s)" % (len(pts), len(tris), time.time() - t0))
    mapgeom.SURFACES[:] = [(mapgeom.MeshSurface(pts, tris, H), True),
                           (lambda x, y: WATER_Z if abs(x) < BOX and abs(y) < BOX else None, False)]


def ground(x, y):
    return mapgeom.SURFACES[0][0](x, y) if mapgeom.SURFACES else height(x, y)


def build_world(mw):
    """The sealing box (sky round and above, rock under) and the lake's water (vrstart's build_world)."""
    w = mw.world
    S = BOX + 32

    def outside(n, c):
        return T("skip") if n[0] * -c[0] + n[1] * -c[1] + n[2] * -c[2] <= 0 else None

    w.append(box(-S, -S, FLOOR_Z - 32, S, S, FLOOR_Z, T("cliff2"), outside))
    w.append(box(-S, -S, SKY_TOP, S, S, SKY_TOP + 32, T("sky")))
    w.append(box(-S, -S, FLOOR_Z, -BOX, S, SKY_TOP, T("sky")))
    w.append(box(BOX, -S, FLOOR_Z, S, S, SKY_TOP, T("sky")))
    w.append(box(-BOX, -S, FLOOR_Z, BOX, -BOX, SKY_TOP, T("sky")))
    w.append(box(-BOX, BOX, FLOOR_Z, BOX, S, SKY_TOP, T("sky")))
    w.append(box(-BOX, -BOX, FLOOR_Z, BOX, BOX, WATER_Z, T("water", scale=1.0)))


# ---------------------------------------------------------------------------------------------------------------------
# The bridge, the landing and the pedestal
def deck_x(out, x0, y0, x1, y1, z, rnd):
    """Planks across the bridge (their long axis x), tops at z (vrstart's deck, along x)."""
    vs.deck(out, x0, y0, x1, y1, z, "x", width=10, gap=1.5, rnd=rnd)


def pile(out, x, y, ztop):
    g = ground(x, y)
    out.append(cylinder((x, y, min(g, -8) - 24), (x, y, ztop), 7, 8, wood("log", (0, 0, 1))))


def build_bridge(mw):
    out = mw.detail("bridge")
    rnd = random.Random(42)
    lx, ly, hw = LANDING["x"], LANDING["y"], BRIDGE_W / 2
    deck_x(out, -hw, -BRIDGE_Y, hw, -ly, DECK_Z, rnd)
    deck_x(out, -lx, -ly, lx, ly, DECK_Z, rnd)
    deck_x(out, -hw, ly, hw, BRIDGE_Y, DECK_Z, rnd)
    # the stringers: logs along the spans and round the landing, under the planks
    zs = DECK_Z - 9
    for sx in (-38, 38):
        for (ya, yb) in ((-BRIDGE_Y - 10, -ly + 6), (ly - 6, BRIDGE_Y + 10)):
            out.append(beam((sx, ya, zs), (sx, yb, zs), 10, 12, wood("log", (0, 1, 0))))
    for sx in (-lx + 8, lx - 8):
        out.append(beam((sx, -ly - 4, zs), (sx, ly + 4, zs), 10, 12, wood("log", (0, 1, 0))))
    for sy in (-ly + 6, ly - 6):
        out.append(beam((-lx - 4, sy, zs - 12), (lx + 4, sy, zs - 12), 9, 10, wood("beam", (1, 0, 0))))
    # the bents: two piles and a cap beam, every 130 units over the water; four piles under the landing's corners
    cap = DECK_Z - 15
    for yy in (-380, -250, -130, 130, 250, 380):
        for sx in (-50, 50):
            pile(out, sx, yy, cap)
        out.append(box(-60, yy - 5, cap - 8, 60, yy + 5, cap, wood("beam", (1, 0, 0))))
        rail(out, (-50, yy, -30), (50, yy, cap - 12), 5, 5)
    for sx in (-lx + 8, lx - 8):
        for sy in (-ly + 6, ly - 6):
            pile(out, sx, sy, cap - 12)
    # the abutments on the islets
    for sy in (-1, 1):
        y0, y1 = sorted((sy * (BRIDGE_Y - 34), sy * (BRIDGE_Y + 14)))
        out.append(chamfer_box(-60, y0, ground(0, sy * BRIDGE_Y) - 40, 60, y1, DECK_Z - 3, 3, T("block", scale=0.5)))
    # the railings: posts, a top rail at 38 (1.16 m), a rope between; round the landing's sides
    for s in (-1, 1):
        pts = [(s * (hw + 2), -BRIDGE_Y + 8, DECK_Z), (s * (hw + 2), -ly, DECK_Z), (s * (lx + 2), -ly, DECK_Z),
               (s * (lx + 2), ly, DECK_Z), (s * (hw + 2), ly, DECK_Z), (s * (hw + 2), BRIDGE_Y - 8, DECK_Z)]
        railing(out, pts, top=38, mid=20, every=54, sq=2.5, rope_mid=True)
    # the tall posts at the bridge's ends (their torches: build_lights)
    for sy in (-1, 1):
        for sx in (-1, 1):
            post(out, sx * (hw + 2), sy * (BRIDGE_Y - 8), DECK_Z - 30, DECK_Z + 70, 5, square=False)


def build_pedestal(mw):
    """A stone pedestal in the landing's middle, its top PEDESTAL_TOP: a plinth, a step, a fluted shaft, a moulded
    capital and a slab, a strip of dark iron on it where the Super Axe lies across the bridge; two lanterns on posts at
    the landing's far corners light it."""
    out = mw.detail("pedestal")
    z = DECK_Z
    stone = T("trim", scale=0.5)
    out.append(chamfer_box(-17, -17, z - 4, 17, 17, z + 3, 2, T("block", scale=0.5)))  # plinth (sunk through the planks)
    out.append(chamfer_box(-13, -13, z + 3, 13, 13, z + 7, 1.5, stone))
    out.append(cylinder((0, 0, z + 7), (0, 0, z + 22), 7.5, 8, stone, math.pi / 8))
    for k in range(8):                                                       # the flutes: thin ribs round the shaft
        a = math.pi / 8 + k * math.pi / 4
        cx, cy = 8 * math.cos(a), 8 * math.sin(a)
        out.append(cylinder((cx, cy, z + 8), (cx, cy, z + 21), 1.4, 6, stone))
    out.append(chamfer_box(-11, -11, z + 22, 11, 11, z + 25, 1.5, stone))
    out.append(chamfer_box(-21, -12, z + 25, 21, 12, PEDESTAL_TOP - 1, 2, stone))
    out.append(box(-17, -6, PEDESTAL_TOP - 1, 17, 6, PEDESTAL_TOP, T("iron", scale=0.5)))
    # the lanterns: posts inside the railing's far corners, arms reaching in, the lanterns hanging from them
    lx, ly = LANDING["x"], LANDING["y"]
    for s in (-1, 1):
        px, py = s * (lx - 8), ly - 10
        post(out, px, py, z - 4, z + 78, 3.5)
        out.append(box(min(px, px - s * 32), py - 2, z + 72, max(px, px - s * 32), py + 2, z + 76, wood("beam", (1, 0, 0))))
        vs.lantern(mw, out, px - s * 28, py, z + 58, 170, hang=4)


def torch_pillar(mw, out, x, y):
    """A stone pillar rising out of the water: a plinth at the waterline, a shaft, a capital, an iron bowl, a large
    flame (vrstart's brazier, taller and broader)."""
    g = ground(x, y)
    stone = T("block", scale=0.5)
    out.append(chamfer_box(x - 24, y - 24, g - 16, x + 24, y + 24, 10, 3, stone))
    out.append(chamfer_box(x - 15, y - 15, 10, x + 15, y + 15, 118, 2, stone))
    out.append(chamfer_box(x - 20, y - 20, 118, x + 20, y + 20, 128, 2, T("trim", scale=0.5)))
    q = [(px, py, 140) for (px, py) in ngon(x, y, 17, 8, math.pi / 8)]
    q += [(px, py, 128) for (px, py) in ngon(x, y, 10, 8, math.pi / 8)]
    out.append(hull(q, T("iron", scale=0.5)))
    ent(mw, "light_flame_large_yellow", x, y, 146, **PILLAR_FLAME)
    split(out)


PILLAR_FLAME = {"light": "300", "_color": "1 0.55 0.25", "wait": "0.9"}  # vrstart's braziers'


def build_lights(mw):
    out = Pieces(mw, "lights: pillars, posts")
    for yy in PILLARS_Y:
        for s in (-1, 1):
            torch_pillar(mw, out, s * PILLARS_X, yy)
    # torches on the bridge's end posts, facing along the deck
    hw = BRIDGE_W / 2
    for sy, yaw in ((-1, 90), (1, 270)):
        for sx in (-1, 1):
            wall_torch(mw, out, sx * (hw + 2), sy * (BRIDGE_Y - 8) - sy * 6, DECK_Z + 56, yaw)
    # the north islet: a torch post to the grunt's left, behind him (his outline lit from the bridge's side)
    gx, gy = GRUNT
    torch_post(mw, out, gx - 120, gy - 70, ground(gx - 120, gy - 70), 30)
    # the south islet: one at the start, behind the player
    torch_post(mw, out, 110, -560, ground(110, -560), 150)
    out.split()


def build_decor(mw):
    rnd = random.Random(808)
    trees = Pieces(mw, "decor: pines")
    rocks = Pieces(mw, "decor: boulders")
    # pines on the islets, clear of the bridge's line and of the view of the grunt
    for (x, y, h) in ((-130, -600, 230), (150, -660, 260), (-60, -720, 200), (40, -780, 180),
                      (-180, 560, 250), (175, 620, 220), (150, 520, 170)):
        if coast_distance(x, y) > 40:
            pine(trees, x, y, ground(x, y), h, rnd)
    # boulders round the islets' shores, half in the water
    for i, (cx, cy, r, _) in enumerate(ISLETS):
        for k in range(14):
            th = 2 * math.pi * (k + rnd.uniform(-0.3, 0.3)) / 14
            if abs(math.cos(th)) < 0.35 and (math.sin(th) > 0) == (cy < 0):
                continue  # (not where the bridge comes in)
            rr = islet_radius(i, th) + rnd.uniform(-20, 25)
            x, y = cx + rr * math.cos(th), cy + rr * math.sin(th)
            s = rnd.uniform(14, 36)
            rock(rocks, x, y, ground(x, y) + s * 0.15, s * rnd.uniform(1.0, 1.5), s * rnd.uniform(0.9, 1.2),
                 s * rnd.uniform(0.6, 0.9), rnd.randrange(1 << 20))
    # at the cliffs' feet
    for k in range(60):
        th = rnd.uniform(0, 2 * math.pi)
        r = vs.cliff_radius(th) / SCALE - rnd.uniform(20, 70)
        x, y = r * math.cos(th), r * math.sin(th)
        if abs(x) > BOX - 60 or abs(y) > BOX - 60:
            continue
        s = rnd.uniform(24, 60)
        rock(rocks, x, y, ground(x, y) + s * 0.1, s * 1.4, s, s * 0.8, rnd.randrange(1 << 20),
             T("cliff", mode="face", scale=2))
    trees.split()
    rocks.split()


# ---------------------------------------------------------------------------------------------------------------------
# Entities
def build_entities(mw):
    sx, sy = START
    mw.add({"classname": "info_player_start", "origin": "%d %d %d" % (sx, sy, DECK_Z + 26), "angle": "90"})
    # the scene: its origin and angle are the player's start for vr_trailer_reset (QC vr_trailer.qc)
    mw.add({"classname": "info_vr_trailer", "origin": "%d %d %d" % (sx, sy, DECK_Z + 26), "angle": "90"})
    gx, gy = GRUNT
    mw.add({"classname": "monster_army", "origin": "%d %d %d" % (gx, gy, ground(gx, gy) + 24), "angle": "90",
            "vr_oblivious": "1"})
    # the Super Axe, lying across the pedestal's top (spawnflags 1 as placed, 2 taken by its handle: QC buttons.qc)
    mw.add({"classname": "func_weapon_grabbable", "origin": "%d %d %d" % (AXE[0], AXE[1], AXE[2]), "weapon": "18",
            "spawnflags": "3", "angles": AXE_ANGLES})


AXE = (0, 0, PEDESTAL_TOP + 3)
AXE_ANGLES = "0 90 90"  # (pitch yaw roll: lying on its side, its handle along x)

WORLD_KEYS = dict(vs.WORLD_KEYS)
WORLD_KEYS["message"] = "The Lake"


def write_map():
    t0 = time.time()
    build_layout()
    mw = MapWriter()
    build_world(mw)
    del vs.NONSOLID[:]
    mw.add({"classname": "func_detail_illusionary"}, vs.NONSOLID)
    build_terrain(mw)
    build_bridge(mw)
    build_pedestal(mw)
    build_lights(mw)
    build_decor(mw)
    build_entities(mw)
    header = "// Game: Quake VR\n// Format: Valve\n// Written by Misc/quakevr/maps/vrtrailer_gen.py: edit that, not this.\n"
    mw.write(OUT, WORLD_KEYS, header)
    nb = len(mw.world) + sum(len(b) for _, b in mw.groups) + sum(len(b) for _, b in mw.entities)
    print("wrote %s: %d brushes, %d entities (%.1f s); islets at y %.0f and %.0f" % (
        OUT, nb, len(mw.entities), time.time() - t0, ISLETS[0][1], ISLETS[1][1]))


def main():
    ap = argparse.ArgumentParser(
        description="Writes quakevr/maps/vrtrailer.map; with --compile also its .bsp, .lit and .lux (vrstart's "
                    "compile: qbsp 0.18.1, 2.0's vis and light, the presets fast and final).")
    ap.add_argument("--compile", action="store_true", help="also build the .bsp, .lit and .lux")
    ap.add_argument("--preset", choices=sorted(vs.PRESETS), default="final", help="the compile preset (default: final)")
    ap.add_argument("--check", type=int, default=0, metavar="RAYS", help="after compiling, the hole test with RAYS rays")
    ap.add_argument("--tools", default=vs.DEFAULT_TOOLS, help="ericw-tools 2.0's folder (vis, light)")
    ap.add_argument("--qbsp", default=vs.DEFAULT_QBSP, help="the qbsp.exe (0.18.1's)")
    ap.add_argument("--work", default=os.path.join(tempfile.gettempdir(), MAPNAME + "_build"))
    args = ap.parse_args()
    write_map()
    if args.compile:
        vs.MAPNAME, vs.OUT = MAPNAME, OUT  # (vrstart's compile_map, for this map)
        vs.compile_map(args.tools, args.work, args.preset, args.check, args.qbsp)


if __name__ == "__main__":
    main()
