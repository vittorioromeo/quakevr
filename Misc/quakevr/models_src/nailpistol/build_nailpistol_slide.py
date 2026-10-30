# build_nailpistol_slide.py -- the nail pistol's second look, a full pistol slide (vr_nailpistol_model 1), added to
# quakevr/progs/v_nailpistol.mdl as its frames 2 and 3. Pure Python (no Blender): run it after export_mdl.py (which writes
# the classic frames 0 and 1 only), or again on its own output (it takes the classic part back out first):
#
#   python Misc/quakevr/models_src/nailpistol/build_nailpistol_slide.py [<in.mdl> [<out.mdl>]]
#
# The classic look (frames 0 rest, 1 its copper slide back 3 units) is kept byte for byte: its vertices, triangles,
# skin texels, box (scale and scale_origin: the corner the game scales the gun about) and frames as they were, the new
# ones appended. The full-slide look (frames 2 rest, 3 the slide back BACK_STEPS whole x steps, ~0.65 units: the box
# ends there; the game blends past frame 3 for more) shares the grip, trigger guard, trigger, muzzle and sights with it and
# adds its own: a frame (the lower receiver, a dust cover in front), a slide (the whole upper receiver from the rear
# sight to the muzzle: the three vents as slide cuts, an ejection port on the right, rear cocking serrations; the sights
# ride on it), a copper barrel that shows when the slide is back, and a cap behind the muzzle's nozzle. Each look's own
# triangles are folded into one point inside the gun in the other look's frames (zero area: nothing drawn), so the game
# picks a look by the frames it draws (Quake/vr/vr_nailslide.cpp, VR_NailSlidePose), live, for every copy of the gun.
#
# The skin grows from 256x256 to 256x512: the classic texels as they were, the new parts' painted below (row 256 on),
# procedurally in the classic skin's style and Quake's palette (rust on iron, worn bright edges, dark crevices).
# Coordinates below are build_nailpistol.py's design coordinates (+X the muzzle, +Y left, +Z up); the MDL's are the
# Body's (origin HAND, the grip's hand point).

import math
import os
import random
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
QVR = os.path.normpath(os.path.join(HERE, "..", ".."))
REPO = os.path.normpath(os.path.join(QVR, "..", ".."))
sys.path.insert(0, QVR)
sys.path.insert(0, os.path.join(QVR, "blender", "addons", "quakevr_models"))
import mdlgen  # noqa: E402
import qpal  # noqa: E402

GRIP_TILT = math.radians(12.0)
GRIP_TOP = (-2.3, 0.0, 1.0)
GRIP_UP = (math.sin(GRIP_TILT), 0.0, math.cos(GRIP_TILT))
HAND = mdlgen.sub(GRIP_TOP, mdlgen.mul(GRIP_UP, 2.0))  # the MDL's origin (build_nailpistol.py's HAND)

BACK_STEPS = 12              # frame 3: the slide this many x steps back (12 * 0.0543 = 0.651 units: as far as the box allows)
FOLD = (1.0, 0.0, 3.0)       # where the other look's triangles are folded (inside both looks' upper receiver)
SKIN_ROWS = 256              # rows added below the classic skin
DENSITY = 10.0               # texels a unit (the classic skin's receiver sides: ~10)

# The slide (design coordinates): x from its rear to its front face; its side walls from the seam up, a chamfer on
# each top edge, a flat top. Vents and the ejection port are recesses in the walls.
SX0, SX1 = -4.6, 7.3
SW, SWT = 1.2, 0.9           # half widths: the walls, the top
SZ0, SZ1, SZ2 = 1.7, 4.7, 5.0  # the seam (its bottom), the walls' top, the top
VENTS = [(2.2, 4.9, 1.9, 2.32), (2.2, 4.9, 2.66, 3.08), (2.2, 4.9, 3.42, 3.84)]  # the classic vents, where they were
VENT_DEPTH = 0.32
PORT = (-2.2, 0.8, 3.95, 4.6)  # the ejection port (right side, -Y)
PORT_DEPTH = 0.5
SERRATIONS = (-4.45, -2.75, 2.05, 4.5)  # x0 x1 z0 z1 on both walls: cocking grooves
# The frame: a side profile (x, z), convex, extruded +-FW; it hides the grip's top and holds the trigger and guard.
FRAME = [(-3.95, 1.0), (6.45, 1.0), (6.8, 1.35), (6.8, SZ0), (-4.5, SZ0), (-4.5, 1.45)]
FW = 1.15
# The barrel: a hexagonal tube on the bore's axis, from inside the slide into the nozzle.
BORE_Z = 2.8
BX0, BX1 = 0.5, 7.25
BR = 0.7
NOZZLE_X = 7.2               # the classic nozzle's back (build_nailpistol.py's lathe)

IRON, DARK, COPPER, BRASS, BORE, SLOT = range(6)
BASE = {IRON: (80, 68, 56), DARK: (68, 60, 52), COPPER: (168, 96, 46), BRASS: (164, 128, 60), BORE: (26, 22, 18),
        SLOT: (36, 30, 24)}
EDGE = {COPPER: (232, 164, 92), BRASS: (220, 184, 108)}
EDGE_IRON = (134, 128, 118)
RUST = (128, 72, 40)
RUSTY = (IRON, DARK)
# The classic skin's tone: its ray-traced occlusion leaves an open face at about this share of its light; its
# texel-to-texel specks this strong (matched by Misc/quakevr scratch statistics: mean colour, spread, palette mix).
EXPOSED = 0.85
SPECK = 0.75   # (0.6 until his note of 2026-10-30: "its colour is too uniform")
BLUE = 0.82  # (and a yellower iron: its dark olive and grey browns, not the pink ramp)
# Less uniform (his note 2026-10-30): the iron's tone swings wider with the noise (MOTTLE: was 0.16 mid, 0.12 fine), and
# bare worn steel shows through in broad patches where the rust is thin (STEEL, up to STEEL_WEAR of it).
MOTTLE = (0.76, 0.36, 0.18)  # col x (a + b * mid + c * fine)
STEEL = (122, 120, 114)
STEEL_WEAR = 0.45
# The back end, steel as the classic look's rear housing was (his note 2026-10-30, "the more silvery/steel-coloured part
# at the back end"): the full slide's and frame's texels behind REAR_X take the classic skin's texel at the same place
# of the classic housing (x -4.6 .. -2.4, the side, back and top faces), then the serrations and worn edges over them.
REAR_X = -2.4
REAR_SAMPLE = None  # (main: the sampler, classic_sampler)
# More distinct (his note 2026-10-30, "more distinct like in the original pre-slide art"): the classic housing is about
# 1.5x its receiver's light (sampled: luminance 40 vs 26); the full slide's front is as light as that housing, so the
# back end takes the sampled texel's light lifted by REAR_GAIN, its texel-to-texel swing cut to REAR_FLAT about the
# housing's mean light (REAR_MEAN: less mottled), a cool steel hue (REAR_TONE) with REAR_HUE of the texel's own left; a
# seam at REAR_X (SEAM wide: a dark groove in front of it, a lit bevel behind) parts it from the rusty slide crisply.
REAR_GAIN = 1.2
REAR_FLAT = 0.75
REAR_MEAN = 40.0
REAR_TONE = (0.93, 1.0, 1.1)
REAR_HUE = 0.3
REAR_FALLBACK = (48, 39, 29)  # the classic housing's mean texel (where no classic face is near)
SEAM = 0.11
SEAM_LIT = (196, 198, 204)
RIVETS = [((6.35, SW, 4.15), 0.22), ((6.35, SW, 2.15), 0.22), ((-1.5, SW, 2.4), 0.22), ((5.9, FW, 1.35), 0.17),
          ((-3.55, FW, 1.35), 0.17)]


# ---------------------------------------------------------------------------------------------------------------
# Small vector helpers (mdlgen's) and a 3D gradient noise like Blender's noise.noise (about -1..1).
add, sub, mul, dot, cross, norm = mdlgen.add, mdlgen.sub, mdlgen.mul, mdlgen.dot, mdlgen.cross, mdlgen.norm


def length(a):
    return math.sqrt(dot(a, a))


NOISE_GAIN = 2.4  # (Blender's noise.noise swings about this much wider than plain Perlin)
_rng = random.Random(23)
_PERM = list(range(256))
_rng.shuffle(_PERM)
_PERM += _PERM
_GRAD = [(1, 1, 0), (-1, 1, 0), (1, -1, 0), (-1, -1, 0), (1, 0, 1), (-1, 0, 1), (1, 0, -1), (-1, 0, -1), (0, 1, 1),
         (0, -1, 1), (0, 1, -1), (0, -1, -1)]


def noise(p):
    xi, yi, zi = (int(math.floor(c)) for c in p)
    xf, yf, zf = p[0] - xi, p[1] - yi, p[2] - zi

    def fade(t):
        return t * t * t * (t * (t * 6 - 15) + 10)

    def g(i, j, k, x, y, z):
        h = _PERM[(_PERM[(_PERM[(xi + i) & 255] + yi + j) & 255] + zi + k) & 255] % 12
        gr = _GRAD[h]
        return gr[0] * x + gr[1] * y + gr[2] * z

    u, v, w = fade(xf), fade(yf), fade(zf)

    def lerp(a, b, t):
        return a + (b - a) * t
    x00 = lerp(g(0, 0, 0, xf, yf, zf), g(1, 0, 0, xf - 1, yf, zf), u)
    x10 = lerp(g(0, 1, 0, xf, yf - 1, zf), g(1, 1, 0, xf - 1, yf - 1, zf), u)
    x01 = lerp(g(0, 0, 1, xf, yf, zf - 1), g(1, 0, 1, xf - 1, yf, zf - 1), u)
    x11 = lerp(g(0, 1, 1, xf, yf - 1, zf - 1), g(1, 1, 1, xf - 1, yf - 1, zf - 1), u)
    return NOISE_GAIN * lerp(lerp(x00, x10, v), lerp(x01, x11, v), w)


def mix(a, b, k):
    k = max(0.0, min(1.0, k))
    return tuple(a[i] + (b[i] - a[i]) * k for i in range(3))


def smooth(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


def seg_dist(p, a, b):
    ab = sub(b, a)
    l2 = dot(ab, ab)
    t = 0.0 if l2 < 1e-12 else max(0.0, min(1.0, dot(sub(p, a), ab) / l2))
    return length(sub(add(a, mul(ab, t)), p))


# ---------------------------------------------------------------------------------------------------------------
# The new parts: faces (convex polygons) grouped in islands (one planar UV projection each).
class Island:
    def __init__(self, name, n, part, edges=(), ao=1.0, chamfer=False):
        self.name, self.n, self.part, self.edges, self.ao, self.chamfer = name, n, part, list(edges), ao, chamfer
        ref = (1.0, 0.0, 0.0) if abs(n[0]) < 0.9 else (0.0, 1.0, 0.0)
        self.a = norm(sub(ref, mul(n, dot(ref, n))))
        self.b = cross(n, self.a)
        self.faces = []
        self.origin = None


class Builder:
    def __init__(self):
        self.islands = {}
        self.faces = []   # (island, [points], moving)

    def island(self, name, n, part, **kw):
        if name not in self.islands:
            self.islands[name] = Island(name, norm(n), part, **kw)
        return self.islands[name]

    def face(self, isl, pts, moving):
        """A convex face; its corners in either order (turned to go counter-clockwise seen from outside)."""
        n = isl.n
        c = mul(pts[0], 0.0)
        for p in pts:
            c = add(c, p)
        c = mul(c, 1.0 / len(pts))
        area = (0.0, 0.0, 0.0)
        for i in range(len(pts)):
            area = add(area, cross(sub(pts[i], c), sub(pts[(i + 1) % len(pts)], c)))
        if dot(area, n) < 0:
            pts = list(reversed(pts))
        isl.faces.append(pts)
        self.faces.append((isl, list(pts), moving))

    def quad_x(self, isl, x0, x1, p, q, moving):
        """A face along x from x0 to x1 between the (y, z) points p and q."""
        self.face(isl, [(x0, p[0], p[1]), (x1, p[0], p[1]), (x1, q[0], q[1]), (x0, q[0], q[1])], moving)


def build():
    b = Builder()
    xs = sorted({SX0, SX1, PORT[0], PORT[1]} | {v[0] for v in VENTS} | {v[1] for v in VENTS})
    zs = sorted({SZ0, SZ1, PORT[2], PORT[3]} | {v[2] for v in VENTS} | {v[3] for v in VENTS})

    def holes(side):
        out = [(v, VENT_DEPTH) for v in VENTS]
        if side < 0:
            out.append((PORT, PORT_DEPTH))
        return out

    # The walls: a grid on every x and z break (no T-junctions), the holes left out; recesses behind them.
    for side in (1.0, -1.0):
        hs = holes(side)
        rims = []
        for (x0, x1, z0, z1), _ in hs:
            rims += [((x0, side * SW, z0), (x1, side * SW, z0)), ((x1, side * SW, z0), (x1, side * SW, z1)),
                     ((x1, side * SW, z1), (x0, side * SW, z1)), ((x0, side * SW, z1), (x0, side * SW, z0))]
        outline = [((SX0, side * SW, SZ0), (SX1, side * SW, SZ0)), ((SX0, side * SW, SZ1), (SX1, side * SW, SZ1)),
                   ((SX0, side * SW, SZ0), (SX0, side * SW, SZ1)), ((SX1, side * SW, SZ0), (SX1, side * SW, SZ1))]
        wall = b.island("wall%+d" % side, (0, side, 0), IRON, edges=[(a, c, True) for a, c in outline + rims])
        for i in range(len(xs) - 1):
            for j in range(len(zs) - 1):
                x0, x1, z0, z1 = xs[i], xs[i + 1], zs[j], zs[j + 1]
                if any(h[0] <= x0 and x1 <= h[1] and h[2] <= z0 and z1 <= h[3] for h, _ in hs):
                    continue
                b.face(wall, [(x0, side * SW, z0), (x1, side * SW, z0), (x1, side * SW, z1), (x0, side * SW, z1)], True)
        for k, ((x0, x1, z0, z1), depth) in enumerate(hs):
            y0, y1 = side * SW, side * (SW - depth)
            port = depth == PORT_DEPTH
            back = b.island("hole%+d_%d" % (side, k), (0, side, 0), COPPER if port else SLOT, ao=0.55)
            b.face(back, [(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)], True)
            for name, n, quad in (("f", (0, 0, 1), [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)]),
                                  ("c", (0, 0, -1), [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]),
                                  ("r", (1, 0, 0), [(x0, y0, z0), (x0, y1, z0), (x0, y1, z1), (x0, y0, z1)]),
                                  ("m", (-1, 0, 0), [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)])):
                isl = b.island("hole%+d_%d%s" % (side, k, name), n, SLOT, ao=0.5 if name != "f" else 0.75)
                b.face(isl, quad, True)
        # The top chamfer.
        ch = b.island("chamfer%+d" % side, (0, side, 1), IRON, chamfer=True,
                      edges=[((SX0, side * SW, SZ1), (SX1, side * SW, SZ1), True),
                             ((SX0, side * SWT, SZ2), (SX1, side * SWT, SZ2), True)])
        for i in range(len(xs) - 1):
            b.quad_x(ch, xs[i], xs[i + 1], (side * SW, SZ1), (side * SWT, SZ2), True)
    top = b.island("top", (0, 0, 1), IRON, edges=[((SX0, SWT, SZ2), (SX1, SWT, SZ2), True),
                                                  ((SX0, -SWT, SZ2), (SX1, -SWT, SZ2), True),
                                                  ((SX1, -SWT, SZ2), (SX1, SWT, SZ2), True),
                                                  ((SX0, -SWT, SZ2), (SX0, SWT, SZ2), True)])
    bottom = b.island("bottom", (0, 0, -1), IRON, ao=0.6)
    for i in range(len(xs) - 1):
        b.quad_x(top, xs[i], xs[i + 1], (-SWT, SZ2), (SWT, SZ2), True)
        b.quad_x(bottom, xs[i], xs[i + 1], (-SW, SZ0), (SW, SZ0), True)
    # The ends: bands on the walls' z breaks, the chamfers' trapezoid on top.
    for x, s in ((SX1, 1.0), (SX0, -1.0)):
        end = b.island("end%+d" % s, (s, 0, 0), IRON,
                       edges=[((x, -SW, SZ0), (x, SW, SZ0), True), ((x, -SW, SZ0), (x, -SW, SZ1), True),
                              ((x, SW, SZ0), (x, SW, SZ1), True), ((x, -SWT, SZ2), (x, SWT, SZ2), True),
                              ((x, -SW, SZ1), (x, -SWT, SZ2), True), ((x, SW, SZ1), (x, SWT, SZ2), True)])
        for j in range(len(zs) - 1):
            b.face(end, [(x, -SW, zs[j]), (x, SW, zs[j]), (x, SW, zs[j + 1]), (x, -SW, zs[j + 1])], True)
        b.face(end, [(x, -SW, SZ1), (x, SW, SZ1), (x, SWT, SZ2), (x, -SWT, SZ2)], True)

    # The frame: its profile on both sides, a strip on each profile edge.
    for side in (1.0, -1.0):
        fs = b.island("frame%+d" % side, (0, side, 0), DARK,
                      edges=[((FRAME[i][0], side * FW, FRAME[i][1]),
                              (FRAME[(i + 1) % len(FRAME)][0], side * FW, FRAME[(i + 1) % len(FRAME)][1]),
                              FRAME[i][1] != SZ0 or FRAME[(i + 1) % len(FRAME)][1] != SZ0) for i in range(len(FRAME))])
        b.face(fs, [(x, side * FW, z) for x, z in FRAME], False)
    for i in range(len(FRAME)):
        (xa, za), (xb, zb) = FRAME[i], FRAME[(i + 1) % len(FRAME)]
        d = (xb - xa, zb - za)
        n = norm((d[1], 0.0, -d[0]))  # the profile goes counter-clockwise in (x, z): outward is (dz, -dx)
        isl = b.island("frame_e%d" % i, n, DARK, ao=0.6 if n[2] < -0.5 else 0.85,
                       edges=[((xa, FW, za), (xb, FW, zb), True), ((xa, -FW, za), (xb, -FW, zb), True)])
        b.face(isl, [(xa, -FW, za), (xb, -FW, zb), (xb, FW, zb), (xa, FW, za)], False)

    # The barrel: six sides, its front closed (dark: the bore's bottom seen down the nozzle). The nozzle's back is the
    # classic cap (shared, a closed hexagon at NOZZLE_X facing back): no ring of our own there (one on the cap's plane,
    # facing the same way, z-fought with it once the slide was drawn: check_faces finds such pairs).
    hexa = [(BR * math.cos(math.radians(60 * k + HEX_ROT)), BR * math.sin(math.radians(60 * k + HEX_ROT)))
            for k in range(6)]
    for k in range(6):
        (ya, za), (yb, zb) = hexa[k], hexa[(k + 1) % 6]
        mid = ((ya + yb) / 2, (za + zb) / 2)
        isl = b.island("barrel%d" % k, (0, mid[0], mid[1]), COPPER,
                       edges=[((BX0, ya, BORE_Z + za), (BX1, ya, BORE_Z + za), True)])
        b.quad_x(isl, BX0, BX1, (ya, BORE_Z + za), (yb, BORE_Z + zb), False)
    cap = b.island("barrel_cap", (1, 0, 0), BORE, ao=0.5)
    b.face(cap, [(BX1, y, BORE_Z + z) for y, z in hexa], False)
    return b


HEX_ROT = 0.0       # set from the classic nozzle (main)
NOZZLE_HEX = []     # the classic nozzle's back hexagon (y, z - BORE_Z), set in main


# ---------------------------------------------------------------------------------------------------------------
# Painting (after build_nailpistol.py's paint_texel, without its ray-traced occlusion: crevices by distance).
def paint(p, isl, speck):
    part, n = isl.part, isl.n
    col = BASE[part]
    big = noise(mul(p, 0.8))
    mid = noise(add(mul(p, 2.6), (3.1, 7.7, 1.3)))
    fine = noise(add(mul(p, 7.5), (9.2, 1.4, 5.5)))
    rear = REAR_SAMPLE is not None and part in RUSTY and p[0] < REAR_X
    classic = (REAR_SAMPLE(p, n) or mul(REAR_FALLBACK, 0.9 + 0.2 * fine)) if rear else None
    if classic:
        # The classic housing's texel (its own speckle, rust and occlusion baked in: none added below but the serrations
        # and edges) as steel: lighter, flatter, cooler.
        lum = (0.299 * classic[0] + 0.587 * classic[1] + 0.114 * classic[2])
        lum = (REAR_MEAN + (lum - REAR_MEAN) * REAR_FLAT) * REAR_GAIN * (0.96 + 0.06 * mid)
        col = mix(mul(REAR_TONE, lum), mul(classic, REAR_GAIN), REAR_HUE)
    elif part in RUSTY:
        rust = smooth(0.05, 0.55, 0.55 * big + 0.45 * mid + 0.25 * fine)
        col = mix(col, RUST, 0.75 * rust)
        col = mix(col, STEEL, STEEL_WEAR * (1.0 - rust) * smooth(0.0, 0.5, -big + 0.3 * fine))
        col = mul(col, MOTTLE[0] + MOTTLE[1] * mid + MOTTLE[2] * fine)
        if fine > 0.45:
            col = mul(col, 0.72)
        col = (col[0], col[1], col[2] * BLUE)
    elif part in (COPPER, BRASS):
        col = mul(col, 0.9 + 0.12 * mid + 0.1 * fine)
        col = mix(col, (92, 72, 44), 0.5 * smooth(0.35, 0.7, big * 0.6 + fine * 0.5))
    else:
        col = mul(col, 0.92 + 0.1 * fine)
    if not classic:
        col = mul(col, 1.0 + speck)
    if part == SLOT and n[2] > 0.5:
        col = mul(col, 1.9)
    x, y, z = p
    # Cocking serrations: grooves across the walls' rear, each a dark cut, a lit edge.
    sx0, sx1, sz0, sz1 = SERRATIONS
    if isl.name.startswith("wall") and sx0 < x < sx1 and sz0 < z < sz1:
        g = ((x - sx0) / 0.28) % 1.0
        col = mul(col, 0.45) if g < 0.4 else (mix(col, EDGE_IRON, 0.55) if g < 0.58 else col)
        if z < sz0 + 0.08 or z > sz1 - 0.08:
            col = mul(col, 0.7)
    # The ejection port's inside: the feed block, copper, two nails' heads in it.
    if isl.part == COPPER and isl.name.startswith("hole"):
        col = mul(col, 0.8)
        for nz in (4.12, 4.42):
            if abs(z - nz) < 0.09 and PORT[0] + 0.3 < x < PORT[1] - 0.3:
                col = mix(BASE[BRASS], EDGE[BRASS], 0.5 if abs(z - nz) < 0.04 else 0.0)
    # The top: a rib down the middle, a sight line.
    if isl.name == "top" and abs(y) < 0.1:
        col = mul(col, 0.6)
    # The barrel's top catches the light; its chamber end is darker.
    if isl.name.startswith("barrel") and x < 2.0:
        col = mul(col, 0.75)
    # Worn bright convex edges, darker concave ones.
    wear = smooth(-0.35, 0.35, noise(add(mul(p, 3.3), (5.0, 2.0, 8.0))))
    edge_col = EDGE.get(part) or add(mul(col, 1.05), mul(EDGE_IRON, 0.45))
    if isl.chamfer:
        col = mix(col, edge_col, 0.2 + 0.35 * wear)
    for a, c, convex in isl.edges:
        dist = seg_dist(p, a, c)
        if classic and convex and dist < 0.13:
            col = mix(col, SEAM_LIT, 0.4 + 0.2 * wear)  # the back end's bright bevelled rim, as the classic housing's
        elif convex and dist < 0.075 and not isl.chamfer:
            col = mix(col, edge_col, (0.35 + 0.5 * wear) * (1.0 - dist / 0.075) + 0.15)
        elif not convex and dist < 0.16:
            col = mul(col, 0.6 + 0.4 * dist / 0.16)
    ao = isl.ao
    # Under the slide's overhang: the frame's sides darken towards the seam; the slide's walls near the vents.
    if isl.name.startswith("frame") and abs(n[1]) > 0.9:
        ao *= 0.55 + 0.45 * smooth(0.0, 0.45, SZ0 - z)
    # Rivets: domes lit from above and in front, a shadow below and behind.
    for (rx, ry, rz), r in RIVETS:
        for s in (1.0, -1.0):
            rn = (0.0, s, 0.0)
            if dot(n, rn) < 0.95 or abs(y - s * ry) > 0.1:
                continue
            v = (x - rx, 0.0, z - rz)
            k = length(v) / r
            if k < 1.0:
                dome = norm(add(mul(rn, math.sqrt(1.0 - k * k)), mul(v, 1.0 / r)))
                light = norm(add(mul(rn, 0.6), (0.35, 0.0, 0.75)))
                col = mix((52, 44, 36), (196, 182, 156), max(0.0, dot(dome, light)) ** 1.5)
            elif k < 1.4 and (v[2] < 0.0 or v[0] < -0.5 * r):
                col = mul(col, 0.55)
    if not classic:
        col = mul(col, 0.3 + 0.7 * ao * EXPOSED)
        col = mul(col, 0.94 + 0.1 * n[2])
    # The back end's seam (every rusty face across REAR_X: the walls, chamfers, top, the frame's sides and edges).
    if part in RUSTY and abs(n[0]) < 0.9 and REAR_X - SEAM <= x < REAR_X + SEAM:
        col = mul(col, 0.4) if x >= REAR_X else mix(col, SEAM_LIT, 0.45)
    return tuple(max(0, min(255, int(round(c)))) for c in col)


# ---------------------------------------------------------------------------------------------------------------
def layout(b, width, top):
    """Each island's texel rectangle: shelves, tallest first, 2 texels apart."""
    order = []
    for isl in b.islands.values():
        us = [dot(p, isl.a) for f in isl.faces for p in f]
        vs = [dot(p, isl.b) for f in isl.faces for p in f]
        isl.umin, isl.vmin = min(us), min(vs)
        isl.w = int(math.ceil((max(us) - isl.umin) * DENSITY)) + 1
        isl.h = int(math.ceil((max(vs) - isl.vmin) * DENSITY)) + 1
        order.append(isl)
    order.sort(key=lambda i: (-i.h, -i.w))
    s, t, shelf = 2, top + 2, 0
    for isl in order:
        if s + isl.w + 2 > width:
            s, t, shelf = 2, t + shelf + 3, 0
        isl.origin = (s, t)
        s += isl.w + 3
        shelf = max(shelf, isl.h)
    return t + shelf + 2


def st_of(isl, p):
    return (isl.origin[0] + (dot(p, isl.a) - isl.umin) * DENSITY, isl.origin[1] + (dot(p, isl.b) - isl.vmin) * DENSITY)


def point_of(isl, s, t):
    u = (s - isl.origin[0]) / DENSITY + isl.umin
    v = (t - isl.origin[1]) / DENSITY + isl.vmin
    # the plane's offset along the normal: any face point's
    p0 = isl.faces[0][0]
    return add(add(mul(isl.a, u), mul(isl.b, v)), mul(isl.n, dot(p0, isl.n)))


def bake(b, skin, w, h):
    rng = random.Random(5)
    quant = qpal.Quantizer()
    for isl in b.islands.values():
        s0, t0 = isl.origin
        for t in range(t0 - 1, t0 + isl.h + 1):
            for s in range(s0 - 1, s0 + isl.w + 1):
                if 0 <= s < w and SKIN_TOP <= t < h:
                    c = paint(point_of(isl, s, t), isl, (rng.random() - 0.5) * SPECK)
                    skin[t * w + s] = quant.nearest(c, False)


SKIN_TOP = 256


def classic_sampler(P0, tris, kinds, front, st, skin, w):
    """The classic skin's colour at a point of the classic rear housing (REAR_X back): its face whose plane is nearest
    the point (within 0.6 units, facing as the normal n does, the point inside it), its texel there."""
    cands = []
    for k, (t, kd) in enumerate(zip(tris, kinds)):
        a, c, d = (P0[i] for i in t)
        if kd != "classic" or max(a[0], c[0], d[0]) > REAR_X + 0.05:
            continue
        nf = cross(sub(c, a), sub(d, a))
        if length(nf) < 1e-6:
            continue
        nf = norm(nf)
        sts = []
        for i in t:
            onseam, s, tt = st[i]
            sts.append((s + (w // 2 if onseam and not front[k] else 0) + 0.5, tt + 0.5))
        cands.append((a, c, d, nf, sts))

    def sample(p, n):
        best = None
        for a, c, d, nf, sts in cands:
            if abs(dot(nf, n)) < 0.85:
                continue
            dist = dot(sub(p, a), nf)
            if abs(dist) > 0.6 or (best and abs(dist) >= best[0]):
                continue
            q = sub(p, mul(nf, dist))
            v0, v1, v2 = sub(c, a), sub(d, a), sub(q, a)
            d00, d01, d11, d20, d21 = dot(v0, v0), dot(v0, v1), dot(v1, v1), dot(v2, v0), dot(v2, v1)
            den = d00 * d11 - d01 * d01
            if abs(den) < 1e-12:
                continue
            u = (d11 * d20 - d01 * d21) / den
            v = (d00 * d21 - d01 * d20) / den
            if u < -0.02 or v < -0.02 or u + v > 1.02:
                continue
            s = sts[0][0] + u * (sts[1][0] - sts[0][0]) + v * (sts[2][0] - sts[0][0])
            t = sts[0][1] + u * (sts[1][1] - sts[0][1]) + v * (sts[2][1] - sts[0][1])
            best = (abs(dist), qpal.PALETTE[skin[min(SKIN_TOP - 1, max(0, int(t))) * w + min(w - 1, max(0, int(s)))]])
        return best[1] if best else None
    return sample


# ---------------------------------------------------------------------------------------------------------------
def check_faces(verts, tris, nt, scale, k_max):
    """The full-slide look's faces in frames 2, 3 and blended past 3 (up to k_max: the game extrapolates 2 -> 3): every
    look's own triangles stay one point in the other's frames (zero area, any blend); each new triangle winds to face
    its normal (Quake: clockwise seen from the front; back faces are culled); and no new triangle lies on another
    drawn one's plane facing the same way and overlapping it (the two z-fight: they flicker). Returns the problems."""
    table = mdlgen.anorms()
    out = []
    for f in (0, 1):
        for k in range(nt, len(tris)):
            if len({tuple(verts[i][0][f]) for i in tris[k]}) > 1:
                out.append("frame %d: new triangle %d not folded" % (f, k))
    for k in range(nt):
        p2 = {tuple(verts[i][0][2]) for i in tris[k]}
        p3 = {tuple(verts[i][0][3]) for i in tris[k]}
        if (len(p2) == 1 or len(p3) == 1) and p2 != p3:
            out.append("classic triangle %d folded in one of frames 2, 3 only (a blend would draw it)" % k)
    for k_blend in (0.0, 1.0, 2.0, k_max):
        P = [tuple((v[0][2][j] + k_blend * (v[0][3][j] - v[0][2][j])) * scale[j] for j in range(3)) for v in verts]
        drawn = []
        for k, t in enumerate(tris):
            a, b, c = (P[i] for i in t)
            n = cross(sub(b, a), sub(c, a))
            area = length(n)
            if area < 1e-6:
                continue
            n = mul(n, -1.0 / area)  # clockwise seen from the front
            if k >= nt and dot(n, table[verts[t[0]][1][2]]) < 0.5:
                out.append("blend %.1f: new triangle %d faces away from its normal" % (k_blend, k))
            lo = [min(p[j] for p in (a, b, c)) for j in range(3)]
            hi = [max(p[j] for p in (a, b, c)) for j in range(3)]
            drawn.append((k, (a, b, c), n, lo, hi))
        for i in range(len(drawn)):
            ki, qi, ni, loi, hii = drawn[i]
            for j in range(i + 1, len(drawn)):
                kj, qj, nj, loj, hij = drawn[j]
                if (ki < nt and kj < nt) or dot(ni, nj) < 0.99 or \
                        any(hii[x] < loj[x] or hij[x] < loi[x] for x in range(3)) or \
                        max(abs(dot(sub(p, qi[0]), ni)) for p in qj) > 0.04:
                    continue
                u = norm(cross(ni, (1.0, 0.0, 0.0) if abs(ni[0]) < 0.9 else (0.0, 1.0, 0.0)))
                v = cross(ni, u)
                A = [(dot(p, u), dot(p, v)) for p in qi]
                B = [(dot(p, u), dot(p, v)) for p in qj]

                def apart(poly):
                    for e in range(3):
                        (x0, y0), (x1, y1) = poly[e], poly[(e + 1) % 3]
                        axis, el = (y0 - y1, x1 - x0), math.hypot(x1 - x0, y1 - y0)
                        pa = [axis[0] * p[0] + axis[1] * p[1] for p in A]
                        pb = [axis[0] * p[0] + axis[1] * p[1] for p in B]
                        if max(pa) < min(pb) + 0.02 * el or max(pb) < min(pa) + 0.02 * el:
                            return True
                    return False
                if not apart(A) and not apart(B):
                    out.append("blend %.1f: triangles %d and %d share a plane facing the same way (z-fight)" %
                               (k_blend, ki, kj))
    return out


def design(p):
    return add(p, HAND)


def body(p):
    return sub(p, HAND)


def main():
    global HEX_ROT, NOZZLE_HEX
    src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, "quakevr", "progs", "v_nailpistol.mdl")
    dst = sys.argv[2] if len(sys.argv) > 2 else src
    import mdl as M
    m = M.Model.read(src)
    names = m.pose_names()
    pb = m.pose_bytes()
    w, h = m.skin_size
    nv, nt = m.num_verts, len(m.tris)
    rerun = len(names) == 4
    if rerun:  # our own output: the classic part back out
        fold = [m.place(pb[2][i][:3]) for i in range(nv)]
        f0 = [m.place(pb[0][i][:3]) for i in range(nv)]
        alt = lambda i: length(sub(f0[i], body(FOLD))) < 0.08 and length(sub(fold[i], body(FOLD))) >= 0.08
        nt = next(k for k, t in enumerate(m.tris) if all(alt(i) for i in t[1:]))
        nv = 1 + max(i for t in m.tris[:nt] for i in t[1:])
        h = SKIN_TOP
        pb = pb[:2]
    elif len(names) != 2:
        raise SystemExit("%s: expected the classic 2 frames (rest, rest1), found %s" % (src, names))
    assert h == SKIN_TOP and w == 256, "the classic skin is 256x256"
    P = [[design(m.place(pb[f][i][:3])) for i in range(nv)] for f in range(2)]
    NB = [[pb[f][i][3] for i in range(nv)] for f in range(2)]
    tris = [t[1:] for t in m.tris[:nt]]
    moved = {i for i in range(nv) if pb[0][i][:3] != pb[1][i][:3]}

    # The classic triangles: shared (grip, guard, trigger, muzzle), the sights (shared; ride the slide in the new
    # look), or the classic look's own (receiver, rear housing, its slide).
    def inside(p, x0, x1, y, z0, z1):
        e = 0.03
        return x0 - e <= p[0] <= x1 + e and abs(p[1]) <= y + e and z0 - e <= p[2] <= z1 + e

    def kind(t):
        ps = [P[0][i] for i in t]
        if any(i in moved for i in t):
            return "classic"
        if all(inside(p, -4.4, -3.45, 0.92, 4.95, 5.42) for p in ps) or \
           all(inside(p, 6.2, 6.6, 0.17, 4.6, 5.42) for p in ps) or all(inside(p, 5.95, 6.85, 0.4, 4.5, 4.74) for p in ps):
            return "sight"
        if all(p[0] >= NOZZLE_X - 0.03 and math.hypot(p[1], p[2] - BORE_Z) <= 1.05 for p in ps):
            return "shared"
        if all(abs(p[2] - 1.0) < 0.03 for p in ps):
            return "classic"  # the receiver's and the housing's bottom (the new frame's is there)
        if all(p[2] <= 1.0 + 0.03 for p in ps) or all(p[2] <= 1.75 and abs(p[1]) <= 1.12 for p in ps):
            return "shared"
        return "classic"
    kinds = [kind(t) for t in tris]
    use = {}
    for t, k in zip(tris, kinds):
        for i in t:
            use.setdefault(i, set()).add(k)
    mixed = [i for i, ks in use.items() if len(ks) > 1]
    count = {k: kinds.count(k) for k in ("shared", "sight", "classic")}
    print("classic triangles: %s; vertices used by two kinds: %d" % (count, len(mixed)))
    if mixed:
        raise SystemExit("a vertex is shared by two kinds: %s" % mixed[:8])
    vkind = {i: next(iter(ks)) for i, ks in use.items()}

    # The nozzle's back hexagon: the shared vertices at its back on its outer radius.
    ring = sorted({(round(p[1], 3), round(p[2] - BORE_Z, 3)) for i, p in enumerate(P[0])
                   if vkind.get(i) == "shared" and abs(p[0] - NOZZLE_X) < 0.05 and math.hypot(p[1], p[2] - BORE_Z) > 0.8},
                  key=lambda q: math.atan2(q[1], q[0]))
    if len(ring) != 6:
        raise SystemExit("the nozzle's back: expected 6 corners, found %s" % ring)
    NOZZLE_HEX = ring
    HEX_ROT = math.degrees(math.atan2(ring[0][1], ring[0][0]))
    print("nozzle back: %s (first corner at %.1f degrees)" % (ring, HEX_ROT))

    global REAR_SAMPLE
    REAR_SAMPLE = classic_sampler(P[0], tris, kinds, [m.tris[k][0] for k in range(nt)], m.st, m.skins[0][2][0], w)
    b = build()
    bottom_row = layout(b, w, SKIN_TOP)
    print("new parts: %d islands, %d faces, skin rows %d..%d used" % (len(b.islands), len(b.faces), SKIN_TOP, bottom_row))
    assert bottom_row <= SKIN_TOP + SKIN_ROWS, "the new parts don't fit the added rows"
    H = SKIN_TOP + SKIN_ROWS
    skin = bytearray(m.skins[0][2][0][:w * SKIN_TOP]) + bytearray(w * SKIN_ROWS)
    bake(b, skin, w, H)

    # Vertices: per frame position (design coordinates), normal (a Quake index or a vector), texel.
    table = mdlgen.anorms()

    def nearest_normal(n):
        return max(range(len(table)), key=lambda i: dot(table[i], n))
    # The classic box (its scale_origin is the corner the game scales the gun about: kept, so the gun sits as before)
    # and the classic bytes as they were. The full slide's back frame has it only BACK_STEPS x steps back (the box ends
    # behind the classic slide's own back frame); the game draws it further by blending past that frame
    # (VR_NailSlidePose): the steps are whole, so every vertex of it moves alike.
    lo, scale, hdr = list(m.origin), list(m.scale), list(m.h)

    def q(p):
        b = body(p)
        out = [int(round((b[k] - lo[k]) / scale[k])) for k in range(3)]
        assert all(0 <= v <= 255 for v in out), "a point outside the classic box: %s" % (p,)
        return out
    fold = q(FOLD)
    verts = []  # ([4 byte triples], [4 normal indices], (s, t))
    for i in range(nv):
        k = vkind.get(i, "classic")
        b0, b1 = list(pb[0][i][:3]), list(pb[1][i][:3])
        if k == "shared":
            fr = [b0, b1, b0, b0]
        elif k == "sight":
            fr = [b0, b1, b0, [b0[0] - BACK_STEPS, b0[1], b0[2]]]
        else:
            fr = [b0, b1, fold, fold]
        verts.append((fr, [NB[0][i], NB[1][i], NB[0][i], NB[0][i]], tuple(m.st[i][1:])))
    new_tris = [tuple(t) for t in tris]
    for isl, pts, moving in b.faces:
        ni = nearest_normal(isl.n)
        base = len(verts)
        for p in pts:
            s, t = st_of(isl, p)
            r = q(p)
            back = [r[0] - BACK_STEPS, r[1], r[2]] if moving else r
            verts.append(([fold, fold, r, back], [ni] * 4, (int(round(s)), int(round(t)))))
        for k in range(1, len(pts) - 1):  # counter-clockwise seen from outside: Quake's clockwise
            new_tris.append((base, base + k + 1, base + k))
    low = min(v[0][3][0] for v in verts)
    assert low >= 0, "the slide's back frame past the box by %d steps" % -low
    k_max = 3.0 / (BACK_STEPS * scale[0])  # Slide Travel's most (3 units) as a blend past frame 3
    problems = check_faces(verts, new_tris, nt, scale, k_max)
    for p in problems[:12]:
        print("  " + p)
    if problems:
        raise SystemExit("the faces: %d problems (above)" % len(problems))
    print("faces: frames 2, 3 and blends to %.1f: every fold degenerate, every new triangle facing out, no z-fighting "
          "pair" % k_max)
    print("model: %d vertices (%d classic), %d triangles (%d classic); the slide's back frame %.4f units back "
          "(%d steps; %d to spare)" % (len(verts), nv, len(new_tris), nt, BACK_STEPS * scale[0], BACK_STEPS, low))
    out = bytearray(mdlgen.HEADER.pack(b"IDPO", 6, *scale, *lo, hdr[8], *hdr[9:12], 1, w, H, len(verts),
                                       len(new_tris), 4, hdr[18], hdr[19], hdr[20]))
    out += struct.pack("<i", 0) + bytes(skin)
    for _, _, (s, t) in verts:
        out += struct.pack("<3i", 0, s, t)
    for a, c, d in new_tris:
        out += struct.pack("<4i", 1, a, c, d)
    for f, name in enumerate(("rest", "rest1", "slide", "slide1")):
        qs = [bytes(fr[f] + [nb[f]]) for fr, nb, _ in verts]
        mins = bytes([min(v[k] for v in qs) for k in range(3)] + [0])
        maxs = bytes([max(v[k] for v in qs) for k in range(3)] + [0])
        out += struct.pack("<i", 0) + mins + maxs + name.encode().ljust(16, b"\0") + b"".join(qs)
    with open(dst, "wb") as fh:
        fh.write(out)
    print("wrote %s: frames rest, rest1 (classic), slide, slide1 (full slide); skin %dx%d" % (dst, w, H))


if __name__ == "__main__":
    main()
