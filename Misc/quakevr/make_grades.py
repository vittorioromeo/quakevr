#!/usr/bin/env python3
# make_grades.py -- generates the eyes' colour grades (vr_grade; Quake/vr/vr_tonemap.cpp):
#   quakevr/gfx/vr/grade_<name>.png   a 32 x 32 x 32 lookup table as a 1024 x 32 strip: the texel at
#                                     x = b * 32 + r, y = g (top row 0) is the graded colour of
#                                     (r, g, b) / 31. The game reads it as a 3D texture, smoothly.
#
# Usage: python Misc/quakevr/make_grades.py [output game folder] [--preview out.png]
#
# The grade is applied after the tone curve and the headset's gamma, to the colours as they are shown (Quake's
# gamma-encoded ones), blended in by vr_grade_strength. Every grade here is subtle and keeps the brightness of
# each colour (its luma) but for a gentle contrast curve about a dark pivot (0.12, Quake VR's typical brightness:
# a frame's mean stays within a few percent), so that the tuned look (vr_light_contrast, bloom) stays: they change
# the mood, not the exposure.
#   film    "Quake VR": a touch of contrast and saturation, shadows a little cool, highlights a little warm
#   cold    steel-blue: a cooler white balance, shadows towards blue, slightly less saturated
#   warm    amber: a warmer white balance, shadows towards brown
#   moss    green-grey: shadows towards moss green, a little less saturated (episode 2's castles)
#   violet  dusk: shadows towards violet, highlights a little rosy (episode 4's otherworld)
# vr_grade 1 film, 2 cold, 3 warm; 4 by episode: e1 cold, e2 moss, e3 warm, e4 violet, others film.

import os
import sys

import numpy as np
from PIL import Image

N = 32
LUMA = np.array([0.2126, 0.7152, 0.0722])

# contrast: strength and pivot (luma kept there, at 0 and at 1); sat: saturation change; wb: white balance
# (normalised to keep luma); shadow, high: multiplicative tints weighted by (1 - luma)^2 and luma^2.
GRADES = {
    "film": dict(contrast=0.2, pivot=0.12, sat=0.08, wb=(1.0, 1.0, 1.0),
                 shadow=(-0.04, 0.0, 0.06), high=(0.03, 0.01, -0.04)),
    "cold": dict(contrast=0.15, pivot=0.12, sat=-0.06, wb=(0.95, 1.0, 1.08),
                 shadow=(-0.05, 0.0, 0.08), high=(0.0, 0.0, 0.0)),
    "warm": dict(contrast=0.15, pivot=0.12, sat=0.05, wb=(1.06, 1.0, 0.9),
                 shadow=(0.05, 0.01, -0.06), high=(0.02, 0.0, -0.03)),
    "moss": dict(contrast=0.15, pivot=0.12, sat=-0.04, wb=(0.98, 1.03, 0.94),
                 shadow=(-0.03, 0.05, -0.02), high=(0.01, 0.01, -0.03)),
    "violet": dict(contrast=0.15, pivot=0.12, sat=0.0, wb=(1.02, 0.95, 1.07),
                   shadow=(0.04, -0.04, 0.08), high=(0.02, -0.01, 0.0)),
}


def luma(c):
    return c @ LUMA


def keep_luma(c, y):
    """Scales c to luma y (black stays black)."""
    cy = luma(c)
    k = np.where(cy > 1e-6, y / np.maximum(cy, 1e-6), 1.0)
    return c * k[..., None]


def grade(c, contrast, pivot, sat, wb, shadow, high):
    y0 = luma(c)
    # contrast about the pivot on luma: y + k y (1 - y) (y - p) / (4 p (1 - p)) keeps 0, the pivot and 1; its
    # slope at the pivot is 1 + k / 4.
    y1 = np.clip(y0 + contrast * y0 * (1 - y0) * (y0 - pivot) / (pivot * (1 - pivot)) * 0.25, 0.0, 1.0)
    c = keep_luma(c, y1)
    # white balance and split toning, luma kept
    c = c * np.array(wb)
    w_s = ((1 - y1) ** 2)[..., None]
    w_h = (y1 ** 2)[..., None]
    c = c * (1 + np.array(shadow) * w_s + np.array(high) * w_h)
    c = keep_luma(c, y1)
    # saturation about luma
    y = luma(c)[..., None]
    c = y + (c - y) * (1 + sat)
    c = keep_luma(np.maximum(c, 0.0), y1)
    return np.clip(c, 0.0, 1.0)


def lut(params):
    v = np.arange(N) / (N - 1)
    b, g, r = np.meshgrid(v, v, v, indexing="ij")  # [b][g][r]
    c = np.stack([r, g, b], axis=-1)
    out = grade(c, **params)
    # strip: x = b * N + r, y = g
    strip = out.transpose(1, 0, 2, 3).reshape(N, N * N, 3)
    return (np.clip(strip, 0, 1) * 255 + 0.5).astype(np.uint8)


def preview(path):
    """A ramp of greys and hues, each grade below the original (for a look at them side by side)."""
    w, h = 512, 48
    x = np.linspace(0, 1, w)
    rows = [np.stack([x, x, x], -1)]
    for hue in range(6):
        a = hue / 6 * 2 * np.pi
        base = 0.5 + 0.5 * np.array([np.cos(a), np.cos(a - 2.094), np.cos(a + 2.094)])
        rows.append(x[:, None] * base[None, :])
    ramp = np.concatenate([np.repeat(r[None], h // len(rows) + 1, 0) for r in rows], 0)
    panels = [ramp] + [grade(ramp, **p) for p in GRADES.values()]
    img = np.concatenate(panels, 0)
    Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8)).save(path)


def main():
    args = [a for a in sys.argv[1:]]
    prev = None
    if "--preview" in args:
        i = args.index("--preview")
        prev = args[i + 1]
        del args[i:i + 2]
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "quakevr")
    game = args[0] if args else root
    out = os.path.join(game, "gfx", "vr")
    os.makedirs(out, exist_ok=True)
    for name, params in GRADES.items():
        path = os.path.join(out, "grade_%s.png" % name)
        Image.fromarray(lut(params)).save(path)
        print("wrote", os.path.normpath(path))
    if prev:
        preview(prev)
        print("wrote", prev)


if __name__ == "__main__":
    main()
