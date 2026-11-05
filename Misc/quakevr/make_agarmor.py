#!/usr/bin/env python3
# make_agarmor.py -- the skins of the armour plates, armed humanoids' and the player's alike (Quake/vr/vr_agarmor.cpp;
# ROUND22.md, "Armour for armed humanoids, P0"; ROUND24.md, "The armour's look"): quakevr/progs/vragarmor.mdl, a
# placeholder quad carrying nine skins in the game's palette, type - 1 + 3 x wear (0 green, 1 yellow, 2 red; wear 0 whole, 1 dented and
# scratched, 2 battered, chipped and scorched), a broken plate's debris the same; and vragarmor.mdl_0_norm.png, the relief (rivets,
# pits, scratches, the plates' edges; heights in its alpha) of them all. In the palette, each texel its own colour: vr_retro has nothing
# to pull or dither (its ordered dither's checker on a full-colour skin's in-between colours), the grain the paint's own.
#
# All painted here from nothing but noise and shapes. The plates' shapes are not made here: the engine cuts them from
# the wearer's own body when it loads "<model>#armor" (vr_agarmor.cpp platesOf) and maps each onto one TILE of this atlas.
# The atlas's layout is shared with vr_agarmor.cpp: keep both alike.
#
#   plates  (0, 0)-(160, 128): TILES_X x TILES_Y tiles, one plate each: the bevel on the tile's outer BEVEL share, the
#            top of the tile the plate's top (the end under the plate above)
#   helmet  (160, 0)-(256, 64): the helmet's walls (vr_agarmor.cpp `helmetOf`: the Ranger's helmet), mirrored left
#            and right: a from the front's middle (0) round the side to the back's (1), b down its rows; the columns'
#            and rows' places are HELM_U and HELM_V (vr_agarmor.cpp's helmU and helmV: keep them alike)
#   cap     (160, 64)-(192, 96): the helmet's flat top seen from above, the front at +a
#   lining  (192, 64)-(224, 96): the helmet's open edges (round the face, under the guards): b 0 outer .. 1 inner
#   bracket (224, 64)-(256, 96): the small block on each side of the helmet: its outer face the whole block
#   inner   (160, 96)-(208, 128): the padding inside (dark leather)
#   rim     (208, 96)-(256, 128): the plates' sides, down into the gaps: b 0 the outer edge .. 1 the inner (grime)
# (Texels of the 256 x 128 skin; the full-colour skins and relief are SCALE times that.)
#
# Usage: py -3.13 Misc/quakevr/make_agarmor.py [output progs folder]   (deterministic: the same bytes every run; needs numpy)

import os
import struct
import sys
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import genguard  # noqa: E402
import mdlgen  # noqa: E402
from mdlpolish import palette  # noqa: E402

SKIN_W, SKIN_H = 256, 128
SCALE = 2
W, H = SKIN_W * SCALE, SKIN_H * SCALE
REGIONS = {
    "whole": (0, 0, 256, 128),  # (the placeholder's quad: every texel in its one island, for the relief's parallax)
}
BLOCKS = {
    "plates": (0, 0, 160, 128),
    "helmet": (160, 0, 256, 64),
    "cap": (160, 64, 192, 96),
    "lining": (192, 64, 224, 96),
    "bracket": (224, 64, 256, 96),
    "inner": (160, 96, 208, 128),
    "rim": (208, 96, 256, 128),
}
TILES_X, TILES_Y = 5, 6  # (vr_agarmor.cpp plateTilesX, plateTilesY)
BEVEL = 0.12             # (vr_agarmor.cpp plateBevelUv)
# What each tier's skin may use of the palette: its own family, so a shade or a stain never turns another colour.
GREYS, BROWNS = list(range(0, 13)), list(range(16, 23))  # (no white; the dark browns: rust, grime, leather)
TIER_COLOURS = {1: list(range(176, 192)) + GREYS + BROWNS, 2: list(range(16, 32)) + list(range(198, 208)) + GREYS, 3: list(range(64, 80)) + list(range(96, 104)) + GREYS + BROWNS}

# The paint, full colour. Not the old ramps' pure hues: a dull painted metal, red the most held back, and still three hues
# told apart in the dark (the generator check: vr_agarmor_look_check's hue distance). sRGB, 0..255. vr_retro pulls what's
# drawn to Quake's palette, which has no true green and few dull reds: each tier stays near one family so it doesn't jump between
# them: green the palette's sage row (176-191; a little more red and it's the olive row, as a shade or a stain), yellow its
# tan row, red between its pure reds and its browns.
PAINT = {1: (64, 88, 48), 2: (152, 136, 44), 3: (118, 26, 26)}
STEEL = (120, 116, 108)   # the bare metal at worn edges
RUST = {1: (58, 62, 48), 2: (104, 80, 36), 3: (88, 40, 28)}  # (each near its own hue: an accent, and no tier's plates turn another's colour)
GRIME = (26, 22, 18)
RUST_SHARE = {1: 0.35, 2: 0.4, 3: 0.45}  # how far a patch of rust goes over the paint
LEATHER_DARK = (46, 40, 26)
LEATHER_BROWN = (62, 42, 26)

# The helmet's columns (the front's middle, the front chamfer's ends, the back chamfer's ends, the back's middle) and
# rows (the top's edge, below its chamfer, the bucket's brow, the ledge under it, the cheekbones, the jaw and nape).
HELM_U = [0.0, 0.16, 0.3, 0.7, 0.84, 1.0]
HELM_V = [0.0, 0.08, 0.38, 0.46, 0.7, 1.0]


def noise_grid(w, h, cell, seed):
    """Smooth value noise, 0..1, `cell` texels a lattice step."""
    rng = np.random.RandomState(seed)
    gw, gh = w // cell + 2, h // cell + 2
    grid = rng.random_sample((gh, gw))
    y, x = np.mgrid[0:h, 0:w]
    fx, fy = x / cell, y / cell
    ix, iy = fx.astype(int), fy.astype(int)
    tx, ty = fx - ix, fy - iy
    tx = tx * tx * (3 - 2 * tx)
    ty = ty * ty * (3 - 2 * ty)
    a = grid[iy, ix] * (1 - tx) + grid[iy, ix + 1] * tx
    b = grid[iy + 1, ix] * (1 - tx) + grid[iy + 1, ix + 1] * tx
    return a * (1 - ty) + b * ty


def fractal(w, h, cell, seed, octaves=3):
    out = np.zeros((h, w))
    amp, total = 1.0, 0.0
    for o in range(octaves):
        out += amp * noise_grid(w, h, max(1, cell >> o), seed + o * 17)
        total += amp
        amp *= 0.5
    return out / total


def white(w, h, seed):
    return np.random.RandomState(seed).random_sample((h, w))


def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def mix(a, b, t):
    """a to b by t (h x w), a and b h x w x 3 or colours."""
    t = t[..., None]
    return a * (1 - t) + np.asarray(b, dtype=float) * t


class Layers:
    """The atlas being painted, as full-colour paint's shares and shading, then its height."""

    def __init__(self):
        # Shared by the tiers: where the wear is, so a tier's upgrade keeps its plates' marks.
        self.fine = white(W, H, 11)
        self.mottle = fractal(W, H, 32, 21)
        self.blotch = fractal(W, H, 20, 31)
        self.grain = fractal(W, H, 4, 41, 2)
        self.rustmask = fractal(W, H, 16, 51)
        self.pits = 0.6 * noise_grid(W, H, 3, 61) + 0.4 * white(W, H, 62)
        self.scratch = self.scratches(71)
        # What each texel is: 0 paint's share of the base (shade), rust, steel, grime, and its height.
        self.shade = np.ones((H, W))
        self.edge = np.zeros((H, W))     # the worn metal's reach, before its noise
        self.rust = np.zeros((H, W))     # rust's reach, before its noise
        self.grime = np.zeros((H, W))
        self.rivet = np.zeros((H, W))    # a rivet's dome (0..1)
        self.height = np.ones((H, W))
        self.tone = np.ones((H, W, 3))   # a plate's own tone
        self.nopaint = np.zeros((H, W))  # leather: not armour's paint
        self.leather = np.zeros((H, W, 3))

    @staticmethod
    def scratches(seed):
        """Thin bright lines, a handful of slants."""
        rng = np.random.RandomState(seed)
        out = np.zeros((H, W))
        for _ in range(110):
            x, y = rng.randint(0, W), rng.randint(0, H)
            ang = rng.uniform(0.0, np.pi)
            n = rng.randint(6, 22)
            amp = rng.uniform(0.4, 1.0)
            for k in range(n):
                px, py = int(x + np.cos(ang) * k) % W, int(y + np.sin(ang) * k) % H
                out[py, px] = max(out[py, px], amp)
        return out


def box_of(block):
    s0, t0, s1, t1 = BLOCKS[block]
    return s0 * SCALE, t0 * SCALE, s1 * SCALE, t1 * SCALE


def paint_rivet(lay, cx, cy, r):
    """A dome at (cx, cy) in the atlas's full-size texels."""
    x0, x1 = int(cx - r - 1), int(cx + r + 2)
    y0, y1 = int(cy - r - 1), int(cy + r + 2)
    for y in range(max(0, y0), min(H, y1)):
        for x in range(max(0, x0), min(W, x1)):
            d = np.hypot(x + 0.5 - cx, y + 0.5 - cy) / r
            if d < 1.0:
                lay.rivet[y, x] = max(lay.rivet[y, x], np.sqrt(1.0 - d * d))
            elif d < 1.5:
                lay.grime[y, x] = max(lay.grime[y, x], 0.45 * (1.5 - d) * 2.0)  # the dirt round it


def paint_tiles(lay):
    """One plate a tile: its own tone, its bevel's worn edge, dirt under the plate above and in its corners, rivets, its rust
    where the wet gets in (edges and low corners, a patch or two)."""
    x0, y0, x1, y1 = box_of("plates")
    tw, th = (x1 - x0) / TILES_X, (y1 - y0) / TILES_Y
    rng = np.random.RandomState(101)
    for ty in range(TILES_Y):
        for tx in range(TILES_X):
            ax, ay = x0 + tx * tw, y0 + ty * th
            xs, ys = np.meshgrid(np.arange(int(ax), int(ax + tw)), np.arange(int(ay), int(ay + th)))
            u, v = (xs + 0.5 - ax) / tw, (ys + 0.5 - ay) / th
            e = np.minimum(np.minimum(u, 1 - u), np.minimum(v, 1 - v))  # 0 the tile's edge
            # The plate's own tone: darker or lighter, a little warmer or cooler.
            tone = rng.uniform(0.88, 1.04)
            warm = rng.uniform(-0.012, 0.012)
            lay.tone[ys, xs] = (tone * (1 + warm), tone, tone * (1 - warm))
            # A slope down the plate: its top under the plate above (dark), its bottom lip catching the light.
            lay.shade[ys, xs] = 0.9 + 0.22 * v - 0.62 * smooth(0.34, 0.0, v)  # (the top in the shadow of the lip above)
            # The corners darker (the plates' overlaps), the bevel's own shade from the geometry.
            corner = np.minimum(np.hypot(u, v), np.minimum(np.hypot(1 - u, v), np.minimum(np.hypot(u, 1 - v), np.hypot(1 - u, 1 - v))))
            lay.shade[ys, xs] *= 1.0 - 0.34 * smooth(0.26, 0.06, corner)
            # A dent or a crease, its own for each plate.
            dx, dy = rng.uniform(0.25, 0.75), rng.uniform(0.3, 0.8)
            dr = rng.uniform(0.1, 0.22)
            dd = smooth(1.0, 0.0, np.hypot(u - dx, v - dy) / dr)
            lay.height[ys, xs] -= 0.35 * dd * rng.uniform(0.2, 1.0)
            # The bevel's worn metal.
            lay.edge[ys, xs] = smooth(BEVEL * 1.0, BEVEL * 0.45, e)
            # The dirt: near its edges inside the bevel, thicker in the low corners, out of the rain's way under its top.
            lay.grime[ys, xs] = np.maximum(lay.grime[ys, xs], 0.46 * smooth(BEVEL * 2.4, BEVEL * 1.1, e) * (e > BEVEL * 0.7))
            lay.grime[ys, xs] = np.maximum(lay.grime[ys, xs], 0.52 * smooth(0.42, 0.1, corner))
            lay.grime[ys, xs] = np.maximum(lay.grime[ys, xs], 0.75 * smooth(0.26, 0.02, v))
            # The rust's reach: along its edges, a low corner; a patch on some plates.
            r = 0.55 * smooth(BEVEL * 2.8, BEVEL * 0.8, e) + 0.5 * smooth(0.5, 0.15, np.hypot(1 - v, np.minimum(u, 1 - u)))
            if rng.uniform() < 0.5:
                px, py = rng.uniform(0.2, 0.8), rng.uniform(0.3, 0.85)
                r = r + 0.9 * smooth(1.0, 0.2, np.hypot(u - px, v - py) / rng.uniform(0.12, 0.26))
            lay.rust[ys, xs] = r
            # The bevel's height: down to the tile's edge (the geometry's bevel a little further).
            lay.height[ys, xs] -= 0.5 * smooth(BEVEL * 1.5, BEVEL * 0.4, e)
            # Rivets: at the corners, some with more along the top or the sides.
            inset_x, inset_y = 0.17, 0.19  # (at each plate's corners, just inside its bevel)
            spots = [(inset_x, inset_y), (1 - inset_x, inset_y), (inset_x, 1 - inset_y), (1 - inset_x, 1 - inset_y)]
            kind = rng.randint(0, 3)
            if kind == 1:
                spots += [(0.5, inset_y), (0.5, 1 - inset_y)]
            if kind == 2:
                spots += [(inset_x, 0.5), (1 - inset_x, 0.5)]
            for su, sv in spots:
                paint_rivet(lay, ax + su * tw, ay + sv * th, 2.6)


def helmet_shade(a, b):
    """The walls' shade: a flat-faced bucket over the crown (its top chamfer lit, its lower edge a lit lip over a dark
    ledge), the cheek guards under it (lit edges at the face and the jaw, three slanted vents low on the side). As before; the
    full-colour paint and its wear over it."""
    U, V = HELM_U, HELM_V
    du, dv = 1.0 / 96.0, 1.0 / 64.0
    facet = [0.06, 0.02, -0.02, -0.08, -0.12][min(4, sum(1 for x in U[1:-1] if a >= x))]
    if b < V[1]:
        shade = 0.88 + facet
        if b > V[1] - 1.5 * dv:
            shade = 1.0
    elif b < V[2]:
        shade = 0.62 + facet + 0.1 * (V[2] - b) / (V[2] - V[1])
        if b < V[1] + 1.2 * dv:
            shade = 0.45
        if b > V[2] - 2.2 * dv:
            shade = 0.95 if b > V[2] - 1.2 * dv else 0.8
    elif b < V[3]:
        shade = 0.12
    else:
        shade = 0.52 + facet - 0.12 * (b - V[3]) / (1.0 - V[3])
        if b < V[3] + 1.5 * dv:
            shade = 0.2
        if b > 1.0 - 1.6 * dv:
            shade = 0.85
        elif b > 1.0 - 2.8 * dv:
            shade = 0.3
        if U[1] <= a < U[1] + 1.3 * du:
            shade = 0.9
        x, y = (a - 0.33) / 0.17, (b - 0.74) / 0.2
        if 0.0 <= x <= 1.0 and 0.0 <= y <= 1.0:
            k = (x * 3.0 + y * 1.2) % 1.0
            band = int(x * 3.0 + y * 1.2)
            if band < 3 and 0.12 < y < 0.92:
                if k < 0.42:
                    shade = 0.02
                elif k < 0.6:
                    shade = 0.85
    if b >= V[1] + 1.2 * dv and not (V[2] - 2.2 * dv < b < V[3] + 1.5 * dv) and b < 1.0 - 2.8 * dv:
        for x in U[1:-1]:
            if abs(a - x) < 0.6 * du and not (x == U[1] and b > V[3]):
                shade = max(shade, 0.78 + facet)
    return shade


def paint_helmet(lay):
    x0, y0, x1, y1 = box_of("helmet")
    for y in range(y0, y1):
        for x in range(x0, x1):
            a, b = (x + 0.5 - x0) / (x1 - x0), (y + 0.5 - y0) / (y1 - y0)
            s = helmet_shade(a, b)
            lay.shade[y, x] = 0.3 + 0.95 * s
            lay.height[y, x] = 0.55 + 0.45 * min(1.0, s * 1.3)
            lay.edge[y, x] = 0.65 if s >= 0.78 else 0.0                  # the lit edges, worn
            lay.grime[y, x] = 0.5 if s < 0.25 else 0.0                   # the ledge's and the vents' dirt
    lay.rust[y0:y1, x0:x1] = 0.28
    # The brow band's rivets, and down the guards.
    for k in range(5):
        ax = x0 + (0.06 + 0.2 * k) * (x1 - x0)
        paint_rivet(lay, ax, y0 + 0.3 * (y1 - y0), 1.7)
        paint_rivet(lay, ax, y0 + 0.62 * (y1 - y0), 1.7)
    # The flat top, a dent; the side block, a frame round a slot.
    x0, y0, x1, y1 = box_of("cap")
    ys, xs = np.mgrid[y0:y1, x0:x1]
    aa, bb = (xs + 0.5 - x0) / (x1 - x0) * 2 - 1, (ys + 0.5 - y0) / (y1 - y0) * 2 - 1
    lay.shade[ys, xs] = 1.0 + 0.1 * aa - 0.1 * np.hypot(aa, bb)
    dent = smooth(0.2, 0.0, np.hypot(aa + 0.3, bb - 0.25))
    lay.height[ys, xs] -= 0.4 * dent
    lay.rust[ys, xs] = 0.3
    x0, y0, x1, y1 = box_of("bracket")
    ys, xs = np.mgrid[y0:y1, x0:x1]
    aa, bb = (xs + 0.5 - x0) / (x1 - x0), (ys + 0.5 - y0) / (y1 - y0)
    e = np.minimum(np.minimum(aa, 1 - aa), np.minimum(bb, 1 - bb))
    slot = (aa > 0.3) & (aa < 0.7) & (bb > 0.28) & (bb < 0.72)
    lay.shade[ys, xs] = np.where(e < 0.08, np.where((aa > 0.92) | (bb > 0.92), 0.5, 1.15), np.where(slot, np.where(bb > 0.36, 0.08, 0.7), 0.95))
    lay.edge[ys, xs] = 0.6 * (e < 0.08)
    lay.height[ys, xs] = np.where(slot, 0.2, 1.0)


def paint_leather_and_gaps(lay):
    # The padding inside: dark leather, quilted.
    x0, y0, x1, y1 = box_of("inner")
    ys, xs = np.mgrid[y0:y1, x0:x1]
    aa = (xs + 0.5 - x0) / (x1 - x0)
    quilt = np.sin(np.pi * ((aa * 6.0) % 1.0))
    lay.nopaint[ys, xs] = 1.0
    lay.leather[ys, xs] = np.asarray(LEATHER_DARK, dtype=float)[None, None] * (0.55 + 0.4 * quilt)[..., None] * (0.8 + 0.4 * lay.grain[ys, xs])[..., None]
    lay.height[ys, xs] = 0.7 + 0.3 * quilt
    # The helmet's lining: dark brown leather, its outer edge lighter.
    x0, y0, x1, y1 = box_of("lining")
    ys, xs = np.mgrid[y0:y1, x0:x1]
    bb = (ys + 0.5 - y0) / (y1 - y0)
    lay.nopaint[ys, xs] = 1.0
    lay.leather[ys, xs] = np.asarray(LEATHER_BROWN, dtype=float)[None, None] * np.where(bb < 0.22, 1.3, 0.75)[..., None] * (0.8 + 0.4 * lay.grain[ys, xs])[..., None]
    # The plates' sides, down into the gaps: the paint under grime, near black at their foot.
    x0, y0, x1, y1 = box_of("rim")
    ys, xs = np.mgrid[y0:y1, x0:x1]
    bb = (ys + 0.5 - y0) / (y1 - y0)
    lay.shade[ys, xs] = 0.62 - 0.42 * bb
    lay.grime[ys, xs] = 0.35 + 0.6 * bb
    lay.edge[ys, xs] = 0.5 * smooth(0.25, 0.05, bb)
    lay.rust[ys, xs] = 0.35


def dents_of(level, seed):
    """P2's dents over the whole atlas: (shading, height), hollows dark above, lit below."""
    shade = np.zeros((H, W))
    height = np.zeros((H, W))
    rng = np.random.RandomState(seed)
    count = 60 if level == 1 else 110
    for _ in range(count):
        cx, cy = rng.uniform(0, W), rng.uniform(0, H)
        r = rng.uniform(3.5, 9.0)
        x0, x1 = max(0, int(cx - r)), min(W, int(cx + r) + 1)
        y0, y1 = max(0, int(cy - r)), min(H, int(cy + r) + 1)
        ys, xs = np.mgrid[y0:y1, x0:x1]
        dx, dy = xs + 0.5 - cx, ys + 0.5 - cy
        q = np.hypot(dx, dy) / r
        depth = np.clip(1.0 - q, 0.0, 1.0)
        shade[y0:y1, x0:x1] += 0.5 * depth * dy / r - 0.15 * depth
        height[y0:y1, x0:x1] -= 0.45 * depth
    return shade, height


def paint(lay, armor, level):
    """A skin: tier `armor`, wear `level`; full colour, H x W x 3, 0..255."""
    base = np.asarray(PAINT[armor], dtype=float)
    img = base[None, None] * lay.tone
    # The paint's own mottling and fine grain.
    img = img * (0.92 + 0.16 * lay.mottle)[..., None] * (0.94 + 0.12 * lay.grain)[..., None]
    img = img * np.clip(lay.shade, 0.0, 1.5)[..., None]
    wear = level * 0.22
    # Rust: an accent. Its reach by the plate, its shape by noise, a tier's own share of the paint.
    rust = smooth(0.57 - wear, 0.74 - wear, lay.rust * 0.62 + lay.rustmask * 0.75)
    rust = rust * (0.7 + 0.3 * lay.pits)
    rust_col = np.asarray(RUST[armor], dtype=float)[None, None] * (0.6 + 0.8 * lay.pits)[..., None]
    img = img * (1 - (rust * RUST_SHARE[armor])[..., None]) + rust_col * (rust * RUST_SHARE[armor])[..., None]
    # The grime.
    g = np.clip(lay.grime * (0.55 + 0.9 * lay.blotch) + 0.22 * smooth(0.55, 0.9, lay.blotch), 0.0, 0.9)
    img = mix(img, GRIME, g)
    # The worn metal on the edges and scratches: bare steel where the paint's gone, in broken lengths.
    steel_mask = np.clip(0.8 * lay.edge * smooth(0.40 - wear, 0.62 - wear, lay.pits * 0.5 + lay.blotch * 0.5), 0.0, 1.0)
    steel_mask = np.maximum(steel_mask, 0.7 * lay.scratch * (lay.mottle > 0.42 - wear))
    # (steel with the paint's stain left on it: no other hue at the plates' edges)
    stained = np.asarray(STEEL, dtype=float) * 0.6 + base * 0.55
    steel = stained[None, None] * (0.62 + 0.5 * lay.pits)[..., None]
    img = img * (1 - steel_mask[..., None]) + steel * steel_mask[..., None]
    # Rivets: dull metal, lit from above (the relief too).
    riv = np.clip(lay.rivet * 2.0, 0.0, 1.0)
    lit = 0.55 + 0.5 * lay.rivet
    img = img * (1 - riv[..., None]) + np.asarray((172, 164, 146), dtype=float)[None, None] * (lit * riv)[..., None]
    # Fine specks.
    img = img * (0.96 + 0.08 * lay.fine)[..., None]
    if level >= 1:
        dshade, _ = lay.dents[level]
        img = img * np.clip(1.0 + dshade, 0.3, 1.5)[..., None]
        # more scratches, the metal showing
        extra = lay.scratch2[level] * (lay.blotch > 0.4 - 0.08 * level)
        img = img * (1 - extra[..., None] * 0.75) + steel * (extra[..., None] * 0.75)
    if level >= 2:
        # chips to the bare metal, soot and scorch
        chips = (fractal(W, H, 10, 201) > 0.68) & (white(W, H, 202) > 0.45)
        img = np.where(chips[..., None], steel * 0.8, img)
        scorch = lay.scorch
        soot = smooth(0.52, 0.68, scorch)
        img = mix(img, (18, 16, 15), soot * 0.85)
    # Leather and padding aren't the armour's paint.
    img = img * (1 - lay.nopaint[..., None]) + lay.leather * lay.nopaint[..., None]
    return np.clip(img, 0.0, 255.0)


def relief(lay):
    """The normal map (+x right, +y up the texture as bake_normals.py's) with the heights in its alpha, 255 the surface."""
    h = lay.height.copy()
    # pits where rust lives, the rivets' domes, the scratches' grooves
    # (no texel-fine noise here: lit by the normal map at a texel a pixel it read as a woven mesh over the plates)
    h -= 0.12 * smooth(0.45, 0.85, lay.rust * 0.62 + lay.rustmask * 0.75) * lay.blotch
    h -= 0.12 * lay.scratch
    h += 1.3 * lay.rivet
    h = h * (1 - lay.nopaint) + (0.75 + 0.25 * lay.grain) * lay.nopaint
    # (texels to slope: a texel of this atlas is about 0.15 model units on a plate; heights' 1 about 0.3 units)
    k = 3.0
    gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5 * k
    gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5 * k
    n = np.stack((-gx, gy, np.ones_like(h)), axis=2)
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    rgb = (n * 0.5 + 0.5) * 255.0
    a = np.clip((h - h.min()) / max(h.max() - h.min(), 1e-6), 0.0, 1.0) * 255.0
    return np.dstack((rgb, a))


def write_png(path, arr):
    """arr: H x W x (3|4), 0..255: a plain PNG (deterministic bytes, no library but zlib)."""
    h, w, c = arr.shape
    raw = np.clip(arr + 0.5, 0, 255).astype(np.uint8)
    body = b"".join(b"\0" + raw[y].tobytes() for y in range(h))

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6 if c == 4 else 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(body, 9)))
        f.write(chunk(b"IEND", b""))


def to_palette(img, armor):
    """The skin in 8 bits, the game's own size: each 2 x 2 texels' mean, a little irregular noise (the paint's grain, never a
    regular dither), the nearest colour of the tier's rows of the palette; leather and grime may use the browns and greys."""
    pal = np.asarray(palette(), dtype=float)
    allowed = sorted(set(TIER_COLOURS[armor]))
    table = pal[allowed]
    small = img.reshape(SKIN_H, SCALE, SKIN_W, SCALE, 3).mean(axis=(1, 3))
    # (red between its pure and its brown rows, an irregular grain of the two its dull colour; the others a row each, a little noise)
    amount = {1: 2.0, 2: 2.0, 3: 9.0}[armor]
    grain = (0.25 * noise_grid(SKIN_W, SKIN_H, 3, 410 + armor) + 0.75 * white(SKIN_W, SKIN_H, 400 + armor) - 0.5)[..., None] * np.asarray((1.0, 1.0, 1.0)) * amount
    small = small + grain
    idx = np.zeros((SKIN_H, SKIN_W), dtype=np.uint8)
    allowed = np.asarray(allowed)
    for y in range(SKIN_H):
        # (hue counts for more than brightness: the distance on the colour's red-green and green-blue differences too)
        diff = small[y][:, None, :] - table[None, :, :]
        d = (diff ** 2).sum(axis=2) + 1.5 * ((diff[:, :, 0] - diff[:, :, 1]) ** 2 + (diff[:, :, 1] - diff[:, :, 2]) ** 2)
        idx[y] = allowed[d.argmin(axis=1)]
    return idx.tobytes()


def build():
    lay = Layers()
    paint_tiles(lay)
    paint_helmet(lay)
    paint_leather_and_gaps(lay)
    lay.dents = {1: dents_of(1, 301), 2: dents_of(2, 302)}
    lay.scratch2 = {1: Layers.scratches(311), 2: np.maximum(Layers.scratches(312), Layers.scratches(313))}
    lay.scorch = fractal(W, H, 28, 321)
    return lay


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "quakevr", "progs")
    lay = build()
    # Nine skins: type - 1 + 3 x wear (0 whole, 1 at two thirds of its pool, 2 at a third: vr_agarmor.cpp relink).
    images = [paint(lay, armor, level) for level in (0, 1, 2) for armor in (1, 2, 3)]
    armors = [armor for level in (0, 1, 2) for armor in (1, 2, 3)]
    skins = [to_palette(img, armor) for img, armor in zip(images, armors)]
    mesh = mdlgen.Mesh(SKIN_W, SKIN_H, REGIONS)
    mesh.box((0.0, 0.0, 0.0), (1.0, 1.0, 1.0), "whole")  # (a placeholder: only the skins are used)
    path = os.path.join(out, "vragarmor.mdl")
    paths = [path, os.path.join(out, "vragarmor.mdl_0_norm.png")]
    guard = genguard.Guard("make_agarmor.py", paths)
    mdlgen.write_mdl(path, mesh, skins, "vragarmor")
    write_png(os.path.join(out, "vragarmor.mdl_0_norm.png"), relief(lay))
    guard.finish()
    print("vragarmor.mdl: %dx%d, %d skins -> %s" % (SKIN_W, SKIN_H, len(skins), os.path.normpath(path)))


if __name__ == "__main__":
    main()
