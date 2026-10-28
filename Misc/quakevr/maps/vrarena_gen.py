# vrarena_gen.py -- writes quakevr/maps/vrarena.map, the Survival Arena (the armed-humanoid survival mode's own map:
# Play > Survival Arena; the wave director is QC/vr_arena.qc, gated on the map's name), and with --compile builds it
# (qbsp, vis and light: ericw-tools 2.0, make_vrtesthall_map.py's and make_vrcalibration_map.py's profile).
#
#   python Misc/quakevr/maps/vrarena_gen.py [--compile [--preset fast|final]] [--check] [--holes RAYS]
#                                          [--tools DIR] [--work DIR] [--dev-textures]
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
#   - 8 waist-high covers (`func_detail`, 96 x 96, 48 high) at 45 deg apart at radius ~512: the ring between the dais
#     and the walls is otherwise clear, so the 8 blocks are where a fight gets broken up. Each is two id crates long
#     and one high, so the crate's own picture fits its faces instead of being cut across;
#   - the room dressed the way id dresses a room (the author's ask, 2026-10-28): skirting, wainscot, rail and cornice
#     bands mitred round the octagon, an upright carrying a lamp at the third and the two-thirds mark of each cardinal
#     wall and one at the middle of each cut corner, a border moulding where the ceiling meets the walls, and a coffer
#     over each of the 8 ceiling lights: a frame, a ring of glowing tiles under it, a lit panel in its recess;
#   - 24 lights: 8 under the coffers, 4 over the towers, 4 over the walkways, 4 over the dais, 4 in the corners, with
#     a soft minimum light so no corner goes black.
# 230 brushes (light on Quest 3). Brushes are kept apart: they meet face to face, exactly coplanar, none runs into
# another, and the shell is closed (qbsp sees no leak). --check verifies the points, the stair steps, that, and that
# the open ring of the floor (700..1000 out, below 160 high) holds nothing.
# The textures are id's own base set -- e1m1's and the base maps around it (the author's ask) -- read from
# quakevr/wads/id_textures.wad, which Misc/trenchbroom/make_id_wad.py makes from your own paks and the repository
# never sees; like vrcalibration's and vrstart's, the compiled vrarena.bsp carries them (MAPPING.md, "Textures and id's
# data"). --dev-textures builds the same arena with quakevr_dev.wad's flat grids instead. Nothing a fight uses moved:
# the points, the steps, the 48-high covers and the shell are as they were (the covers grew inwards, so their
# clearance to the staircases is the same), and the new brushes are the room's own trim: 4 to 16 units out of a wall.
#
# Entities (point entities for the wave director; none is in quakevr.fgd -- no QuakeC spawn function names them yet,
# as with vrtesthall's plain info_player_start, so Misc/trenchbroom/fgdgen.py --check stays green):
#   info_player_start   (1)  0 0 120                8 above the 16-high pad on the dais' top (its top face is at 112)
#   info_arena_spawn   (12)  "kind" 1 = ground level, 2 = high (a drop):
#                            4 tower tops     402 402 256   and 4 walkway landings 736 +-288 256  -> kind 2
#                            4 ground corners 640 640 0                                           -> kind 1
#   info_arena_item     (3)  196 196 48, -196 196 48, 196 -196 48 (mid-height at the dais' diagonal faces)
import argparse
import math
import os
import re
import shutil
import struct
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

# The palette: role -> (texture name, how it is laid, how many tiles, at what scale, from what anchor). The names are
# id's (quakevr/wads/id_textures.wad; `--list` there prints them with their sizes and the map each came from). `laid`
# (below) does the laying, three ways:
#   world -- mapgeom's own paraxial projection, the texture's corner at `anchor`: coplanar faces continue into each
#          other, which is what a floor and a ceiling want; `anchor` is where a tile corner is asked to land, so a
#          field of panels can butt exactly with the mouldings that bound it;
#   fitv  -- `reps`[1] tiles exactly down the face and the same scale across it, tiles still on the world grid: id's
#          way of making a band of plates or of trim land where the band is (make_vrcalibration_map.py's fitv);
#   fit   -- `reps` tiles across and `reps`[1] down, exactly (`0`: as many as come nearest at `scale`): for a face
#          that is one thing -- a crate, a lamp, the lit panel at the bottom of a coffer.
# `reps` may be fractional (1.5 tiles) and `scale` multiplies the texture: either lets a face land on whole tiles
# without squashing the picture, which is what the crate faces, the dais' fluting and a 32-high coffer frame need.
IDTEX = dict(floor=("ground1_2", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),      # 64: id's base floor plates: 49 m of them
             wall=("tech14_1", "world", (0, 1), 1.0, (0.0, 0.0, 96.0)),       # 128: the walls' panels, butted with the
                                                                              # rail at 96 and the cornice at 352
             tower=("tech14_1", "fitv", (0, 2), 1.0, (0.0, 0.0, 0.0)),        # 128: exactly two panels up a 256 tower
             wainscot=("metal4_4", "fitv", (0, 1), 1.0, (0.0, 0.0, 0.0)),     # 64: one plate exactly in the 64-high band
             strip=("tech04_1", "fitv", (0, 1), 1.0, (0.0, 0.0, 0.0)),        # 128 x 16: skirting, rail, cornice, frame
             pilaster=("tech04_3", "fit", (1, 0), 1.0, (0.0, 0.0, 0.0)),      # 16 x 128: one flute across an upright exactly
                                                                            # and as many down it as it is tall, its U being
                                                                            # the wall's own direction (geometry())
             dais=("tech04_3", "fit", (0, 1), 1.0, (0.0, 0.0, 0.0)),          # 16 x 128 at 0.75 down the dais' 96: whole
                                                                            # flutes across a 272-wide face and across the
                                                                            # 96 a cut corner measures
             ceiling=("metal1_1", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),     # 64: the ceiling's plates, not the walls'
             recess=("metal1_1", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),      # 64: the lining inside a coffer, a hood
             frame=("tech04_1", "fitv", (0, 2), 1.0, (0.0, 0.0, 0.0)),        # 128 x 16: a 32-high coffer frame
             ring=("ceil1_1", "fit", (0, 0), 1.0, (0.0, 0.0, 0.0)),           # 16, and it glows: the ring's tiles
             lit=("light3_3", "fit", (0, 0), 1.0, (0.0, 0.0, 0.0)),           # 64: 2 x 2 lit panels in a coffer's back
             lamp=("tlight01", "fit", (1, 1), 1.0, (0.0, 0.0, 0.0)),          # 32, and it glows: one lamp per upright
             deck=("sfloor4_2", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),       # 64: the dais' top, the tower tops, decks
             deck_side=("metal4_4", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),   # 64: one plate spans a 64-thick deck
             stair=("metal1_1", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),       # 64: every step, the dais' six, the towers' sixteen
             cover=("crate0_side", "fit", (2, 1), 1.0, (0.0, 0.0, 0.0)),      # 64 at 0.75: 2 crates along 96, 1 up 48
             cover_top=("crate0_top", "fit", (2, 2), 1.0, (0.0, 0.0, 0.0)),   # 64 at 0.75: 2 x 2 tops on a 96 square
             pad=("plat_top1", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),        # 64: 2 plates across the 128-wide pad
             pad_side=("plat_side1", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)))  # 64 x 16: 1 up the 16-high lip, 2 across
DEVTEX = dict(floor=("qvr_floor", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),
              wall=("qvr_wall", "world", (0, 1), 1.0, (0.0, 0.0, 96.0)),
              tower=("qvr_panel", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),
              wainscot=("qvr_wall", "world", (0, 1), 1.0, (0.0, 0.0, 16.0)),
              strip=("qvr_trim", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),
              pilaster=("qvr_trim", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),
              dais=("qvr_panel", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),
              ceiling=("qvr_ceiling", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),
              recess=("qvr_ceiling", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),
              frame=("qvr_trim", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),
              ring=("qvr_light", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),
              lit=("qvr_light", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),
              lamp=("qvr_lantern", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),
              deck=("qvr_floor", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),
              deck_side=("qvr_wall", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),
              stair=("qvr_floor", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),
              cover=("qvr_panel", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),
              cover_top=("qvr_panel", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),
              pad=("qvrc_pad", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)),
              pad_side=("qvr_trim", "world", (0, 1), 1.0, (0.0, 0.0, 0.0)))
TEX = IDTEX                        # --dev-textures swaps the palette for DEVTEX (and the WAD list for the one WAD)
WADS = ["quakevr/wads/id_textures.wad", "quakevr/wads/quakevr_dev.wad"]


def texture_sizes():
    """The width and height of every texture in the WADs this map names. mapgeom lays a face without knowing them;
    `laid` needs them to fit a face to a whole number of tiles. Later WADs win on a name, and a WAD that is not there
    is skipped -- main() says what to do about that."""
    out = {}
    for wad in WADS:
        path = os.path.join(ROOT, wad)
        if not os.path.exists(path):
            continue
        with open(path, "rb") as f:
            d = f.read()
        n, ofs = struct.unpack("<ii", d[4:12])
        for i in range(n):
            pos = struct.unpack("<i", d[ofs + 32 * i: ofs + 32 * i + 4])[0]
            name = d[ofs + 32 * i + 16: ofs + 32 * i + 32].split(b"\0")[0].decode("latin1")
            out[name.lower()] = struct.unpack("<II", d[pos + 16:pos + 24])
    return out


TEXSIZE = texture_sizes()


def fit(extent, size, reps, scale):
    """The scale that puts `reps` tiles of a texture `size` wide exactly along `extent` (0 `reps`: as many tiles as
    come nearest at `scale`) -- make_vrcalibration_map.py's fitted()."""
    reps = reps or max(1, round(extent / (size * scale)))
    return extent / (size * float(reps))


def axes(n, rot=False):
    """The face's U and V: mapgeom's own paraxial choice, so a face laid by `laid` and one laid by a Tex agree."""
    ax, ay, az = abs(n[0]), abs(n[1]), abs(n[2])
    if az >= ax and az >= ay:
        u, v = (1.0, 0.0, 0.0), (0.0, -1.0, 0.0)
    elif ax >= ay:
        u, v = (0.0, 1.0, 0.0), (0.0, 0.0, -1.0)
    else:
        u, v = (1.0, 0.0, 0.0), (0.0, 0.0, -1.0)
    if rot:
        u, v = v, (-u[0], -u[1], -u[2])
    return u, v


def face_points(points, n):
    """The corners that lie on the face of the brush `points` builds which faces along `n`. A TexFn is handed only the
    normal and the centre, but a fit needs that face's own extent -- the mitred end of a band is longer than the band
    is deep, and a 1200-long wall is not a 64-thick box."""
    k = [mg.dot(n, p) for p in points]
    top = max(k)
    on = [p for p, d in zip(points, k) if d >= top - 1e-4]
    return on if len(on) >= 3 else list(points)


def laid(points, name, mode="world", reps=(0, 1), scale=1.0, anchor=(0.0, 0.0, 0.0), rot=False, u_axis=None):
    """A TexFn for mapgeom: `name` laid on the faces of the brush built from `points`, the way the palette asks.
    mapgeom wants (name, U, V, u offset, v offset, u scale, v scale). `world` keeps mapgeom's scales and puts a tile
    corner at `anchor`, so a field of plates continues across brushes and butts with the mouldings around it. `fit`
    pins the tiles to the face's own four edges; `fitu` and `fitv` do it along one axis only -- and `fitv` at one
    scale both ways, so a moulding or a pilaster is exact at its own top and bottom without being stretched.
    `u_axis` asks for that as the face's U (a lamp on a slanted upright: measured along the wall, not across it)."""
    def f(n, c):
        if u_axis is not None and abs(mg.dot(n, u_axis)) < 1e-6:
            u = u_axis
            v = mg.cross(u, n)                       # (U, V, N) the same way round as mapgeom's own axes
        else:
            u, v = axes(n, rot)
        tw, th = TEXSIZE[name.lower()]
        fc = face_points(points, n)
        us = [mg.dot(u, p) for p in fc]
        vs = [mg.dot(v, p) for p in fc]
        if mode == "world":
            su = sv = float(scale)
            ou, ov = -mg.dot(anchor, u) / su, -mg.dot(anchor, v) / sv
        else:
            su = sv = float(scale)                  # fitu leaves the other axis at that scale
            if mode != "fitv":
                su = fit(max(us) - min(us), tw, reps[0], scale)
            if mode != "fitu":
                sv = fit(max(vs) - min(vs), th, reps[1], scale)
            if mode == "fitv":
                su = sv
            ou, ov = -min(us) / su, -min(vs) / sv
        return (name, u, v, ou % tw, ov % th, su, sv)
    return f


def bind(spec, points):
    """A palette role -- 'wall', or ('frame', dict(reps=(0, 1))) for one use that wants other tiles -- as a TexFn
    bound to the corners of the brush it is laid on. A mapgeom Tex or a TexFn passes through as it was."""
    if isinstance(spec, str):
        return laid(points, *TEX[spec])
    if isinstance(spec, tuple) and isinstance(spec[0], str):
        name, mode, reps, scale, anchor = TEX[spec[0]]
        over = spec[1]
        return laid(points, name, over.get("mode", mode), over.get("reps", reps), over.get("scale", scale),
                    over.get("anchor", anchor), over.get("rot", False), over.get("u_axis"))
    return mg.as_texfn(spec)

H, CUT, TH = 800, 200, 64                    # the octagon: sides at +-800, corners cut 200 (1000 out); walls 64 thick
XOUT, ZH, ZTOP, ZF = H + TH, 384, 448, 0     # the shell: walls 0..384, the floor slab down to -64, the ceiling to 448
DAIS_A, DAIS_C, DAIS_TOP = 232, 96, 96       # the dais: sides at 232, corners 268 out, its top at 96
TOW_A, TOW_B, WALK_TOP = 312, 492, 256       # the towers: 312..492 in x and y, 256 high; the wall walk at 256
DECK_X0, DECK_Y = 672, 576                   # the decks: x 672..800, y -576..576, z 192..256 (the bridges 370..434)
COVER_R, COVER_HALF, COVER_TOP = 512, 48, 48  # the covers: 96 x 96 blocks, 48 high, at radius ~512 (2 crates long, 1 high)
SKIRT, WAIN, RAIL = 16, 80, 96                # the walls' bands: skirting, wainscot, rail (the panels run 96..352)
CORN = 368                                    # ... the cornice 352..368, the ceiling's border 368..384
PIL_W, PIL_D = 16, 8                          # the uprights: 16 wide, 8 out of the wall, from the rail to the cornice
PIL_TOP = CORN - 16                           # ... stopping exactly where the cornice begins (they meet face to face)
LAMP_D, LAMP_Z0, LAMP_Z1 = 8, 272, 304        # their lamps: 32 square, 8 out from the upright, at eye level on the walk
COFFER_R, COFFER_DROP = 650, 32               # the ceiling's coffers: 192 frames, hung 32 below the ceiling
LIGHT_MIN = "90"                              # the worldspawn's minimum light (soft: the room seen everywhere)

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


def roles(top, side, bottom, pts):
    """A TexFn for mapgeom (it wants a face's texture as a (name, U, V, uoff, voff, scale, scale) tuple, and is given
    only the face's normal and centre) out of the three palette roles of a brush: `top` on the upward face, `bottom` on
    the downward one (`side` again if none is given), `side` on the rest. Each is fitted to `pts`, the corners of the
    brush it belongs to, so a fitted texture is exact at that face's own edges."""
    t, s = bind(top, pts), bind(side, pts)
    b = None if bottom is None else bind(bottom, pts)
    def f(n, c):
        fn = t if n[2] > 0.99 else (b if (n[2] < -0.99 and b is not None) else s)
        return fn(n, c)
    return f


def prism(poly, zb, zt, top, side):
    """The prism over the polygon `poly`: mapgeom.hull of its rings at zb and zt (the points are snapped, so the faces
    come out exactly as meant, mapgeom.precision). (mapgeom.prism is terrain's own: its sides are only 64 tall.)"""
    pts = [(x, y, z) for z in (zt, zb) for x, y in poly]
    return mg.hull(pts, roles(top, side, None, pts))


def cuboid(x0, y0, z0, x1, y1, z1, top, side, bottom=None):
    """A box, its faces each laid for themselves: mapgeom.box with the three roles bound to the box's own corners."""
    pts = [(x, y, z) for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)]
    return mg.box(x0, y0, z0, x1, y1, z1, roles(top, side, bottom, pts))


def band(dst, poly, z0, z1, depth, role, reps=None):
    """A moulding standing `depth` out of the room's walls from z0 to z1: one prism per side of `poly`, mitred where
    two sides meet (make_vrcalibration_map.py's band). They belong to the shell: each one stands face to face on the
    wall behind it and on the band below it, and none of them reaches into another brush (--check)."""
    spec = role if reps is None else (role, dict(reps=reps))
    inner = inset(poly, depth)
    for i in range(len(poly)):
        j = (i + 1) % len(poly)
        dst.append(prism([poly[i], poly[j], inner[j], inner[i]], z0, z1, spec, spec))


def flight(dst, what, x0, y0, dx, dy, wide, steps, rise, tread):
    """A straight staircase, solid from the floor up: the lowest step begins at (x0, y0) and the flight climbs along
    (dx, dy) (one of the four axis directions), `wide` across, every step `rise` high and `tread` deep (MAPPING.md: at
    most 16 high, at least 16 deep). Each step is recorded for --check."""
    for k in range(steps):
        if dx:
            a, b = (x0 + tread * k, x0 + tread * (k + 1)) if dx > 0 else (x0 - tread * (k + 1), x0 - tread * k)
            dst.append(cuboid(a, y0, ZF, b, y0 + wide, rise * (k + 1), "stair", "stair"))
        else:
            a, b = (y0 + tread * k, y0 + tread * (k + 1)) if dy > 0 else (y0 - tread * (k + 1), y0 - tread * k)
            dst.append(cuboid(x0, a, ZF, x0 + wide, b, rise * (k + 1), "stair", "stair"))
        STEPS.append((what, rise, tread))


def geometry(mw):
    """Builds the arena into `mw` (a mapgeom.MapWriter) and returns (its point entities, its feature brushes -- the
    dais, the towers, the stairs, the wall walk, the covers -- the shell being everything else)."""
    shell, coffers, dais, towers, walk, covers = [], [], [], [], [], []
    oct8, outer = octagon(H, CUT), inset(octagon(H, CUT), -TH)

    def at(a, t, nin, s, d):
        """A point on a wall: from its corner `a`, `s` along the wall's own direction `t`, `d` inwards along `nin`."""
        return (a[0] + t[0] * s + nin[0] * d, a[1] + t[1] * s + nin[1] * d)

    # ---- the shell: the floor slab, one wall brush behind each of the octagon's 8 sides, the 4 wedges that close its
    # cut corners against the shell's own corners, and the ceiling. The walls' own faces carry id's panels, butted with
    # the rail and the cornice that stand in front of them; the wedges are inside the shell and never seen.
    shell.append(cuboid(-XOUT, -XOUT, ZF - TH, XOUT, XOUT, ZF, "floor", "wall"))
    shell.append(cuboid(-XOUT, -XOUT, ZH, XOUT, XOUT, ZTOP, "ceiling", "ceiling"))
    for i in range(8):
        j = (i + 1) % 8
        shell.append(prism([oct8[i], oct8[j], outer[j], outer[i]], ZF, ZH, "wall", "wall"))
    for i in range(0, 8, 2):
        far = (XOUT if outer[i][0] > 0 else -XOUT, XOUT if outer[i + 1][1] > 0 else -XOUT)
        shell.append(prism([outer[i], outer[i + 1], far], ZF, ZH, "wall", "wall"))

    # ---- the walls dressed in bands, mitred where two sides meet (the calibration room's idioms at the arena's
    # height): 16 of skirting on the floor, 64 of plated wainscot, 16 of rail, the wall's own panels from 96 to 352,
    # then 16 of cornice and 16 of border moulding where the ceiling meets the walls. The wall walk at 192..256 passes
    # between the rail and the cornice, and nothing here reaches into the walk or the bridges (--check).
    band(shell, oct8, ZF, SKIRT, 8, "strip")
    band(shell, oct8, SKIRT, WAIN, 4, "wainscot")
    band(shell, oct8, WAIN, RAIL, 8, "strip")
    band(shell, oct8, CORN - 16, CORN, 8, "strip")
    band(shell, oct8, CORN, ZH, 16, "strip")

    # ---- the uprights and their lamps: one at the middle of each cut corner, one at the third and one at the
    # two-thirds mark of each cardinal wall -- 16 wide, 8 out of the wall, from the rail to the cornice, each with a
    # hooded lamp (id's tlight01, one lamp exactly on its front face) at eye level for someone on the wall walk. On the
    # east and the west wall each upright is cut in two where the walk passes, so the walk keeps its 128 width.
    for i in range(8):
        a, b = oct8[i], oct8[(i + 1) % 8]
        t = mg.norm((b[0] - a[0], b[1] - a[1], 0.0))
        nin = (-t[1], t[0], 0.0)                                   # the wall's inward normal
        span = mg.length((b[0] - a[0], b[1] - a[1], 0.0))
        for frac in ((0.5,) if i % 2 == 0 else (1.0 / 3.0, 2.0 / 3.0)):
            c = (a[0] + t[0] * span * frac, a[1] + t[1] * span * frac)     # the upright, on the wall
            for z0, z1 in (((RAIL, WALK_TOP - 64), (WALK_TOP, PIL_TOP)) if i in (1, 5) else ((RAIL, PIL_TOP),)):
                q = [at(c, t, nin, -8, 0), at(c, t, nin, 8, 0), at(c, t, nin, 8, PIL_D), at(c, t, nin, -8, PIL_D)]
                flute = ("pilaster", dict(u_axis=(-t[0], -t[1], 0.0)))     # measured along the wall, as the lamp is
                shell.append(prism(q, z0, z1, "recess", flute))
            q = [at(c, t, nin, -16, PIL_D), at(c, t, nin, 16, PIL_D),
                 at(c, t, nin, 16, PIL_D + LAMP_D), at(c, t, nin, -16, PIL_D + LAMP_D)]
            pts = [(x, y, z) for z in (LAMP_Z1, LAMP_Z0) for x, y in q]
            lamp = laid(pts, *TEX["lamp"], u_axis=(-t[0], -t[1], 0.0))      # one lamp, along the wall, not across it
            shell.append(mg.hull(pts, roles("recess", "frame", "recess", pts),
                                 lambda n, c, nin=nin, lamp=lamp: lamp if mg.dot(n, nin) > 0.9 else None))

    # ---- the dais, its spawn pad (a 16-high lip of id's platform plate, 2 plates across its top) and its 4
    # staircases, one onto each cardinal face
    dais.append(prism(octagon(DAIS_A, DAIS_C), ZF, DAIS_TOP, "deck", "dais"))
    dais.append(cuboid(-64, -64, DAIS_TOP, 64, 64, DAIS_TOP + 16, "pad", "pad_side", "pad"))
    flight(dais, "dais stair (north)", -48, 376, 0, -1, 96, 6, 16, 24)
    flight(dais, "dais stair (south)", -48, -376, 0, 1, 96, 6, 16, 24)
    flight(dais, "dais stair (east)", 376, -48, -1, 0, 96, 6, 16, 24)
    flight(dais, "dais stair (west)", -376, -48, 1, 0, 96, 6, 16, 24)

    # ---- the 4 corner towers and their pinwheel staircases: each flight hugs the face its neighbour's gives
    for sx, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1)):
        towers.append(cuboid(sx * TOW_A, sy * TOW_A, ZF, sx * TOW_B, sy * TOW_B, WALK_TOP, "deck", "tower"))
    flight(towers, "tower stair (north-east)", 248, 236, 0, 1, 64, 16, 16, 16)     # up the north-east tower's west face
    flight(towers, "tower stair (north-west)", -236, 248, -1, 0, 64, 16, 16, 16)   # up the north-west tower's south face
    flight(towers, "tower stair (south-west)", -312, -236, 0, -1, 64, 16, 16, 16)  # up the south-west tower's east face
    flight(towers, "tower stair (south-east)", 236, -312, 1, 0, 64, 16, 16, 16)    # up the south-east tower's north face

    # ---- the wall walk at 256: 2 decks along the east and the west wall, and a bridge from each to the two tower
    # tops it passes (so the high ring is reached only by way of the towers, and drops 256 to the floor)
    for s in (1, -1):
        walk.append(cuboid(s * H, -DECK_Y, WALK_TOP - 64, s * DECK_X0, DECK_Y, WALK_TOP, "deck", "deck_side"))
    for sx, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1)):
        walk.append(cuboid(sx * TOW_B, sy * 370, WALK_TOP - 64, sx * DECK_X0, sy * 434, WALK_TOP, "deck", "deck_side"))

    # ---- the 8 covers, 45 deg apart at radius ~512, each its own func_detail: 96 x 96 x 48, so id's crate fits them
    for i in range(8):
        a = math.radians(22.5 + 45 * i)
        cx, cy = (round(COVER_R * math.cos(a) / 8) * 8, round(COVER_R * math.sin(a) / 8) * 8)
        covers.append(cuboid(cx - COVER_HALF, cy - COVER_HALF, ZF, cx + COVER_HALF, cy + COVER_HALF, COVER_TOP,
                             "cover_top", "cover"))
        mw.detail("cover %d" % (i + 1)).append(covers[-1])

    # ---- the ceiling's own fixtures, over each of the 8 ceiling lights: a coffer hung 32 below the ceiling -- a frame
    # 192 square with 32-wide arms of id's trim, a ring of glowing tiles under it (ceil1_1: relight_textures.cfg makes
    # it a fixture, so it gives its own light in the game) and a 16-deep recess lined with plates whose back is a lit
    # panel (light3_3), the light entity a little under the mouth of it
    lights = []
    for i in range(8):
        a = math.radians(22.5 + 45 * i)
        x, y = (round(COFFER_R * math.cos(a) / 8) * 8, round(COFFER_R * math.sin(a) / 8) * 8)
        for dx0, dy0, dx1, dy1 in ((-96, -96, 96, -64), (-96, 64, 96, 96), (-96, -64, -64, 64), (64, -64, 96, 64)):
            coffers.append(cuboid(x + dx0, y + dy0, ZH - COFFER_DROP, x + dx1, y + dy1, ZH, "recess", "frame", "ring"))
        coffers.append(cuboid(x - 64, y - 64, ZH - 16, x + 64, y + 64, ZH, "recess", ("frame", dict(reps=(0, 1))),
                              "lit"))
        lights.append(((x, y, ZH - 44), "300", "0.47"))
    for sx, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1)):
        lights.append(((sx * 402, sy * 402, 320), "180", "0.6"))      # over each tower's top
        lights.append(((sx * 736, sy * 288, 320), "160", "0.6"))      # over each walkway landing
        lights.append(((sx * 640, sy * 640, 140), "150", "0.66"))     # in each corner of the floor
    for sx, sy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        lights.append(((sx * 180, sy * 180, 140), "140", "0.66"))     # over the dais, one facing each staircase

    mw.world.extend(shell + coffers)
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
    keys = {"classname": "worldspawn", "mapversion": "220", "wad": ";".join(WADS),
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
    ap.add_argument("--dev-textures", action="store_true", help="texture the arena with quakevr_dev.wad's flat grids "
                        "instead of id's base ones: for a checkout without quakevr/wads/id_textures.wad, which "
                        "Misc/trenchbroom/make_id_wad.py makes from your own paks")
    args = ap.parse_args()
    global TEX, WADS
    if args.dev_textures:
        TEX, WADS = DEVTEX, WADS[-1:]
    elif not os.path.exists(os.path.join(ROOT, WADS[0])):
        print("no %s: run python Misc/trenchbroom/make_id_wad.py (it reads your own paks and git never sees the "
              "result), or pass --dev-textures" % WADS[0])
        sys.exit(1)
    # every name in the palette must exist in a WAD: `laid` fits a face to a whole number of TILES, so it needs to know
    # the tile's size, and a name that is simply wrong would compile as the tools' flat checkerboard
    missing = sorted(name for name in (spec[0] for spec in TEX.values()) if name.lower() not in TEXSIZE)
    if missing:
        print("no such texture in %s: %s (Misc/trenchbroom/make_id_wad.py --list prints what the WADs hold)"
              % (", ".join(WADS), ", ".join(missing)))
        sys.exit(1)
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
