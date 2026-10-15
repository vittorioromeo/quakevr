"""Paint the gloves' worn leather (vr_gloves) on the jointed hand's skin layout, twice the skin's resolution:
  progs/hand_rig_00_00_glove.png         RGB the leather's detail, multiplied with the glove's colour (vr_glove_hue,
                                         _saturation, _lightness): 128 grey leaves it as it is, darker darkens it,
                                         lighter lightens it, a coloured texel shifts its hue; A its wear (0 none, 255
                                         worn through to a pale, greyed, matte scuff);
  progs/hand_rig_00_00_glove_height.png  its relief, 128 flat (lighter raised), for the shader's bumps.
Every hand skin (hand_rig_0N_00) uses these unless it has files of its own of the same names (vr_render.cpp).
What is painted: a pebbled grain with a coarser network of creases through it and a mottled tone; the knuckles
scuffed paler, and the backs of the finger joints less so, with the leather bunched into wrinkles over them and folded
into deep creases on the palm's side; the palm's own folds; the seams along the fingers' sides and round the thumb
(make_skin_masks.py's lines: its groove, a row of thread stitches either side) and three stitched "points" down the
back of the hand; the fingertips worn; some scratches and scuffs. Deterministic (fixed seeds).
The shader (vr_glsl.h GloveLeather) keeps the glove's hem and fine grain of its own on top.

Edit the PNGs by hand in any image editor (docs/vr-port/GLOVES.md); this generator then refuses to overwrite them
(genguard: --keep-edited, --force). Run with numpy and Pillow (py 3.13):
  py -3.13 Misc/quakevr/make_glove_paint.py [--preview out.png]
--preview also writes a flat brown preview of the colour and wear (for a quick look, not shipped)."""
from pathlib import Path
import sys
import numpy as np
from PIL import Image
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import genguard
import make_hand_rig as hand
from make_skin_masks import seam_lines, spread, STITCH_OFF, STITCH_STEP, SEAM

PROGS = HERE.parent.parent / 'quakevr/progs'
SCALE = 2  # texels to the skin's (512: 1.5 a mm) -> 3 a mm
OUT = PROGS / 'hand_rig_00_00_glove.png'
OUT_H = PROGS / 'hand_rig_00_00_glove_height.png'


# ---- noise (hashed lattices, deterministic) -------------------------------------------------------------------------

def hash3(ix, iy, iz, seed):
    h = (ix.astype(np.uint32) * np.uint32(73856093)) ^ (iy.astype(np.uint32) * np.uint32(19349663)) \
        ^ (iz.astype(np.uint32) * np.uint32(83492791)) ^ np.uint32(seed * 2654435761 & 0xffffffff)
    h ^= h >> np.uint32(13)
    h *= np.uint32(1274126177)
    h ^= h >> np.uint32(16)
    return h


def unit_hash(ix, iy, iz, seed):
    return (hash3(ix, iy, iz, seed) & np.uint32(0xffffff)).astype(np.float64) / float(0xffffff)


def value_noise(p, scale, seed):
    """Smooth value noise (-1..1) at points p (N, 3), features `scale` apart."""
    q = p / scale + 1000.0
    i = np.floor(q).astype(np.int64)
    f = q - i
    f = f * f * (3 - 2 * f)
    out = np.zeros(len(p))
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                w = (f[:, 0] if dx else 1 - f[:, 0]) * (f[:, 1] if dy else 1 - f[:, 1]) * (f[:, 2] if dz else 1 - f[:, 2])
                out += w * unit_hash(i[:, 0] + dx, i[:, 1] + dy, i[:, 2] + dz, seed)
    return out * 2 - 1


def fbm(p, scale, seed, octaves=3):
    out, amp, tot = 0.0, 1.0, 0.0
    for o in range(octaves):
        out = out + amp * value_noise(p, scale / 2 ** o, seed + 17 * o)
        tot += amp
        amp *= 0.5
    return out / tot


def worley(p, cell, seed):
    """Cellular noise at p (N, 3): F2 - F1 (0 on a crevice between cells, in cell units) and the nearest cell's own
    random 0..1."""
    q = p / cell + 1000.0
    i = np.floor(q).astype(np.int64)
    d1 = np.full(len(p), 9.0)
    d2 = np.full(len(p), 9.0)
    tone = np.zeros(len(p))
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dz in (-1, 0, 1):
                c = i + np.array([dx, dy, dz])
                r = np.stack([unit_hash(c[:, 0], c[:, 1], c[:, 2], seed + k) for k in range(3)], 1)
                d = np.linalg.norm(c + 0.1 + 0.8 * r - q, axis=1)
                t = unit_hash(c[:, 0], c[:, 1], c[:, 2], seed + 5)
                closer = d < d1
                d2 = np.where(closer, d1, np.minimum(d2, d))
                tone = np.where(closer, t, tone)
                d1 = np.where(closer, d, d1)
    return d2 - d1, tone


def band(d, width):
    """1 on the line d = 0, falling smoothly to 0 at |d| = width."""
    t = np.clip(1 - np.abs(d) / width, 0, 1)
    return t * t * (3 - 2 * t)


def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


# ---- the hand -------------------------------------------------------------------------------------------------------

def texels():
    models, shifts, rig, mesh, islands = hand.build(str(PROGS))
    layout, size = hand.layout_islands(islands)
    layout = {k: (x * SCALE, y * SCALE, d * SCALE) for k, (x, y, d) in layout.items()}
    size = (size[0] * SCALE, size[1] * SCALE)
    pos, norm, part = hand.rasterize(mesh, layout, size)
    return rig, pos, norm, part, layout


class Paint:
    """The layers, per covered texel: the colour's shade (1 as it is), its tint (rgb times), the wear (0..1), the
    height (0 flat, hand units / 12: ~mm)."""
    def __init__(self, n):
        self.shade = np.ones(n)
        self.tint = np.ones((n, 3))
        self.wear = np.zeros(n)
        self.height = np.zeros(n)


def stitches(pt, d, run, w, offset=STITCH_OFF, step=STITCH_STEP, slant=0.25):
    """Rows of thread stitches either side of a seam's line (d across it, run along it, hand units, weight w): their
    thread (0..1) and the needle holes between them (0..1)."""
    thread = np.zeros_like(d)
    holes = np.zeros_like(d)
    for sgn in (1.0, -1.0):
        x = d - sgn * offset
        r = run + sgn * slant * x  # slanted a little, as a lock stitch's thread lies
        ph = np.mod(r / step, 1.0)
        on = band(x, 0.055) * smooth(0.0, 0.08, ph) * smooth(0.0, 0.08, 0.62 - ph)
        core = band(x, 0.03) * band(ph - 0.31, 0.3)
        thread = np.maximum(thread, np.maximum(on * 0.75, core))
        hole = band(x, 0.035) * (band(ph - 0.66, 0.07) + band(ph - 0.97, 0.07))
        holes = np.maximum(holes, np.clip(hole, 0, 1))
    return thread * w, holes * w


def sew(pt, d, run, w, groove=True):
    """A seam drawn into the paint: its groove (folded leather, dark) and its stitches (thread, raised, paler)."""
    if groove:
        g = band(d, SEAM * 1.3) * w
        pt.shade *= 1 - 0.45 * g
        pt.height -= 0.35 * g
        lip = (band(np.abs(d) - SEAM * 1.6, SEAM * 0.9)) * w  # the leather rolled up either side, rubbed
        pt.height += 0.12 * lip
        pt.wear = np.maximum(pt.wear, 0.22 * lip)
    th, holes = stitches(pt, d, run, w)
    pt.shade *= 1 - 0.35 * holes
    pt.height -= 0.12 * holes
    pt.shade = pt.shade * (1 - th) + 1.45 * th
    pt.tint = pt.tint * (1 - th[:, None]) + np.array([1.03, 1.0, 0.94]) * th[:, None]
    pt.wear = np.maximum(pt.wear, 0.55 * th)
    pt.height += 0.22 * th


def scuffed(p, patch, base, extra):
    """A rubbed area's wear: `base` all over, `extra` more in patches, abraded (speckled at the patches' edges, a
    few strokes of the scuffing through it)."""
    speck = 0.5 + 0.5 * value_noise(p, 0.05, 61)
    strokes = 0.5 + 0.5 * value_noise(p * np.array([3.0, 1.0, 0.6]), 0.12, 67)
    w = base * (0.75 + 0.25 * speck) + extra * smooth(0.35, 0.65, patch + 0.25 * (speck - 0.5) + 0.2 * (strokes - 0.5))
    return np.clip(w, 0, 1)


def finger_joints(rig, name, p, n, pt, seed):
    """The finger's joints: the backs scuffed paler (most at the knuckle), the leather wrinkled across them, folded
    into creases on the palm's side; the tip worn."""
    pts = rig.fingers[name]['points']
    segs = rig.fingers[name]['segments']
    seg, along, side, lateral = hand.finger_coords(rig, name, p, n)
    lengths = [np.linalg.norm(pts[k + 1] - pts[k]) for k in range(3)]
    back = smooth(0.05, 0.7, side)
    front = smooth(0.05, 0.6, -side)
    wob = value_noise(p, 0.35, seed)
    patch = 0.5 + 0.5 * fbm(p, 0.4, seed + 3)
    scale = (0.75, 1.0, 0.8) if name != 'thumb' else (0.7, 0.85, 0.75)
    for k in (1, 2):  # PIP, DIP (the knuckle, k 0, is the palm's)
        x = 0.5 * (segs[k - 1][1] + segs[k][1])
        x /= np.linalg.norm(x)
        a = (p - pts[k]) @ x
        near = np.exp(-(a / (0.55 if k == 1 else 0.45)) ** 2)
        s = scale[k]
        # scuffed over the joint's back
        pt.wear = np.maximum(pt.wear, s * back * near * scuffed(p, patch + 0.15 * wob, 0.3, 0.45))
        # wrinkles across the back, bunched over the joint
        dd = a + 0.06 * wob
        wr = sum(band(dd - c, 0.035) * h for c, h in ((-0.42, 0.5), (-0.24, 0.8), (-0.08, 1.0), (0.08, 0.9), (0.24, 0.6)))
        wr *= back * smooth(1.0, 0.35, np.abs(lateral)) * s
        pt.shade *= 1 - 0.32 * wr
        pt.height -= 0.22 * wr
        ridges = sum(band(dd - c, 0.06) for c in (-0.33, -0.16, 0.0, 0.16)) * back * near * s
        pt.wear = np.maximum(pt.wear, 0.3 * ridges * patch)
        pt.height += 0.08 * ridges
        # the palm side's fold: deep, dark, a second fainter one beside it
        fold = (band(a + 0.02 + 0.03 * wob, 0.05) + 0.5 * band(a - 0.16 + 0.03 * wob, 0.035)) * front
        pt.shade *= 1 - 0.35 * fold
        pt.height -= 0.35 * fold
    # the palm side's fold at the finger's root (the palm's crease under it)
    root = band(along - 0.12 + 0.04 * wob, 0.05) * (seg == 0) * front
    pt.shade *= 1 - 0.35 * root
    pt.height -= 0.25 * root
    # the tip worn all round, more at its very end
    tipness = np.where(seg == 2, smooth(0.45 * lengths[2], 1.0 * lengths[2], along), 0.0)
    pt.wear = np.maximum(pt.wear, tipness * (0.25 + 0.4 * smooth(0.3, 0.8, patch)))
    pt.shade *= 1 - 0.08 * tipness * front


def palm_marks(rig, p, n, pt, seed):
    """The palm's island: the knuckles scuffed, the back's stitched points, the palm's folds, the grip's wear."""
    side = -n[:, 1]
    x = p[:, 0]
    st = hand.PALM_STATIONS
    xs = [s["x"] for s in st[:5]] + [4.4, 5.3]
    zu = np.interp(x, xs, [s["zu"] for s in st])
    zr = np.interp(x, xs, [s["zr"] for s in st])
    width = zr - zu
    u = np.clip((p[:, 2] - zu) / width, 0, 1)
    back = smooth(0.1, 0.6, side)
    front = smooth(0.1, 0.6, -side)
    knuckles = np.array([hand.arc_x(t) for t in u])
    wob = value_noise(p, 0.4, seed)
    patch = 0.5 + 0.5 * fbm(p, 0.45, seed + 5)
    # the knuckles (MCP): the most worn, scuffed through in patches, grime round them
    for name in ("index", "middle", "ring", "pinky"):
        j = rig.fingers[name]['points'][0]
        d = np.linalg.norm((p - j) * np.array([1.0, 0.6, 1.0]), axis=1)
        k = np.exp(-(d / 0.8) ** 2) * back
        pt.wear = np.maximum(pt.wear, k * scuffed(p, patch + 0.2 * wob, 0.5, 0.5))
        pt.shade *= 1 - 0.12 * np.exp(-((d - 0.95) / 0.3) ** 2) * back
    # the leather creased across the back behind the knuckles, as a fist pulls it
    dk = x - knuckles + 0.45 + 0.08 * wob
    cr = sum(band(dk + c, 0.04) * h for c, h in ((0.0, 1.0), (0.22, 0.7), (0.45, 0.45))) * back
    pt.shade *= 1 - 0.25 * cr
    pt.height -= 0.18 * cr
    # three stitched points down the back, between the tendons, from the wrist to short of the knuckles
    for uc in (0.27, 0.5, 0.74):
        d = (u - uc) * width
        span = smooth(-3.6, -3.1, x) * smooth(knuckles - 1.0, knuckles - 1.6, x) * back
        ridge = band(d, 0.09) * span
        pt.height += 0.2 * ridge
        pt.shade *= 1 + 0.05 * ridge
        pt.wear = np.maximum(pt.wear, 0.15 * ridge)
        th, holes = stitches(pt, d, x, span, offset=0.075, step=0.2, slant=0.0)
        pt.shade *= 1 - 0.3 * holes
        pt.shade = pt.shade * (1 - th) + 1.4 * th
        pt.wear = np.maximum(pt.wear, 0.5 * th)
        pt.height += 0.15 * th
    # the palm's folds (as the skin's: the heart line under the fingers, the head line across, the life line round
    # the thumb's ball), the wrist's
    heart = knuckles - 1.7 - 0.5 * np.clip(u - 0.1, 0, 1)
    head = 1.5 - 1.8 * u
    life_u = 0.62 + 0.08 * np.clip(-x - 0.5, 0, 3) - 0.03 * np.clip(x - 0.5, 0, 2)
    folds = (band(x - heart + 0.05 * wob, 0.1) * (u < 0.8) + 0.9 * band(x - head + 0.05 * wob, 0.09) * (u > 0.28) * (u < 0.98)
             + 0.9 * band(u - life_u + 0.01 * wob, 0.025) * (x < 2.0) * (x > -3.3)) * front
    folds = np.clip(folds, 0, 1)
    pt.shade *= 1 - 0.3 * folds
    pt.height -= 0.3 * folds
    # the grip: the pads under the fingers and the heel rubbed (a little paler, grime in between)
    pads = (np.exp(-((x - knuckles + 0.9) / 0.6) ** 2) + np.exp(-((x + 2.6) / 1.0) ** 2) * 0.8) * front
    pt.wear = np.maximum(pt.wear, 0.3 * pads * smooth(0.3, 0.8, patch))
    pt.shade *= 1 - 0.1 * front * np.exp(-((u - 0.45) / 0.25) ** 2 - ((x - 0.8) / 1.5) ** 2)


def grain(p, pt):
    """The leather's own surface: pebbles (~1.7 mm) between crevices, a coarser network of creases through them, a
    mottled tone, grime in the crevices."""
    e, tone = worley(p, 0.14, 11)
    peb = smooth(0.0, 0.4, e)  # 0 in a crevice, 1 on a pebble's rounded top
    pt.shade *= 0.85 + 0.15 * peb
    pt.shade *= 0.97 + 0.06 * tone
    pt.height += 0.12 * peb
    e2, _ = worley(p * np.array([1.0, 1.0, 1.6]), 0.55, 23)
    net = 1 - smooth(0.0, 0.18, e2 + 0.04 * value_noise(p, 0.12, 27))
    net *= smooth(-0.1, 0.5, value_noise(p, 0.8, 29))  # in places only
    pt.shade *= 1 - 0.12 * net
    pt.height -= 0.08 * net
    mot = fbm(p, 1.2, 31, 3)
    pt.shade *= 1 + 0.09 * mot
    pt.tint *= (1 + np.outer(fbm(p, 2.0, 37, 2), [0.03, 0.0, -0.04]))
    pt.wear = np.maximum(pt.wear, 0.12 * smooth(0.2, 0.7, fbm(p, 0.6, 41, 3)))  # faint rubbed patches all over


def scratches(img_wear, img_shade, img_h, covered, rng, back_map):
    """Thin scratches in the skin's own texels: pale (the leather's top layer scored off), a little sunk."""
    H, W = covered.shape
    ys, xs = np.nonzero(covered & back_map)
    count = 0
    while count < 70:
        i = rng.integers(len(ys))
        y0, x0 = float(ys[i]), float(xs[i])
        ang = rng.uniform(0, np.pi)
        length = rng.uniform(10, 40) * SCALE / 2
        bend = rng.uniform(-0.6, 0.6)
        strength = rng.uniform(0.4, 1.0)
        pts = []
        for t in np.linspace(0, 1, 6):
            a = ang + bend * (t - 0.5)
            pts.append((x0 + np.cos(a) * length * (t - 0.5), y0 + np.sin(a) * length * (t - 0.5)))
        x_lo = int(max(0, min(q[0] for q in pts) - 3))
        x_hi = int(min(W, max(q[0] for q in pts) + 4))
        y_lo = int(max(0, min(q[1] for q in pts) - 3))
        y_hi = int(min(H, max(q[1] for q in pts) + 4))
        gy, gx = np.mgrid[y_lo:y_hi, x_lo:x_hi] + 0.5
        d = np.full(gx.shape, 99.0)
        along = np.zeros(gx.shape)
        for k in range(len(pts) - 1):
            ax, ay = pts[k]
            bx, by = pts[k + 1]
            vx, vy = bx - ax, by - ay
            ll = vx * vx + vy * vy + 1e-9
            t = np.clip(((gx - ax) * vx + (gy - ay) * vy) / ll, 0, 1)
            dk = np.hypot(gx - ax - t * vx, gy - ay - t * vy)
            closer = dk < d
            along = np.where(closer, (k + t) / (len(pts) - 1), along)
            d = np.minimum(d, dk)
        taper = np.sin(np.pi * along) ** 0.5
        s = np.clip(1 - d / (0.9 * SCALE / 2 + 0.35), 0, 1) * taper * strength
        s *= covered[y_lo:y_hi, x_lo:x_hi]
        img_wear[y_lo:y_hi, x_lo:x_hi] = np.maximum(img_wear[y_lo:y_hi, x_lo:x_hi], 0.6 * s)
        img_shade[y_lo:y_hi, x_lo:x_hi] *= 1 + 0.25 * s
        img_h[y_lo:y_hi, x_lo:x_hi] -= 0.12 * s
        count += 1


def main():
    preview = None
    if '--preview' in sys.argv:
        preview = sys.argv[sys.argv.index('--preview') + 1]
    guard = genguard.Guard("make_glove_paint.py", [str(OUT), str(OUT_H)])
    rig, pos, norm, part, layout = texels()
    H, W = part.shape
    covered = part >= 0
    p = pos[covered]
    n = norm[covered]
    prt = part[covered]
    pt = Paint(len(p))
    grain(p, pt)
    palm = prt == 0
    sub = Paint(palm.sum())
    sub.shade, sub.tint, sub.wear, sub.height = pt.shade[palm], pt.tint[palm], pt.wear[palm], pt.height[palm]
    palm_marks(rig, p[palm], n[palm], sub, 101)
    pt.shade[palm], pt.tint[palm], pt.wear[palm], pt.height[palm] = sub.shade, sub.tint, sub.wear, sub.height
    for i, name in enumerate(hand.FINGERS):
        m = prt == i + 1
        sub = Paint(m.sum())
        sub.shade, sub.tint, sub.wear, sub.height = pt.shade[m], pt.tint[m], pt.wear[m], pt.height[m]
        # the knuckle's scuff carries on onto the finger's root
        j = rig.fingers[name]['points'][0]
        side = hand.finger_coords(rig, name, p[m], n[m])[2]
        if name != 'thumb':
            d = np.linalg.norm(p[m] - j, axis=1)
            patch = 0.5 + 0.5 * fbm(p[m], 0.45, 106)
            sub.wear = np.maximum(sub.wear, np.exp(-(d / 0.8) ** 2) * smooth(0.05, 0.7, side) * scuffed(p[m], patch, 0.5, 0.5))
        finger_joints(rig, name, p[m], n[m], sub, 200 + i)
        lines, has_base = seam_lines(rig, name, p[m], n[m])
        for k, (d, run, w) in enumerate(lines):
            sew(sub, d, run, w)
        pt.shade[m], pt.tint[m], pt.wear[m], pt.height[m] = sub.shade, sub.tint, sub.wear, sub.height
    # back to the image, the scratches in it, the pads round the islands
    shade = np.ones((H, W))
    tint = np.ones((H, W, 3))
    wear = np.zeros((H, W))
    height = np.zeros((H, W))
    shade[covered], tint[covered], wear[covered], height[covered] = pt.shade, pt.tint, pt.wear, pt.height
    back_map = np.zeros((H, W), bool)
    back_map[covered] = (-norm[covered][:, 1] > -0.2) | (part[covered] > 0)
    scratches(wear, shade, height, covered, np.random.default_rng(7), back_map)
    rgb = np.clip(0.5 * shade[..., None] * tint, 0, 1)
    rgb = np.stack([spread(rgb[..., c], covered, 6) for c in range(3)], -1)
    rgb[~covered & ~(spread(covered * 1.0, covered, 6) > 0)] = 0.5
    wear = np.clip(spread(wear, covered, 6), 0, 1)
    h = np.clip(0.5 + 0.6 * spread(height, covered, 6), 0, 1)
    pad = spread(covered * 1.0, covered, 6) > 0
    wear[~pad] = 0
    h[~pad] = 0.5
    rgba = np.concatenate([rgb, wear[..., None]], -1)
    Image.fromarray(np.round(rgba * 255).astype(np.uint8), 'RGBA').save(OUT, optimize=True)
    Image.fromarray(np.round(h * 255).astype(np.uint8), 'L').save(OUT_H, optimize=True)
    guard.finish()
    print(f'glove paint {W}x{H}: {covered.sum()} texels; wear mean {wear[covered].mean():.3f}, '
          f'> 0.4 {(wear[covered] > 0.4).mean() * 100:.1f}%; height {h[covered].min():.2f}..{h[covered].max():.2f}')
    if preview:
        brown = np.array([0.29, 0.17, 0.11])
        lum = brown @ np.array([0.299, 0.587, 0.114])
        worn = (lum + 0.35 * (brown - lum)) * 1.5 + 0.06
        col = brown * rgb * 2
        col = col * (1 - wear[..., None]) + worn * wear[..., None]
        gx, gy = np.gradient(h)
        lit = np.clip(1.0 + 4.0 * (gx - gy), 0.5, 1.5)
        Image.fromarray(np.clip(np.round(col * lit[..., None] * 2.2 * 255), 0, 255).astype(np.uint8), 'RGB').save(preview)


if __name__ == '__main__':
    main()
