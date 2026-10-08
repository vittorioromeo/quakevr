# vrtutorial2_gen.py -- writes quakevr/maps/vrtutorial2.map, Quake VR's tutorial (a military base by day), and with
# --compile builds it (qbsp 0.18.1, ericw-tools 2.0's vis and light; presets "fast" and "final", as vrstart_gen.py).
#
#   python Misc/quakevr/maps/vrtutorial2_gen.py [--compile [--preset fast|final] [--check RAYS]] [--tools DIR] [--qbsp EXE]
#   (first: python Misc/trenchbroom/make_id_wad.py, the id textures' WAD; the sky: make_day_sky.py)
#
# Everything in the map is made here (reproducible; the .map opens in TrenchBroom): edit this script, not the .map.
#
# How it is built (MAPPING.md, "vrtutorial2"):
#   - the rooms, halls, doorways and pools are boxes of AIR (Air); the structural world is their shells (each box grown
#     by the walls' thickness T on every side) minus every air box, cut into disjoint boxes (carve()) and at the walls'
#     bands' heights. Each face's texture is the style of the air it faces (Air.face_spec): floors, ceilings, the wall's
#     band (skirting, panels, rail, upper wall), a doorway's jambs and lintel, a pool's tiles, the sky; laid with
#     Valve 220 axes from the room's corners, so panels, tiles and trims fit their faces (the rooms' sizes are
#     multiples of the panels' 128, the doorways on the panels' grid);
#   - curved halls are annular sectors (arc_hall), their walls banded too, the textures running on round the curve;
#   - fittings (door frames, lamps, pipes, vents, railings, ladders, signs...) are func_detail, painted arrows
#     func_detail_illusionary sheets ('{qvr_arrow');
#   - every point on the 1/8 grid (mapgeom.py), the rooms on 16: no slivers, no nearly coplanar faces.
#
# Layout (x east, y north; 32.8 units a metre): rooms 1-4 on the upper floors (z 0 and 240), a shaft down to the lower
# floor (z -144) and rooms 5-12 there, the arena last. Room by room: build_room1() ... (the ROOMS table's order).
import argparse
import math
import os
import random
import shutil
import struct
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import mapgeom  # noqa: E402
from mapgeom import MapWriter, hull, cylinder, beam, norm, cross, dot, sub, add, mul  # noqa: E402

ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
DEFAULT_TOOLS = "C:/OHWorkspace/ericw-tools-2.0.0-alpha11-win64"  # vis, light
# qbsp: ericw-tools 0.18.1's, as vrstart's (the author's decision, 2026-10-07: 2.0-alpha11's lost faces at slivers;
# 0.18.1's makes faces by CSG). ROUND21.md, "vrstart on ericw-tools 2.0 again".
DEFAULT_QBSP = "C:/OHWorkspace/ericw-tools-v0.18.1-32-g6660c5f-win64/bin/qbsp.exe"
MAPNAME = "vrtutorial2"
OUT = os.path.join(ROOT, "quakevr", "maps", MAPNAME + ".map")
WADS = ["quakevr/wads/id_textures.wad", "quakevr/wads/quakevr_dev.wad"]

T = 16      # the walls', floors' and ceilings' thickness
N = "\\n"   # a new line in an entity's text


# ---------------------------------------------------------------------------------------------------------------------
# Textures: id's own (quakevr/wads/id_textures.wad, made from the player's paks by make_id_wad.py; the compiled map
# embeds them: the author's decision for our maps) and Quake VR's (quakevr_dev.wad: the arrows, the hazard stripes).
def wad_sizes():
    sizes = {}
    for rel in WADS:
        path = os.path.join(ROOT, rel)
        if not os.path.exists(path):
            sys.exit("%s is missing: python Misc/trenchbroom/make_id_wad.py makes it from your id1 paks" % rel)
        with open(path, "rb") as f:
            d = f.read()
        n, ofs = struct.unpack("<ii", d[4:12])
        for i in range(n):
            pos, _, _, _, _, _, name = struct.unpack("<iiibbh16s", d[ofs + 32 * i: ofs + 32 * i + 32])
            w, h = struct.unpack("<II", d[pos + 16:pos + 24])
            sizes.setdefault(name.split(b"\0")[0].decode("latin1").lower(), (w, h))
    return sizes


TEXSIZE = wad_sizes()


def tsize(name):
    return TEXSIZE[name.lower()]


def spec(name, u, v, org, su=1.0, sv=None):
    """Valve 220 axes `u`, `v` (unit vectors), texel 0 at the point `org`, `su`/`sv` units a texel."""
    sv = su if sv is None else sv
    w, h = tsize(name)
    return (name, u, v, (-dot(org, u) / su) % w, (-dot(org, v) / sv) % h, su, sv)


def right_of(n):
    """The way to the right on a vertical face seen from in front of it (looking along -n)."""
    return (-n[1], n[0], 0.0)


DOWN = (0.0, 0.0, -1.0)
X = (1.0, 0.0, 0.0)
MY = (0.0, -1.0, 0.0)


def wall_spec(name, n, left, ztop, su=1.0, sv=None):
    """A vertical face's texture: texel 0 at the face's `left` coordinate along right_of(n) and at height `ztop`."""
    u = right_of(n)
    org = mul(u, left)
    return spec(name, u, DOWN, (org[0], org[1], ztop), su, sv)


def flat_spec(name, x0, y1, su=1.0):
    return spec(name, X, MY, (x0, y1, 0.0), su)


class Box:
    """A detail brush's texturing: per face direction ('top', 'bottom', 'side', or '+x', '-x', '+y', '-y') a texture
    name, laid from the box's own corner (fit=True: stretched to one copy across the face)."""

    def __init__(self, lo, hi, tex, fit=(), anchor=None):
        self.lo, self.hi, self.tex, self.fit = lo, hi, tex, fit
        self.anchor = anchor  # (x, y, z): texel 0 there instead of the box's corner

    def name_for(self, n):
        key = "top" if n[2] > 0.5 else "bottom" if n[2] < -0.5 else (
            ("+x" if n[0] > 0 else "-x") if abs(n[0]) > abs(n[1]) else ("+y" if n[1] > 0 else "-y"))
        if isinstance(self.tex, str):
            return self.tex, key
        for k in (key, "side" if key not in ("top", "bottom") else key, "all"):
            if k in self.tex:
                return self.tex[k], key
        return self.tex.get("side", "black"), key

    def __call__(self, n, c):
        name, key = self.name_for(n)
        lo, hi = self.lo, self.hi
        w, h = tsize(name)
        fit = key in self.fit or "all" in self.fit
        if key in ("top", "bottom"):
            org = self.anchor or (lo[0], hi[1], 0.0)
            su = (hi[0] - lo[0]) / w if fit else 1.0
            sv = (hi[1] - lo[1]) / h if fit else 1.0
            return spec(name, X, MY, org, su, sv)
        u = right_of(n)
        corners = [(x, y) for x in (lo[0], hi[0]) for y in (lo[1], hi[1])]
        left = min(c_[0] * u[0] + c_[1] * u[1] for c_ in corners)
        width = max(c_[0] * u[0] + c_[1] * u[1] for c_ in corners) - left
        if self.anchor:
            left = self.anchor[0] * u[0] + self.anchor[1] * u[1]
        ztop = self.anchor[2] if self.anchor else hi[2]
        su = width / w if fit else 1.0
        sv = (hi[2] - lo[2]) / h if fit else 1.0
        return wall_spec(name, n, left, ztop, su, sv)


def dbox(out, lo, hi, tex, fit=(), anchor=None):
    """A detail box from `lo` to `hi` into the brush list `out`."""
    b = mapgeom.box(lo[0], lo[1], lo[2], hi[0], hi[1], hi[2], Box(lo, hi, tex, fit, anchor))
    out.append(b)
    return b


# The palette (id's base textures: a well-kept military base)
TX = {
    "floor": "sfloor4_2", "floor2": "metflor2_1", "floor3": "sfloor4_6", "ceiling": "sfloor4_1",
    "skirting": "tech04_1", "rail": "tech04_1", "strip_v": "tech04_3", "upper": "metal1_2",
    "panel": "tech14_1", "panel2": "tech06_1", "panel3": "tech08_2", "panel4": "tech09_3", "panel5": "tech07_2",
    "jamb": "tech04_3", "lintel": "tech04_1", "threshold": "metal1_3",
    "pool": "sfloor4_5", "water": "*04water1", "sky": "sky1",
    "lamp": "light3_3", "lampwall": "tlight01", "lamp_frame": "metal1_1", "pipe": "metal5_1", "vent": "comp1_4",
    "door": "tech06_2", "door_frame": "tech04_3", "hazard": "qvr_hazard", "arrow": "{qvr_arrow", "skip": "skip",
    "rung": "metal1_1", "rail_post": "metal1_1", "crate": "crate0_side", "dark": "twall2_1", "dark2": "metal2_4",
    "button": "+0basebtn", "shoot": "+0shoot", "trigger": "trigger", "clip": "clip", "black": "black",
    "target": "qvr_target", "wood": "wood1_1", "window": "sfloor4_4", "teleport": "*teleport", "court": "city4_2", "court_floor": "afloor1_8",
}


# ---------------------------------------------------------------------------------------------------------------------
# Air: rooms, halls, doorways, pools. The structural world is carved round them.
class Style:
    """How a room's surfaces look: its floor's and ceiling's textures and its walls' bands, (z0, z1, texture) from its
    floor up (the last band up to the ceiling: z1 None)."""

    def __init__(self, floor=None, ceiling=None, bands=None, sky=False, jamb=None):
        self.floor = floor or TX["floor"]
        self.ceiling = ceiling or TX["ceiling"]
        self.bands = bands or [(0, 16, TX["skirting"]), (16, 144, TX["panel"]), (144, 160, TX["rail"]),
                               (160, None, TX["upper"])]
        self.sky = sky
        self.jamb = jamb


def bands(panel, upper=None, floor=None, ceiling=None, sky=False, rail=None):
    return Style(floor, ceiling, [(0, 16, TX["skirting"]), (16, 144, panel), (144, 160, rail or TX["rail"]),
                                  (160, None, upper or TX["upper"])], sky)


class Air:
    """A box of air (lo, hi): a room, a hall, a doorway through a wall, a pool, a shaft. `shell`: walls round it
    (T thick; `open` names the sides left without: '+x', '-y'...); `floor`: its bands' base height."""

    PRIORITY = {"door": 0, "pool": 1, "shaft": 2, "room": 3}

    def __init__(self, name, lo, hi, kind="room", style=None, shell=True, open=(), floor=None):
        self.name, self.lo, self.hi, self.kind = name, tuple(lo), tuple(hi), kind
        self.style = style or Style()
        self.shell = shell and kind != "door"
        self.open = set(open)
        self.floor = lo[2] if floor is None else floor

    def contains(self, p):
        return all(self.lo[i] < p[i] < self.hi[i] for i in range(3))

    def face_spec(self, n, c):
        st = self.style
        lo, hi = self.lo, self.hi
        if self.kind == "door":
            # the doorway's passage: its thin axis is the wall's
            thin = 0 if hi[0] - lo[0] < hi[1] - lo[1] else 1
            if n[2] > 0.5:
                return flat_spec(TX["threshold"], lo[0], hi[1])
            if n[2] < -0.5:
                # the lintel's underside: the strip along the opening, its 16 across the wall
                if thin == 0:
                    return spec(TX["lintel"], (0.0, 1.0, 0.0), (1.0, 0.0, 0.0), (lo[0], lo[1], 0.0))
                return spec(TX["lintel"], X, (0.0, 1.0, 0.0), (lo[0], lo[1], 0.0))
            jamb = st.jamb or TX["jamb"]
            u = right_of(n)
            left = min(lo[0] * u[0] + lo[1] * u[1], hi[0] * u[0] + hi[1] * u[1])
            return wall_spec(jamb, n, left, hi[2])
        if n[2] > 0.5:
            name = TX["pool"] if self.kind == "pool" else st.floor
            return flat_spec(name, lo[0], hi[1])
        if n[2] < -0.5:
            if st.sky:
                return spec(TX["sky"], X, MY, (0.0, 0.0, 0.0))
            return flat_spec(st.ceiling, lo[0], hi[1])
        u = right_of(n)
        left = min(lo[0] * u[0] + lo[1] * u[1], hi[0] * u[0] + hi[1] * u[1])
        if self.kind == "pool":
            return wall_spec(TX["pool"], n, left, self.hi[2])
        z = c[2] - self.floor
        for z0, z1, name in st.bands:
            top = hi[2] - self.floor if z1 is None else z1
            if z0 <= z < top:
                if z1 is None:  # the upper wall: whole tiles down from the ceiling
                    return wall_spec(name, n, left, hi[2])
                return wall_spec(name, n, left, self.floor + z1)
        return wall_spec(st.bands[-1][2], n, left, hi[2])


AIRS = []
SOLIDS = []      # extra structural boxes (platforms, blocks, steps): (lo, hi)
WATER = []       # water boxes (lo, hi)
CLIPS = []       # player clip boxes (lo, hi): structural, in the clipping hulls only


def air(name, lo, hi, **kw):
    a = Air(name, lo, hi, **kw)
    AIRS.append(a)
    return a


def door_air(name, lo, hi, style=None):
    return air(name, lo, hi, kind="door", style=style)


def air_at(p):
    best = None
    for a in AIRS:
        if a.contains(p) and (best is None or Air.PRIORITY.get(a.kind, 3) < Air.PRIORITY.get(best.kind, 3)):
            best = a
    return best


def box_sub(b, a):
    """Box b minus box a: disjoint boxes (z first: floors and ceilings stay whole slabs)."""
    bl, bh = b
    al, ah = a
    if any(ah[i] <= bl[i] or al[i] >= bh[i] for i in range(3)):
        return [b]
    out = []
    lo, hi = list(bl), list(bh)
    for i in (2, 0, 1):
        if al[i] > lo[i]:
            h2 = hi[:]
            h2[i] = al[i]
            out.append((tuple(lo), tuple(h2)))
            lo[i] = al[i]
        if ah[i] < hi[i]:
            l2 = lo[:]
            l2[i] = ah[i]
            out.append((tuple(l2), tuple(hi)))
            hi[i] = ah[i]
    return out


def overlaps(a, b):
    return all(a[0][i] < b[1][i] and b[0][i] < a[1][i] for i in range(3))


def world_tex(n, c):
    """A structural face's texture: the style of the air in front of it (none: a face qbsp removes)."""
    a = air_at(add(c, mul(n, 0.5)))
    if a is None:
        return mapgeom.Tex(TX["black"]).spec(n, c)
    return a.face_spec(n, c)


def carve():
    """The structural world: every shell minus every air box, as disjoint boxes, cut at the bands' heights."""
    pieces = []
    for a in AIRS:
        if not a.shell:
            continue
        lo = [a.lo[i] - T for i in range(3)]
        hi = [a.hi[i] + T for i in range(3)]
        for i, ax in enumerate("xyz"):
            if "-" + ax in a.open:
                lo[i] = a.lo[i]
            if "+" + ax in a.open:
                hi[i] = a.hi[i]
        frags = [(tuple(lo), tuple(hi))]
        for b in AIRS:
            frags = [f for g in frags for f in box_sub(g, (b.lo, b.hi))]
        for p in pieces:
            if any(overlaps(f, p) for f in frags):
                frags = [f for g in frags for f in box_sub(g, p)]
        pieces += frags
    for s in SOLIDS:
        frags = [s]
        for p in pieces:
            if any(overlaps(f, p) for f in frags):
                frags = [f for g in frags for f in box_sub(g, p)]
        pieces += frags
    # the bands' heights: every room's (a face then lies in one band)
    levels = sorted({a.floor + z for a in AIRS if a.kind == "room" for z0, z1, _ in a.style.bands for z in (z0, z1)
                     if z is not None and z > 0})
    out = []
    for lo, hi in pieces:
        cuts = [z for z in levels if lo[2] < z < hi[2]]
        zs = [lo[2]] + cuts + [hi[2]]
        for z0, z1 in zip(zs, zs[1:]):
            out.append(((lo[0], lo[1], z0), (hi[0], hi[1], z1)))
    return out


# ---------------------------------------------------------------------------------------------------------------------
# Curved halls: an annular sector round (cx, cy) from angle a0 to a1 (degrees), radii r0..r1, floor z0, ceiling z1;
# walls, floor and ceiling as segment prisms, the walls banded as a room's and their textures running on round it.
CURVED = []      # (brush) structural


def arc_point(cx, cy, r, a):
    return (cx + r * math.cos(math.radians(a)), cy + r * math.sin(math.radians(a)))


def arc_hall(cx, cy, r0, r1, a0, a1, z0, z1, style, segs=8):
    angs = [a0 + (a1 - a0) * i / segs for i in range(segs + 1)]

    def ring(r):
        return [arc_point(cx, cy, r, a) for a in angs]

    def prism(pts2, zlo, zhi, texfn):
        return hull([(p[0], p[1], z) for p in pts2 for z in (zlo, zhi)], texfn)

    def floor_fn(n, c):
        if n[2] > 0.5:
            return flat_spec(style.floor, 0, 0)
        if n[2] < -0.5:
            return spec(TX["sky"], X, MY, (0, 0, 0)) if style.sky else flat_spec(style.ceiling, 0, 0)
        return mapgeom.Tex(TX["upper"]).spec(n, c)
    ri, ro = ring(r0), ring(r1)
    rii, roo = ring(r0 - T), ring(r1 + T)
    levels = [z0 + b[0] for b in style.bands] + [z1]
    for wall, (ra, rb), inner in (("in", (rii, ri), True), ("out", (ro, roo), False)):
        # cumulative lengths along the wall's face (the face on the hall's side)
        face = rb if inner else ra
        L = [0.0]
        for i in range(segs):
            L.append(L[-1] + math.hypot(face[i + 1][0] - face[i][0], face[i + 1][1] - face[i][1]))
        for i in range(segs):
            p, q = face[i], face[i + 1]
            d = norm((q[0] - p[0], q[1] - p[1], 0.0))
            mid = ((p[0] + q[0]) / 2, (p[1] + q[1]) / 2)
            nrm = norm((-d[1], d[0], 0.0))
            # the face's normal points into the hall: away from the centre for the inner wall, towards it for the outer
            away = (mid[0] - cx, mid[1] - cy)
            if (nrm[0] * away[0] + nrm[1] * away[1] > 0) != inner:
                nrm = mul(nrm, -1)
            u = right_of(nrm)
            sign = 1.0 if dot(u, d) > 0 else -1.0
            for b, (bz0, bz1, name) in enumerate(style.bands):
                zlo = z0 + bz0
                zhi = z1 if bz1 is None else z0 + bz1
                ztop = z1 if bz1 is None else zhi
                org = (p[0] - u[0] * sign * L[i], p[1] - u[1] * sign * L[i], ztop)

                def fn(n, c, name=name, org=org, nrm=nrm, u=u):
                    if dot(n, nrm) > 0.99:
                        return spec(name, u, DOWN, org)
                    return mapgeom.Tex(TX["upper"]).spec(n, c)
                CURVED.append(prism([ra[i], ra[i + 1], rb[i + 1], rb[i]], zlo, zhi, fn))
            # the walls' feet and tops (under the floor, over the ceiling)
            CURVED.append(prism([ra[i], ra[i + 1], rb[i + 1], rb[i]], z0 - T, z0, floor_fn))
            CURVED.append(prism([ra[i], ra[i + 1], rb[i + 1], rb[i]], z1, z1 + T, floor_fn))
    for i in range(segs):
        quad = [ri[i], ri[i + 1], ro[i + 1], ro[i]]
        CURVED.append(prism(quad, z0 - T, z0, floor_fn))
        CURVED.append(prism(quad, z1, z1 + T, floor_fn))
    return levels


# ---------------------------------------------------------------------------------------------------------------------
# Entities and fittings
ENTS = []        # (keys, brushes)
DETAIL = []      # func_detail brushes (one TrenchBroom group per room: DETAIL_GROUPS)
DETAIL_GROUPS = []
ILLUSION = []    # func_detail_illusionary: painted arrows, signs
RND = random.Random(2026)


def group(name):
    g = (name, [])
    DETAIL_GROUPS.append(g)
    return g[1]


def ent(cls, x, y, z, **keys):
    k = {"classname": cls, "origin": "%g %g %g" % (x, y, z)}
    k.update({a: str(b) for a, b in keys.items()})
    ENTS.append((k, []))
    return k


def item(cls, x, y, z, **keys):
    """An item standing at (x, y, z): ammo and health boxes have their origin at a corner (id's 0 0 0 .. 32 32 56)."""
    corner = cls in ("item_shells", "item_spikes", "item_rockets", "item_cells", "item_health")
    return ent(cls, x - 16 if corner else x, y - 16 if corner else y, z, **keys)


def bent(cls, brushes, **keys):
    k = {"classname": cls}
    k.update({a: str(b) for a, b in keys.items()})
    ENTS.append((k, brushes))
    return k


WHITE = "0.96 0.97 1"


def light(x, y, z, value=220, color=WHITE, **kw):
    return ent("light", x, y, z, light=value, _color=color, **kw)


def banner(text, x, y, z, angle, scale="0.3", speed=None):
    """A text board (func_worldtext_banner) facing `angle`; lines split with N, pages with $."""
    k = {"worldtext": text, "worldtext_halign": "1", "worldtext_scale": scale, "angle": angle}
    if speed:
        k["speed"] = speed
    return ent("func_worldtext_banner", x, y, z, **k)


def tip(name, message, x, y, z, distance=200, target=None, size=None, flags=0, delay=None, trig=None):
    """A map tip (func_vr_tip). `trig`: shown only once that targetname is fired (QC: its TRIGGERED flag, 8)."""
    k = {"tipname": name, "message": message, "distance": distance}
    if trig:
        k["targetname"] = trig
        flags |= 8
    if target:
        k["target"] = target
    if size:
        k["tip_size"] = size
    if flags:
        k["spawnflags"] = flags
    if delay is not None:
        k["tip_delay"] = delay
    return ent("func_vr_tip", x, y, z, **k)


BUTTON_TEX = 32      # +0basebtn's size (vrstart_gen.py's button_tex: one copy fitted to the front)
BUTTON_BAND = 4


def button_tex(lo, hi, d, name):
    right = (d[1], -d[0], 0.0)
    corners = [(x, y, z) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
    r0, r1 = min(dot(c, right) for c in corners), max(dot(c, right) for c in corners)
    d0, d1 = min(dot(c, d) for c in corners), max(dot(c, d) for c in corners)
    z1 = hi[2]
    w = tsize(name)[0]
    su, sv, sd = (r1 - r0) / w, (hi[2] - lo[2]) / w, (d1 - d0) / BUTTON_BAND

    def off(o, sc):
        return (-o / sc) % w

    def fn(n, c):
        if abs(dot(n, d)) > 0.9:
            return (name, right, DOWN, off(r0, su), off(-z1, sv), su, sv)
        if abs(n[2]) > 0.9:
            return (name, right, d, off(r0, su), off(d0, sd), su, sd)
        return (name, d, DOWN, off(d0, sd), off(-z1, sv), sd, sv)
    return fn


def button(label, command, x, y, z, angle, depth=6, size=18, scale="0.2", target=None, tex=None, **keys):
    """A func_button on a wall: (x, y) the middle of its back on the wall, z its centre, pushed towards `angle` (it
    faces the opposite way). `command`: a console command (buttonEffect 3) or None (it fires `target`)."""
    a = math.radians(angle)
    d = (round(math.cos(a)), round(math.sin(a)), 0.0)
    h = size / 2
    rx, ry = abs(d[1]) * h, abs(d[0]) * h
    fx, fy = x - d[0] * depth, y - d[1] * depth
    lo = (min(x, fx) - rx, min(y, fy) - ry, z - h)
    hi = (max(x, fx) + rx, max(y, fy) + ry, z + h)
    k = {"angle": angle, "wait": "1", "speed": "50", "lip": "4", "sounds": "1"}
    if label:
        k.update({"worldtext": label, "worldtext_halign": "1", "worldtext_scale": scale})
    if command:
        k.update({"buttonEffect": "3", "targetname": command + "\\n"})
    if target:
        k["target"] = target
    k.update(keys)
    b = mapgeom.box(lo[0], lo[1], lo[2], hi[0], hi[1], hi[2], button_tex(lo, hi, d, tex or TX["button"]))
    return bent("func_button", [b], **k)


def setting_button(label, key, x, y, z, angle):
    return button(label, "vr_setup_option " + key, x, y, z, angle, depth=8)


# ---- fittings ----------------------------------------------------------------------------------------------------
def wall_axes(side):
    """For a wall on `side` of a room ('-x' the west wall...): its inward normal and the axis along it."""
    n = {"-x": (1, 0), "+x": (-1, 0), "-y": (0, 1), "+y": (0, -1)}[side]
    return n


def door_frame(out, door, sides=("in", "out")):
    """A door frame round a doorway (an Air of kind door): jambs and a header on both faces of the wall, 16 wide, 8
    deep (the riveted strips)."""
    lo, hi = door.lo, door.hi
    thin = 0 if hi[0] - lo[0] < hi[1] - lo[1] else 1
    along = 1 - thin
    for face in (lo[thin] - 8, hi[thin]):
        def B(a0, a1, z0, z1, tex):
            l, h = [0, 0, z0], [0, 0, z1]
            l[thin], h[thin] = face, face + 8
            l[along], h[along] = a0, a1
            return dbox(out, tuple(l), tuple(h), tex)
        strip_v = {"side": TX["strip_v"], "top": TX["lintel"], "bottom": TX["lintel"]}
        B(lo[along] - 16, lo[along], lo[2], hi[2], strip_v)
        B(hi[along], hi[along] + 16, lo[2], hi[2], strip_v)
        B(lo[along] - 16, hi[along] + 16, hi[2], hi[2] + 16, {"side": TX["lintel"], "top": TX["lintel"],
                                                               "bottom": TX["lintel"]})


def sliding_door(door, name=None, keys=None, tex=None, wait=-1, speed=100):
    """Two leaves in the middle of the doorway, sliding apart into the wall (func_door; `name`: its targetname, opened
    by what targets it; None: it opens when the player comes near). The door texture (128 square) once across both
    leaves, from the doorway's left edge and top: the leaves part along its middle."""
    lo, hi = door.lo, door.hi
    thin = 0 if hi[0] - lo[0] < hi[1] - lo[1] else 1
    along = 1 - thin
    mid = (lo[thin] + hi[thin]) / 2
    centre = (lo[along] + hi[along]) / 2
    tex = tex or TX["door"]
    for leaf, (a0, a1) in enumerate(((lo[along], centre), (centre, hi[along]))):
        l, h = [0, 0, lo[2]], [0, 0, hi[2]]
        l[thin], h[thin] = mid - 4, mid + 4
        l[along], h[along] = a0, a1

        def fn(n, c, l=tuple(l), h=tuple(h)):
            if abs(n[2]) > 0.5 or abs(n[along]) > 0.5:
                return Box(l, h, TX["lamp_frame"])(n, c)
            u = right_of(n)
            left = min(lo[0] * u[0] + lo[1] * u[1], hi[0] * u[0] + hi[1] * u[1])
            w = tsize(tex)[0]
            return wall_spec(tex, n, left, hi[2], (hi[along] - lo[along]) / w, (hi[2] - lo[2]) / tsize(tex)[1])
        b = mapgeom.box(l[0], l[1], l[2], h[0], h[1], h[2], fn)
        # the leaves move apart along the wall: the far one towards +along, the near one the other way
        ang = (0 if along == 0 else 90) if leaf == 1 else (180 if along == 0 else 270)
        k = {"angle": ang, "speed": speed, "wait": wait, "lip": 4, "sounds": 3}
        if name:
            k["targetname"] = name
        if keys:
            k.update(keys)
        bent("func_door", [b], **k)


def ceiling_lamp(out, x, y, zc, w=64, d=32, value=200, color=WHITE, dark=False):
    """A flat lamp under a ceiling at zc: a frame and a glowing panel, a light below."""
    if any(sx0 - 40 < x < sx1 + 40 and sy0 - 24 < y < sy1 + 24 for sx0, sy0, sx1, sy1 in SKYLIGHTS):
        return  # (a skylight there)
    dbox(out, (x - w / 2 - 4, y - d / 2 - 4, zc - 4), (x + w / 2 + 4, y + d / 2 + 4, zc), TX["lamp_frame"])
    dbox(out, (x - w / 2, y - d / 2, zc - 6), (x + w / 2, y + d / 2, zc - 4),
         {"bottom": TX["lamp"], "side": TX["lamp_frame"], "top": TX["lamp_frame"]}, fit=("bottom",))
    if not dark:
        light(x, y, zc - 24, value, color, wait="0.55")   # (a slower falloff: the floor 200 below well lit)


def wall_lamp(out, side, x, y, z, value=150, color=WHITE):
    """A small round lamp on a wall (side: the wall's side of the room; (x, y) on the wall's face)."""
    n = wall_axes(side)
    if n[0]:
        dbox(out, (x if n[0] > 0 else x - 4, y - 8, z - 8), (x + 4 if n[0] > 0 else x, y + 8, z + 8),
             {"side": TX["lampwall"], "top": TX["lamp_frame"], "bottom": TX["lamp_frame"]}, fit=("+x", "-x"))
    else:
        dbox(out, (x - 8, y if n[1] > 0 else y - 4, z - 8), (x + 8, y + 4 if n[1] > 0 else y, z + 8),
             {"side": TX["lampwall"], "top": TX["lamp_frame"], "bottom": TX["lamp_frame"]}, fit=("+y", "-y"))
    light(x + n[0] * 16, y + n[1] * 16, z, value, color, wait="0.8")


def pipe_run(out, p, q, r=4, brackets=96):
    """A pipe from p to q (horizontal), with brackets back to the wall every `brackets` units."""
    out.append(cylinder(p, q, r, 8, mapgeom.Tex(TX["pipe"], mode="grain", axis=norm(sub(q, p)), scale=0.5)))


def floor_arrow(x, y, z, yaw, size=48):
    """A painted arrow on the floor at (x, y, z) pointing `yaw` degrees (0 east, 90 north): a 1-unit sheet
    (func_detail_illusionary) the arrow texture fitted to its top, its other faces skip."""
    h = size / 2
    ux, uy = round(math.cos(math.radians(yaw)), 6), round(math.sin(math.radians(yaw)), 6)
    name = TX["arrow"]
    w = tsize(name)[0]
    # the texture's top (minus V) towards the arrow's way: V = -(ux, uy); U = (uy, -ux) (right, seen from above)
    u = (uy, -ux, 0.0)
    v = (-ux, -uy, 0.0)
    org = (x - u[0] * h - v[0] * h, y - u[1] * h - v[1] * h, 0.0)

    def fn(n, c):
        if n[2] > 0.5:
            return spec(name, u, v, org, size / w)
        return mapgeom.Tex(TX["skip"]).spec(n, c)
    ILLUSION.append(mapgeom.box(x - h, y - h, z, x + h, y + h, z + 1, fn))


def wall_arrow(side, x, y, z, yaw_dir, size=48):
    """A painted arrow on a wall (side as wall_lamp: the room's side the wall is on; (x, y) on its face), pointing
    'left', 'right' (seen facing the wall), 'up' or 'down'."""
    n = wall_axes(side)
    h = size / 2
    name = TX["arrow"]
    w = tsize(name)[0]
    nn = (float(n[0]), float(n[1]), 0.0)
    r = right_of(nn)
    ways = {"up": ((r[0], r[1], 0.0), DOWN), "down": ((-r[0], -r[1], 0.0), (0.0, 0.0, 1.0)),
            "right": ((0.0, 0.0, -1.0), (-r[0], -r[1], 0.0)), "left": ((0.0, 0.0, 1.0), (r[0], r[1], 0.0))}
    u, v = ways[yaw_dir]
    org = (x - r[0] * h, y - r[1] * h, z + h)
    # texel 0 at the sheet's top-left (seen facing it) for 'up'; the other ways turn about the middle
    cx, cy, cz = x, y, z
    org = (cx - u[0] * h - v[0] * h, cy - u[1] * h - v[1] * h, cz - u[2] * h - v[2] * h)

    def fn(nf, c):
        if dot(nf, nn) > 0.5:
            return spec(name, u, v, org, size / w)
        return mapgeom.Tex(TX["skip"]).spec(nf, c)
    if n[0]:
        x0 = x if n[0] > 0 else x - 1
        ILLUSION.append(mapgeom.box(x0, y - h, z - h, x0 + 1, y + h, z + h, fn))
    else:
        y0 = y if n[1] > 0 else y - 1
        ILLUSION.append(mapgeom.box(x - h, y0, z - h, x + h, y0 + 1, z + h, fn))


def hazard_edge(out, lo, hi):
    dbox(out, lo, hi, TX["hazard"])


def railing(out, p, q, z, h=40, every=64, post=3):
    """A steel railing from p to q (2D) on the floor at z: posts every `every` units, a top rail and a mid rail."""
    L = math.hypot(q[0] - p[0], q[1] - p[1])
    n = max(1, int(round(L / every)))
    tex = TX["rail_post"]
    for i in range(n + 1):
        t = i / n
        x, y = p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t
        dbox(out, (x - post / 2, y - post / 2, z), (x + post / 2, y + post / 2, z + h), tex)
    for zz in (z + h - 3, z + h / 2):
        out.append(beam((p[0], p[1], zz + 1.5), (q[0], q[1], zz + 1.5), 3, 3, tex))


# ---------------------------------------------------------------------------------------------------------------------
# The rooms
#
# Each build_roomN() makes its air (room(), doorway(), ...), its fittings (into its TrenchBroom group) and its
# entities. Coordinates: rooms on the 16 grid, their sizes multiples of 128 (the panels), doorways 128 wide on the
# panels' grid; upper floors at z 0 (rooms 1-3) and 240 (rooms 3's top, 4), the lower floor at z -144 (rooms 5-12).
LOW = -144      # the lower floor
UP = 256        # room 4's floor (room 3's top: the jump wall, 112 over the ladder block)


def room(name, x0, y0, x1, y1, z, top, style, **kw):
    a = air(name, (x0, y0, z), (x1, y1, top), style=style, **kw)
    a.trim = True   # its corner columns and crown moulding (room_trims)
    return a


SKYLIGHTS = []   # (x0, y0, x1, y1): no ceiling lamp under them (ceiling_lamp)


def skylight(x0, y0, x1, y1, ztop, depth=64):
    """A light well in a ceiling at ztop: a shaft `depth` high open to the sky, its sides plain metal, a frame round
    its mouth. The sun comes down it."""
    air("skylight_%d_%d" % (x0, y0), (x0, y0, ztop), (x1, y1, ztop + depth),
        style=Style(TX["floor2"], None, [(0, None, TX["upper"])], sky=True))
    SKYLIGHTS.append((x0, y0, x1, y1))
    out = group("skylight_%d_%d" % (x0, y0))
    fr = {"side": TX["lamp_frame"], "bottom": TX["lamp_frame"], "top": TX["lamp_frame"]}
    dbox(out, (x0 - 8, y0 - 8, ztop - 4), (x1 + 8, y0, ztop), fr)
    dbox(out, (x0 - 8, y1, ztop - 4), (x1 + 8, y1 + 8, ztop), fr)
    dbox(out, (x0 - 8, y0, ztop - 4), (x0, y1, ztop), fr)
    dbox(out, (x1, y0, ztop - 4), (x1 + 8, y1, ztop), fr)
    # cross bars over the opening (a grille: daylight through it, nothing falls in)
    for x in range(x0 + 32, x1, 32):
        dbox(out, (x - 2, y0, ztop + 8), (x + 2, y1, ztop + 12), TX["rail_post"])


def room_trims():
    """Every room's corner columns (16 square, floor to ceiling: the riveted strip) and its crown moulding (8 by 8
    along the walls at the ceiling; a courtyard's: a cornice 16 high under its top), all detail; none where a
    doorway, window or alcove meets them."""
    out = group("trims")
    doors = [(a.lo, a.hi) for a in AIRS if a.kind == "door"]
    others = [(a.lo, a.hi) for a in AIRS if a.kind != "door"]

    def clear(lo, hi, room):
        g = ((lo[0] - 1, lo[1] - 1, lo[2] - 1), (hi[0] + 1, hi[1] + 1, hi[2] + 1))
        if any(overlaps(g, d) for d in doors):
            return False
        # (not into another air box but its own: an alcove, a pool, a skylight beside it)
        return not any(overlaps((lo, hi), o) for o in others if o != (room.lo, room.hi))

    col = {"side": TX["strip_v"], "top": TX["lamp_frame"], "bottom": TX["lamp_frame"]}
    for a in AIRS:
        if a.kind != "room" or not getattr(a, "trim", False):
            continue
        (x0, y0, z0), (x1, y1, z1) = a.lo, a.hi
        for (cx, cy) in ((x0, y0), (x1 - 16, y0), (x0, y1 - 16), (x1 - 16, y1 - 16)):
            lo, hi = (cx, cy, z0), (cx + 16, cy + 16, z1)
            if clear(lo, hi, a):
                dbox(out, lo, hi, col)
        if a.style.sky:
            zc0, zc1, d = z1 - 48, z1 - 32, 8
        else:
            zc0, zc1, d = z1 - 8, z1, 8
        crown = {"side": TX["lamp_frame"], "top": TX["lamp_frame"], "bottom": TX["lamp_frame"]}
        for lo, hi in (((x0 + 16, y0, zc0), (x1 - 16, y0 + d, zc1)), ((x0 + 16, y1 - d, zc0), (x1 - 16, y1, zc1)),
                       ((x0, y0 + 16, zc0), (x0 + d, y1 - 16, zc1)), ((x1 - d, y0 + 16, zc0), (x1, y1 - 16, zc1))):
            if clear(lo, hi, a):
                dbox(out, lo, hi, crown)
        # services under an indoor room's ceiling: a pipe along each long wall on brackets, wall vents at the ends
        if not a.style.sky and z1 - z0 >= 192:
            long_x = (x1 - x0) >= (y1 - y0)
            pz = z1 - 30
            for side in (0, 1):
                if long_x:
                    wy = y0 if side == 0 else y1
                    py = wy + (10 if side == 0 else -10)
                    plo, phi = (x0 + 24, min(wy, py) - 6, pz - 6), (x1 - 24, max(wy, py) + 6, pz + 6)
                    if clear(plo, phi, a):
                        out.append(cylinder((x0 + 24, py, pz), (x1 - 24, py, pz), 4, 8, TX["pipe"]))
                        for bx in range(x0 + 64, x1 - 32, 128):
                            dbox(out, (bx - 2, min(wy, py), pz - 2), (bx + 2, max(wy, py), pz + 2), TX["rail_post"])
                else:
                    wx = x0 if side == 0 else x1
                    px = wx + (10 if side == 0 else -10)
                    plo, phi = (min(wx, px) - 6, y0 + 24, pz - 6), (max(wx, px) + 6, y1 - 24, pz + 6)
                    if clear(plo, phi, a):
                        out.append(cylinder((px, y0 + 24, pz), (px, y1 - 24, pz), 4, 8, TX["pipe"]))
                        for by in range(y0 + 64, y1 - 32, 128):
                            dbox(out, (min(wx, px), by - 2, pz - 2), (max(wx, px), by + 2, pz + 2), TX["rail_post"])
            # a vent in the middle of each short wall, under the pipes
            vz0, vz1 = z1 - 72, z1 - 48
            for side in (0, 1):
                if long_x:
                    vx = x0 if side == 0 else x1
                    cy = (y0 + y1) // 2
                    lo = (vx if side == 0 else vx - 2, cy - 24, vz0)
                    hi = (vx + 2 if side == 0 else vx, cy + 24, vz1)
                else:
                    vy = y0 if side == 0 else y1
                    cx = (x0 + x1) // 2
                    lo = (cx - 24, vy if side == 0 else vy - 2, vz0)
                    hi = (cx + 24, vy + 2 if side == 0 else vy, vz1)
                if clear(lo, hi, a):
                    dbox(out, lo, hi, {"side": TX["vent"], "top": TX["lamp_frame"], "bottom": TX["lamp_frame"]},
                         fit=("+x", "-x", "+y", "-y"))
        # a courtyard's daylight in its shade: the sky's light off the walls, soft fill lights high over the yard (the
        # sky dome alone left the shaded walls near black under the ambient occlusion)
        if a.style.sky:
            for fx in range(x0 + 192, x1 - 96, 384):
                for fy in range(y0 + 192, y1 - 96, 384):
                    light(fx, fy, z1 - 96, 160, "0.85 0.9 1", wait="0.3", _dirt="-1", _shadow="0")
        # a courtyard's windows: rows of daylit panes in its walls (the base's offices round the yard), on the panels'
        # grid, none over a doorway or another room's opening
        for wz in getattr(a, "windows", ()):
            zl, zh = z0 + wz, z0 + wz + 48
            for along in range(128, 100000, 128):
                for side in ("-y", "+y", "-x", "+x"):
                    if side in ("-y", "+y"):
                        if x0 + along + 56 > x1 - 64:
                            continue
                        cx = x0 + along
                        yy = y0 if side == "-y" else y1
                        s = 1 if side == "-y" else -1
                        lo = (cx - 28, min(yy, yy + 3 * s), zl - 4)
                        hi = (cx + 28, max(yy, yy + 3 * s), zh + 4)
                        glass_lo, glass_hi = (cx - 24, min(yy, yy + 4 * s), zl), (cx + 24, max(yy, yy + 4 * s), zh)
                    else:
                        if y0 + along + 56 > y1 - 64:
                            continue
                        cy = y0 + along
                        xx = x0 if side == "-x" else x1
                        s = 1 if side == "-x" else -1
                        lo = (min(xx, xx + 3 * s), cy - 28, zl - 4)
                        hi = (max(xx, xx + 3 * s), cy + 28, zh + 4)
                        glass_lo, glass_hi = (min(xx, xx + 4 * s), cy - 24, zl), (max(xx, xx + 4 * s), cy + 24, zh)
                    probe = ((lo[0] - 8, lo[1] - 8, lo[2] - 8), (hi[0] + 8, hi[1] + 8, hi[2] + 8))
                    if not clear(probe[0], probe[1], a) or any(overlaps((lo, hi), s_) for s_ in SOLIDS):
                        continue
                    dbox(out, lo, hi, TX["lamp_frame"])
                    face = {"+x": TX["window"], "-x": TX["window"], "+y": TX["window"], "-y": TX["window"],
                            "top": TX["lamp_frame"], "bottom": TX["lamp_frame"]}
                    dbox(out, glass_lo, glass_hi, face, fit=("+x", "-x", "+y", "-y"))


def doorway(name, x0, y0, x1, y1, z, out, h=128, frames=True):
    d = door_air(name, (x0, y0, z), (x1, y1, z + h))
    if frames:
        door_frame(out, d)
    return d


def lamp_grid(out, x0, y0, x1, y1, zc, nx, ny, value=200, **kw):
    for i in range(nx):
        for j in range(ny):
            ceiling_lamp(out, x0 + (i + 0.5) * (x1 - x0) / nx, y0 + (j + 0.5) * (y1 - y0) / ny, zc, value=value, **kw)


def arrows(points, z, size=48):
    """Floor arrows along a path of (x, y) points, each pointing to the next."""
    for (x, y), (x2, y2) in zip(points, points[1:]):
        floor_arrow(x, y, z, math.degrees(math.atan2(y2 - y, x2 - x)), size)


def checkpoint(name, x, y, z, angle, distance=96):
    """Where the player comes back after dying (QC info_vr_checkpoint: taken when he comes within `distance`)."""
    ent("info_vr_checkpoint", x, y, z + 24, angle=angle, distance=distance, targetname=name)


def restock(contents, x, y, z, count=1, wait=4, distance=96, **keys):
    """A QC func_vr_restock: puts a new `contents` (a classname) here when fewer than `count` are within `distance`."""
    return ent("func_vr_restock", x, y, z, contents=contents, count=count, wait=wait, distance=distance, **keys)


def trigger(name, lo, hi, cls="trigger_once", **keys):
    tex = mapgeom.Tex(TX["trigger"])
    return bent(cls, [mapgeom.box(lo[0], lo[1], lo[2], hi[0], hi[1], hi[2], tex)], **keys)


def table(out, x0, y0, x1, y1, z, h=32, top=None):
    """A steel table: a top 4 thick and four legs."""
    top = top or TX["floor2"]
    dbox(out, (x0, y0, z + h - 4), (x1, y1, z + h), {"top": top, "side": TX["lamp_frame"], "bottom": TX["lamp_frame"]})
    for x in (x0 + 2, x1 - 6):
        for y in (y0 + 2, y1 - 6):
            dbox(out, (x, y, z), (x + 4, y + 4, z + h - 4), TX["lamp_frame"])


# ---- Room 1: locomotion and turning. A plain room (nothing to touch), a board, arrows to a bending hall.
def build_room1():
    out = group("room1")
    room("room1", 0, 0, 640, 512, 0, 224, bands(TX["panel"]))
    d = doorway("room1_exit", 640, 192, 656, 320, 0, out)
    lamp_grid(out, 0, 0, 640, 512, 224, 3, 2)
    ent("info_player_start", 96, 256, 24, angle=0)
    checkpoint("cp1", 96, 256, 0, 0)
    banner(N.join(["WELCOME TO QUAKE VR", "LESSON 1 OF 12: MOVING", "", "MOVE: push the left stick.", "TURN: push the right stick left or right.",
                   "Or simply turn your head and body.", "", "Follow the yellow arrows."]), 634, 416, 100, 180, "0.32")
    arrows([(176, 256), (272, 256), (368, 256), (464, 256), (560, 256), (650, 256)], 0)
    tip("t2_move", "Push the left stick to walk." + N + "Push the right stick sideways to turn.", 220, 256, 60, 220)
    return d


# ---- The bending hall: east from room 1, a curve north, a curve east, into room 2's vestibule.
def build_bend():
    st = bands(TX["panel3"])
    air("bend_a", (656, 192, 0), (720, 320, 160), style=st, open=("+x",))
    arc_hall(720, 448, 128, 256, -90, 0, 0, 160, st)
    air("bend_b", (848, 448, 0), (976, 512, 160), style=st, open=("-y", "+y"))
    arc_hall(1104, 512, 128, 256, 180, 90, 0, 160, st)
    air("bend_c", (1104, 640, 0), (1168, 768, 160), style=st, open=("-x",))
    out = group("bend")
    pts = [(688, 256)] + [arc_point(720, 448, 192, a) for a in (-75, -45, -15)] + [(912, 480)] + \
          [arc_point(1104, 512, 192, a) for a in (165, 135, 105)] + [(1136, 704), (1180, 704)]
    arrows(pts, 0, 40)
    for (x, y) in ((688, 256), (912, 480), (1136, 704)):
        ceiling_lamp(out, x, y, 160, 32, 32, 200)
    for a, (cx, cy) in ((-45, (720, 448)), (135, (1104, 512))):
        px, py = arc_point(cx, cy, 192, a)
        light(px, py, 130, 200)
    tip("t2_turn", "The hall bends: turn with the right stick" + N + "(or turn your body) to follow it.",
        700, 256, 60, 160)


# ---- Room 2: buttons. A vestibule (a closed door, a button beside it), then the settings room.
def build_room2():
    out = group("room2")
    room("room2a", 1184, 512, 1440, 896, 0, 224, bands(TX["panel2"]))
    doorway("room2a_in", 1168, 640, 1184, 768, 0, out)
    d = doorway("room2_door", 1440, 640, 1456, 768, 0, out)
    sliding_door(d, "r2_door")
    checkpoint("cp2", 1232, 704, 0, 0)
    button("PUSH", None, 1440, 592, 56, 0, target="r2_door", size=20, wait="-1")
    lamp_grid(out, 1184, 512, 1440, 896, 224, 1, 3)
    banner(N.join(["LESSON 2: BUTTONS", "Push the button beside the door.", "Your hand, anything you hold, your body",
                   "or something you throw presses a button."]), 1432, 704, 176, 180, "0.28")
    tip("t2_button", "Reach out and push the button" + N + "with your hand.", 1428, 592, 76, 150)
    # ---- the settings room: moving and turning
    room("room2b", 1456, 384, 2096, 1024, 0, 256, bands(TX["panel"]))
    skylight(1712, 640, 1840, 768, 256)
    doorway("room2b_exit", 2096, 640, 2112, 768, 0, out)
    lamp_grid(out, 1456, 384, 2096, 1024, 256, 3, 3)
    rows = [[("TURNING", "turning"), ("TURN SPEED", "turnspeed"), ("MOVE" + N + "TOWARDS", "movedir"),
             ("SWAP STICKS", "sticks")],
            [("RUN OR WALK", "run"), ("TELEPORT", "teleport"), ("STANDING" + N + "OR SEATED", "position"),
             ("WORLD SCALE", "scale")]]
    for row, entries in enumerate(rows):
        for (label, key), x in zip(entries, (1648, 1712, 1840, 1904)):
            setting_button(label, key, x, 1024, 44 + 52 * (1 - row), 90)
    banner(N.join(["MOVING AND TURNING", "Each button steps a setting and saves it.", "More: {menu:Locomotion}"]),
           1776, 1020, 180, 270, "0.3")
    tip("t2_settings", "Optional: these change how you move" + N + "and turn. Try Snap turning if smooth" + N +
        "turning makes you queasy.", 1776, 980, 76, 260)
    tip("t2_teleport", "Teleport: aim and release the stick" + N + "to jump to a spot (if you turn it on).",
        1712, 990, 40, 120)
    arrows([(1520, 704), (1648, 704), (1776, 704), (1904, 704), (2032, 704), (2104, 704)], 0)


# ---- Room 3: jumping and climbing (a courtyard). Low barriers to jump, a wall to climb by its ladder, a wall whose top
# is caught at the top of a jump; the way on at its top (z 240).
R3 = dict(x0=2256, y0=448, x1=3408, y1=960)
R3_B1, R3_B2 = 2576, 2768     # the barriers' west faces
R3_PLAT, R3_WALL = 2960, 3168  # the ladder block's west face, the jump wall's
R3_PLAT_Z = 144                # its top
COURT = Style(TX["court_floor"], None, [(0, 16, TX["skirting"]), (16, 144, TX["panel4"]), (144, 160, TX["rail"]),
                                         (160, None, TX["court"])], sky=True)


def build_room3():
    out = group("room3")
    r = R3
    air("hall23", (2112, 640, 0), (2240, 768, 160), style=bands(TX["panel3"]))
    doorway("room3_in", 2240, 640, 2256, 768, 0, out)
    room("room3", r["x0"], r["y0"], r["x1"], r["y1"], 0, 480, COURT).windows = (208, 352)
    checkpoint("cp3", 2320, 704, 0, 0)
    # the barriers (detail: hazard stripes): 32 and 40 high, across the court
    dbox(out, (R3_B1, r["y0"], 0), (R3_B1 + 16, r["y1"], 32), TX["hazard"])
    dbox(out, (R3_B2, r["y0"], 0), (R3_B2 + 32, r["y1"], 40), TX["hazard"])
    # the ladder block (structural: its faces in the court's bands) and the jump wall's block
    SOLIDS.append(((R3_PLAT, r["y0"], 0), (R3_WALL, r["y1"], R3_PLAT_Z)))
    SOLIDS.append(((R3_WALL, r["y0"], 0), (r["x1"], r["y1"], UP)))
    # hazard strips on the lips
    dbox(out, (R3_PLAT - 2, r["y0"], R3_PLAT_Z - 8), (R3_PLAT, r["y1"], R3_PLAT_Z), TX["hazard"])
    dbox(out, (R3_WALL - 2, r["y0"], UP - 8), (R3_WALL, r["y1"], UP), TX["hazard"])
    # the ladder: rungs every 20 units from 16 (vrclimb's), 6 off the wall, between two stiles
    ly0, ly1 = 680, 728
    for z in range(16, R3_PLAT_Z, 20):
        out.append(cylinder((R3_PLAT - 7, ly0, z), (R3_PLAT - 7, ly1, z), 1.75, 8, TX["rung"]))
    for y in (ly0 - 4, ly1):
        dbox(out, (R3_PLAT - 10, y, 0), (R3_PLAT, y + 4, R3_PLAT_Z + 24), TX["rung"])
    # the exit, up top
    doorway("room3_exit", r["x1"], 640, r["x1"] + 16, 768, UP, out)
    # wall lamps round the court (the sun does the rest)
    for x in (2448, 2688, 2880):
        wall_lamp(out, "-y", x, r["y0"], 120, 160)
        wall_lamp(out, "+y", x, r["y1"], 120, 160)
    wall_lamp(out, "-y", 3300, r["y0"], UP + 100, 160)
    wall_lamp(out, "+y", 3300, r["y1"], UP + 100, 160)
    arrows([(2336, 704), (2448, 704), (2528, 704)], 0)
    arrows([(2656, 704), (2720, 704)], 0)
    arrows([(2864, 704), (2928, 704)], 0)
    arrows([(3040, 704), (3120, 704)], R3_PLAT_Z)
    arrows([(3248, 704), (3328, 704), (3400, 704)], UP)
    wall_arrow("-x", R3_PLAT, 768, 96, "up", 40)
    banner(N.join(["LESSON 3: JUMPING AND CLIMBING", "", "JUMP: press A (the right controller).",
                   "Jump the low barriers."]), 2380, r["y1"] - 4, 120, 270, "0.32")
    banner(N.join(["CLIMB: grip a rung or a ledge with an empty hand", "and pull yourself up, hand over hand.",
                   "Pull up at the top to climb onto it."]), R3_PLAT - 4, 840, 112, 180, "0.28")
    banner(N.join(["TOO HIGH TO REACH?", "Jump, and grab the edge", "at the top of the jump."]),
           R3_WALL - 4, 600, R3_PLAT_Z + 120, 180, "0.3")
    tip("t2_jump", "Press A to jump." + N + "Jump over the yellow barriers.", 2500, 704, 56, 220)
    tip("t2_climb", "Grip a rung with an empty hand, pull" + N + "down; grip the next one with the other.",
        R3_PLAT - 16, 704, 70, 150)
    tip("t2_climb_off", "Can't take hold? Climbing may be off:" + N + "press CLIMBING to turn it on.",
        R3_PLAT - 16, 600, 64, 160)
    setting_button("CLIMBING", "climb", R3_PLAT, 600, 48, 0)
    tip("t2_catch", "Jump, reach up and grip the edge" + N + "as you rise: then pull yourself up.",
        R3_WALL - 24, 704, R3_PLAT_Z + 60, 160)


# ---- Room 4: swimming. A pool, a passage under water (down, round a corner, up into the next room's pool), then a room
# with a hole in its floor down to the lower floor (a fall that hurts).
R4A = dict(x0=3568, y0=448, x1=4080, y1=960)
R4B = dict(x0=3568, y0=1008, x1=4336, y1=1392)
R4C = dict(x0=4496, y0=1008, x1=4880, y1=1392)
POOL_A = ((3824, 704), (4016, 896))
POOL_B = ((4064, 1088), (4256, 1280))
POOL_FLOOR = 64
WATER_Z = UP - 12
SHAFT = ((4640, 1152), (4736, 1248))


def build_room4():
    out = group("room4")
    air("hall34", (3424, 640, UP), (3552, 768, UP + 160), style=bands(TX["panel3"]))
    doorway("room4_in", 3552, 640, 3568, 768, UP, out)
    pool_st = bands(TX["panel5"])
    r = R4A
    room("room4a", r["x0"], r["y0"], r["x1"], r["y1"], UP, UP + 256, pool_st)
    checkpoint("cp4", 3632, 704, UP, 0)
    (px0, py0), (px1, py1) = POOL_A
    air("pool_a", (px0, py0, POOL_FLOOR), (px1, py1, UP), kind="pool")
    WATER.append(((px0, py0, POOL_FLOOR), (px1, py1, WATER_Z)))
    # the passage: north under the walls, then east, into pool B
    air("swim_1", (3872, py1, POOL_FLOOR), (3968, 1184, POOL_FLOOR + 96), kind="pool")
    air("swim_2", (3968, 1088, POOL_FLOOR), (POOL_B[0][0], 1184, POOL_FLOOR + 96), kind="pool")
    WATER.append(((3872, py1, POOL_FLOOR), (3968, 1184, POOL_FLOOR + 96)))
    WATER.append(((3968, 1088, POOL_FLOOR), (POOL_B[0][0], 1184, POOL_FLOOR + 96)))
    # steps out of pool A (its south-west corner): 48 x 48 blocks
    # steps out of the pools: 16 high (a step up), 24 deep, the top one 16 under the floor
    for i in range(4):
        SOLIDS.append(((px0, py0, POOL_FLOOR), (px0 + 24 * (i + 1), py0 + 64, UP - 16 * (i + 1))))
    railing(out, (px0 + 64, py0 - 8), (px1, py0 - 8), UP)
    railing(out, (px1 + 8, py0), (px1 + 8, py1), UP)
    lamp_grid(out, r["x0"], r["y0"], r["x1"], r["y1"], UP + 256, 2, 2, 260)
    light((px0 + px1) / 2, (py0 + py1) / 2, POOL_FLOOR + 40, 260, "0.7 0.85 1", wait="0.6")
    light(3920, 1040, POOL_FLOOR + 48, 220, "0.7 0.85 1")
    light(4016, 1136, POOL_FLOOR + 48, 220, "0.7 0.85 1")
    setting_button("SWIMMING", "swim", 3760, r["y1"], UP + 48, 90)
    banner(N.join(["LESSON 4: SWIMMING", "Dive in, swim down into the passage,", "round the corner and up into the next room.",
                   "Immersive: stroke with your arms.", "Vanilla: the stick moves you where you look;", "A swims up."]),
           3760, r["y1"] - 4, UP + 150, 270, "0.28")
    tip("t2_swim", "Immersive swimming: pull your arms" + N + "through the water like a breast stroke." + N +
        "Press SWIMMING for the stick instead.", 3760, r["y1"] - 30, UP + 64, 200)
    tip("t2_dive", "Dive in here: the way on is" + N + "under the water, to the north.", px0 + 96, py0 + 96, UP + 40,
        200)
    arrows([(3632, 704), (3728, 800), (3800, 800)], UP)
    # ---- room 4b: pool B, then on east
    r = R4B
    room("room4b", r["x0"], r["y0"], r["x1"], r["y1"], UP, UP + 224, pool_st)
    (qx0, qy0), (qx1, qy1) = POOL_B
    air("pool_b", (qx0, qy0, POOL_FLOOR), (qx1, qy1, UP), kind="pool")
    WATER.append(((qx0, qy0, POOL_FLOOR), (qx1, qy1, WATER_Z)))
    for i in range(4):
        SOLIDS.append(((qx1 - 24 * (i + 1), qy1 - 64, POOL_FLOOR), (qx1, qy1, UP - 16 * (i + 1))))
    lamp_grid(out, r["x0"], r["y0"], r["x1"], r["y1"], UP + 224, 3, 2, 240)
    light((qx0 + qx1) / 2, (qy0 + qy1) / 2, POOL_FLOOR + 40, 260, "0.7 0.85 1", wait="0.6")
    doorway("room4b_exit", r["x1"], 1136, r["x1"] + 16, 1264, UP, out)
    air("hall4bc", (4352, 1136, UP), (R4C["x0"] - 16, 1264, UP + 160), style=bands(TX["panel3"]))
    doorway("room4c_in", R4C["x0"] - 16, 1136, R4C["x0"], 1264, UP, out)
    tip("t2_out", "Climb out: grip the edge and pull," + N + "or swim to the steps.", qx0 + 96, qy0 + 96, UP + 30, 200)
    arrows([(4200, 1312), (4300, 1200), (4344, 1200)], UP)
    # ---- room 4c: the hole
    r = R4C
    room("room4c", r["x0"], r["y0"], r["x1"], r["y1"], UP, UP + 224, bands(TX["panel2"]))
    (sx0, sy0), (sx1, sy1) = SHAFT
    air("shaft", (sx0, sy0, 64), (sx1, sy1, UP), kind="shaft", style=Style(bands=[(0, None, TX["upper"])]))
    for (lo, hi) in (((sx0 - 16, sy0 - 16), (sx1 + 16, sy0)), ((sx0 - 16, sy1), (sx1 + 16, sy1 + 16)),
                     ((sx0 - 16, sy0), (sx0, sy1)), ((sx1, sy0), (sx1 + 16, sy1))):
        dbox(out, (lo[0], lo[1], UP), (hi[0], hi[1], UP + 1), TX["hazard"])
    railing(out, (sx0 - 24, sy0 - 24), (sx1 + 24, sy0 - 24), UP)
    railing(out, (sx0 - 24, sy1 + 24), (sx1 + 24, sy1 + 24), UP)
    railing(out, (sx1 + 24, sy0 - 24), (sx1 + 24, sy1 + 24), UP)
    lamp_grid(out, r["x0"], r["y0"], r["x1"], r["y1"], UP + 224, 2, 2)
    banner(N.join(["THE ONLY WAY ON IS DOWN", "Jump into the hole.", "A long fall hurts a little:",
                   "the next room has health."]), r["x1"] - 4, 1200, UP + 120, 180, "0.3")
    tip("t2_hole", "Step into the hole. Falling this far" + N + "costs you a little health.", sx0 - 40, 1200,
        UP + 50, 180)
    arrows([(4528, 1200), (4592, 1200)], UP)


# ---- Room 5: healing and props (under room 4c). Health kits; grabbing, force grabbing, putting away; a door that opens
# only at full health.
R5 = dict(x0=4432, y0=960, x1=4944, y1=1472)


def build_room5():
    out = group("room5")
    r = R5
    room("room5", r["x0"], r["y0"], r["x1"], r["y1"], LOW, 64, bands(TX["panel"]))
    checkpoint("cp5", 4688, 1200, LOW, 0, 128)
    lamp_grid(out, r["x0"], r["y0"], r["x1"], r["y1"], 64, 2, 2)
    # the health kits: on a table (by hand), on the floor (walked over), out of reach on a shelf (force grab)
    # (tables 40 deep, things to take 16 in from the near edge: in reach of a player against it, leaning a little)
    table(out, 4480, 1376, 4608, 1416, LOW)
    for x in (4512, 4576):
        item("item_health", x, 1392, LOW + 34, spawnflags=1)
    item("item_health", 4560, 1040, LOW + 2, spawnflags=1)
    dbox(out, (4860, 1360, LOW + 96), (r["x1"], 1440, LOW + 100), {"side": TX["lamp_frame"], "top": TX["floor2"]})
    item("item_health", 4880, 1384, LOW + 102, spawnflags=1)
    restock("item_health", 4544, 1392, LOW + 34, contentsflags=1, distance=128, wait=6)
    banner(N.join(["LESSON 5: HEALING", "Health kits mend you. Grip one and hold it", "to your body, or put it in a holster:",
                   "at your hips, over your shoulders."]), 4544, r["y1"] - 4, LOW + 120, 270, "0.28")
    banner(N.join(["GRAB: close your hand round it.", "FORCE GRAB: point at something far", "and grip: it flies to your hand.",
                   "COLLECT: put it in a holster, your pouch,", "or over your shoulder into your pack."]),
           r["x1"] - 4, 1260, LOW + 104, 180, "0.28")
    tip("t2_health", "You are hurt. Take a health kit:" + N + "grip it, then hold it to your belly" + N +
        "or a holster.", 4544, 1380, LOW + 70, 200)
    tip("t2_forcegrab", "Point at the kit on the shelf" + N + "and grip: it flies to your hand.", 4840, 1384,
        LOW + 110, 220)
    # the door: opens only at full health (QC trigger_vr_health_gate, in front of it)
    d = doorway("room5_exit", r["x1"], 1152, r["x1"] + 16, 1280, LOW, out)
    sliding_door(d, "r5_door")
    trigger("r5_gate", (r["x1"] - 64, 1152, LOW), (r["x1"], 1280, LOW + 96), "trigger_vr_health_gate",
            target="r5_door", message="Heal to full health (100) first:" + N + "take a health kit.")
    banner(N.join(["THIS DOOR OPENS", "ONLY AT FULL HEALTH"]), r["x1"] - 4, 1216, LOW + 156, 180, "0.3")
    arrows([(4700, 1216), (4800, 1216), (4900, 1216)], LOW)


# ---- Room 6: melee and the keycard. A crate holds the key; punch it open, take the card, open the door.
R6 = dict(x0=4960, y0=1088, x1=5472, y1=1472)


def build_room6():
    out = group("room6")
    r = R6
    room("room6", r["x0"], r["y0"], r["x1"], r["y1"], LOW, 64, bands(TX["panel4"]))
    skylight(5152, 1280, 5280, 1408, 64)
    checkpoint("cp6", 5024, 1216, LOW, 0)
    lamp_grid(out, r["x0"], r["y0"], r["x1"], r["y1"], 64, 2, 2)
    ent("vr_crate", 5216, 1344, LOW + 2, spawnflags=1, angle=20, skin=1, contents="item_key1", target="r6_crate")
    for (x, y, a) in ((5408, 1408, 5), (5400, 1160, 40)):
        ent("vr_crate", x, y, LOW + 2, angle=a, skin=2)
    banner(N.join(["LESSON 6: MELEE", "Make a fist and punch the crate", "until it breaks. Swing hard!"]),
           5216, r["y1"] - 4, LOW + 120, 270, "0.3")
    tip("t2_punch", "Make a fist (grip and trigger)" + N + "and punch the crate until it breaks.", 5216, 1300,
        LOW + 60, 200)
    tip("t2_key", "A keycard! Grip it, then hold it" + N + "to a holster to keep it.", 5216, 1320, LOW + 40, 300,
        delay=0, trig="r6_crate")
    d = doorway("room6_exit", 5152, r["y0"] - 16, 5280, r["y0"], LOW, out)
    sliding_door(d, None, keys={"spawnflags": 16})
    banner(N.join(["KEYCARD DOOR", "Walk up to it with the card."]), 5216, r["y0"] + 4, LOW + 156, 90, "0.3")




# ---- Room 7: a fist fight. A warning, a grunt from the alcove, its rifle, three targets to shoot, a button for another.
R7 = dict(x0=4896, y0=416, x1=5536, y1=928)


def build_room7():
    out = group("room7")
    r = R7
    air("hall67", (5152, 944, LOW), (5280, 1072, LOW + 160), style=bands(TX["panel3"]))
    doorway("room7_in", 5152, 928, 5280, 944, LOW, out)
    room("room7", r["x0"], r["y0"], r["x1"], r["y1"], LOW, LOW + 288, bands(TX["panel2"]))
    checkpoint("cp7", 5216, 880, LOW, 270)
    lamp_grid(out, r["x0"], r["y0"], r["x1"], r["y1"], LOW + 288, 3, 2, 220)
    banner(N.join(["LESSON 7: A FIGHT", "WARNING: AN ENEMY IS COMING", "Fight it with your fists:", "block its blows, then strike."]),
           5216, 1068, LOW + 104, 270, "0.3")
    # the alcove the grunts come from (a spawner in it); the button for another
    air("r7_alcove", (r["x1"], 608, LOW), (r["x1"] + 64, 736, LOW + 128), style=bands(TX["panel5"]))
    door_frame(out, door_air("r7_alcove_mouth", (r["x1"], 608, LOW), (r["x1"] + 1, 736, LOW + 128)))
    wall_lamp(out, "+x", r["x1"] + 64, 672, LOW + 100, 120, "1 0.5 0.4")
    ent("func_vr_spawner", r["x1"] + 32, 672, LOW + 32, weapon=0, angle=180, spawnflags=3, targetname="r7_spawn",
        target="r7_dead", message="Finish this one first.")
    trigger("r7_start", (5152, 800, LOW), (5280, 928, LOW + 96), target="r7_spawn", delay=2)
    button("ANOTHER" + N + "ENEMY", None, 5408, r["y1"], LOW + 52, 90, target="r7_spawn", size=20)
    tip("t2_newenemy", "Out of bullets, or want practice?" + N + "Press for another enemy.", 5408, r["y1"] - 20,
        LOW + 76, 150)
    tip("t2_parry", "PARRY: hold your fist or weapon across" + N + "an enemy's blow to block it: it staggers," + N +
        "open to your strike.", 5216, 780, LOW + 70, 220)
    tip("t2_rifle", "Take its rifle: grip it by its handle," + N + "aim, pull the trigger. Its magazine is all" + N +
        "it has: it can't be reloaded.", 5216, 640, LOW + 40, 500, delay=0, trig="r7_dead")
    # three targets to shoot (shootable buttons) on the west wall
    for y in (544, 672, 800):
        button("", None, r["x0"], y, LOW + 120, 180, size=28, target="r7_count", tex=TX["shoot"], health=1,
               wait="-1")
    ent("trigger_counter", 5216, 672, LOW + 64, count=3, targetname="r7_count", target="r7_open")
    banner(N.join(["SHOOT ALL THREE TARGETS", "with the enemy's rifle"]), r["x0"] + 4, 672, LOW + 200, 0, "0.3")
    d = doorway("room7_exit", 5152, r["y0"] - 16, 5280, r["y0"], LOW, out)
    sliding_door(d, "r7_open")


# ---- Room 8: weapons and reloading. A shotgun and shells on the bench; a firing range of targets that take more
# shots than the gun holds (it must be reloaded); holsters, two-handed aiming; weapon settings.
R8 = dict(x0=4768, y0=-544, x1=5664, y1=400)
R8_COUNTER = 144    # the counter's south face (the stand is north of it)
R8_TARGETS = [(4880, -64, 48), (5040, -224, 64), (5200, -96, 40), (5360, -320, 72), (5520, -160, 56),
              (4960, -400, 80), (5280, -464, 48), (5600, -416, 96), (5120, -32, 88), (5440, 16, 60)]


def build_room8():
    out = group("room8")
    r = R8
    room("room8", r["x0"], r["y0"], r["x1"], r["y1"], LOW, LOW + 256, bands(TX["panel"]))
    for sx in (4992, 5376):
        skylight(sx, -352, sx + 128, -224, LOW + 256)
    checkpoint("cp8", 5216, 352, LOW, 270)
    lamp_grid(out, r["x0"], R8_COUNTER, r["x1"], r["y1"], LOW + 256, 4, 1, 200)
    lamp_grid(out, r["x0"], r["y0"], r["x1"], R8_COUNTER, LOW + 256, 3, 3, 220)
    # the counter, and a clip wall over it to the ceiling (shots pass, the player doesn't)
    dbox(out, (r["x0"], R8_COUNTER, LOW), (r["x1"], R8_COUNTER + 16, LOW + 40),
         {"top": TX["floor2"], "side": TX["hazard"]})
    CLIPS.append(((r["x0"], R8_COUNTER, LOW + 40), (r["x1"], R8_COUNTER + 16, LOW + 256)))
    # the bench: the shotgun and shells
    table(out, 4832, 352, 5088, 392, LOW, 36)
    ent("weapon_shotgun", 4880, 368, LOW + 40)
    for x in (4976, 5040):
        item("item_shells", x, 368, LOW + 40)
    restock("weapon_shotgun", 4880, 368, LOW + 40, distance=128, wait=8)
    restock("item_shells", 5008, 368, LOW + 40, count=2, distance=96, wait=6)
    # the targets: boards on posts (QC func_vr_target), each needing a hit
    for i, (x, y, z) in enumerate(R8_TARGETS):
        dbox(out, (x - 2, y - 2, LOW), (x + 2, y + 2, LOW + z - 16), TX["rail_post"])
        lo, hi = (x - 16, y - 2, LOW + z - 16), (x + 16, y + 2, LOW + z + 16)
        b = mapgeom.box(lo[0], lo[1], lo[2], hi[0], hi[1], hi[2],
                        Box(lo, hi, {"+y": TX["target"], "-y": TX["target"], "side": TX["wood"], "top": TX["wood"],
                                     "bottom": TX["wood"]}, fit=("+y", "-y")))
        bent("func_vr_target", [b], health=10, target="r8_count")
    ent("trigger_counter", 5216, 0, LOW + 64, count=len(R8_TARGETS), targetname="r8_count", target="r8_open")
    # weapon settings on the north wall
    for (label, key), x in zip([("RELOADING", "reload"), ("WEAPON MODE", "holsters"),
                                ("TWO-HANDED" + N + "AIM", "twohand"), ("WEAPON GRIP", "grip"),
                                ("CROSSHAIR", "crosshair")], (5376, 5440, 5504, 5568, 5632)):
        setting_button(label, key, x, r["y1"], LOW + 64, 90)
    banner(N.join(["LESSON 8: WEAPONS", "Take the shotgun from the bench: grip it.", "Load shells: take one from the pouch at",
                   "your belly, push it into the gun's port.", "Destroy every target to go on."]),
           5216, r["y1"] - 4, LOW + 160, 270, "0.28")
    banner(N.join(["HOLSTERS: let go of a gun at your hip", "or shoulder to put it away; grip there",
                   "to draw it again."]), 4928, r["y1"] - 4, LOW + 150, 270, "0.25")
    banner(N.join(["TWO HANDS: grip the gun's front with", "your other hand to steady it. Virtual",
                   "stock: hold it to your shoulder."]), 5504, r["y1"] - 4, LOW + 150, 270, "0.25")
    tip("t2_shotgun", "Grip the shotgun to take it.", 4880, 350, LOW + 70, 160)
    tip("t2_shells", "Shells: grip the box and hold it to" + N + "a holster (or your belly): they go" + N +
        "to your ammo pouch.", 5008, 350, LOW + 70, 160)
    tip("t2_reload", "To load: with your other hand grip at" + N + "your belly pouch for a shell, then push it" + N +
        "into the port under the gun.", 5216, 260, LOW + 60, 200)
    tip("t2_range", "Each target needs a good hit. The gun" + N + "holds 8 shells: reload when it clicks.",
        5216, 190, LOW + 64, 220)
    d = doorway("room8_exit", r["x0"] - 16, 224, r["x0"], 352, LOW, out)
    sliding_door(d, "r8_open")


# ---- Room 9: darkness. A lit hall (the flashlight's tips), then a dark course: a serpentine with blocks to climb.
R9V = dict(x0=4528, y0=160, x1=4752, y1=352)
R9 = dict(x0=3456, y0=-352, x1=4512, y1=352)
DARK = Style(TX["dark2"], TX["dark2"], [(0, 16, TX["dark"]), (16, None, TX["dark"])])
R9_WALLS = [(4256, -224, 352), (4000, -352, 224), (3744, -224, 352)]     # (x, y0, y1): 16 thick
R9_BLOCKS = [((4272, -32), (4512, 16), 40), ((4016, -128), (4256, 96), 40), ((3760, 32), (4000, 80), 48),
             ((3456, -160), (3744, -112), 40), ((3456, -112), (3744, 64), 80)]


def build_room9():
    out = group("room9")
    r = R9V
    room("room9v", r["x0"], r["y0"], r["x1"], r["y1"], LOW, LOW + 192, bands(TX["panel2"]))
    lamp_grid(out, r["x0"], r["y0"], r["x1"], r["y1"], LOW + 192, 1, 1, 260)
    checkpoint("cp9", 4704, 288, LOW, 180)
    banner(N.join(["LESSON 9: DARKNESS", "Your flashlight hangs at your hip:", "grip it; pull the trigger to switch",
                   "it on or off."]), 4640, r["y0"] + 4, LOW + 120, 90, "0.28")
    tip("t2_torch", "Take your flashlight: grip at your hip" + N + "(the side set by TORCH SIDE), trigger" + N +
        "to switch it on.", 4640, 200, LOW + 60, 200)
    setting_button("TORCH SIDE", "torch", 4592, r["y0"], LOW + 52, 270)
    d = doorway("room9_in", R9["x1"], 192, R9["x1"] + 16, 320, LOW, out)
    sliding_door(d, None)
    # the course: no lamps; dark metal
    r = R9
    room("room9", r["x0"], r["y0"], r["x1"], r["y1"], LOW, LOW + 192, DARK)
    for (x, y0, y1) in R9_WALLS:
        SOLIDS.append(((x, y0, LOW), (x + 16, y1, LOW + 192)))
    for (lo, hi, h) in R9_BLOCKS:
        SOLIDS.append(((lo[0], lo[1], LOW), (hi[0], hi[1], LOW + h)))
    tip("t2_flip", "Press B or Y with the flashlight in hand" + N + "to flip it round in your fist.", 4400, 200,
        LOW + 60, 160)
    tip("t2_clipgun", "Hold the flashlight to a gun in your other" + N + "hand and press B or Y: it clips on.",
        4136, -260, LOW + 60, 160)
    tip("t2_cliphead", "Hold it to your temple and press B or Y:" + N + "a head torch, both hands free.",
        3880, 260, LOW + 60, 160)
    tip("t2_climbdark", "Climb in the dark: light the edge first.", 3600, -200, LOW + 60, 160)
    d = doorway("room9_exit", r["x0"] - 16, 192, r["x0"], 320, LOW, out)
    sliding_door(d, None)


# ---- Room 10: throwing (a courtyard). A button too high to reach; rocks and bricks to throw at it. Rockets, grenades.
R10 = dict(x0=2560, y0=-352, x1=3440, y1=352)
R10_GRATE = (2880, 2976)      # the barred opening in the south wall (x), the button 48 behind it


def build_room10():
    out = group("room10")
    r = R10
    room("room10", r["x0"], r["y0"], r["x1"], r["y1"], LOW, 352, COURT).windows = (240, 368)
    checkpoint("cp10", 3380, 256, LOW, 180)
    wall_lamp(out, "-y", 3000, r["y0"], LOW + 120, 160)
    wall_lamp(out, "+y", 3000, r["y1"], LOW + 120, 160)
    # the button, out of reach behind a grate in the south wall (an alcove 48 deep): a thrown rock goes through the bars
    gx0, gx1 = R10_GRATE
    door_frame(out, door_air("r10_grate", (gx0, r["y0"] - 16, LOW + 16), (gx1, r["y0"], LOW + 112)))
    air("r10_alcove", (gx0, r["y0"] - 64, LOW), (gx1, r["y0"] - 16, LOW + 128), style=bands(TX["panel5"]))
    for x in range(gx0 + 12, gx1, 24):   # (bars 24 apart: a rock goes between them, the player does not)
        out.append(cylinder((x, r["y0"] - 8, LOW + 16), (x, r["y0"] - 8, LOW + 112), 2, 8, TX["rung"]))
    bx = (gx0 + gx1) // 2
    dbox(out, (bx - 32, r["y0"] - 64, LOW + 32), (bx + 32, r["y0"] - 62, LOW + 96), TX["hazard"])
    button("HIT ME", None, bx, r["y0"] - 62, LOW + 64, 270, depth=8, size=40, target="r10_open", wait="-1",
           scale="0.3")
    wall_lamp(out, "-y", bx, r["y0"] - 64, LOW + 112, 120)
    # the rocks and bricks on a table beside it, more as they go
    table(out, 2752, -320, 2848, -280, LOW, 32)
    for i, mdl in enumerate(("vr_rock1", "vr_brick1", "vr_rock3", "vr_brick2", "vr_rock5")):
        ent("vr_debris_piece", 2768 + 16 * i, -296, LOW + 40, model="progs/%s.mdl" % mdl, angle=RND.randrange(360))
    restock("vr_debris_piece", 2784, -296, LOW + 40, count=3, distance=64, wait=3, model="progs/vr_rock3.mdl")
    restock("vr_debris_piece", 2816, -296, LOW + 40, count=3, distance=64, wait=3, model="progs/vr_brick1.mdl")
    banner(N.join(["LESSON 10: THROWING", "The button is out of reach behind the bars.", "Throw a rock or a brick at it:",
                   "grip, swing, let go."]), 2928, r["y0"] + 4, LOW + 160, 90, "0.3")
    tip("t2_throw", "Grip a rock, swing your arm and let go" + N + "as it comes forward. Hit the button.",
        2800, -260, LOW + 70, 200)
    # rockets: the back pouch, grenades
    table(out, 3200, -320, 3328, -280, LOW, 32)
    for x in (3232, 3296):
        item("item_rockets", x, -296, LOW + 36)
    restock("item_rockets", 3264, -296, LOW + 36, distance=96, wait=8)
    banner(N.join(["GRENADES", "With rockets in your pack, reach behind you:", "the pouch at the small of your back gives a grenade.",
                   "Trigger pulls the pin; throw it. Or drop it", "where they will come: a trap. No launcher needed."]),
           3264, r["y0"] + 4, LOW + 120, 90, "0.26")
    tip("t2_grenade", "Take the rockets (hold the box to a holster)." + N + "Then reach to the small of your back" + N + "for a grenade.",
        3264, -296, LOW + 64, 180)
    d = doorway("room10_exit", r["x0"] - 16, -256, r["x0"], -128, LOW, out)
    sliding_door(d, "r10_open")
    # windows into room 11 (high, barred): openings in the shared wall
    for y in (-96, 96):
        door_air("r10_window_%d" % y, (r["x0"] - 16, y - 48, LOW + 128), (r["x0"], y + 48, LOW + 192))
        for yy in range(y - 40, y + 41, 16):
            out.append(cylinder((r["x0"] - 8, yy, LOW + 128), (r["x0"] - 8, yy, LOW + 192), 1.5, 8, TX["rung"]))


# ---- Room 11: fire. Wall torches; a passage stacked with crates: burn it. Beyond: torches, a nailgun, nails.
R11A = dict(x0=2160, y0=-288, x1=2544, y1=288)
R11B = dict(x0=1664, y0=-288, x1=2032, y1=288)
TORCH = {"light": "200", "_color": "1 0.6 0.28", "wait": "1.1"}


def wall_torch(out, side, x, y, z):
    """A wall torch (QC light_torch_small_walltorch: taken off its bracket by hand) on a bracket from the wall."""
    n = wall_axes(side)
    yaw = math.degrees(math.atan2(n[1], n[0]))
    out.append(beam((x - n[0] * 1, y - n[1] * 1, z - 4), (x + n[0] * 6, y + n[1] * 6, z - 4), 3, 3, TX["rung"]))
    ent("light_torch_small_walltorch", x + n[0] * 7, y + n[1] * 7, z, angle=round(yaw), **TORCH)


def build_room11():
    out = group("room11")
    r = R11A
    room("room11a", r["x0"], r["y0"], r["x1"], r["y1"], LOW, LOW + 256, bands(TX["panel3"]))
    checkpoint("cp11", 2480, -256, LOW, 180)
    lamp_grid(out, r["x0"], r["y0"], r["x1"], r["y1"], LOW + 256, 2, 2, 200)
    for (side, x, y) in (("-y", 2272, r["y0"]), ("-y", 2432, r["y0"]), ("+y", 2272, r["y1"]), ("+y", 2432, r["y1"])):
        wall_torch(out, side, x, y, LOW + 64)
    # the passage, stacked with crates
    doorway("r11_pass_in", r["x0"] - 16, -64, r["x0"], 64, LOW, out)
    air("r11_pass", (2048, -64, LOW), (r["x0"] - 16, 64, LOW + 128), style=bands(TX["panel5"]))
    doorway("r11_pass_out", 2032, -64, 2048, 64, LOW, out)
    k = 0
    for zl in (0, 48):
        for x in (2072, 2120):
            for y in (-42, 0, 42):
                ent("vr_crate", x, y, LOW + 2 + zl, spawnflags=1, angle=(k * 7) % 20, skin=k % 3)
                k += 1
    for (x, y) in ((2190, -100), (2190, 100), (2220, -128)):
        ent("vr_barrel", x, y, LOW + 18, skin=k % 3)
        k += 1
    banner(N.join(["LESSON 11: FIRE", "Take a torch off the wall: grip it.", "Set the crates alight and watch it spread.",
                   "Enemies burn too. So do you: careful!"]), r["x1"] - 4, 0, LOW + 120, 180, "0.3")
    tip("t2_torch_take", "Grip a torch and pull it off the wall.", 2272, r["y0"] + 24, LOW + 64, 160)
    tip("t2_burn", "Touch the crates with the flame" + N + "or hit them with the torch.", 2200, 0, LOW + 64, 160)
    # ---- beyond: the nailgun
    r = R11B
    room("room11b", r["x0"], r["y0"], r["x1"], r["y1"], LOW, LOW + 256, bands(TX["panel"]))
    skylight(1792, -64, 1920, 64, LOW + 256)
    lamp_grid(out, r["x0"], r["y0"], r["x1"], r["y1"], LOW + 256, 2, 2, 220)
    for (side, x, y) in (("-x", r["x0"], -128), ("-x", r["x0"], 128)):
        wall_torch(out, side, x, y, LOW + 64)
    table(out, 1760, 192, 1920, 232, LOW, 36)
    ent("weapon_nailgun", 1792, 208, LOW + 40)
    for x in (1856, 1888):
        item("item_spikes", x, 208, LOW + 40)
    restock("item_spikes", 1872, 208, LOW + 40, count=2, distance=128, wait=6)
    restock("weapon_nailgun", 1792, 208, LOW + 40, distance=128, wait=8)
    banner(N.join(["LAVA NAILS", "Fire nails through a torch's flame:", "they come out burning.",
                   "(Dissolution of Eternity's lava nails)"]), 1840, r["y1"] - 4, LOW + 120, 270, "0.28")
    tip("t2_lavanails", "Hold a torch in front of the nailgun" + N + "and fire through the flame.", 1840, 200,
        LOW + 70, 180)
    d = doorway("room11_exit", 1792, r["y0"] - 16, 1920, r["y0"], LOW, out)
    sliding_door(d, None)


# ---- Room 12: the arena (a courtyard). The door shuts, a countdown, three waves; then the way out to the slipgate.
R12 = dict(x0=256, y0=-1536, x1=1408, y1=-384)
PIT = ((640, -1152), (1024, -768))
ARENA_POOL = ((704, -1120), (960, -992))
MEZZ_N = ((256, -512), (1408, -384))
MEZZ_W = ((256, -1408), (384, -512))
MEZZ_Z = LOW + 176


def build_room12():
    out = group("room12")
    air("hall_12a", (1792, -528, LOW), (1920, -304, LOW + 160), style=bands(TX["panel3"]))
    air("hall_12b", (1424, -656, LOW), (1920, -528, LOW + 160), style=bands(TX["panel3"]))
    r = R12
    room("room12", r["x0"], r["y0"], r["x1"], r["y1"], LOW, 400, COURT).windows = (272, 416)
    d = doorway("room12_in", r["x1"], -656, r["x1"] + 16, -528, LOW, out)
    # the door stays open until he is in, then shuts (START_OPEN, used once)
    sliding_door(d, "r12_shut", keys={"spawnflags": 1}, wait=-1)
    checkpoint("cp12", 1340, -592, LOW, 180)
    # the mezzanines (structural), their ladders
    for lo, hi in (MEZZ_N, MEZZ_W):
        SOLIDS.append(((lo[0], lo[1], LOW), (hi[0], hi[1], MEZZ_Z)))
    for z in range(16, MEZZ_Z - LOW, 20):
        out.append(cylinder((704, -519, LOW + z), (752, -519, LOW + z), 1.75, 8, TX["rung"]))
        out.append(cylinder((391, -1000, LOW + z), (391, -952, LOW + z), 1.75, 8, TX["rung"]))
    railing(out, (384, -520), (1408, -520), MEZZ_Z, 36)
    # the pit, its stairs, the pool at its bottom
    (px0, py0), (px1, py1) = PIT
    air("arena_pit", (px0, py0, LOW - 128), (px1, py1, LOW), style=bands(TX["panel5"]))
    for i in range(8):
        SOLIDS.append(((768, py1 - 24 * (i + 1), LOW - 128), (896, py1 - 24 * i, LOW - 16 * (i + 1))))
    (qx0, qy0), (qx1, qy1) = ARENA_POOL
    air("arena_pool", (qx0, qy0, LOW - 240), (qx1, qy1, LOW - 128), kind="pool")
    WATER.append(((qx0, qy0, LOW - 240), (qx1, qy1, LOW - 140)))
    for (a, b) in (((px0, py0 - 8), (px1, py0 - 8)), ((px0 - 8, py0), (px0 - 8, py1)),
                   ((px1 + 8, py0), (px1 + 8, py1))):
        railing(out, a, b, LOW, 36)
    # cover: blocks and crates
    for (x, y, w, h) in ((1152, -1296, 64, 64), (448, -800, 64, 96), (1200, -896, 96, 48), (496, -1344, 48, 48)):
        SOLIDS.append(((x, y, LOW), (x + w, y + w, LOW + h)))
    for (x, y, sk) in ((1100, -760, 0), (1140, -760, 1), (1120, -720, 2), (600, -1460, 0), (420, -1200, 1)):
        ent("vr_crate", x, y, LOW + 2, angle=RND.randrange(30), skin=sk)
    for (x, y) in ((1300, -1450), (1330, -1430), (300, -650)):
        ent("vr_barrel", x, y, LOW + 18)
    # weapons and supplies by the entrance; health round the walls
    table(out, 1352, -800, 1392, -672, LOW, 36)
    ent("weapon_shotgun", 1368, -700, LOW + 40, angle=90)
    ent("weapon_nailgun", 1368, -768, LOW + 40, angle=90)
    table(out, 1352, -960, 1392, -832, LOW, 36)
    for (cls, y) in (("item_shells", -856), ("item_spikes", -896), ("item_rockets", -936)):
        item(cls, 1368, y, LOW + 40)
    restock("item_shells", 1368, -856, LOW + 40, distance=32, wait=10)
    restock("item_spikes", 1368, -896, LOW + 40, distance=32, wait=10)
    restock("weapon_shotgun", 1368, -700, LOW + 40, distance=128, wait=10)
    for (x, y) in ((300, -1500), (1370, -1500), (448, -560), (830, -1110)):
        item("item_health", x, y, LOW + 2)
    restock("item_health", 1370, -1500, LOW + 2, distance=96, wait=15)
    item("item_health", 330, -460, MEZZ_Z + 2, spawnflags=2)  # a megahealth on the mezzanine
    for x in (512, 832, 1152):
        wall_lamp(out, "-y", x, r["y0"], LOW + 140, 180)
    # the fight: shut the door, count down, three waves (QC func_vr_spawner: each fires its target when its monsters
    # are dead; a trigger_counter per wave)
    # (well in: the weapons' tables by the door are taken first)
    trigger("r12_enter", (1024, -768, LOW), (1152, -512, LOW + 96), target="r12_go")
    ent("trigger_relay", 1300, -600, LOW + 40, targetname="r12_go", target="r12_shut")
    for i, (msg, t) in enumerate((("Get ready...", 1), ("3", 3), ("2", 4), ("1", 5), ("FIGHT!", 6))):
        k = {"targetname": "r12_go", "delay": t, "message": msg}
        if msg == "FIGHT!":
            k["target"] = "r12_w1"
        ent("trigger_relay", 1300, -600, LOW + 48 + 8 * i, **k)
    # (all on the floor: one up on a mezzanine could keep out of sight and hold the wave up)
    waves = [
        [(0, 1200, -1300, 135), (0, 450, -1200, 45), (0, 830, -620, 270)],
        [(0, 440, -640, 0), (7, 1300, -1000, 180), (7, 500, -1450, 90), (0, 1100, -620, 270)],
        [(8, 440, -900, 0), (8, 1220, -1100, 180), (0, 830, -1450, 90), (7, 1300, -1300, 180)],
    ]
    for w, spawns in enumerate(waves):
        name = "r12_w%d" % (w + 1)
        for (mon, x, y, a) in spawns:
            z = LOW
            ent("func_vr_spawner", x, y, z + 32, weapon=mon, angle=a, spawnflags=2, targetname=name,
                target=name + "_done")
        nxt = "r12_w%d" % (w + 2) if w + 1 < len(waves) else "r12_won"
        ent("trigger_counter", 830, -900, LOW + 64, count=len(spawns), targetname=name + "_done", spawnflags=1,
            target=name + "_next")
        k = {"targetname": name + "_next", "delay": 3, "target": nxt}
        if w + 1 < len(waves):
            k["message"] = "Wave %d!" % (w + 2)
        ent("trigger_relay", 830, -900, LOW + 72, **k)
    ent("trigger_relay", 830, -900, LOW + 80, targetname="r12_won", target="r12_exit",
        message="Arena cleared! The way out is open.")
    banner(N.join(["LESSON 12: THE ARENA", "When you step in, the door shuts", "and enemies come in waves.",
                   "Take the weapons on the tables."]), 1400, -800, LOW + 130, 180, "0.3")
    tip("t2_arena", "Take a gun and ammunition here" + N + "before you step further in.", 1340, -760, LOW + 70,
        200)
    # the way out: to the slipgate room
    d = doorway("room12_exit", 768, r["y0"] - 16, 896, r["y0"], LOW, out)
    sliding_door(d, "r12_exit")
    room("slipgate", 640, -1856, 1024, -1552, LOW, LOW + 224, bands(TX["panel"]))
    lamp_grid(out, 640, -1856, 1024, -1552, LOW + 224, 2, 1, 220)
    # the slipgate: a frame on the south wall, its surface, the changelevel to the hub
    gx0, gx1, gy = 768, 896, -1856
    dbox(out, (gx0 - 16, gy, LOW), (gx0, gy + 16, LOW + 144), TX["strip_v"])
    dbox(out, (gx1, gy, LOW), (gx1 + 16, gy + 16, LOW + 144), TX["strip_v"])
    dbox(out, (gx0 - 16, gy, LOW + 128), (gx1 + 16, gy + 16, LOW + 144), TX["lintel"])
    dbox(out, (gx0, gy, LOW), (gx1, gy + 4, LOW + 128), TX["black"])   # (the void behind the translucent surface)
    surf = mapgeom.box(gx0, gy + 4, LOW, gx1, gy + 12, LOW + 128, mapgeom.Tex(TX["teleport"]))
    bent("func_illusionary", [surf])
    trigger("to_hub", (gx0, gy, LOW), (gx1, gy + 24, LOW + 128), "trigger_changelevel", map="vrstart", spawnflags=1)
    light(832, -1830, LOW + 64, 250, "0.8 0.6 1")
    banner(N.join(["WELL DONE!", "You know the basics. The slipgate", "takes you to the hub: the campaigns,",
                   "the settings, the firing range."]), 832, gy + 20, LOW + 176, 90, "0.3")


ROOMS = [build_room1, build_bend, build_room2, build_room3, build_room4, build_room5, build_room6, build_room7,
         build_room8, build_room9, build_room10, build_room11, build_room12]


WORLD_KEYS = {
    "classname": "worldspawn", "mapversion": "220", "wad": ";".join(WADS),
    "_tb_mod": "hipnotic;rogue;quakevr", "message": "Quake VR: Tutorial", "worldtype": "2", "sounds": "0",
    "sky": "qvrday", "light": "0", "_sunlight": "260", "_sunlight_mangle": "225 -55 0",
    "_sunlight_color": "1 0.96 0.88", "_sunlight2": "420", "_sunlight2_color": "0.6 0.72 1.0", "_bounce": "1",
    "_vr_debris": "0", "_vr_crates": "0", "_qvr_prelit": "1",
}


def check_layout():
    """Air boxes of different rooms must not overlap (but a doorway, a pool or a shaft may touch its room), and rooms
    not joined by a doorway must keep a wall's thickness apart."""
    bad = 0
    rooms = [a for a in AIRS if a.kind != "door"]
    for i, a in enumerate(rooms):
        for b in rooms[i + 1:]:
            if overlaps((a.lo, a.hi), (b.lo, b.hi)):
                print("layout: %s overlaps %s" % (a.name, b.name))
                bad += 1
    if bad:
        sys.exit("layout: %d overlaps" % bad)


def write_map():
    t0 = time.time()
    for build in ROOMS:
        build()
    room_trims()
    check_layout()
    mw = MapWriter()
    for lo, hi in carve():
        mw.world.append(mapgeom.box(lo[0], lo[1], lo[2], hi[0], hi[1], hi[2], world_tex))
    mw.world.extend(CURVED)
    for lo, hi in WATER:
        mw.world.append(mapgeom.box(lo[0], lo[1], lo[2], hi[0], hi[1], hi[2], mapgeom.Tex(TX["water"])))
    for lo, hi in CLIPS:
        mw.world.append(mapgeom.box(lo[0], lo[1], lo[2], hi[0], hi[1], hi[2], mapgeom.Tex(TX["clip"])))
    for name, brushes in DETAIL_GROUPS:
        if brushes:
            mw.detail(name).extend(brushes)
    if ILLUSION:
        mw.detail("painted", classname="func_detail_illusionary").extend(ILLUSION)
    for keys, brushes in ENTS:
        mw.add(keys, brushes)
    header = "// Game: Quake VR\n// Format: Valve\n// Written by Misc/quakevr/maps/vrtutorial2_gen.py: edit that, not this.\n"
    mw.write(OUT, WORLD_KEYS, header)
    nb = len(mw.world) + sum(len(b) for _, b in mw.groups) + sum(len(b) for _, b in mw.entities)
    print("wrote %s: %d brushes (%d structural), %d entities (%.1f s)" % (OUT, nb, len(mw.world), len(mw.entities),
                                                                      time.time() - t0))


# Compiling (as vrstart_gen.py): qbsp 0.18.1 -bsp2, then 2.0's vis and light.
QBSP_ARGS = ["-bsp2"]
PRESETS = {
    "fast": dict(vis=["-fast"], light=["-lit", "-lux", "-lightgrid", "-lightgrid_dist", "128", "128", "128"]),
    "final": dict(vis=[], light=["-extra4", "-dirt", "-dirtscale", "1.5", "-dirtdepth", "96", "-bounce", "-lit", "-lux",
                                 "-lightgrid", "-lightgrid_dist", "64", "64", "64"]),
}


def compile_map(tools, work, preset, check=0, qbsp=DEFAULT_QBSP):
    """qbsp (0.18.1's), vis and light (ericw-tools 2.0 in `tools`) in `work`, the .bsp, .lit and .lux copied next to
    the .map. Prints each stage's time and its warnings; `check`: rays of the hole test (bsp_holes.py)."""
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
        warnings = [l for l in lines if ("WARNING" in l.upper() or "ERROR" in l.upper() or "LEAK" in l.upper()
                                         or "not found" in l or "couldn't create" in l.lower())]
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
        description="Writes quakevr/maps/vrtutorial2.map; with --compile also its .bsp, .lit and .lux (qbsp 0.18.1, "
                    "2.0's vis and light).",
        epilog="Presets (--preset): fast = vis -fast, light -lit -lux and a 128-unit light grid; final = full vis, "
               "light %s: the shipped build. Check a build for holes: --check 1000000 (bsp_holes.py)."
               % " ".join(PRESETS["final"]["light"]))
    ap.add_argument("--compile", action="store_true", help="also build the .bsp, .lit and .lux")
    ap.add_argument("--preset", choices=sorted(PRESETS), default="final", help="the compile preset (default: final)")
    ap.add_argument("--fast", action="store_true", help="the same as --preset fast")
    ap.add_argument("--check", type=int, default=0, metavar="RAYS", help="after compiling, the hole test with RAYS rays")
    ap.add_argument("--tools", default=DEFAULT_TOOLS, help="ericw-tools 2.0's folder (vis, light)")
    ap.add_argument("--qbsp", default=DEFAULT_QBSP, help="the qbsp.exe (0.18.1's)")
    ap.add_argument("--work", default=os.path.join(tempfile.gettempdir(), MAPNAME + "_build"))
    args = ap.parse_args()
    write_map()
    if args.compile:
        compile_map(args.tools, args.work, "fast" if args.fast else args.preset, args.check, args.qbsp)


if __name__ == "__main__":
    main()
