# head_clip.py -- the decapitated head overlay: Blender's head layer (blender_scene.py --pass head) with the neck's
# arterial spurts, its droplets and a red mist in 2D, shaken as the no-grunt intro is, written as straight-alpha
# RGBA PNGs numbered as the no-grunt version (frame NOGRUNT_LEAD = the cut, at the intro's burst).
#
#   python head_clip.py --width 1920 --work <dir> --out <dir> [--frames a-b] [--shake 1]

import argparse
import json
import math
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C  # noqa: E402
import fx  # noqa: E402
from composite import shift_rotate, to_straight_u8  # noqa: E402


def rot(axis, ang):
    x, y, z = axis
    c, s = math.cos(ang), math.sin(ang)
    K = np.array([[0, -z, y], [z, 0, -x], [-y, x, 0]])
    return np.eye(3) + s * K + (1 - c) * K @ K


def neck_world(g, meta):
    """The stump's place and outward normal at (fractional) frame g."""
    p, ax, ang = C.head_state(max(g, C.NOGRUNT_LEAD))
    R = rot(ax, ang)
    return np.array(p), R @ np.array(meta["normal"])


def head_vel(g):
    t = (g - C.NOGRUNT_LEAD) / C.FPS
    return np.array([C.HEAD_VEL[0], C.HEAD_VEL[1], C.HEAD_VEL[2] - C.GRAVITY * t])


def spurts(meta, seed=31):
    """The neck's blood: a burst at the cut, then arterial spurts pulsing (the heart still beating) and dying away,
    every drop thrown out of the stump along its normal on top of the head's own motion."""
    rng = np.random.default_rng(seed)
    P, V, size, t0, life = [], [], [], [], []
    g0 = C.NOGRUNT_LEAD
    # The cut: a spray all round the neck.
    for _ in range(140):
        p, n = neck_world(g0, meta)
        d = rng.normal(0, 1, 3)
        d /= np.linalg.norm(d)
        v = head_vel(g0) * 0.4 + (n * 0.6 + d) * rng.uniform(1.5, 6.0)
        P.append(p + rng.normal(0, 0.02, 3))
        V.append(v)
        size.append(rng.uniform(0.004, 0.016))
        t0.append(g0 + rng.uniform(0, 1.5))
        life.append(rng.uniform(0.4, 1.0))
    # Spurts: about four beats a second for 1.3 s, each a stream of drops.
    T = 1.3
    n_try = 4200
    for _ in range(n_try):
        t = rng.uniform(0, T)
        beat = max(0.0, math.sin(2 * math.pi * 4.2 * t + 0.3)) ** 3
        rate = (0.15 + beat) * math.exp(-t / 0.6)
        if rng.random() > rate:
            continue
        g = g0 + t * C.FPS
        p, n = neck_world(g, meta)
        jitter = rng.normal(0, 0.08, 3)
        v = head_vel(g) + (n + jitter) * (1.5 + 4.5 * beat) * rng.uniform(0.8, 1.2)
        P.append(p)
        V.append(v)
        size.append(rng.uniform(0.005, 0.014) * (1 + beat * 0.6))
        t0.append(g)
        life.append(rng.uniform(0.5, 1.1))
    return dict(P=np.array(P), V=np.array(V), size=np.array(size), t0=np.array(t0), life=np.array(life))


def frame(g, W, work, sp, sprites, shake):
    H = W * 9 // 16
    S = W / 3840.0
    out = np.zeros((H, W, 4), np.float32)
    if g < C.NOGRUNT_LEAD:
        return out
    # A red mist at the cut (behind the head).
    k = g - C.NOGRUNT_LEAD
    if k < 24:
        p, _ = neck_world(C.NOGRUNT_LEAD, META)
        rng = np.random.default_rng(5)
        for j in range(6):
            q = p + rng.normal(0, 0.06, 3)
            d = np.array([[q[0], q[1] - 0.05, q[2], rng.normal(0, 0.4), -0.3, 0.5, 0.12 + 0.03 * (j % 3),
                           C.NOGRUNT_LEAD, 0.4, j]], np.float64)
            fx.draw_dust(out, d, sprites, g, W, base=(0.32, 0.02, 0.025))
    # The blood behind and around the head, then the head over it, then the drops nearest the camera over that.
    fx.draw_droplets(out, sp["P"], sp["V"], sp["size"], sp["t0"], sp["life"], g, W, color=(0.3, 0.012, 0.02),
                     shutter=0.6)
    p = os.path.join(work, "head", "head_%04d.png" % g)
    if os.path.exists(p):
        fx.over(out, fx.load_rgba(p, W))
    if shake > 0:
        dx, dy, r = C.shake(g + C.NOGRUNT_OFFSET, 8 * S * shake)
        if abs(dx) + abs(dy) > 0.05 or abs(r) > 1e-5:
            out = shift_rotate(out, dx, dy, r * shake)
    return out


META = None


def main():
    global META
    ap = argparse.ArgumentParser()
    ap.add_argument("--width", type=int, default=1920)
    ap.add_argument("--work", default=C.WORK)
    ap.add_argument("--out", required=True)
    ap.add_argument("--frames")
    ap.add_argument("--shake", type=float, default=1.0)
    a = ap.parse_args()
    with open(os.path.join(a.work, "head_meta.json")) as f:
        META = json.load(f)
    sp = spurts(META)
    sprites = fx.puff_sprites()
    lo, hi = 0, C.HEAD_FRAMES
    if a.frames:
        x, _, y = a.frames.partition("-")
        lo, hi = int(x), int(y or x)
    os.makedirs(a.out, exist_ok=True)
    for g in range(lo, hi + 1):
        prem = frame(g, a.width, a.work, sp, sprites, a.shake)
        Image.fromarray(to_straight_u8(prem, g), "RGBA").save(os.path.join(a.out, "head_%04d.png" % g),
                                                             compress_level=2)
    print("HEAD CLIP DONE: frames %d-%d, %d drops" % (lo, hi, len(sp["P"])))


if __name__ == "__main__":
    main()
