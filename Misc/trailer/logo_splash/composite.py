# composite.py -- the logo splash's frames: Blender's layers (blender_scene.py) and the 2D effects (fx.py, with
# fx_precompute.py's fields) composited in premultiplied float RGBA, shaken on the impacts, and written as straight
# alpha RGBA PNGs (dithered to 8 bits; transparent pixels carry their neighbours' colour, so no dark fringes).
#
#   python composite.py --width 3840 [--work <dir>] [--out <dir>] [--frames a-b] [--step n] [--jobs 8]
#                       [--shake 1.0] [--sheet] [--layers]

import argparse
import json
import math
import os
import sys
import time
from multiprocessing import Pool

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C  # noqa: E402
import fx  # noqa: E402

G = {}


def init(work, W, shake, variant="full"):
    H = W * 9 // 16
    S = W / 3840.0
    pre = dict(np.load(os.path.join(work, "pre", "fields.npz")))
    with open(os.path.join(work, "pre", "meta.json")) as f:
        meta = json.load(f)
    st = os.path.join(work, "stills")
    rests = []
    for i in range(7):
        a = fx.load_rgba(os.path.join(st, "rest_%d.png" % i), W)[..., 3]
        ys, xs = np.nonzero(a > 0.02)
        rests.append((a, (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)))
    G.update(variant=variant, W=W, H=H, S=S, work=work, pre=pre, drips=meta["drips"], tip=meta["tip"], shake=shake,
             rests=rests,
             letters_rest=fx.load_rgba(os.path.join(st, "letters_rest.png"), W),
             letters_fire=fx.load_rgba(os.path.join(st, "letters_rest_fire.png"), W),
             unl=fx.load_rgba(os.path.join(st, "unleashed.png"), W),
             unl_fire=fx.load_rgba(os.path.join(st, "unleashed_fire.png"), W),
             fire=fx.Fire(**dict(np.load(os.path.join(work, "pre", "fire.npz")))),
             fuel=np.load(os.path.join(work, "pre", "fuel.npy")),
             ramp=fx.fire_ramp(),
             burst=fx.burst_particles(),
             sprites=fx.puff_sprites())
    G["imp_blood"], G["dust"], G["chips"], G["crown"] = fx.impact_particles([r[0] for r in rests], W)
    G["embers"] = fx.embers(G["fuel"], W, H)
    G["ripple"] = fx.noise(H, W, 22 * S, 55, 3) + 0.5 * fx.noise(H, W, 7 * S, 56, 2)
    # When each pixel of "Unleashed" materialises: left to right, rising from the bottom of each letter.
    ua = G["unl"][..., 3]
    ys, xs = np.nonzero(ua > 0.02)
    x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max()
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    span = C.MAT_END - C.MAT_START
    jit = fx.noise(H, W, 14 * S, 77, 3)
    G["mat_t"] = (C.MAT_START + ((xx - x0) / max(1, x1 - x0)) * span * 0.55
                  + ((y1 - yy) / max(1, y1 - y0)) * span * 0.3 + jit * span * 0.15).astype(np.float32)
    G["unl_box"] = (x0, y0, x1, y1)


LUMA = np.array([0.2126, 0.7152, 0.0722], np.float32)
WARM = np.array([1.0, 0.86, 0.74], np.float32)


def firelit(rest, lit, burn, f):
    """The letters lit by the fire, keeping their own colour (steel, the red): the fire-lit render's extra light,
    capped, as a nearly neutral lift with a hint of warmth that flickers (not the golden fire-lit render itself)."""
    flick = 0.8 + 0.2 * math.sin(f * 0.9) * math.sin(f * 0.37 + 1)
    lift = np.clip((lit[..., :3] - rest[..., :3]) @ LUMA, 0, 0.22) * (burn * flick)
    out = rest.copy()
    out[..., :3] += lift[..., None] * WARM
    out[..., :3] = np.minimum(out[..., :3], out[..., 3:4])
    return out


def ramp01(x):
    return np.clip(x, 0.0, 1.0)


def blood_T(f):
    pre, S = G["pre"], G["S"]
    if f < C.HIT:
        return None
    T = pre["T_main"] * ramp01((f - pre["A_main"] + 1) / 2)
    T += pre["T_imp"] * ramp01((f - pre["A_imp"] + 1) / 2)
    fx.draw_drips(T, G["drips"], f, S)
    # Each landing sends a ripple through the wet blood round the letter (it catches the light).
    for i, (a, box) in enumerate(G["rests"]):
        k = f - C.LAND[i]
        if 0 <= k < 24:
            x0, y0, x1, y1 = box
            cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
            R = 40 * S + k * 16 * S
            pad = int(R + 30 * S)
            bx0, by0 = max(0, int(x0 - pad)), max(0, int(y0 - pad))
            bx1, by1 = min(G["W"], int(x1 + pad)), min(G["H"], int(y1 + pad))
            yy, xx = np.mgrid[by0:by1, bx0:bx1].astype(np.float32)
            # A rounded ring round the letter.
            hw, hh = (x1 - x0) / 2, (y1 - y0) / 2
            q = np.sqrt(((xx - cx) / hw) ** 2 + ((yy - cy) / hh) ** 2)
            d = np.maximum(q - 1, 0) * min(hw, hh)
            amp = 0.35 * math.exp(-k / 8.0)
            ring = amp * np.exp(-((d - R) / (14 * S)) ** 2) - 0.5 * amp * np.exp(-((d - R + 26 * S) / (12 * S)) ** 2)
            sub = T[by0:by1, bx0:bx1]
            sub += ring * (sub > 0.2)
    if f >= C.FINGER_START:
        T *= 1 - ramp01((f - pre["C"] + 1) / 2.5)
        T += pre["rim"] * ramp01((f - pre["Crim"]) / 4)
        # The finger's tip: blood heaped up just ahead of it.
        if f <= C.FINGER_END + 1:
            cur = [p for p in G["tip"] if p[2] <= f]
            if cur:
                x, y, _ = cur[-1]
                fx._cap_add(T, (x, y), (x, y), 16 * S, 16 * S, 1.1)
    return T


def shadows(f, blood_a):
    """The falling letters' shadows on the wall (light from the top left): far and soft high up, a tight contact
    shadow once landed."""
    W, H, S = G["W"], G["H"], G["S"]
    ppm = C.px_per_m(W)
    acc = np.zeros((H, W), np.float32)
    for i, (a, box) in enumerate(G["rests"]):
        h = C.letter_height(i, f)
        if h is None:
            continue
        he = max(0.0, h) + C.LETTER_DEPTH
        dx, dy = he * 0.2 * ppm, he * 0.3 * ppm
        sig = 2.5 * S + he * 0.035 * ppm
        k = C.CAM_DIST / (C.CAM_DIST - max(h, 0))     # the shadow is the size of the letter on the wall
        strength = 0.9 * math.exp(-max(h, 0) / 5.0)
        if strength < 0.02:
            continue
        pad = int(3 * sig + 4)
        x0, y0, x1, y1 = box
        x0, y0 = max(0, int(x0 + dx) - pad), max(0, int(y0 + dy) - pad)
        x1, y1 = min(W, int(x1 + dx) + pad), min(H, int(y1 + dy) + pad)
        if x0 >= x1 or y0 >= y1:
            continue
        src = a[max(0, y0 - int(dy)):max(0, y1 - int(dy)), max(0, x0 - int(dx)):max(0, x1 - int(dx))]
        reg = np.zeros((y1 - y0, x1 - x0), np.float32)
        reg[:src.shape[0], :src.shape[1]] = src
        acc[y0:y1, x0:x1] = np.maximum(acc[y0:y1, x0:x1], fx.blur(reg, sig) * strength)
        del k
    a = acc * (0.55 + 0.45 * blood_a)
    out = np.zeros((H, W, 4), np.float32)
    out[..., 3] = a
    return out


def fire_heat(f):
    return G["fire"].heat(f)


def frame_rgba(f):
    W, H, S = G["W"], G["H"], G["S"]
    out = np.zeros((H, W, 4), np.float32)
    heat = fire_heat(f)
    fire_up = fx.resize_f(heat, W, H, Image.BICUBIC) if heat is not None else None
    burn = fx.smoothstep(C.FIRE_START, C.FIRE_START + 50, f) if heat is not None else 0.0

    # The blood on the wall, its red aura, and the letters' shadows on it.
    T = blood_T(f)
    blood_a = np.zeros((H, W), np.float32)
    if T is not None:
        heat_edge = np.clip(fire_up * 1.5, 0, 1) if fire_up is not None else None
        b = fx.shade_blood(T, S, char=0.45 * burn, heat=heat_edge, ripple=G["ripple"])
        blood_a = b[..., 3]
        aura = fx.blur(blood_a, 28 * S) * 0.3
        halo = np.zeros_like(out)
        halo[..., 0] = aura * 0.55
        halo[..., 3] = aura
        fx.over(out, halo)
        fx.over(out, b)
        fx.over(out, shadows(f, blood_a))

    # The fire behind the letters.
    if heat is not None:
        fire = fx.fire_rgba(heat, W, H, G["ramp"], up=fire_up)
        fx.over(out, fire)

    # "Unleashed" materialising in its wiped letters: a hot front rising through each, then the red, then a glint.
    if f >= C.MAT_START:
        u = G["unl"] if burn <= 0 else firelit(G["unl"], G["unl_fire"], burn, f)
        m = ramp01((f - G["mat_t"]) / 3.0)
        hot = np.exp(-np.maximum(f - G["mat_t"], 0) / 4.0) * (f >= G["mat_t"])
        lay = u * m[..., None]
        ua = lay[..., 3]
        lay[..., 0] += ua * hot * 1.0
        lay[..., 1] += ua * hot * 0.55
        lay[..., 2] += ua * hot * 0.2
        if C.MAT_END - 4 <= f <= C.MAT_END + 40:
            x0, y0, x1, y1 = G["unl_box"]
            yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
            pos = x0 - 300 * S + (x1 - x0 + 600 * S) * (f - C.MAT_END + 4) / 44.0
            band = np.exp(-(((xx + (yy - y0) * 0.6) - pos) / (40 * S)) ** 2)
            lay[..., :3] += ua[..., None] * band[..., None] * 0.55
        np.clip(lay, 0, None, out=lay)
        lay[..., :3] = np.minimum(lay[..., :3], lay[..., 3:4])
        fx.over(out, lay)

    # Blender's foreground: the grunt, the axe, the gibs.
    if f <= C.GIB_END:
        p = os.path.join(G["work"], "fg_nogrunt" if G["variant"] == "nogrunt" else "fg", "fg_%04d.png" % f)
        if os.path.exists(p):
            fx.over(out, fx.load_rgba(p, W))

    # The letters (falling: Blender's frames; landed: the still, lit by the fire at the end).
    if f >= C.LAND[0] - C.FALL:
        if f <= C.LETTERS_END:
            p = os.path.join(G["work"], "letters", "letters_%04d.png" % f)
            lt = fx.load_rgba(p, W) if os.path.exists(p) else G["letters_rest"]
        elif burn > 0:
            lt = firelit(G["letters_rest"], G["letters_fire"], burn, f)
        else:
            lt = G["letters_rest"]
        fx.over(out, lt)

    # Flames licking in front of the letters (thinner).
    if heat is not None:
        front = fx.fire_rgba(heat, W, H, G["ramp"], gain=0.8, up=fire_up)
        front *= 0.2
        fx.over(out, front)

    # The burst: a flash and a red mist.
    if C.HIT <= f < C.HIT + 30:
        cx, cy, d = C.project(C.CHEST, W)
        t = (f - C.HIT) / C.FPS
        if f < C.HIT + 3:
            k = 1 - (f - C.HIT) / 3
            fx.glow_rgba(out, (cx, cy), 420 * S, (0.9, 0.08, 0.04), 0.7 * k)
            fx.glow_rgba(out, (cx, cy), 150 * S, (1.0, 0.93, 0.85), 0.95 * k)
        rng = np.random.default_rng(13)
        for k in range(16):
            ox, oz = rng.normal(0, 0.3), rng.normal(0, 0.2)
            vx, vz = ox * 3.0, oz * 3.0 + 0.4
            p = (C.CHEST[0] + ox + vx * t, C.CHEST[1] - 0.2 - t * 2.0, C.CHEST[2] + oz + vz * t)
            dust = np.array([[p[0], p[1], p[2], 0, 0, 0, 0.3 + 0.12 * (k % 3), C.HIT, 0.45, k]], np.float64)
            fx.draw_dust(out, dust, G["sprites"], f, W, base=(0.42, 0.02, 0.02))

    # Thrown blood, dust and chips of the landings, the burst's droplets.
    if C.HIT <= f < C.HIT + 90:
        bp = G["burst"]
        fx.draw_droplets(out, bp["p"], bp["v"], bp["size"], bp["t0"], bp["life"], f, W)
    if f >= C.LAND[0] and f < C.LAND[-1] + 110:
        fx.draw_dust(out, G["dust"], G["sprites"], f, W)
        ib = G["imp_blood"]
        fx.draw_droplets(out, ib[:, 0:3], ib[:, 3:6], ib[:, 6], ib[:, 7], ib[:, 8], f, W)
        cr = G["crown"]
        fx.draw_droplets(out, cr[:, 0:3], cr[:, 3:6], cr[:, 6], cr[:, 7], cr[:, 8], f, W, color=(0.78, 0.05, 0.03),
                         shutter=2.0)
        ch = G["chips"]
        fx.draw_droplets(out, ch[:, 0:3], ch[:, 3:6], ch[:, 6], ch[:, 7], ch[:, 8], f, W, color=(0.2, 0.19, 0.18),
                         spec=False)

    if heat is not None:
        fx.draw_embers(out, G["embers"], f, burn)

    # Camera shake (baked; --shake 0 for none).
    if G["shake"] > 0:
        dx, dy, rot = C.shake(f, 8 * S * G["shake"])
        rot *= G["shake"]
        if abs(dx) + abs(dy) > 0.05 or abs(rot) > 1e-5:
            out = shift_rotate(out, dx, dy, rot)
    return out


def shift_rotate(img, dx, dy, rot):
    H, W = img.shape[:2]
    c, s = math.cos(rot), math.sin(rot)
    cx, cy = W / 2, H / 2
    # Output (x, y) samples input at R^-1 (x - c - d) + c.
    a, b = c, s
    d_, e = -s, c
    tx = cx - a * (cx + dx) - b * (cy + dy)
    ty = cy - d_ * (cx + dx) - e * (cy + dy)
    out = np.empty_like(img)
    for k in range(4):
        out[..., k] = np.asarray(Image.fromarray(img[..., k]).transform(
            (W, H), Image.AFFINE, (a, b, tx, d_, e, ty), Image.BILINEAR))
    return out


def to_straight_u8(prem, seed, dither=True):
    """Premultiplied float RGBA -> straight 8-bit RGBA, dithered; colour bled into the transparent pixels."""
    H, W = prem.shape[:2]
    a = np.clip(prem[..., 3], 0, 1)
    rgb = prem[..., :3] / np.maximum(a, 1e-4)[..., None]
    # Colour for the (nearly) transparent pixels: the alpha-weighted mean colour around them.
    k = 16
    hs, ws = H // k, W // k
    pa = prem[:hs * k, :ws * k].reshape(hs, k, ws, k, 4).mean((1, 3))
    for _ in range(2):
        pa = np.stack([fx.blur(pa[..., c], 2.0) for c in range(4)], -1)
    fill = pa[..., :3] / np.maximum(pa[..., 3:4], 1e-6)
    fill = np.where(pa[..., 3:4] > 1e-5, fill, 0.0)
    fill = np.repeat(np.repeat(fill, k, 0), k, 1)
    fill = np.pad(fill, ((0, H - fill.shape[0]), (0, W - fill.shape[1]), (0, 0)), mode="edge")
    w = np.clip(a / 0.02, 0, 1)[..., None]
    rgb = rgb * w + fill * (1 - w)
    out = np.concatenate([rgb, a[..., None]], -1)
    out = out * 255
    if dither:      # against banding in the PNGs; off for the movies (the noise costs a codec bits, not quality)
        out[..., :3] += np.random.default_rng(seed).random(out.shape[:2] + (3,), np.float32) - 0.5
    return np.clip(np.round(out), 0, 255).astype(np.uint8)


def work_frame(args):
    g, out_dir, dither = args
    f = g + (C.NOGRUNT_OFFSET if G["variant"] == "nogrunt" else 0)
    t0 = time.time()
    prem = frame_rgba(f)
    u8 = to_straight_u8(prem, f, dither)
    Image.fromarray(u8, "RGBA").save(os.path.join(out_dir, "logo_splash_%04d.png" % g), compress_level=2)
    return f, time.time() - t0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--width", type=int, default=3840)
    ap.add_argument("--work", default=C.WORK)
    ap.add_argument("--out", default=None)
    ap.add_argument("--frames", default=None)
    ap.add_argument("--step", type=int, default=1)
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--shake", type=float, default=1.0)
    ap.add_argument("--variant", default="full", choices=("full", "nogrunt"),
                    help="nogrunt: starts NOGRUNT_LEAD empty frames before the burst, no grunt, no axe (frames renumbered)")
    ap.add_argument("--dither", type=int, default=1, help="0: no dither noise in the colour (for the movies)")
    a = ap.parse_args()
    out_dir = a.out or os.path.join(C.OUT_ROOT, "png_%d" % a.width)
    os.makedirs(out_dir, exist_ok=True)
    lo, hi = (0, (C.NOGRUNT_FRAMES if a.variant == "nogrunt" else C.FRAMES) - 1)
    if a.frames:
        x, _, y = a.frames.partition("-")
        lo, hi = int(x), int(y or x)
    frames = list(range(lo, hi + 1, a.step))
    t0 = time.time()
    jobs = [(f, out_dir, a.dither > 0) for f in frames]
    if a.jobs <= 1:
        init(a.work, a.width, a.shake, a.variant)
        res = [work_frame(j) for j in jobs]
    else:
        with Pool(a.jobs, initializer=init, initargs=(a.work, a.width, a.shake, a.variant)) as pool:
            res = []
            for r in pool.imap_unordered(work_frame, jobs):
                res.append(r)
                if len(res) % 20 == 0:
                    print("%d/%d frames, %.0f s" % (len(res), len(jobs), time.time() - t0), flush=True)
    per = sum(r[1] for r in res) / max(1, len(res))
    print("COMPOSITE DONE: %d frames in %.1f s (%.2f s a frame per worker)" % (len(res), time.time() - t0, per))


if __name__ == "__main__":
    main()
