#!/usr/bin/env python3
# make_menu_banner.py -- the menus' vertical "Quake VR: Unleashed" banner, drawn where Quake's plaque (gfx/qplaque.lmp)
# was (Quake/vr/vr_menubrand.cpp):
#   quakevr/gfx/vr/menu_banner.png   the vertical logo, its pixels and alpha exactly as drawn, on a transparent canvas
#                                    of 768 x 2048 (centred): the engine's mipmaps halve a texture's rows and columns
#                                    two at a time, so every level down to a few texels wide must have even sides.
#                                    The engine finds the logo's own rectangle in it from the alpha.
# The fully transparent texels get the colour of the nearest opaque ones (alpha untouched), so that the smaller
# mipmap levels and the smooth filtering do not darken the logo's edges with the transparent black around it.
#
# Usage: python Misc/quakevr/make_menu_banner.py <logo_vertical.webp|png> [output game folder]
# (Python with Pillow's WebP support and numpy.)

import os
import sys

import numpy as np
from PIL import Image

WIDTH, HEIGHT = 768, 2048


def fill_transparent(rgba):
    """The RGB of alpha-0 texels from their opaque neighbours (push-pull: averages weighted by alpha, coarser and
    coarser, then filled back down). Alpha and every visible texel are left as they are."""
    rgb = rgba[..., :3].astype(np.float64)
    a = (rgba[..., 3] > 0).astype(np.float64)
    levels = [(rgb * a[..., None], a)]
    while levels[-1][1].shape[0] > 1 or levels[-1][1].shape[1] > 1:
        c, w = levels[-1]
        h2, w2 = (c.shape[0] + 1) // 2 * 2, (c.shape[1] + 1) // 2 * 2
        c = np.pad(c, ((0, h2 - c.shape[0]), (0, w2 - c.shape[1]), (0, 0)))
        w = np.pad(w, ((0, h2 - w.shape[0]), (0, w2 - w.shape[1])))
        c = c.reshape(h2 // 2, 2, w2 // 2, 2, 3).sum((1, 3))
        w = w.reshape(h2 // 2, 2, w2 // 2, 2).sum((1, 3))
        levels.append((c, w))
    colour = levels[-1][0] / np.maximum(levels[-1][1], 1e-9)[..., None]
    for c, w in reversed(levels[:-1]):
        up = np.repeat(np.repeat(colour, 2, 0), 2, 1)[: c.shape[0], : c.shape[1]]
        own = c / np.maximum(w, 1e-9)[..., None]
        colour = np.where((w > 0)[..., None], own, up)
    out = rgba.copy()
    hole = rgba[..., 3] == 0
    out[..., :3][hole] = np.clip(np.round(colour[hole]), 0, 255).astype(np.uint8)
    return out


def main():
    if len(sys.argv) < 2:
        print(__doc__ if __doc__ else "usage: make_menu_banner.py <logo> [game folder]")
        return 1
    src = Image.open(sys.argv[1]).convert("RGBA")
    game = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), "..", "..", "quakevr")
    if src.width > WIDTH or src.height > HEIGHT:
        print(f"{sys.argv[1]}: {src.width} x {src.height} is bigger than {WIDTH} x {HEIGHT}")
        return 1
    canvas = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    left, top = (WIDTH - src.width) // 2, (HEIGHT - src.height) // 2
    canvas.paste(src, (left, top))
    pixels = fill_transparent(np.asarray(canvas))
    placed = pixels[top : top + src.height, left : left + src.width]
    original = np.asarray(src)
    assert np.array_equal(placed[..., 3], original[..., 3]), "alpha changed"
    visible = original[..., 3] > 0
    assert np.array_equal(placed[visible], original[visible]), "visible texels changed"
    out = os.path.join(game, "gfx", "vr", "menu_banner.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    Image.fromarray(pixels).save(out, optimize=True)
    print(f"{out}: {WIDTH} x {HEIGHT}, the logo ({src.width} x {src.height}) at {left}, {top}, "
          f"{os.path.getsize(out) // 1024} KB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
