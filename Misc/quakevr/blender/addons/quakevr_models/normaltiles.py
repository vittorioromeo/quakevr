# normaltiles.py -- the alias models' (.mdl) baked relief: Quake VR's generated models (the flashlight, the wrist
# gadget and its strap, the shell, the holster, the pauldrons) and the view models (v_*.mdl). normalbake.py and
# docs/vr-port/ROUND21.md, "Baked normal maps".
#
# Their shape: each face's normal as the model's facets stand for it (a faceted tube is round, a box stays a box:
# normaldetail.smooth_normals), and the hard creases rounded a couple of texels wide (normaldetail.bevel_normals).
# Their detail: the generated models map every face of a material onto that material's tile of the skin, so what a
# tile carries is on every face drawn from it: each generator's own knowledge of its tiles (the knurling it paints
# on the flashlight's tube, the ribs on its cap, the fins on its head, the strap's webbing and stitching) raised
# in texel units, the same slope on every face. The view models' and the leather models' painted skins give theirs:
# the dark seams painted into them as grooves, their painted rivets and stitches as bumps, the wood's grain.

import math

import numpy as np

try:
    from . import normalbake as nb
except ImportError:
    import normalbake as nb
try:
    from . import normaldetail as nd
except ImportError:
    import normaldetail as nd


def smooth(x):
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3 - 2 * x)


def vgroove(d, w):
    """A V groove's depth profile (1 at its line, 0 from w texels off it)."""
    return np.clip(1.0 - np.abs(d) / w, 0.0, 1.0)


def modd(x, p):
    """Distance from x to the nearest multiple of p."""
    f = np.mod(x, p)
    return np.minimum(f, p - f)


def dome(d, r):
    return np.sqrt(np.clip(1.0 - (d / r) ** 2, 0.0, 1.0))


# ----------------------------------------------------------------------------
# Tiles: (s0, t0, s1, t1) as the generators lay them out (bake_normals.py checks them against the generators), and
# for each a height in texels as a function of the texel's place in it: i across (s), j down (t), both from the
# tile's corner, and its size (w, h).


def flashlight_tiles():
    def body(i, j, w, h):  # knurling: a diamond grid of cut lines, 4 texels apart (paint(): (i +- j) % 4 == 0)
        return -0.9 * np.maximum(vgroove(modd(i + j, 4.0) / math.sqrt(2), 0.9), vgroove(modd(i - j, 4.0) / math.sqrt(2), 0.9))

    def head(i, j, w, h):  # cooling fins: grooves every 5 texels across the axis, the flare smooth
        u = (i + 0.5) / w
        return -1.0 * vgroove(modd(i, 5.0), 1.1) * smooth((u - 0.3) * w / 1.5 + 0.5)

    def cap(i, j, w, h):  # ribs round the cap: grooves 2 texels wide every 4
        f = np.mod(i, 4.0)
        return -0.8 * smooth((1.0 - np.abs(f - 1.0)) / 0.5)

    def ring(i, j, w, h):  # the grip rings: crowned across
        u = (i + 0.5) / w
        return 2.5 * (1.0 - (2 * u - 1) ** 2)

    def rubber(i, j, w, h):  # a stippled grip
        return 0.45 * dome(np.hypot(modd(i, 3.0), modd(j + 1.5 * (np.floor(i / 3.0 + 0.5) % 2), 3.0)), 1.2)

    return {"regions": {"body": (0, 0, 64, 32), "head": (64, 0, 96, 32), "cap": (96, 0, 128, 32),
                        "ring": (0, 32, 32, 48), "bezel": (32, 32, 64, 48), "rubber": (64, 32, 96, 64),
                        "lens": (96, 32, 128, 64), "face": (0, 48, 64, 64)},
            "tiles": {"body": body, "head": head, "cap": cap, "ring": ring, "rubber": rubber}}


GADGET_REGIONS = {"casing": (0, 0, 32, 32), "metal": (32, 0, 64, 32), "screen": (0, 32, 32, 64),
                  "strap": (32, 32, 56, 64), "led": (56, 32, 64, 64)}


def gadget_tiles():
    def casing(i, j, w, h):  # a parting line round each face, a little in from its edges (the lid's and the shell's)
        d = np.minimum(np.minimum(i + 0.5, w - 0.5 - i), np.minimum(j + 0.5, h - 0.5 - j))
        return -0.8 * vgroove(d - 4.0, 0.9)

    return {"regions": GADGET_REGIONS, "tiles": {"casing": casing}}


def strap_tiles():
    def strap(i, j, w, h):
        # webbing: fine ribs along the band (its faces run round the forearm across i, along it down j); stitched
        # along both edges
        rib = 0.25 * np.abs(np.sin(np.pi * (i + 0.5) / 2.0))
        edge = np.minimum(np.abs(j + 0.5 - 3.0), np.abs(h - 3.5 - j))
        stitch = 0.5 * smooth((0.9 - edge) / 0.4) * smooth((1.6 - modd(i - 1.0, 3.0)) / 0.5) - 0.35 * smooth((1.3 - edge) / 0.4)
        return rib * smooth((edge - 1.2) / 0.5) + stitch

    return {"regions": GADGET_REGIONS, "tiles": {"strap": strap}}


def shell_tiles():
    def hull(i, j, w, h):  # faint ribs (paint(): int(u * 6) % 2)
        u = (i + 0.5) / w
        return 0.5 * smooth((np.abs(np.mod(u * 6.0, 2.0) - 1.0) - 0.35) / 0.3)

    def base(i, j, w, h):  # the primer: a cup set in a groove
        d = np.hypot((i + 0.5) / w - 0.5, (j + 0.5) / h - 0.5) * 2
        return -0.6 * vgroove(d - 0.33, 0.07) + 0.3 * smooth((0.28 - d) / 0.05)

    return {"regions": {"hull": (0, 0, 16, 16), "brass": (16, 0, 32, 16), "base": (0, 16, 16, 32),
                        "mouth": (16, 16, 32, 32)},
            "tiles": {"hull": hull, "base": base}}


# What each model gets. "tiles": a generator's tiles; "paint": its painted skin's seams, rivets and grain (not for
# the dithered skins of mdlgen.py's models, whose speckle is no shape); "bevel": the creases' width in skin texels.
MODELS = {
    "vrflashlight.mdl": {"tiles": flashlight_tiles, "paint": False, "bevel": 1.6, "scale": 4},
    "vrgadget.mdl": {"tiles": gadget_tiles, "paint": False, "bevel": 1.6, "scale": 4},
    "vrgadget_strap.mdl": {"tiles": strap_tiles, "paint": False, "bevel": 1.2, "scale": 4},
    "vr_shell.mdl": {"tiles": shell_tiles, "paint": False, "bevel": 1.2, "scale": 4},
    "legholster.mdl": {"paint": True, "grain": 0.3, "bevel": 1.4, "scale": 4},
    "vrpauldron.mdl": {"paint": True, "grain": 0.3, "bevel": 1.6, "scale": 4},
    "vrpauldron_arm.mdl": {"paint": True, "grain": 0.3, "bevel": 1.6, "scale": 4},
}
VIEW_MODEL = {"paint": True, "grain": 0.55, "bevel": 1.8, "scale": 2}
# The axe's head is painted with streaks of dried blood in the wood's own browns: only its handle is wood.
MODELS["v_axe.mdl"] = dict(VIEW_MODEL, wood_rects=[(440, 0, 512, 130)])


def recipe(name):
    """The recipe for a model file name: its own, or a view model's (v_*.mdl)."""
    return MODELS.get(name.lower(), VIEW_MODEL)


def tile_heights(low, tiles, scale):
    """The tiles' heights (texels) as an image `scale` times the skin's size."""
    W, H = low.W * scale, low.H * scale
    ys, xs = np.mgrid[0:H, 0:W]
    s = (xs + 0.5) / scale  # texel coordinates, as the model's (0.5 the first texel's middle)
    t = (ys + 0.5) / scale
    out = np.zeros((H, W))
    for name, fn in tiles["tiles"].items():
        s0, t0, s1, t1 = tiles["regions"][name]
        m = (s >= s0) & (s < s1) & (t >= t0) & (t < t1)
        out[m] = fn(s[m] - s0 - 0.5, t[m] - t0 - 0.5, float(s1 - s0), float(t1 - t0))
    return out


def painted_dots(rgb, cov, lo=0.25, hi=0.6, most=3.5):
    """Small round spots painted into a skin (rivets, studs, stitches): 0..1, where the brightness has a peak or a pit
    about a texel or two across, round, and clear of the speckle."""
    lum = (rgb[..., 0] * 0.3 + rgb[..., 1] * 0.59 + rgb[..., 2] * 0.11) / 255.0
    L = nb.blur(lum, 0.8, cov)
    Lp = np.pad(L, 1, mode="edge")
    dxx = Lp[1:-1, 2:] - 2 * L + Lp[1:-1, :-2]
    dyy = Lp[2:, 1:-1] - 2 * L + Lp[:-2, 1:-1]
    dxy = (Lp[2:, 2:] - Lp[2:, :-2] - Lp[:-2, 2:] + Lp[:-2, :-2]) * 0.25
    tr, det = dxx + dyy, dxx * dyy - dxy * dxy
    disc = np.sqrt(np.maximum(tr * tr * 0.25 - det, 0))
    l1, l2 = tr * 0.5 + disc, tr * 0.5 - disc
    peak = np.where(l1 < 0, -l1, 0.0)  # both curvatures down: a bright spot
    mu = nb.blur(lum, 3.0, cov)
    sd = np.sqrt(np.maximum(nb.blur(lum * lum, 3.0, cov) - mu * mu, 1e-6))
    v = smooth((peak / (sd + 0.02) - lo) / (hi - lo)) * cov
    lab = nb.labels(v > 0.05)
    keep = np.zeros(lab.max() + 2)
    ys, xs = np.nonzero(lab >= 0)
    ids = lab[ys, xs]
    order = np.argsort(ids, kind="stable")
    ids, ys, xs = ids[order], ys[order], xs[order]
    starts = np.flatnonzero(np.r_[True, ids[1:] != ids[:-1]]) if len(ids) else np.zeros(0, np.int64)
    for a, b in zip(starts, np.r_[starts[1:], len(ids)]):
        ext = max(xs[a:b].max() - xs[a:b].min(), ys[a:b].max() - ys[a:b].min()) + 1
        keep[ids[a]] = 1.0 if ext <= most else 0.0
    return np.where(lab >= 0, v * keep[np.maximum(lab, 0)], 0.0)


def paint_heights(low, skin, grain_depth, scale, wood_rects=None):
    """A painted skin's heights (texels) at `scale`: its seams grooved, its rivets and stitches raised, its wood and
    leather grained."""
    rgb = skin.astype(np.float64)
    cov = nb.coverage(low, 1)
    lines = nb.blur(nb.painted_lines(rgb, cov, lo=0.12, hi=0.4, min_len=6.0, full_len=16.0), 0.5, cov)
    dots = nb.blur(painted_dots(rgb, cov), 0.6, cov)
    wood = nd.wood_mask(rgb, cov)
    if wood_rects:  # the recipe knows where the wood is
        m = np.zeros(wood.shape)
        for s0, t0, s1, t1 in wood_rects:
            m[t0:t1, s0:s1] = 1.0
        wood *= m
    h = -0.8 * lines * (1 - 0.6 * wood) + 0.9 * dots + nd.grain(rgb, cov, wood, grain_depth)
    h = nb.dilate(np.dstack([h] * 3), cov, 6)[0][..., 0]
    # up to the map's size (bilinear), where the tiles' heights are made
    return nb.sampler(h)(np.stack(np.meshgrid((np.arange(low.W * scale) + 0.5) / scale,
                                              (np.arange(low.H * scale) + 0.5) / scale), -1).reshape(-1, 2)
                         ).reshape(low.H * scale, low.W * scale)


def bake(low, skin, rec=None, supersample=2, base=None, details=True):
    """An alias model's map, (h, w, 3) floats: `scale` (the recipe's) times its skin's size. `base(low, raster)`: the
    shape's normals (a high poly's), else the facets' and creases' as above; `details`: the tiles' and the paint's."""
    rec = rec or recipe(low.name)
    scale = rec["scale"] * (2 if low.W < 256 and rec["scale"] < 4 else 1)  # a small skin: at least 4 times
    r = nb.Raster(low, scale * supersample)
    if base:
        nw = base(low, r)
    else:
        nw, _ = nd.bevel_normals(low, r, width=rec["bevel"], base=nd.smooth_normals(low, r, nd.SMOOTH_ANGLE),
                                 min_angle=nd.SMOOTH_ANGLE)
    detail = None
    if details:
        hs = scale * supersample
        h = np.zeros((low.H * hs, low.W * hs))
        if rec.get("tiles"):
            h += tile_heights(low, rec["tiles"](), hs)
        if rec.get("paint"):
            h += paint_heights(low, skin, rec.get("grain", 0.5), hs, rec.get("wood_rects"))
        detail = nb.tangent_uv(r, nb.sampler(h, hs), d=0.5 / hs)
    img, _ = nb.finish(r, nw, detail, supersample=supersample)
    return img
