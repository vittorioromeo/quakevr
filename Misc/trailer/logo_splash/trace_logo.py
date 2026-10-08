# trace_logo.py -- the logo's own lettering as clean vector outlines, traced from docs/images/quakevr-unleashed-square
# (the "QUAKE VR" capitals with the real Q and its nail, and the red "UNLEASHED"), for blender_scene.py to extrude.
#
#   python trace_logo.py [--logo <png|webp>] [--out <letters.json>] [--debug <png>]
#
# The pale stone letters and the red ones are told apart from the blood by colour, the blood stains and cracks on
# them filled, each letter's mask smoothed and upsampled, and its outline taken at the half-way level (marching
# squares) and simplified. The JSON keeps the logo's own layout: {"title": [letter...], "sub": [letter...]}, a letter
# being a list of closed loops of (x, y) in the logo's pixels (y down), outer outlines and holes alike.

import argparse
import json
import math
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, HERE)
import common as C  # noqa: E402

UP = 6          # the outline is traced on a copy this many times bigger


def label(mask):
    """8-connected components: (labels, count)."""
    h, w = mask.shape
    lab = np.zeros((h, w), np.int32)
    n = 0
    ys, xs = np.nonzero(mask)
    for y, x in zip(ys, xs):
        if lab[y, x]:
            continue
        n += 1
        lab[y, x] = n
        stack = [(y, x)]
        while stack:
            a, b = stack.pop()
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = a + dy, b + dx
                    if 0 <= yy < h and 0 <= xx < w and mask[yy, xx] and not lab[yy, xx]:
                        lab[yy, xx] = n
                        stack.append((yy, xx))
    return lab, n


def fill_small_holes(mask, max_area):
    """Holes (background not reaching the border) smaller than max_area filled: stains and cracks, not counters."""
    lab, n = label(~mask)
    out = mask.copy()
    h, w = mask.shape
    for k in range(1, n + 1):
        ys, xs = np.nonzero(lab == k)
        if ys.min() == 0 or xs.min() == 0 or ys.max() == h - 1 or xs.max() == w - 1:
            continue
        if len(ys) <= max_area:
            out[ys, xs] = True
    return out


def morph(mask, r, grow):
    out = mask.copy()
    for _ in range(r):
        p = np.pad(out, 1, constant_values=not grow)
        nb = [p[1 + dy:p.shape[0] - 1 + dy, 1 + dx:p.shape[1] - 1 + dx] for dy in (-1, 0, 1) for dx in (-1, 0, 1)]
        out = np.logical_or.reduce(nb) if grow else np.logical_and.reduce(nb)
    return out


def marching_squares(F, iso=0.5):
    """Closed loops of the iso line of F (padded so every loop closes), as lists of (x, y)."""
    F = np.pad(F, 1)
    h, w = F.shape
    a, b = F[:-1, :-1] > iso, F[:-1, 1:] > iso
    c, d = F[1:, 1:] > iso, F[1:, :-1] > iso
    case = a * 1 + b * 2 + c * 4 + d * 8
    segs = {}

    def pt(edge):
        kind, i, j = edge
        if kind == "h":   # between (i, j) and (i, j + 1)
            v0, v1 = F[i, j], F[i, j + 1]
            t = (iso - v0) / (v1 - v0)
            return (j + t - 1, i - 1)
        v0, v1 = F[i, j], F[i + 1, j]
        t = (iso - v0) / (v1 - v0)
        return (j - 1, i + t - 1)

    links = {}

    def link(e0, e1):
        links.setdefault(e0, []).append(e1)
        links.setdefault(e1, []).append(e0)

    ys, xs = np.nonzero((case > 0) & (case < 15))
    for i, j in zip(ys, xs):
        k = case[i, j]
        top, right, bottom, left = ("h", i, j), ("v", i, j + 1), ("h", i + 1, j), ("v", i, j)
        table = {1: [(left, top)], 2: [(top, right)], 3: [(left, right)], 4: [(right, bottom)],
                 6: [(top, bottom)], 7: [(left, bottom)], 8: [(bottom, left)], 9: [(bottom, top)],
                 11: [(bottom, right)], 12: [(right, left)], 13: [(right, top)], 14: [(top, left)]}
        if k in (5, 10):
            centre = (F[i, j] + F[i, j + 1] + F[i + 1, j] + F[i + 1, j + 1]) / 4 > iso
            if k == 5:
                pairs = [(left, bottom), (right, top)] if not centre else [(left, top), (right, bottom)]
            else:
                pairs = [(top, left), (bottom, right)] if not centre else [(top, right), (bottom, left)]
        else:
            pairs = table[k]
        for e0, e1 in pairs:
            link(e0, e1)
    loops = []
    seen = set()
    for e in links:
        if e in seen:
            continue
        loop = [e]
        seen.add(e)
        prev, cur = None, e
        while True:
            nxt = [x for x in links[cur] if x != prev and x not in seen]
            if not nxt:
                break
            prev, cur = cur, nxt[0]
            seen.add(cur)
            loop.append(cur)
        if len(loop) > 8:
            loops.append([pt(x) for x in loop])
    return loops


def rdp(pts, eps):
    """Ramer-Douglas-Peucker on a closed loop."""
    P = np.array(pts)
    if len(P) < 8:
        return pts

    def simp(a, b):
        seg = P[a:b + 1]
        p0, p1 = seg[0], seg[-1]
        d = p1 - p0
        n = math.hypot(*d) or 1e-9
        dist = np.abs(d[0] * (seg[:, 1] - p0[1]) - d[1] * (seg[:, 0] - p0[0])) / n
        k = int(np.argmax(dist))
        if dist[k] > eps and 0 < k < len(seg) - 1:
            return simp(a, a + k)[:-1] + simp(a + k, b)
        return [a, b]

    far = int(np.argmax(np.hypot(P[:, 0] - P[0, 0], P[:, 1] - P[0, 1])))
    idx = simp(0, far)[:-1] + simp(far, len(P) - 1)
    return [tuple(map(float, P[i])) for i in idx[:-1]]


def outline(mask, box, smooth=1.1, eps=0.12):
    """A letter's loops in logo pixels: the mask blurred, upsampled, traced at 0.5, simplified."""
    x0, y0, x1, y1 = box
    pad = 4
    sub = np.pad(mask[y0:y1, x0:x1], pad)
    soft = blur_mask(sub, smooth)
    big = np.asarray(Image.fromarray(soft).resize((sub.shape[1] * UP, sub.shape[0] * UP), Image.BICUBIC))
    loops = marching_squares(big, 0.5)
    out = []
    for lp in loops:
        pts = [((x + 0.5) / UP - 0.5 + x0 - pad, (y + 0.5) / UP - 0.5 + y0 - pad) for x, y in lp]
        pts = rdp(pts, eps)
        if len(pts) >= 3:
            out.append(pts)
    return out


def blur_mask(mask, sigma):
    from PIL import ImageFilter
    im = Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(sigma))
    return np.asarray(im, np.float32) / 255


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--logo", default=os.path.join(REPO, "docs", "images", "quakevr-unleashed-square.webp"))
    ap.add_argument("--out", default=C.LETTERS_JSON)
    ap.add_argument("--debug")
    a = ap.parse_args()
    s = np.asarray(Image.open(a.logo).convert("RGBA")).astype(np.int32)
    r, g, b, al = s[..., 0], s[..., 1], s[..., 2], s[..., 3]
    mn = np.minimum(np.minimum(r, g), b)
    mx = np.maximum(np.maximum(r, g), b)
    pale = (mn > 90) & (mx - mn < 80) & (al > 128)
    red = (r > 170) & (g < 110) & (b < 100) & (al > 128)
    H, W = pale.shape
    # Where the lettering is in the square logo (below the big QR emblem).
    band_t = (805, 1240)      # rows of "QUAKE VR" (the Q's nail reaches lower)
    band_s = (1005, 1110)     # rows of "UNLEASHED"
    res = {}
    dbg = np.zeros((H, W, 3), np.uint8)

    # "QUAKE VR": pale letters with the blood on them (red touching them) closed up, so cracks and stains don't split a
    # letter. The QR emblem above is cut off (its nail tip and its R's leg reach down near the A and the V).
    rows = np.arange(H)[:, None]
    cols = np.arange(W)[None, :]
    keep = (rows >= 805) & (rows < band_t[1]) & (cols >= 0)
    keep &= ~((cols >= 860) & (cols < 1000) & (rows < 853))
    keep &= ~((cols >= 575) & (cols < 665) & (rows < 866))
    m0 = pale & keep
    m0 = m0 | (red & morph(m0, 3, True) & keep)
    m0 = morph(morph(m0, 2, True), 2, False) & keep
    lab, n = label(m0)
    letters = []
    for k in range(1, n + 1):
        ys, xs = np.nonzero(lab == k)
        if len(ys) < 1500 or ys.min() > 990:
            continue
        letters.append((xs.min(), k, (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)))
    letters.sort()
    title = []
    for i, (_, k, (x0, y0, x1, y1)) in enumerate(letters):
        m = fill_small_holes(lab == k, 350)
        if i == 0:
            # The Q's nail: below the ring the logo's nail is covered in blood (traced, it is ragged), so below the
            # ring it is drawn again as the logo has it: a small guard, then a long taper to the point.
            ys, xs = np.nonzero(m)
            widths = {y: (np.nonzero(m[y])[0].min(), np.nonzero(m[y])[0].max()) for y in range(ys.min(), ys.max() + 1)
                      if m[y].any()}
            yn = next(y for y in sorted(widths) if y > ys.min() + 150 and widths[y][1] - widths[y][0] < 45)
            span = [widths[y] for y in range(yn, yn + 10) if y in widths]
            cx = float(np.median([(l + r_) / 2 for l, r_ in span]))
            hw0 = float(np.median([(r_ - l) / 2 for l, r_ in span]))
            m[yn:, :] = False
            tip = 1215
            for y in range(yn, tip):
                t = (y - yn) / (tip - yn)
                hw = hw0 * (1 - t) ** 0.85
                g = (y - yn - 10) / 9.0                      # the guard: a diamond across the nail
                if abs(g) < 1:
                    hw = max(hw, hw0 * (1 + 0.9 * (1 - abs(g))))
                m[y, int(round(cx - hw)):int(round(cx + hw)) + 1] = True
            y1 = tip + 1
        mf = blur_mask(m, 1.7) > 0.5
        box = (max(0, x0 - 8), max(0, y0 - 8), min(W, x1 + 8), min(H, y1 + 8))
        title.append(outline(mf, box, smooth=1.3))
        dbg[mf] = (255, 255, 255)
    res["title"] = title

    # "UNLEASHED": the red capitals in their row.
    rowm = (np.arange(H)[:, None] >= band_s[0]) & (np.arange(H)[:, None] < band_s[1])
    lab, n = label(red & rowm)
    letters = []
    for k in range(1, n + 1):
        ys, xs = np.nonzero(lab == k)
        if len(ys) < 400 or (ys.max() - ys.min()) < 35 or xs.min() < 270 or xs.max() > 1070:
            continue
        letters.append((xs.min(), k, (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)))
    letters.sort()
    sub = []
    for _, k, (x0, y0, x1, y1) in letters:
        m = lab == k
        m = morph(morph(m, 1, True), 1, False)
        m = fill_small_holes(m, 60)
        mf = blur_mask(m, 0.9) > 0.5
        box = (max(0, x0 - 6), max(0, y0 - 6), min(W, x1 + 6), min(H, y1 + 6))
        sub.append(outline(mf, box, smooth=0.9, eps=0.08))
        dbg[mf] = (255, 40, 40)
    res["sub"] = sub
    res["source"] = os.path.basename(a.logo)
    res["size"] = [W, H]
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, "w") as f:
        json.dump(res, f)
    print("%s: %d title letters (%s loops), %d sub letters" % (
        a.out, len(title), [len(t) for t in title], len(sub)))
    if a.debug:
        from PIL import ImageDraw
        im = Image.fromarray(dbg).resize((W * 2, H * 2))
        d = ImageDraw.Draw(im)
        for L in title + sub:
            for lp in L:
                d.line([(x * 2, y * 2) for x, y in lp] + [(lp[0][0] * 2, lp[0][1] * 2)], fill=(0, 200, 255), width=1)
        im.crop((0, 1620, W * 2, 2500)).save(a.debug)


if __name__ == "__main__":
    main()
