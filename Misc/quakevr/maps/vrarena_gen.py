# vrarena_gen.py -- writes quakevr/maps/vrarena.map, the Survival Arena (the armed-humanoid survival mode's own map:
# Play > Survival Arena; the wave director is QC/vr_arena.qc, gated on the map's name), and with --compile builds it
# (qbsp, vis and light: ericw-tools 2.0, make_vrtesthall_map.py's and make_vrcalibration_map.py's profile).
#
#   python Misc/quakevr/maps/vrarena_gen.py [--compile [--preset fast|final]] [--check] [--holes RAYS]
#                                          [--tools DIR] [--work DIR]
#
# The arena (VR scale: MAPPING.md's 32.8 units = 1 m; every number below is in units). One sealed octagonal room, no
# sky, lit here (`_qvr_prelit 1`, like vrtesthall's and vrcalibration's):
#   - the open floor: an octagon whose straight sides are x, y = +-800 (49 m across) with the corners cut back 200, so
#     the corners are 1000 out: 75 m of open walking room. The walls are 64 thick, the ceiling is at 384 (11.7 m);
#   - the central dais: an octagon (sides at 232, corners 268 out -- about 250 across, the brief's ~250), 96 high, with
#     4 staircases (6 steps each: 16 high, 24 deep) onto its four cardinal faces, and the spawn pad on its top;
#   - 4 corner towers: 180-square pillars at the four cut corners, 256 high, each climbed by an outer staircase that
#     hugs one of its faces (16 steps: 16 high, 16 deep) -- the four flights are rotations of one, pinwheel-wise;
#   - the wall walk at 256: a 128-wide deck along the east and the west wall (y -576..576), joined to each tower's top
#     by a 64-wide bridge, so the high ring is reached only by way of the towers and drops 256 (7.8 m) to the floor;
#   - 8 waist-high covers (`func_detail`, 80 x 80, 48 high) at 45 deg apart at radius ~520: the ring between the dais
#     and the walls is otherwise clear, so the 8 blocks are where a fight gets broken up;
#   - 24 lights: 8 ceiling lamps (a qvr_light block each, as vrtesthall's), 4 over the towers, 4 over the walkways, 4
#     over the dais, 4 in the corners, with a soft minimum light so no corner goes black.
# 130 brushes (light on Quest 3). Brushes are kept apart: they meet face to face, exactly coplanar, none runs into
# another, and the shell is closed (qbsp sees no leak). --check verifies the points, the stair steps, that, and that
# the open ring of the floor (700..1000 out, below 160 high) holds nothing.
#
# Entities (point entities for the wave director; none is in quakevr.fgd -- no QuakeC spawn function names them yet,
# as with vrtesthall's plain info_player_start, so Misc/trenchbroom/fgdgen.py --check stays green):
#   info_player_start   (1)  0 0 120                the pad on the dais' top (the player starts unarmed)
#   info_arena_spawn   (12)  "kind" 1 = ground level, 2 = high (a drop):
#                            4 tower tops     402 402 256   and 4 walkway landings 736 +-288 256  -> kind 2
#                            4 ground corners 640 640 0                                           -> kind 1
#   info_arena_item     (3)  196 196 48, -196 196 48, 196 -196 48 (mid-height at the dais' diagonal faces)
import argparse
import math
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))   # the checkout: qbsp's -wadpath (as vrtutorial_gen.py's)
sys.path.insert(0, HERE)
import mapgeom as mg  # noqa: E402  (the hull / box / MapWriter helpers)

MAPNAME = "vrarena"
DEFAULT_TOOLS = "C:/OHWorkspace/ericw-tools-2.0.0-alpha11-win64"  # (qbsp 2.0: this map has no slivers to lose faces at)
OUT = os.path.join(ROOT, "quakevr", "maps", MAPNAME + ".map")
QBSP_ARGS = ["-bsp2"]
PRESETS = {
    "fast": dict(vis=["-fast"], light=["-lit", "-lux", "-lightgrid", "-lightgrid_dist", "128", "128", "128"]),
    "final": dict(vis=[], light=["-extra4", "-dirt", "-dirtscale", "1.5", "-dirtdepth", "96", "-bounce", "-lit",
                                 "-lux", "-lightgrid", "-lightgrid_dist", "64", "64", "64"]),
}

FLOOR, WALL, CEIL, PANEL, TRIM, LAMP = "qvr_floor", "qvr_wall", "qvr_ceiling", "qvr_panel", "qvr_trim", "qvr_light"

H, CUT, TH = 800, 200, 64                    # the octagon: sides at +-800, corners cut 200 (1000 out); walls 64 thick
XOUT, ZH, ZTOP, ZF = H + TH, 384, 448, 0     # the shell: walls 0..384, the floor slab down to -64, the ceiling to 448
DAIS_A, DAIS_C, DAIS_TOP = 232, 96, 96       # the dais: sides at 232, corners 268 out, its top at 96
TOW_A, TOW_B, WALK_TOP = 312, 492, 256       # the towers: 312..492 in x and y, 256 high; the wall walk at 256
DECK_X0, DECK_Y = 672, 576                   # the decks: x 672..800, y -576..576, z 192..256 (the bridges 370..434)
COVER_R, COVER_HALF, COVER_TOP = 520, 40, 48  # the covers: 80 x 80 blocks, 48 high, at radius ~520
LIGHT_MIN = "90"                             # the worldspawn's minimum light (soft: the room seen everywhere)

SPAWNS, ITEMS, STEPS = [], [], []   # filled by geometry(): the spawn points, the item points, every stair step


def octagon(h, cut):
    """The octagon whose straight sides are x = +-h and y = +-h with each corner cut back by `cut` (counter-clockwise
    from above, as make_vrcalibration_map.py's OCT)."""
    return [(h - cut, -h), (h, -(h - cut)), (h, h - cut), (h - cut, h),
            (-(h - cut), h), (-h, h - cut), (-h, -(h - cut)), (-(h - cut), -h)]


def inset(poly, d):
    """The polygon moved in by d (out for d < 0), each side parallel to its own, the corners mitred
    (make_vrcalibration_map.py's inset)."""
    out, k = [], len(poly)
    for i in range(k):
        a, b, c = poly[i - 1], poly[i], poly[(i + 1) % k]
        lines = []
        for p, q in ((a, b), (b, c)):
            t = mg.norm((q[0] - p[0], q[1] - p[1], 0.0))
            lines.append(((p[0] - t[1] * d, p[1] + t[0] * d), t))
        (p1, t1), (p2, t2) = lines
        den = t1[0] * t2[1] - t1[1] * t2[0]
        s = ((p2[0] - p1[0]) * t2[1] - (p2[1] - p1[1]) * t2[0]) / den
        out.append((p1[0] + t1[0] * s, p1[1] + t1[1] * s))
    return out


def texf(top, side, bottom=None):
    """A TexFn for mapgeom (it wants a face's texture as a (name, U, V, uoff, voff, scale, scale) tuple): `top` on the
    upward face, `bottom` on the downward one (by default `side` too), `side` on the rest. Tex's world-aligned default
    axes are kept, so coplanar faces of neighbouring brushes continue into each other."""
    tex = {n: mg.Tex(n) for n in (top, side, bottom) if n}
    def f(n, c):
        name = top if n[2] > 0.99 else ((bottom or side) if n[2] < -0.99 else side)
        return tex[name].spec(n, c)
    return f


def prism(poly, zb, zt, top, side):
    """The prism over the polygon `poly`: mapgeom.hull of its rings at zb and zt (the points are snapped, so the faces
    come out exactly as meant, mapgeom.precision). (mapgeom.prism is terrain's own: its sides are only 64 tall.)"""
    return mg.hull([(x, y, z) for z in (zt, zb) for x, y in poly], texf(top, side))


def flight(dst, what, x0, y0, dx, dy, wide, steps, rise, tread):
    """A straight staircase, solid from the floor up: the lowest step begins at (x0, y0) and the flight climbs along
    (dx, dy) (one of the four axis directions), `wide` across, every step `rise` high and `tread` deep (MAPPING.md: at
    most 16 high, at least 16 deep). Each step is recorded for --check."""
    for k in range(steps):
        if dx:
            a, b = (x0 + tread * k, x0 + tread * (k + 1)) if dx > 0 else (x0 - tread * (k + 1), x0 - tread * k)
            dst.append(mg.box(a, y0, ZF, b, y0 + wide, rise * (k + 1), texf(FLOOR, WALL)))
        else:
            a, b = (y0 + tread * k, y0 + tread * (k + 1)) if dy > 0 else (y0 - tread * (k + 1), y0 - tread * k)
            dst.append(mg.box(x0, a, ZF, x0 + wide, b, rise * (k + 1), texf(FLOOR, WALL)))
        STEPS.append((what, rise, tread))


def geometry(mw):
    """Builds the arena into `mw` (a mapgeom.MapWriter) and returns (its point entities, its feature brushes -- the
    dais, the towers, the stairs, the wall walk, the covers -- the shell being everything else)."""
    shell, lamps, dais, towers, walk, covers = [], [], [], [], [], []
    oct8, outer = octagon(H, CUT), inset(octagon(H, CUT), -TH)

    # ---- the shell: the floor slab, one wall brush behind each of the octagon's 8 sides, the 4 wedges that close its
    # cut corners against the shell's own corners, and the ceiling
    shell.append(mg.box(-XOUT, -XOUT, ZF - TH, XOUT, XOUT, ZF, texf(FLOOR, WALL)))
    shell.append(mg.box(-XOUT, -XOUT, ZH, XOUT, XOUT, ZTOP, texf(CEIL, WALL)))
    for i in range(8):
        j = (i + 1) % 8
        shell.append(prism([oct8[i], oct8[j], outer[j], outer[i]], ZF, ZH, WALL, WALL))
    for i in range(0, 8, 2):
        far = (XOUT if outer[i][0] > 0 else -XOUT, XOUT if outer[i + 1][1] > 0 else -XOUT)
        shell.append(prism([outer[i], outer[i + 1], far], ZF, ZH, WALL, WALL))

    # ---- the dais, its spawn pad (vrtesthall's own: 2 high) and its 4 staircases, one onto each cardinal face
    dais.append(prism(octagon(DAIS_A, DAIS_C), ZF, DAIS_TOP, PANEL, WALL))
    dais.append(mg.box(-48, -48, DAIS_TOP, 48, 48, DAIS_TOP + 2, texf(PANEL, TRIM)))
    flight(dais, "dais stair (north)", -48, 376, 0, -1, 96, 6, 16, 24)
    flight(dais, "dais stair (south)", -48, -376, 0, 1, 96, 6, 16, 24)
    flight(dais, "dais stair (east)", 376, -48, -1, 0, 96, 6, 16, 24)
    flight(dais, "dais stair (west)", -376, -48, 1, 0, 96, 6, 16, 24)

    # ---- the 4 corner towers and their pinwheel staircases: each flight hugs the face its neighbour's gives
    for sx, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1)):
        towers.append(mg.box(sx * TOW_A, sy * TOW_A, ZF, sx * TOW_B, sy * TOW_B, WALK_TOP, texf(PANEL, WALL)))
    flight(towers, "tower stair (north-east)", 248, 236, 0, 1, 64, 16, 16, 16)     # up the north-east tower's west face
    flight(towers, "tower stair (north-west)", -236, 248, -1, 0, 64, 16, 16, 16)   # up the north-west tower's south face
    flight(towers, "tower stair (south-west)", -312, -236, 0, -1, 64, 16, 16, 16)  # up the south-west tower's east face
    flight(towers, "tower stair (south-east)", 236, -312, 1, 0, 64, 16, 16, 16)    # up the south-east tower's north face

    # ---- the wall walk at 256: 2 decks along the east and the west wall, and a bridge from each to the two tower
    # tops it passes (so the high ring is reached only by way of the towers, and drops 256 to the floor)
    for s in (1, -1):
        walk.append(mg.box(s * H, -DECK_Y, WALK_TOP - 64, s * DECK_X0, DECK_Y, WALK_TOP, texf(PANEL, WALL)))
    for sx, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1)):
        walk.append(mg.box(sx * TOW_B, sy * 370, WALK_TOP - 64, sx * DECK_X0, sy * 434, WALK_TOP, texf(PANEL, WALL)))

    # ---- the 8 covers, 45 deg apart at radius ~520, each its own func_detail
    for i in range(8):
        a = math.radians(22.5 + 45 * i)
        cx, cy = (round(COVER_R * math.cos(a) / 8) * 8, round(COVER_R * math.sin(a) / 8) * 8)
        covers.append(mg.box(cx - COVER_HALF, cy - COVER_HALF, ZF, cx + COVER_HALF, cy + COVER_HALF, COVER_TOP,
                             texf(TRIM, PANEL)))
        mw.detail("cover %d" % (i + 1)).append(covers[-1])

    # ---- the lamps (vrtesthall's pattern: a glowing block at the ceiling, its light entity under it)
    lights = []
    for i in range(8):
        a = math.radians(22.5 + 45 * i)
        x, y = (round(650 * math.cos(a) / 8) * 8, round(650 * math.sin(a) / 8) * 8)
        lamps.append(mg.box(x - 32, y - 32, ZH - 4, x + 32, y + 32, ZH, LAMP))
        lights.append(((x, y, ZH - 44), "300", "0.47"))
    for sx, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1)):
        lights.append(((sx * 402, sy * 402, 320), "180", "0.6"))      # over each tower's top
        lights.append(((sx * 736, sy * 288, 320), "160", "0.6"))      # over each walkway landing
        lights.append(((sx * 640, sy * 640, 140), "150", "0.66"))     # in each corner of the floor
    for sx, sy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        lights.append(((sx * 180, sy * 180, 140), "140", "0.66"))     # over the dais, one facing each staircase

    mw.world.extend(shell + lamps)
    mw.group("dais").extend(dais)
    mw.group("towers").extend(towers)
    mw.group("walkways").extend(walk)

    # ---- the points: the player, the 12 spawn points (4 ground, 8 high), the 3 item points, the 24 lights
    SPAWNS[:] = ([(sx * 402, sy * 402, WALK_TOP, 2) for sx, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1))]
                 + [(sx * 736, sy * 288, WALK_TOP, 2) for sx, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1))]
                 + [(sx * 640, sy * 640, ZF, 1) for sx, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1))])
    ITEMS[:] = [(196, 196, 48), (-196, 196, 48), (196, -196, 48)]
    ents = [{"classname": "info_player_start", "origin": "0 0 120", "angle": "90"}]
    ents += [{"classname": "info_arena_spawn", "origin": "%d %d %d" % p[:3], "kind": str(p[3])} for p in SPAWNS]
    ents += [{"classname": "info_arena_item", "origin": "%d %d %d" % p} for p in ITEMS]
    ents += [{"classname": "light", "origin": "%d %d %d" % pos, "light": val, "wait": atten,
              "_color": "1 0.96 0.9"} for pos, val, atten in lights]
    return ents, dais + towers + walk + covers


def all_brushes(mw):
    """Every brush in the map: the worldspawn's, the groups' and the brush entities'."""
    out = [b for b in mw.world if b is not None]
    for _, brushes in mw.groups:
        out += [b for b in brushes if b is not None]
    for _, brushes in mw.entities:
        out += [b for b in brushes if b is not None]
    return out


def write(path):
    mw = mg.MapWriter()
    ents, feat = geometry(mw)
    keys = {"classname": "worldspawn", "mapversion": "220", "wad": "quakevr/wads/quakevr_dev.wad",
            "_tb_mod": "hipnotic;rogue;quakevr", "mapname": MAPNAME, "message": "Quake VR Survival Arena",
            "worldtype": "2", "light": LIGHT_MIN, "_vr_debris": "0", "_qvr_prelit": "1"}
    header = ("// %s.map -- generated by Misc/quakevr/maps/%s_gen.py: regenerate it, do not edit it by hand.\n"
              "// That file's header describes the arena and lists the info_arena_spawn / info_arena_item points the\n"
              "// wave director (QC/vr_arena.qc) reads.\n" % (MAPNAME, MAPNAME))
    for e in ents:
        mw.add(e)          # (a point the writer never sees is a point the map does not have: see check_written)
    mw.write(path, keys, header)
    return mw, ents, feat


# ---- the checks (--check): the points, the stair steps, the brushes, the open ring
def grid_points(b):
    """The brush's corner points, as a sorted tuple of integers in 1/SNAP units (mapgeom writes its points snapped);
    a brush's points identify it, so the same tuple twice means the same brush twice."""
    return tuple(sorted(set((int(round(f[i][0] * mg.SNAP)), int(round(f[i][1] * mg.SNAP)),
                             int(round(f[i][2] * mg.SNAP))) for f in b.faces for i in range(3))))


def normals(b):
    """Each face's integer normal, in 1/SNAP units (the directions the separating-axis test needs)."""
    out = []
    for p0, p1, p2, _ in b.faces:
        q = [tuple(int(round(v * mg.SNAP)) for v in p) for p in (p0, p1, p2)]
        n = mg.cross(mg.sub(q[1], q[0]), mg.sub(q[2], q[0]))
        g = math.gcd(math.gcd(abs(n[0]), abs(n[1])), abs(n[2]))
        if g:
            n = (n[0] // g, n[1] // g, n[2] // g)
        if n not in out:
            out.append(n)
    return out


def intrudes(na, pa, nb, pb):
    """Do these two brushes run into each other? `pa`, `pb` are their corner points and `na`, `nb` their faces'
    normals: the separating-axis test over those normals. Meeting face to face -- the planes matching exactly, as
    MAPPING.md wants brushes to meet -- does not count."""
    for i in range(3):
        if max(p[i] for p in pa) <= min(p[i] for p in pb) or max(p[i] for p in pb) <= min(p[i] for p in pa):
            return False
    for n in na + nb:
        sa = [mg.dot(n, p) for p in pa]
        sb = [mg.dot(n, p) for p in pb]
        if max(sa) <= min(sb) or max(sb) <= min(sa):
            return False
    return True


def inside(b, p):
    """Is a point inside convex brush `b`? The average of its corners is inside, so the point must lie on the same
    side of every face as that point -- exactly on a face counts as inside (a spawn standing on a floor is on it)."""
    cp = sorted(set(f[i] for f in b.faces for i in range(3)))
    cen = tuple(sum(q[k] for q in cp) / len(cp) for k in range(3))
    for f in b.faces:
        n = mg.cross(mg.sub(f[1], f[0]), mg.sub(f[2], f[0]))
        if mg.dot(n, mg.sub(p, f[0])) * mg.dot(n, mg.sub(cen, f[0])) < 0:
            return False
    return True


def surfaces(brushes, p):
    """The heights of the solid surfaces straight below a point, topmost first (scanned down in 8-unit steps): the
    first is what a spawn there stands on, the next what a fall from it lands on."""
    out, d, was = [], 0, False
    while d <= 700:
        hit = any(inside(b, (p[0], p[1], p[2] - d)) for b in brushes)
        if hit and not was:
            out.append(p[2] - d)
        was = hit
        d += 8
    return out


def check(brushes, feat, ents):
    """The map's own checks: the point counts, every stair step, no duplicate brush, no brush running into another,
    and the open ring of the floor clear of the features. Returns the number of problems."""
    bad = 0
    n = {}
    for e in ents:
        n[e["classname"]] = n.get(e["classname"], 0) + 1
    for name, want in (("info_player_start", 1), ("info_arena_spawn", 12), ("info_arena_item", 3)):
        if n.get(name, 0) != want:
            print("  BAD %s: %d (want %d)" % (name, n.get(name, 0), want))
            bad += 1
    high = sum(1 for e in ents if e["classname"] == "info_arena_spawn" and e.get("kind") == "2")
    print("  spawn points: %d high (kind 2), %d ground (kind 1)" % (high, 12 - high))
    # every spawn stands on a floor (or on the 24 it may sit above it), and how far a fall from it is: the ground ones
    # have none, the high ones a real drop -- the numbers the wave director needs to keep both kinds safe
    buried, hangs, drops = 0, 0, {}
    for e in ents:
        cn = e["classname"]
        if cn not in ("info_arena_spawn", "info_player_start", "info_arena_item"):
            continue
        p = tuple(int(v) for v in e["origin"].split())
        if cn == "info_arena_item":
            buried += sum(1 for b in brushes if inside(b, p))
            continue
        s = surfaces(brushes, p)
        if not s or p[2] - s[0] > 24:      # a spawn sits on its floor or up to 24 above it (the house's entity height)
            hangs += 1
            continue
        drops.setdefault(e.get("kind", "start"), []).append(s[0] - (s[1] if len(s) > 1 else 0))
    for k, what in (("1", "ground spawns"), ("2", "high spawns"), ("start", "player start")):
        got = sorted(drops.get(k, []))
        print("  how far one falls from the %s: %s" % (what, ", ".join("%d" % v for v in got)))
        if k == "1":
            bad += sum(1 for v in got if v > 16)
        elif k == "2":
            bad += sum(1 for v in got if v < 128)
    if hangs or buried:
        print("  BAD: %d spawn point(s) not standing on a floor, %d item point(s) inside solid" % (hangs, buried))
        bad += hangs + buried
    steps_bad = sum(1 for w, r, d in STEPS if r > 16 or d < 16)
    print("  stair steps: %d, %d not climbable (each step's rise over the previous tread, and its depth)"
          % (len(STEPS), steps_bad))
    bad += steps_bad
    seen, dup = set(), 0
    pts = [grid_points(b) for b in brushes]
    nrm = [normals(b) for b in brushes]
    for p in pts:
        if p in seen:
            dup += 1
        seen.add(p)
    pairs = 0
    for i in range(len(brushes)):
        for j in range(i + 1, len(brushes)):
            if intrudes(nrm[i], pts[i], nrm[j], pts[j]):
                pairs += 1
    # the open ring: from 700 to 1000 out, below 160 high, no feature stands in the way (the walls are the room's own
    # shell, not an obstacle in it, and the wall walk starts above the ring's height)
    ring = 0
    for b in feat:
        if any(700 < math.hypot(q[0], q[1]) / mg.SNAP < 1000 and q[2] / mg.SNAP <= 160 for q in grid_points(b)):
            ring += 1
    print("  brushes: %d; duplicates: %d; brushes running into another: %d; features standing in the open ring: %d"
          % (len(brushes), dup, pairs, ring))
    return bad + dup + pairs + ring


def written_points(path):
    """What the map FILE holds, classname -> count. The points geometry() built are not the map until the writer has
    them, so --check compares the two: a spawn point that never reached the writer is a spawn point that does not
    exist, and the wave director would fall back to its ring without the map noticing."""
    counts = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = re.match(r'\s*"classname"\s+"([^"]+)"\s*$', line)
            if m:
                counts[m.group(1)] = counts.get(m.group(1), 0) + 1
    return counts


def compile_map(tools, work, preset, holes=0):
    """qbsp, vis and light in `work` (their .prt and the copies stay there), and the .bsp, .lit and .lux copied next
    to the .map. Prints each stage's time and its warnings; a warning line or a non-zero exit fails (MAPPING.md)."""
    os.makedirs(work, exist_ok=True)
    src = os.path.join(work, MAPNAME + ".map")
    shutil.copyfile(OUT, src)
    bsp = os.path.join(work, MAPNAME + ".bsp")
    pr = PRESETS[preset]
    cmds = [[os.path.join(tools, "qbsp.exe"), "-nolog", "-nopercent"] + QBSP_ARGS + ["-wadpath", ROOT, src, bsp],
            [os.path.join(tools, "vis.exe"), "-nolog", "-nopercent"] + pr["vis"] + [bsp],
            [os.path.join(tools, "light.exe"), "-nolog", "-nopercent"] + pr["light"] + [bsp]]
    total = time.time()
    for cmd in cmds:
        t0 = time.time()
        r = subprocess.run(cmd, capture_output=True, text=True)
        out = (r.stdout or "") + (r.stderr or "")
        print("%s: %.1f s%s" % (os.path.basename(cmd[0]), time.time() - t0,
                                "" if r.returncode == 0 else ", exit %d" % r.returncode))
        for line in out.splitlines():
            low = line.lower()
            if "warning" in low or "error" in low or "leak" in low or "could not" in low or "not found" in low:
                print("  " + line.strip())
        if r.returncode:
            print(out[-2000:])
            sys.exit(1)
    print("compiled (%s preset) in %.0f s" % (preset, time.time() - total))
    if holes:
        r = subprocess.run([sys.executable, os.path.join(HERE, "bsp_holes.py"), bsp, "--rays", str(holes),
                            "--show", "10"], capture_output=True, text=True)
        print((r.stdout or "") + (r.stderr or ""))
        if r.returncode:
            sys.exit(1)
    for ext in (".bsp", ".lit", ".lux"):
        shutil.copyfile(os.path.join(work, MAPNAME + ext), os.path.join(os.path.dirname(OUT), MAPNAME + ext))


def main():
    ap = argparse.ArgumentParser(description="Writes quakevr/maps/%s.map, the Survival Arena; with --compile also its "
                                             ".bsp, .lit and .lux (ericw-tools: qbsp -bsp2, vis, light)." % MAPNAME)
    ap.add_argument("--compile", action="store_true", help="also build the .bsp, .lit and .lux")
    ap.add_argument("--preset", choices=sorted(PRESETS), default="final", help="which compile: fast (vis -fast, light "
                        "-lit -lux, a 128-unit light grid) while blocking the map out, final (MAPPING.md's full light "
                        "profile) for what gets played")
    ap.add_argument("--check", action="store_true", help="run the map's own checks (the points, the stair steps, the "
                        "brushes, the open ring)")
    ap.add_argument("--holes", type=int, default=0, metavar="RAYS", help="after compiling, the hole test with RAYS "
                        "rays (Misc/quakevr/maps/bsp_holes.py)")
    ap.add_argument("--tools", default=DEFAULT_TOOLS)
    ap.add_argument("--work", default=os.path.join(ROOT, "scratch", MAPNAME + "_build"),
                    help="the folder the compile runs in")
    args = ap.parse_args()
    mw, ents, feat = write(OUT)
    brushes = all_brushes(mw)
    print("wrote %s: %d brushes, %d entities (%d info_arena_spawn, %d info_arena_item, %d light)"
          % (OUT, len(brushes), len(ents), len(SPAWNS), len(ITEMS), sum(1 for e in ents if e["classname"] == "light")))
    bad = 0
    # the points as they stand IN THE FILE, read back off the disk: that is what the wave director will find
    want, got = {}, written_points(OUT)
    for e in ents:
        want[e["classname"]] = want.get(e["classname"], 0) + 1
    for cn in sorted(want):
        if got.get(cn, 0) != want[cn]:
            print("  BAD %s: the file has %d, the generator built %d" % (cn, got.get(cn, 0), want[cn]))
            bad += 1
    print("the file holds: %s" % ", ".join("%s %d" % (cn, got[cn]) for cn in sorted(got)))
    if args.check:
        bad += check(brushes, feat, ents)
        print("arena_map: %d spawns, %d items, %d bad steps"
              % (len(SPAWNS), len(ITEMS), sum(1 for w, r, d in STEPS if r > 16 or d < 16)))
        print("arena_check: %s" % ("done" if bad == 0 else "%d problems" % bad))
    if args.compile:
        if bad:
            print("not compiled: the map's own checks come first")
            sys.exit(1)
        compile_map(args.tools, args.work, args.preset, args.holes)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
