# normalbody.py -- the body's baked relief (vrbody*.md5mesh; the three builds share their texture coordinates, so one
# map serves them all): its skin's blocks as make_vrbody.py lays them out and paints them (its *_texel functions:
# the same places, u round each loft from its first axis, v along it), raised where the paint draws seams,
# stitches, straps, buckles, plates, folds and muscles. Heights in Quake units (1 m = 26.2). normalbake.py and
# docs/vr-port/ROUND21.md, "Baked normal maps".

import numpy as np

try:
    from . import normalbake as nb
except ImportError:
    import normalbake as nb

BODY_SKIN = 256
BODY_PAD = 2.0 / 128
BODY_BLOCKS = {
    "skin": (0.0, 0.0, 0.5, 0.4),
    "head": (0.0, 0.4, 0.5, 0.5),
    "leather": (0.5, 0.0, 1.0, 0.5),
    "cloth": (0.0, 0.5, 0.5, 1.0),
    "boots": (0.5, 0.5, 0.625, 1.0),
    "shaft": (0.625, 0.5, 0.75, 1.0),
    "bracer": (0.75, 0.5, 1.0, 1.0),
}


def smooth(x):
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3 - 2 * x)


def around(bu, at):
    """How far round the ring (0 .. 0.5) bu is from `at` (make_vrbody.py's around)."""
    d = np.abs(bu - at) % 1.0
    return np.minimum(d, 1.0 - d)


def band(x, lo, hi, soft):
    """1 inside [lo, hi], falling to 0 over `soft` either side."""
    return smooth((x - lo) / soft + 1.0) * smooth((hi - x) / soft + 1.0)


def ridge(d, w):
    return np.exp(-(d / w) ** 2)


def dashes(x, period, duty=0.55, soft=0.12):
    """1 on each stitch of a row along x, 0 in the gaps between them."""
    f = (x / period) % 1.0
    return smooth(f / soft) * smooth((duty - f) / soft)


def stitch_row(d, along, period, w, amp=0.03):
    """A stitched seam at distance d from its line: a groove, the stitches standing in it."""
    return -amp * ridge(d, w * 1.6) + amp * 1.4 * ridge(d, w) * dashes(along, period)


def block_of(uv):
    """Each texel's block (index into BODY_BLOCKS) and its (bu, bv), as make_vrbody.py's block_uv gives them."""
    s = uv[:, 0] / BODY_SKIN
    t = uv[:, 1] / BODY_SKIN
    which = np.full(len(s), -1)
    bu = np.zeros(len(s))
    bv = np.zeros(len(s))
    for i, (u0, v0, u1, v1) in enumerate(BODY_BLOCKS.values()):
        m = (s >= u0) & (s < u1) & (t >= v0) & (t < v1)
        which[m] = i
        bu[m] = (s[m] - u0 - BODY_PAD) / (u1 - u0 - 2 * BODY_PAD)
        bv[m] = (t[m] - v0 - BODY_PAD) / (v1 - v0 - 2 * BODY_PAD)
    return which, bu, bv


def arm_h(bu, bv):
    """The bare arms (u 0 the back of the upper arm, 0.5 its front; v the shoulder 0 to the wrist 1, the elbow at 5/9):
    the deltoid's cap, biceps and triceps and the groove between them, the elbow, the forearm's top."""
    h = 0.40 * smooth((0.16 - bv) / 0.13)
    k = np.sin(np.pi * np.clip(bv / 0.5, 0, 1))
    h += 0.50 * k * np.clip(1 - around(bu, 0.5) * 4, 0, 1) ** 1.5
    h += 0.38 * k * np.clip(1 - around(bu, 0.0) * 4, 0, 1) ** 1.5
    h -= 0.12 * k * (ridge(around(bu, 0.25), 0.04) + ridge(around(bu, 0.75), 0.04))
    e = np.clip(1 - np.abs(bv - 0.556) / 0.07, 0, 1)
    h -= 0.35 * e * smooth((0.17 - around(bu, 0.5)) / 0.06)
    h += 0.22 * e * smooth((0.11 - around(bu, 0.0)) / 0.05)
    h += 0.28 * smooth((bv - 0.62) / 0.06) * np.clip(1 - around(bu, 0.25) * 4, 0, 1) ** 1.5 * smooth((0.97 - bv) / 0.1)
    return h


def head_h(bu, bv):
    """The face (u 0 its middle; v the neck 0 to the crown 1): the nose's ridge, the eye sockets, the brows, the mouth."""
    f = around(bu, 0.0)
    face = smooth((0.2 - f) / 0.04) * band(bv, 0.45, 0.84, 0.03)
    h = 0.12 * ridge(f, 0.016) * band(bv, 0.6, 0.72, 0.02)
    h -= 0.10 * ridge(np.hypot((f - 0.046) / 0.03, (bv - 0.75) / 0.03), 1.0)
    h += 0.07 * ridge(bv - 0.795, 0.012) * band(f, 0.01, 0.085, 0.01)
    h -= 0.035 * ridge(bv - 0.6, 0.006) * band(f, 0.0, 0.04, 0.01)
    return h * face


def belt_h(bu, bv):
    """The belt (the torso block's v 0.1 .. 0.2), its buckle in front, the loops, stitched along both edges."""
    front = around(bu, 0.0)
    belt = band(bv, 0.1, 0.2, 0.006)
    h = 0.10 * belt
    h += stitch_row(np.minimum(np.abs(bv - 0.117), np.abs(bv - 0.183)), bu, 1 / 90.0, 0.004, 0.025) * belt
    h += 0.05 * (band(bu, 0.24, 0.27, 0.004) + band(bu, 0.73, 0.76, 0.004)) * belt
    frame = band(front, 0.0, 0.03, 0.003) * band(bv, 0.125, 0.175, 0.004)
    inner = band(front, 0.0, 0.021, 0.003) * band(bv, 0.134, 0.166, 0.004)
    return h + 0.09 * (frame - inner) + 0.03 * inner


def collar_h(bu, bv):
    c = smooth((bv - 0.9) / 0.006)
    return c * (0.08 + 0.1 * np.sin(np.pi * np.clip((bv - 0.9) / 0.1, 0, 1)))


def vest_h(bu, bv):
    """The torso without armour (u 0 the front; v the trousers' top 0 to the collar 1): the belt, the quilted vest as
    the author painted it (24fa2de6: padded bands across the body between stitched rows, no channels down it), the
    laced front, the side seams and the back's yoke, the rolled collar."""
    front = around(bu, 0.0)
    row = (bv - 0.2) % 0.1
    quilt = 0.09 * np.sin(np.pi * row / 0.1) ** 0.7
    quilt -= 0.07 * ridge(np.minimum(row, 0.1 - row), 0.012) + 0.03 * ridge(row - 0.02, 0.008)
    lace_zone = smooth((0.022 - front) / 0.004)
    k = (bv * 45) % 1.0
    lace = np.maximum(ridge(front / 0.02 - k, 0.2), ridge(front / 0.02 - (1 - k), 0.2))
    v = quilt * (1 - lace_zone) + lace_zone * (0.06 * lace - 0.06)
    v -= 0.05 * (ridge(around(bu, 0.25), 0.006) + ridge(around(bu, 0.75), 0.006))
    v -= 0.04 * ridge(bv - 0.8, 0.006) * smooth((front - 0.3) / 0.02)
    return belt_h(bu, bv) + band(bv, 0.2, 0.9, 0.006) * v + collar_h(bu, bv)


def armor_h(bu, bv):
    """The armoured torso (skins 4-15): five overlapping lames, each rising to its rolled lower edge over the gap under
    it, the front closure, rivets near each lame's top, the side straps; the belt and collar as the vest's."""
    plate = band(bv, 0.2, 0.9, 0.004)
    front = around(bu, 0.0)
    lame = (bv - 0.2) / 0.7 * 5.0
    within = lame - np.floor(lame)
    ph = 0.16 + 0.12 * (1 - within) + 0.08 * ridge(within - 0.09, 0.035) - 0.2 * smooth((0.05 - within) / 0.015)
    ph -= 0.10 * ridge(front, 0.008)
    for c in (0.035, 0.2, 0.3, 0.465):
        for side in (c, 1.0 - c):
            du = (bu - side) * 128
            dv = (within - 0.72) * 128 * 0.7 / 5
            ph += 0.07 * np.clip(1 - (du * du + dv * dv) / 2.2, 0, 1) ** 0.5
    straps = smooth((0.02 - np.minimum(around(bu, 0.25), around(bu, 0.75))) / 0.004)
    buckle = straps * (np.abs(((bv - 0.2) * 5 / 0.7) % 1.0 - 0.5) < 0.18)
    ph = ph * (1 - straps) + straps * (0.08 + 0.05 * buckle)
    return belt_h(bu, bv) + collar_h(bu, bv) + plate * ph


def cloth_h(bu, bv):
    """The legs (u 0 the front; v the hip 0 to the ankle 1): the trousers' stitched side seams, folds behind the knee
    and across it, bunched at the ankle; the ridged thigh plates, a rivet on each ridge."""
    front = around(bu, 0.0)
    h = stitch_row(np.minimum(around(bu, 0.25), around(bu, 0.75)), bv, 1 / 60.0, 0.006, 0.03)
    f = np.sin((bv - 0.42) / 0.2 * np.pi * 3 + 3 * around(bu, 0.5))
    h += 0.10 * f * band(bv, 0.42, 0.62, 0.02) * smooth((0.2 - around(bu, 0.5)) / 0.05)
    h += 0.06 * np.sin((bv - 0.44) / 0.12 * np.pi * 2) * band(bv, 0.44, 0.56, 0.015) * smooth((0.18 - front) / 0.04)
    h += 0.08 * np.sin(bu * np.pi * 16) * smooth((bv - 0.85) / 0.03)
    plates = band(front, 0.0, 0.118, 0.004) * band(bv, 0.072, 0.408, 0.004)
    k = ((bv - 0.06) % 0.07) / 0.07
    ph = 0.12 + 0.12 * k - 0.14 * smooth((0.1 - k) / 0.06) - 0.03 * smooth((front - 0.08) / 0.03)
    ph += 0.06 * np.clip(1 - np.hypot((front - 0.1) * 256, (k - 0.5) * 0.07 * 128) / 1.4, 0, 1) ** 0.5
    rim = band(front, 0.0, 0.13, 0.004) * band(bv, 0.06, 0.42, 0.004)
    return h * (1 - rim) + rim * (0.1 * (1 - plates) + plates * ph)


def shaft_h(bu, bv):
    """The boots' shafts (v the ankle 0 to below the knee 1): the turned-down top, two buckled straps, the stitched back
    seam, creases over the ankle."""
    h = 0.14 * smooth((bv - 0.8) / 0.01) + 0.06 * ridge(bv - 0.855, 0.02)
    buck = smooth((0.035 - np.minimum(around(bu, 0.25), around(bu, 0.75))) / 0.006)
    for sb in (0.3, 0.6):
        s = band(bv, sb - 0.04, sb + 0.04, 0.006)
        h += s * (0.08 + 0.06 * buck * (1 - band(bv, sb - 0.015, sb + 0.015, 0.004)))
    h += stitch_row(around(bu, 0.5), bv, 1 / 50.0, 0.006, 0.025) * smooth((0.79 - bv) / 0.01)
    h += 0.07 * np.sin(bu * np.pi * 12 + bv * 30) * np.clip(1 - bv / 0.22, 0, 1)
    return h


def foot_h(bu, bv):
    """The feet (u 0 the top of the foot, 0.5 the sole; v the heel 0 to the toe 1): the welt round the sole, the
    crossed laces, the toe cap's edge."""
    top = around(bu, 0.0)
    h = 0.12 * smooth((0.36 - top) / 0.01) + 0.05 * ridge(top - 0.37, 0.012)
    k = (bv * 30) % 1.0
    lace = np.maximum(ridge(top / 0.06 - k, 0.22), ridge(top / 0.06 - (1 - k), 0.22))
    h += (0.05 * lace - 0.02) * smooth((0.06 - top) / 0.008) * band(bv, 0.3, 0.72, 0.01)
    h -= 0.04 * ridge(bv - 0.78, 0.01) * smooth((0.34 - top) / 0.02)
    return h


def bracer_h(bu, bv):
    """The bracers (u round the forearm, 0.5 its top; v the elbow's end 0 to the wrist's 1), as the author painted them:
    thick rolled rims at both ends, a stitched row inside each, two raised straps with stitched edges, each through a
    buckle (a raised frame, its hollow, the prong) on top, a rivet either side of it; the leather between."""
    h = 0.22 * (np.sqrt(np.clip(1 - ((bv - 0.045) / 0.06) ** 2, 0, 1)) + np.sqrt(np.clip(1 - ((bv - 0.955) / 0.06) ** 2, 0, 1)))
    h -= 0.06 * (ridge(bv - 0.1, 0.012) + ridge(bv - 0.9, 0.012))  # the rims' edges tucked in
    h += stitch_row(np.minimum(np.abs(bv - 0.12), np.abs(bv - 0.88)), bu, 1 / 48.0, 0.008, 0.05)
    top = around(bu, 0.5)
    for sb in (0.38, 0.66):
        s = band(bv, sb - 0.06, sb + 0.06, 0.005)
        edge = np.minimum(np.abs(bv - (sb - 0.045)), np.abs(bv - (sb + 0.045)))
        strap = 0.15 + 0.02 * np.cos(np.pi * np.clip((bv - sb) / 0.06, -1, 1))  # a little crowned
        strap += stitch_row(edge, bu, 1 / 60.0, 0.005, 0.03)
        frame = band(top, 0.0, 0.045, 0.004) * band(bv, sb - 0.075, sb + 0.075, 0.004)
        hollow = band(top, 0.0, 0.028, 0.003) * band(bv, sb - 0.05, sb + 0.05, 0.003)
        prong = ridge(top, 0.006) * band(bv, sb - 0.05, sb + 0.02, 0.004)
        buckle = 0.12 * (frame - hollow) + 0.05 * prong
        # the block is 64 texels round and 128 along: a rivet about 2.5 texels across
        rivets = sum(0.09 * np.sqrt(np.clip(1 - ((around(bu, 0.5 + side * 0.1) * 64) ** 2 + ((bv - sb) * 128) ** 2) / 1.6,
                                            0, 1)) for side in (-1, 1))
        h += s * (strap - 0.15 * frame) + buckle + s * rivets
    return h


HEIGHTS = {"skin": arm_h, "head": head_h, "leather": vest_h, "cloth": cloth_h, "boots": foot_h, "shaft": shaft_h,
           "bracer": bracer_h}


# How much of the author's painted shading becomes relief in each block (model units per unit of the band-passed
# brightness): his muscles, folds and leather are painted as light and shade, so their shapes follow the paint.
PAINT_FORMS = {"skin": 3.0, "head": 1.0, "leather": 1.6, "cloth": 2.0, "boots": 1.2, "shaft": 1.2, "bracer": 1.6}
PAINT_LINES = 0.06   # a painted dark line (a seam, a stitch row, a crease): a groove this deep
PAINT_DOTS = 0.05    # a painted round spot (a rivet, a stud): raised this much


def paint_heights(low, skin):
    """The author's painting of a body skin as heights (model units) on its texel grid: his painted shading's shapes
    (brightness between about 1.5 and 6 texels across, per block), his dark lines grooved and his small bright spots
    raised; spread past the islands' edges."""
    try:
        from . import normaltiles as nt
    except ImportError:
        import normaltiles as nt
    rgb = skin.astype(np.float64)
    cov = nb.coverage(low, 1)
    lum = (rgb[..., 0] * 0.3 + rgb[..., 1] * 0.59 + rgb[..., 2] * 0.11) / 255.0
    forms = nb.blur(lum, 1.5, cov) - nb.blur(lum, 6.0, cov)
    H, W = lum.shape
    ys, xs = np.mgrid[0:H, 0:W]
    which, _, _ = block_of(np.stack([(xs.ravel() + 0.5) * BODY_SKIN / W, (ys.ravel() + 0.5) * BODY_SKIN / H], 1))
    k = np.zeros(H * W)
    for i, n in enumerate(BODY_BLOCKS):
        k[which == i] = PAINT_FORMS[n]
    k = nb.blur(k.reshape(H, W), 1.0)  # no step where two blocks meet
    lines = nb.blur(nb.painted_lines(rgb, cov, lo=0.12, hi=0.4, min_len=6.0, full_len=16.0), 0.5, cov)
    dots = nb.blur(nt.painted_dots(rgb, cov), 0.6, cov)
    h = k * forms - PAINT_LINES * lines + PAINT_DOTS * dots
    return nb.dilate(np.dstack([h] * 3), cov, 8)[0][..., 0]


def field(armor, paint=None):
    """The body's heights at texel coordinates: the clothes' (skins 0-3) or the armoured torso's (4-15), with the
    author's painting's (`paint`: paint_heights) where given."""
    names = list(BODY_BLOCKS)
    painted = nb.sampler(paint, paint.shape[1] / BODY_SKIN) if paint is not None else None

    def f(uv, ctx=None):
        which, bu, bv = block_of(uv)
        h = np.zeros(len(uv))
        for i, n in enumerate(names):
            m = which == i
            if m.any():
                h[m] = (armor_h if armor and n == "leather" else HEIGHTS[n])(bu[m], bv[m])
        if painted is not None:
            h += painted(uv)
        return h

    return f


def bake(low, scale, armor=False, base=None, details=True, skin=None):
    """The body's map (h, w, 3), `scale` times its skin's size, and its heights for parallax mapping (alpha, or None);
    `base(low, raster)`: the shape's normals (a high poly's), else the mesh's own; `skin`: the author's painted skin
    (RGB) the relief follows."""
    r = nb.Raster(low, scale)
    n = base(low, r) if base else r.n
    alpha = None
    if details:
        f = field(armor, paint_heights(low, skin) if skin is not None else None)
        n = nb.bend(n, nb.gradient_uv(r, f, d=0.3))
        alpha = nb.finish_heights(r, f(r.uv), supersample=1)
    img, _ = nb.finish(r, n, supersample=1)
    return img, alpha
