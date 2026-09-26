#!/usr/bin/env python3
# make_detail.py -- generates the detail textures (vr_detail.cpp, docs/vr-port/ROUND17.md "Detail textures"):
#   quakevr/textures/vr/detail_<kind>.png   512 x 512, grey, tileable, averaging exactly mid-grey (128)
#
# A detail texture is a fine grain blended over the world's textures close to the eye, multiplied around
# mid-grey: the shader takes 1 + strength x (2 x detail - 1), so a texture's average brightness is unchanged
# and the smaller mips (averaging to 128) fade the grain out with distance by themselves. Each is made from
# noise filtered in the frequency domain (periodic by construction: tiles with no seam), cell noise with
# wrapped distances, and lines drawn wrapped round the edges.
#
# Kinds (the engine's layers, in this order; detail.cfg names them):
#   stone    lumpy rock, sand grain, pits and faint cracks
#   metal    brushed streaks along x, scratches (most along the brushing), wear blotches, pits
#   wood     fibres and wavy growth rings along x, dark pores
#   dirt     grime: stains, grit and faint streaks down along y
#   organic  cells, veins and pores (flesh, slime-covered walls)
#   plaster  soft fine grain and tiny pits (the fallback for everything else)
# Wood's and metal's grain runs along the texture's s axis (x here); the engine turns it to run along t
# where a texture's own pattern runs that way.
#
# Usage: python Misc/quakevr/make_detail.py [--out quakevr/textures/vr] [--size 512] [--preview sheet.png]
#        [--only stone,metal]
# Needs numpy. The output is deterministic (fixed seeds): tune the kinds' recipes below, rerun, restart the
# map (the engine loads the images at the first map; `vr_detail_reload` reloads them and detail.cfg).

import argparse
import os
import struct
import sys
import zlib

import numpy as np

KINDS = ["stone", "metal", "wood", "dirt", "organic", "plaster"]

# How far each kind's grey strays from 128, as a standard deviation in 0..1 (the engine's per-kind strength
# scales it further; see detail.cfg).
CONTRAST = {"stone": 0.11, "metal": 0.10, "wood": 0.11, "dirt": 0.11, "organic": 0.11, "plaster": 0.09}


def freqs(n):
    f = np.fft.fftfreq(n) * n
    return f[None, :], f[:, None]


def unit(a):
    a = a - a.mean()
    s = a.std()
    return a / s if s > 0 else a


def spectral(rng, n, beta, fmin, fmax, ax=1.0, ay=1.0):
    """Periodic noise with power ~ 1 / f^beta between fmin and fmax cycles per tile; ax, ay > 1 squeeze that
    axis's frequencies (ax 10: streaks ten times longer along x than across)."""
    fx, fy = freqs(n)
    f = np.sqrt((fx * ax) ** 2 + (fy * ay) ** 2)
    with np.errstate(divide="ignore"):
        amp = np.where(f > 0, f ** (-beta / 2.0), 0.0)
    amp *= 1.0 - np.exp(-((f / max(fmin, 1e-3)) ** 4))
    amp *= np.exp(-((f / fmax) ** 2))
    white = rng.standard_normal((n, n))
    return unit(np.real(np.fft.ifft2(np.fft.fft2(white) * amp)))


def blur(a, sigma):
    """Periodic Gaussian blur (sigma in pixels)."""
    n = a.shape[0]
    fx, fy = freqs(n)
    g = np.exp(-2.0 * (np.pi * sigma / n) ** 2 * (fx ** 2 + fy ** 2))
    return np.real(np.fft.ifft2(np.fft.fft2(a) * g))


def worley(rng, n, cells):
    """Periodic cell noise: the distances to the nearest and second nearest of `cells` random points."""
    pts = rng.random((cells, 2)) * n
    y, x = np.mgrid[0:n, 0:n].astype(np.float32)
    f1 = np.full((n, n), 1e9, np.float32)
    f2 = np.full((n, n), 1e9, np.float32)
    for px, py in pts:
        dx = np.abs(x - px)
        dx = np.minimum(dx, n - dx)
        dy = np.abs(y - py)
        dy = np.minimum(dy, n - dy)
        d = np.sqrt(dx * dx + dy * dy)
        f2 = np.where(d < f1, f1, np.minimum(f2, d))
        f1 = np.minimum(f1, d)
    return f1, f2


def lines(rng, n, count, length, angle, spread, value, step=0.35):
    """Thin lines splatted bilinearly, wrapping round the edges: `count` of them, lengths in `length` (pixels),
    at `angle` +- `spread` radians (spread pi: any direction), each of a random value from `value`."""
    acc = np.zeros(n * n)
    for _ in range(count):
        ln = rng.uniform(*length)
        a = angle + rng.uniform(-spread, spread)
        x0, y0 = rng.random(2) * n
        v = rng.uniform(*value)
        t = np.arange(0.0, ln, step)
        # a slight bend and a taper at both ends, as a real scratch
        bend = rng.uniform(-0.002, 0.002)
        px = x0 + np.cos(a + bend * t) * t
        py = y0 + np.sin(a + bend * t) * t
        w = np.sin(np.pi * np.clip(t / ln, 0, 1)) ** 0.5 * v
        ix = np.floor(px).astype(int)
        iy = np.floor(py).astype(int)
        fx = px - ix
        fy = py - iy
        for ox, oy, k in ((0, 0, (1 - fx) * (1 - fy)), (1, 0, fx * (1 - fy)), (0, 1, (1 - fx) * fy), (1, 1, fx * fy)):
            np.add.at(acc, ((iy + oy) % n) * n + (ix + ox) % n, w * k * step)
    return acc.reshape(n, n)


def dots(rng, n, count, radius, value):
    """Small round spots (pits), wrapping: `count` of them, radius about `radius` pixels (from half to 1.5
    times), each peaking at a random value from `value`."""
    out = np.zeros((n, n))
    for r in (radius * 0.6, radius, radius * 1.5):
        acc = np.zeros((n, n))
        for (x, y) in rng.random((count // 3, 2)) * n:
            acc[int(y) % n, int(x) % n] += rng.uniform(*value)
        out += blur(acc, r) * (2 * np.pi * r * r)
    return out


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


# ---- the kinds -----------------------------------------------------------------------------------------------


def stone(n, rng):
    lumps = spectral(rng, n, 2.0, 1, n * 0.3)
    grain = spectral(rng, n, 0.8, n * 0.06, n * 0.35)
    pits = dots(rng, n, int(n * n / 1200), 1.1, (0.8, 2.0))
    f1, f2 = worley(rng, n, 14)
    crack = blur(np.exp(-(f2 - f1) / 1.2), 0.6)
    mask = smoothstep(0.0, 1.0, spectral(rng, n, 2.0, 1, 12))  # cracks only in places
    mid = spectral(rng, n, 1.3, 8, n * 0.15)
    return 0.55 * lumps + 0.45 * mid + 0.35 * grain - pits - 1.8 * crack * mask


def metal(n, rng):
    brushed = spectral(rng, n, 0.8, 2, n * 0.5, ax=14.0)
    wear = spectral(rng, n, 2.2, 1, 30)
    scratches = lines(rng, n, 70, (n * 0.04, n * 0.3), 0.0, 0.25, (0.4, 1.0))
    scratches += lines(rng, n, 35, (n * 0.02, n * 0.15), 0.0, np.pi, (-0.8, 0.8))
    scratches = blur(scratches, 0.5)
    pits = dots(rng, n, int(n * n / 4000), 0.8, (0.5, 1.2))
    return 0.55 * brushed + 0.4 * wear + 3.5 * scratches / (np.abs(scratches).max() + 1e-6) - pits


def wood(n, rng):
    y = np.mgrid[0:n, 0:n][0] / n
    warp = spectral(rng, n, 2.6, 1, 6)
    warp2 = spectral(rng, n, 1.8, 2, 24, ax=4.0)
    ph = 9.0 * y + 0.35 * warp + 0.06 * warp2  # 9 rings a tile (integer: periodic), wavy
    r = np.sin(2 * np.pi * ph)
    rings = np.sign(r) * np.abs(r) ** 3  # thin late-wood lines between wide early wood
    fibres = spectral(rng, n, 1.0, 3, n * 0.5, ax=22.0)
    pore_n = spectral(rng, n, 0.4, n * 0.1, n * 0.5, ax=7.0)
    pores = smoothstep(1.8, 2.8, pore_n)
    return 0.45 * rings + 0.75 * fibres - 1.2 * pores


def dirt(n, rng):
    stains = smoothstep(0.1, 1.4, spectral(rng, n, 2.4, 1, 40))
    grit = spectral(rng, n, 0.3, n * 0.15, n * 0.5)
    streaks = spectral(rng, n, 1.4, 2, n * 0.25, ay=9.0)  # long along y: down a wall
    mid = spectral(rng, n, 1.6, 4, 90)
    specks = dots(rng, n, int(n * n / 900), 0.7, (-1.5, 1.0))
    return -1.1 * stains + 0.45 * grit + 0.3 * streaks + 0.35 * mid + specks


def organic(n, rng):
    f1, f2 = worley(rng, n, 40)
    r = np.sqrt(n * n / 40.0) * 0.5
    cells = blur(1.0 - np.clip(f1 / r, 0, 1.5), 2.0)
    edges = blur(np.exp(-(f2 - f1) / 3.0), 1.5) * smoothstep(-0.6, 0.8, spectral(rng, n, 2.0, 1, 10))
    v = spectral(rng, n, 2.2, 2, 16)
    veins = blur(np.exp(-np.abs(v) * 6.0), 0.8)  # a soft ridged network
    fine = spectral(rng, n, 0.8, n * 0.1, n * 0.5)
    pores = dots(rng, n, int(n * n / 800), 0.8, (0.5, 1.2))
    return 0.5 * unit(cells) - 0.45 * unit(edges) - 0.3 * unit(veins) + 0.35 * fine - pores


def plaster(n, rng):
    fine = spectral(rng, n, 0.9, n * 0.04, n * 0.5)
    soft = spectral(rng, n, 2.2, 1, 24)
    pits = dots(rng, n, int(n * n / 2000), 0.8, (0.5, 1.5))
    return 0.65 * fine + 0.45 * soft - pits


RECIPES = {"stone": stone, "metal": metal, "wood": wood, "dirt": dirt, "organic": organic, "plaster": plaster}


def to_grey(img, contrast):
    """To 8-bit grey round 128: `contrast` the standard deviation in 0..1, softly limited to 1..254, and the mean
    put back at exactly 128 after rounding (so that the smallest mips are 128 and add nothing)."""
    img = unit(img) * contrast * 255.0
    img = 126.0 * np.tanh(img / 126.0)  # soft limit
    img -= img.mean()
    q = np.round(img)
    err = int(round(q.sum()))  # the rounding's drift, fixed a unit at a time on the pixels nearest a half
    if err:
        frac = (img - q).ravel()
        order = np.argsort(frac) if err > 0 else np.argsort(-frac)
        q.ravel()[order[:abs(err)]] -= np.sign(err)
    return np.clip(q + 128, 0, 255).astype(np.uint8)


def write_png(path, grey):
    h, w = grey.shape
    raw = b"".join(b"\x00" + grey[y].tobytes() for y in range(h))

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description="Generates the detail textures (quakevr/textures/vr/detail_*.png).")
    ap.add_argument("--out", default=os.path.join(here, "..", "..", "quakevr", "textures", "vr"))
    ap.add_argument("--size", type=int, default=512)
    ap.add_argument("--only", default="")
    ap.add_argument("--preview", default="", help="also write a sheet of the kinds, each tiled 2 x 2 (seams show)")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    kinds = [k for k in KINDS if not a.only or k in a.only.split(",")]
    sheet = []
    for i, k in enumerate(KINDS):
        if k not in kinds:
            continue
        rng = np.random.default_rng(1000 + i)
        grey = to_grey(RECIPES[k](a.size, rng), CONTRAST[k])
        path = os.path.join(a.out, "detail_%s.png" % k)
        write_png(path, grey)
        print("%-8s mean %.3f sd %.1f min %d max %d -> %s" % (k, grey.mean(), grey.std(), grey.min(), grey.max(),
                                                           os.path.normpath(path)))
        sheet.append(np.tile(grey, (2, 2)))
    if a.preview and sheet:
        cols = 3
        rows = (len(sheet) + cols - 1) // cols
        s = sheet[0].shape[0]
        big = np.full((rows * (s + 8), cols * (s + 8)), 128, np.uint8)
        for j, g in enumerate(sheet):
            r, c = divmod(j, cols)
            big[r * (s + 8):r * (s + 8) + s, c * (s + 8):c * (s + 8) + s] = g
        write_png(a.preview, big)


if __name__ == "__main__":
    sys.exit(main())
