# fx_precompute.py -- what composite.py needs that doesn't change from frame to frame, from Blender's stills:
# the burst's splash and the letters' splatters (thickness and arrival frame per pixel), the drips, when each pixel
# of "Unleashed" is wiped clear and the ridge of blood pushed up beside it, and and what the fire burns (its fuel, where it catches when).
#
#   python fx_precompute.py --width 3840 [--work <dir>]      (reads <work>/stills, writes <work>/pre)

import argparse
import json
import os
import sys
import time

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C  # noqa: E402
import fx  # noqa: E402


def alpha_of(path, W):
    im = Image.open(path).convert("RGBA")
    if im.width != W:
        im = im.resize((W, W * 9 // 16), Image.LANCZOS)
    return np.asarray(im, np.float32)[..., 3] / 255.0


def blood_at(pre, drips, frame, S):
    T = pre["T_main"] * np.clip((frame - pre["A_main"] + 1) / 2, 0, 1)
    T += pre["T_imp"] * np.clip((frame - pre["A_imp"] + 1) / 2, 0, 1)
    fx.draw_drips(T, drips, frame, S)
    return T


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--width", type=int, default=3840)
    ap.add_argument("--work", default=C.WORK)
    a = ap.parse_args()
    W = a.width
    H = W * 9 // 16
    S = W / 3840.0
    st = os.path.join(a.work, "stills")
    out = os.path.join(a.work, "pre")
    os.makedirs(out, exist_ok=True)
    t0 = time.time()
    rests = [alpha_of(os.path.join(st, "rest_%d.png" % i), W) for i in range(7)]
    letters = alpha_of(os.path.join(st, "letters_rest.png"), W)
    unl = alpha_of(os.path.join(st, "unleashed.png"), W)

    main_f, drips_main = fx.splash_fields(W, H)
    print("splash %.1f s" % (time.time() - t0), flush=True)
    imp_f, drips_imp = fx.impact_fields(W, H, rests)
    print("impacts %.1f s" % (time.time() - t0), flush=True)

    # "Unleashed": wiped a touch wider than the letters (a dark gap round them, as in the logo).
    wipe = fx.blur(fx.dilate((unl > 0.3).astype(np.float32), int(5 * S) + 1), 1.5 * S)
    Cw, tip = fx.finger_times(wipe, C.FINGER_START, C.FINGER_END, S)
    inside = Cw < 1e8
    # The ridge pushed up beside each stroke, appearing as the stroke passes.
    ring = fx.dilate(inside.astype(np.float32), int(16 * S) + 1)
    rim = fx.blur(ring, 4 * S) * (1 - fx.blur(inside.astype(np.float32), 1.5 * S))
    rim *= 0.55 + 0.6 * fx.noise(H, W, 25 * S, 31, 3)
    Cfill = np.where(inside, Cw, 1e9).astype(np.float32)
    Crim = -fx.dilate(-Cfill, int(18 * S) + 2)
    print("finger %.1f s" % (time.time() - t0), flush=True)

    pre = dict(T_main=main_f.T, A_main=main_f.A, T_imp=imp_f.T, A_imp=imp_f.A, C=Cw, Crim=Crim,
               rim=rim.astype(np.float32))
    drips = drips_main + drips_imp
    np.savez(os.path.join(out, "fields.npz"), **pre)
    with open(os.path.join(out, "meta.json"), "w") as f:
        json.dump(dict(width=W, drips=drips, tip=tip[::2]), f)

    # The fire: fed by the blood and the letters as they are when it starts, hottest along their tops.
    Tf = blood_at(pre, drips, C.FIRE_START, S)
    Tf *= Cw > C.FIRE_START                      # "Unleashed" is wiped clean by then
    blood_a = np.clip(Tf / 0.09, 0, 1)
    solid = np.maximum(letters, unl)
    k = int(14 * S) + 1
    # The splash's outline, smoothed (its rays and drops would light everywhere): its top edge burns.
    sil = fx.blur((fx.blur(blood_a, 18 * S) > 0.35).astype(np.float32), 3 * S)
    k2 = int(26 * S) + 1
    tops_b = np.clip(sil - np.roll(sil, k2, 0), 0, 1)                  # the outline's top, a band k2 thick
    tops_s = np.clip(solid - np.roll(solid, k, 0), 0, 1)               # the letters' tops
    clumps = fx.smoothstep(0.3, 0.62, fx.noise(H, W, 110 * S, 41, 3))
    edge = np.clip(tops_b * 0.85 + tops_s, 0, 1) * (0.35 + 0.75 * clumps)
    area = np.clip(blood_a * 0.8 + solid * 0.4, 0, 1) * (0.3 + 0.7 * clumps)
    fuel = np.clip(edge + area * 0.5, 0, 1)
    GW, GH = fx.Fire.GW, fx.Fire.GH
    g_edge = np.clip(fx.resize_f(edge, GW, GH, Image.BILINEAR) * 2.2, 0, 1.15)
    g_area = np.clip(fx.resize_f(area, GW, GH, Image.BILINEAR), 0, 1)
    # Ignition: from the chest out, fast, a little ragged.
    yy, xx = np.mgrid[0:GH, 0:GW].astype(np.float32)
    cxp, cyp = fx.chest_px(W)
    d = np.hypot(xx * W / GW - cxp, yy * H / GH - cyp) / (W * 0.5)
    ignite = C.FIRE_START + d * 9 + fx.noise(GH, GW, 30, 42, 3) * 5
    np.savez(os.path.join(out, "fire.npz"), fuel_edge=g_edge, fuel_area=g_area, ignite=ignite)
    np.save(os.path.join(out, "fuel.npy"), (fuel > 0.4).astype(np.uint8))
    print("fire fuel %.1f s" % (time.time() - t0), flush=True)
    print("PRECOMPUTE DONE in %.1f s" % (time.time() - t0))


if __name__ == "__main__":
    main()
