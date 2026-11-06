# texgen.py -- the texturing helpers' pixel work (docs/vr-port/MODELS_IN_BLENDER.md, "5. Texturing"), numpy only (no
# bpy): Quake's palette as arrays, quantizing as the export does, the texels the helpers leave alone (the fullbright
# ones, the flat area at texel (0, 0) the engine floods), shading along a palette ramp, the material fills' patterns,
# rasterizing UV triangles and edges, and the palette files for other paint programs.
#
# Images here are (h, w, 3) int arrays of 0..255 in Blender's row order (row 0 at the bottom: the skin's last row);
# the skin's texel (0, 0) is [h - 1, 0].

import numpy as np

from . import qpal

PAL = np.array(qpal.PALETTE, np.int32)                       # (256, 3)
LUM = PAL @ np.array([0.299, 0.587, 0.114])                  # (256,) 0..255
FB = qpal.FULLBRIGHT
ROWS = FB // 16                                              # the 14 ordinary ramps of 16 colours (224 / 16)
KEYS = (PAL[:, 0] << 16) | (PAL[:, 1] << 8) | PAL[:, 2]

# The ramps' names (the palette's rows of 16), for the fills' ramp choice.
RAMP_NAMES = ("Grey", "Brown", "Blue grey", "Olive", "Red", "Ochre", "Orange", "Tan", "Pink", "Purple", "Beige",
              "Green grey", "Yellow", "Blue")


def keys_of(rgb):
    rgb = np.asarray(rgb, np.int64)
    return (rgb[..., 0] << 16) | (rgb[..., 1] << 8) | rgb[..., 2]


def exact_index(rgb):
    """The palette index whose colour each texel is exactly (an ordinary one before a fullbright one), else -1."""
    k = keys_of(rgb)
    out = np.full(k.shape, -1, np.int32)
    for i in range(255, -1, -1):  # (low indices last: they win)
        out[k == KEYS[i]] = i
    return out


def _nearest(colors, glow):
    """colors (n, 3): the nearest palette index, among 0..223 or (glow) all, a fullbright winning a tie."""
    d = ((colors[:, None, :].astype(np.int64) - PAL[None, :, :]) ** 2).sum(2)
    if not glow:
        return np.argmin(d[:, :FB], 1).astype(np.int32)
    order = np.r_[np.arange(FB, 256), np.arange(FB)]
    return order[np.argmin(d[:, order], 1)].astype(np.int32)


def nearest_ordinary(rgb):
    """(h, w) indices of the nearest ordinary colours (an exact one: itself)."""
    flat = np.asarray(rgb, np.int64).reshape(-1, 3)
    uk, inv = np.unique(keys_of(flat), return_inverse=True)
    ucol = np.stack([(uk >> 16) & 255, (uk >> 8) & 255, uk & 255], 1)
    return _nearest(ucol, False)[inv.reshape(-1)].reshape(rgb.shape[:2]).astype(np.int32)


def quantize(rgb, old=None):
    """As the export puts a skin on the palette (qpal.Quantizer.indices): rgb (h, w, 3), old (h, w) indices or None.
    Returns the (h, w) indices."""
    shape = rgb.shape[:2]
    flat = np.asarray(rgb, np.int64).reshape(-1, 3)
    k = keys_of(flat)
    uk, inv = np.unique(k, return_inverse=True)
    inv = inv.reshape(-1)
    ucol = np.stack([(uk >> 16) & 255, (uk >> 8) & 255, uk & 255], 1)
    ordinary = _nearest(ucol, False)[inv]
    out = ordinary
    exact_fb = np.full(len(uk), -1, np.int32)
    for i in range(255, FB - 1, -1):  # (as qpal.EXACT_FULLBRIGHT: the lowest index wins)
        if KEYS[i] not in KEYS[:FB]:
            exact_fb[uk == KEYS[i]] = i
    efb = exact_fb[inv]
    out = np.where(efb >= 0, efb, out)
    if old is not None:
        o = np.asarray(old, np.int32).reshape(-1)
        glow = o >= FB
        if glow.any():
            out = np.where(glow, _nearest(ucol, True)[inv], out)
        out = np.where(KEYS[o] == k, o, out)
    return out.reshape(shape).astype(np.int32)


def corner_mask(rgb):
    """The flat area of texel (0, 0)'s colour joined to it (Mod_FloodFillSkin repaints it): a (h, w) bool mask."""
    k = keys_of(rgb)
    h, w = k.shape
    same = k == k[h - 1, 0]
    m = np.zeros((h, w), bool)
    m[h - 1, 0] = True
    while True:
        g = m.copy()
        g[1:, :] |= m[:-1, :]
        g[:-1, :] |= m[1:, :]
        g[:, 1:] |= m[:, :-1]
        g[:, :-1] |= m[:, 1:]
        g &= same
        if (g == m).all():
            return m
        m = g


def protected(rgb, fullbright=True, corner=True):
    """The texels the helpers leave alone unless asked: the glowing ones and the corner area."""
    m = np.zeros(rgb.shape[:2], bool)
    if fullbright:
        m |= exact_index(rgb) >= FB
    if corner:
        m |= corner_mask(rgb)
    return m


def ramp(row):
    """A ramp's 16 indices, dark to light."""
    idx = np.arange(row * 16, row * 16 + 16)
    return idx[np.argsort(LUM[idx], kind="stable")]


def shade_in_ramp(idx, factor, add=0.0):
    """Each texel's colour made darker (factor < 1) or lighter (add, 0..1 of white) along its own ramp: the index of
    its row whose brightness is nearest. idx (n,) ordinary indices."""
    idx = np.asarray(idx, np.int32)
    target = LUM[idx] * factor + np.asarray(add) * 255.0
    rows = idx // 16
    cand = rows[:, None] * 16 + np.arange(16)[None, :]
    best = np.argmin(np.abs(LUM[cand] - target[:, None]), 1)
    return cand[np.arange(len(idx)), best].astype(np.int32)


def pick_in_ramp(row, v, lo=0.15, hi=0.9):
    """v (0..1) to an index of the ramp, from its lo..hi part (the near-black and the brightest left for the shading)."""
    r = ramp(row)
    pos = np.clip(lo + np.asarray(v) * (hi - lo), 0.0, 1.0) * 15.0
    return r[np.clip(np.round(pos).astype(np.int32), 0, 15)]


# ----------------------------------------------------------------------------
# Patterns (0..1 values over the skin's texels; deterministic for a seed)


def value_noise(h, w, cell_y, cell_x, rng):
    """Smooth noise: a random grid every cell_x x cell_y texels, bilinear between (wraps)."""
    gy, gx = max(2, int(np.ceil(h / max(cell_y, 1e-3))) + 1), max(2, int(np.ceil(w / max(cell_x, 1e-3))) + 1)
    g = rng.random((gy, gx))
    y = np.arange(h)[:, None] / max(cell_y, 1e-3)
    x = np.arange(w)[None, :] / max(cell_x, 1e-3)
    y0, x0 = np.floor(y).astype(int), np.floor(x).astype(int)
    fy, fx = y - y0, x - x0
    fy, fx = fy * fy * (3 - 2 * fy), fx * fx * (3 - 2 * fx)
    y0, x0 = y0 % gy, x0 % gx
    y1, x1 = (y0 + 1) % gy, (x0 + 1) % gx
    a = g[y0, x0] * (1 - fx) + g[y0, x1] * fx
    b = g[y1, x0] * (1 - fx) + g[y1, x1] * fx
    return a * (1 - fy) + b * fy


def fbm(h, w, cell_y, cell_x, rng, octaves=3):
    out = np.zeros((h, w))
    amp, tot = 1.0, 0.0
    for _ in range(octaves):
        out += amp * value_noise(h, w, cell_y, cell_x, rng)
        tot += amp
        amp *= 0.5
        cell_y, cell_x = cell_y / 2, cell_x / 2
    return out / tot


def scratches(h, w, n, length, rng):
    """n short straight scratches (1 texel wide): a (h, w) 0/1 map."""
    out = np.zeros((h, w))
    for _ in range(n):
        y, x = rng.random() * h, rng.random() * w
        a = rng.random() * np.pi
        ln = length * (0.5 + rng.random())
        t = np.linspace(0, ln, int(ln) + 2)
        ys = np.clip(np.round(y + np.sin(a) * t).astype(int), 0, h - 1)
        xs = np.clip(np.round(x + np.cos(a) * t).astype(int), 0, w - 1)
        out[ys, xs] = 1.0
    return out


# The fills: (default ramp, how the pattern is made). The value goes to the ramp's lo..hi part.
MATERIALS = {
    "METAL": ("Metal", "Brushed metal: streaks along the skin's x, scratches", 0),
    "WOOD": ("Wood", "Wood: grain along the skin's x, rings and knots", 1),
    "LEATHER": ("Leather", "Leather: pebbled grain, worn blotches", 7),
    "RUST": ("Rust", "Rusted metal: orange-brown blotches eating grey metal", 6),
    "CLOTH": ("Cloth", "Woven cloth: threads crossing, a little fuzz", 11),
}


def pattern(kind, h, w, scale=1.0, seed=1):
    """(values (h, w) 0..1, second ramp mask (h, w) bool or None: texels of the metal under the rust)."""
    rng = np.random.default_rng(seed)
    s = max(scale, 0.1)
    if kind == "METAL":
        v = 0.55 + 0.35 * (fbm(h, w, 1.5 * s, 24 * s, rng) - 0.5) + 0.25 * (fbm(h, w, 12 * s, 12 * s, rng) - 0.5)
        v += 0.25 * scratches(h, w, int(h * w / (400 * s * s)) + 1, 6 * s, rng)
        return np.clip(v, 0, 1), None
    if kind == "WOOD":
        warp = fbm(h, w, 10 * s, 40 * s, rng)
        rings = (np.arange(h)[:, None] / (5.0 * s) + 6.0 * warp) % 1.0
        grain = fbm(h, w, 1.0 * s, 16 * s, rng)
        v = 0.35 + 0.35 * np.abs(rings * 2 - 1) ** 1.5 + 0.3 * (grain - 0.5)
        return np.clip(v, 0, 1), None
    if kind == "LEATHER":
        pebble = value_noise(h, w, 1.3 * s, 1.3 * s, rng)
        blotch = fbm(h, w, 14 * s, 14 * s, rng)
        v = 0.5 + 0.18 * (pebble - 0.5) + 0.45 * (blotch - 0.5)
        return np.clip(v, 0, 1), None
    if kind == "RUST":
        metal = 0.55 + 0.3 * (fbm(h, w, 1.5 * s, 20 * s, rng) - 0.5)
        blot = fbm(h, w, 9 * s, 9 * s, rng, 4)
        rust = blot > 0.48
        v = np.where(rust, 0.25 + 0.9 * (blot - 0.48) + 0.2 * (value_noise(h, w, s, s, rng) - 0.5), metal)
        return np.clip(v, 0, 1), ~rust
    if kind == "CLOTH":
        p = max(2, int(round(2 * s)))
        y, x = np.arange(h)[:, None], np.arange(w)[None, :]
        warp = ((x // p + y // p) % 2) == 0
        along = np.where(warp, (y % p) / max(p - 1, 1), (x % p) / max(p - 1, 1))
        v = 0.4 + 0.35 * np.sin(np.pi * along) - 0.1 * warp + 0.25 * (fbm(h, w, 6 * s, 6 * s, rng) - 0.5)
        return np.clip(v, 0, 1), None
    raise ValueError("no material %s" % kind)


# ----------------------------------------------------------------------------
# UV space


def raster_triangles(uvs, w, h, pad=0.5):
    """uvs (n, 3, 2) in 0..1: the texels whose centre lies in a triangle or within pad texels of it ((h, w) bool,
    Blender's rows)."""
    m = np.zeros((h, w), bool)
    for tri in np.asarray(uvs, np.float64):
        p = tri * np.array([w, h])
        x0, y0 = np.floor(p.min(0) - pad).astype(int)
        x1, y1 = np.ceil(p.max(0) + pad).astype(int)
        x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, w - 1), min(y1, h - 1)
        if x1 < x0 or y1 < y0:
            continue
        ys, xs = np.mgrid[y0:y1 + 1, x0:x1 + 1]
        cx, cy = xs + 0.5, ys + 0.5
        area = (p[1, 0] - p[0, 0]) * (p[2, 1] - p[0, 1]) - (p[2, 0] - p[0, 0]) * (p[1, 1] - p[0, 1])
        if abs(area) < 1e-9:
            continue
        sgn = 1.0 if area > 0 else -1.0
        inside = np.ones(cx.shape, bool)
        for a, b in ((0, 1), (1, 2), (2, 0)):
            ex, ey = p[b] - p[a]
            ln = np.hypot(ex, ey)
            if ln < 1e-9:
                continue
            dist = sgn * (ex * (cy - p[a, 1]) - ey * (cx - p[a, 0])) / ln  # texels inside the edge (positive)
            inside &= dist >= -pad
        m[y0:y1 + 1, x0:x1 + 1] |= inside
    return m


def raster_edges(edges, w, h):
    """edges (n, 2, 2) in 0..1: the texels the lines cross ((h, w) bool)."""
    m = np.zeros((h, w), bool)
    for e in np.asarray(edges, np.float64):
        p = e * np.array([w, h])
        n = int(np.ceil(np.abs(p[1] - p[0]).max())) + 2
        t = np.linspace(0, 1, n)[:, None]
        q = p[0] + (p[1] - p[0]) * t
        xs = np.clip(np.floor(q[:, 0]).astype(int), 0, w - 1)
        ys = np.clip(np.floor(q[:, 1]).astype(int), 0, h - 1)
        m[ys, xs] = True
    return m


# ----------------------------------------------------------------------------
# Palette files for other paint programs (written into the add-on's palettes folder: write_palette_files)


def palette_files():
    """{file name: text}: GIMP/Krita/Aseprite .gpl (the glowing colours named "FULLBRIGHT"), Aseprite/Paint Shop .pal
    (JASC), .hex (Aseprite, Lospec), and the glowing colours alone (quake_fullbright.gpl)."""
    def gpl(title, idx):
        lines = ["GIMP Palette", "Name: %s" % title, "Columns: 16",
                 "# Quake's palette (gfx/palette.lmp). Indices %d..255 are FULLBRIGHT: they glow in the dark. Paint "
                 "them only where a texel should glow (sights, screens, lights); everything else the game puts on "
                 "the ordinary colours (0..%d)." % (FB, FB - 1)]
        for i in idx:
            r, g, b = qpal.PALETTE[i]
            lines.append("%3d %3d %3d\t%s %d" % (r, g, b, "FULLBRIGHT" if i >= FB else "index", i))
        return "\n".join(lines) + "\n"
    pal = ["JASC-PAL", "0100", "256"] + ["%d %d %d" % c for c in qpal.PALETTE]
    hexs = ["%02x%02x%02x" % c for c in qpal.PALETTE]
    return {
        "quake.gpl": gpl("Quake", range(256)),
        "quake_fullbright.gpl": gpl("Quake fullbright (glows)", range(FB, 256)),
        "quake_ordinary.gpl": gpl("Quake (no fullbright)", range(FB)),
        "quake.pal": "\r\n".join(pal) + "\r\n",
        "quake.hex": "\n".join(hexs) + "\n",
    }


def write_palette_files(folder):
    import os
    os.makedirs(folder, exist_ok=True)
    out = []
    for name, text in palette_files().items():
        p = os.path.join(folder, name)
        with open(p, "w", newline="") as f:
            f.write(text)
        out.append(p)
    return out
