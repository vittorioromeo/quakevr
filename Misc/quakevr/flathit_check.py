"""flathit_check.py <a|b|c> <log> [screenshots...] -- flathit_test.sh's checks (docs/vr-port/ROUND24.md, "Flat hit feedback").
Prints one line a check, `1|what` or `0|what`, for the script's `check`. Pixels:
  the punch: how far the picture moved sideways between the punched and the settled screenshot, from the column brightness
    profile of the screen's upper part (the drawn hands, locked to the view, are in the lower), against the turn's pixels;
  the marks: pixels redder with the marks drawn than one frame later without, on the marks' ring round the crosshair, per side;
  the low-health blood: how much redder (r - (g + b) / 2, mean) the screen's edge band and its middle got at 10 health."""
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pixdiff_ab import read_png as read_flat  # noqa: E402

RADIUS, WIDTH = 0.15, 0.012  # vr_flat_hitdir_radius, _width: shares of the screen's height


def read_png(path):
    # (pixdiff_ab gives the pixels as one run of bytes: here a row of RGB bytes each.)
    w, h, data = read_flat(path)
    return w, h, [data[y * w * 3:(y + 1) * w * 3] for y in range(h)]


def say(ok, what):
    print("%d|%s" % (1 if ok else 0, what))


def lines(log, tag):
    with open(log, "rb") as f:
        text = f.read().decode("latin-1").replace("\r", "")
    return [l for l in text.split("\n") if l.startswith(tag)]


def nums(line):
    return [float(x) for x in re.findall(r"-?\d+(?:\.\d+)?", line)]


def column_profile(path):
    w, h, rows = read_png(path)
    y0, y1 = int(h * 0.05), int(h * 0.5)
    cols = [0.0] * w
    for y in range(y0, y1):
        r = rows[y]
        for x in range(w):
            cols[x] += r[x * 3] + r[x * 3 + 1] + r[x * 3 + 2]
    return w, h, cols


def best_shift(a, b, most):
    # The shift d (b[i] ~ a[i - d]) with the least mean squared difference over the overlap.
    n = len(a)
    best, bestd = None, 0
    for d in range(-most, most + 1):
        lo, hi = max(0, d), min(n, n + d)
        e = sum((b[i] - a[i - d]) ** 2 for i in range(lo, hi)) / (hi - lo)
        if best is None or e < best:
            best, bestd = e, d
    return bestd


def ring_sides(on, off):
    w, h, ra = read_png(on)
    _, _, rb = read_png(off)
    cx, cy = w / 2.0, h / 2.0
    inner, outer = RADIUS * h - 3, (RADIUS + WIDTH) * h + 3
    side = {"right": 0, "left": 0, "up": 0, "down": 0}
    for y in range(h):
        a, b = ra[y], rb[y]
        for x in range(w):
            dx, dy = x - cx, y - cy
            if not inner <= math.hypot(dx, dy) <= outer:
                continue
            i = x * 3
            if (a[i] - (a[i + 1] + a[i + 2]) / 2.0) - (b[i] - (b[i + 1] + b[i + 2]) / 2.0) < 30:
                continue
            if abs(dx) > abs(dy):
                side["right" if dx > 0 else "left"] += 1
            else:
                side["down" if dy > 0 else "up"] += 1
    return side


def edge_and_middle(full, low):
    w, h, ra = read_png(full)
    _, _, rb = read_png(low)
    band = int(min(w, h) * 0.06)
    edge, mid = [0.0, 0], [0.0, 0]
    for y in range(0, h, 2):
        a, b = ra[y], rb[y]
        for x in range(0, w, 2):
            i = x * 3
            d = (b[i] - (b[i + 1] + b[i + 2]) / 2.0) - (a[i] - (a[i + 1] + a[i + 2]) / 2.0)
            if x < band or x >= w - band or y < band or y >= h - band:
                edge[0] += d
                edge[1] += 1
            elif abs(x - w / 2) < w * 0.15 and abs(y - h / 2) < h * 0.15:
                mid[0] += d
                mid[1] += 1
    return edge[0] / max(edge[1], 1), mid[0] / max(mid[1], 1)


def section_a(log, shots):
    hit = lines(log, "flathit: hit 1 ")
    angle = nums(hit[0])[2] if hit else 0.0
    say(hit and 75 < angle < 105, "the grunt's hit came from his right (%s)" % (hit[0] if hit else "no hit"))
    punch = lines(log, "flathitpunch")
    if len(punch) < 2 or len(shots) < 4:
        say(False, "the probes and screenshots (%d probes, %d screenshots)" % (len(punch), len(shots)))
        return
    p1, p2 = nums(punch[0]), nums(punch[1])  # hits, peak, raw, now pitch, now yaw, aim pitch, aim yaw, fov x, fov y
    yaw1, yaw2, fov = p1[4], p2[4], p1[7]
    say(yaw1 > 0.3, "punch: the view turned left, away from the hit (%.3f deg in the aim, 4 frames on)" % yaw1)
    say(abs(yaw2) < 0.05, "punch: settled, the aim back where it was (%.3f deg left in it)" % yaw2)
    w, h, ca = column_profile(shots[0])
    _, _, cb = column_profile(shots[3])
    dx = best_shift(ca, cb, w // 8)
    # From punched (turned left: the picture moved right) to settled: back left, by the turn's pixels.
    want = -(w / 2.0) * math.tan(math.radians(yaw1 - yaw2)) / math.tan(math.radians(fov / 2.0))
    say(abs(want) > 2 and abs(dx - want) <= 0.35 * abs(want) + 3,
        "punch: the picture moved %d px from punched to settled (the turn says %.1f)" % (dx, want))
    s = ring_sides(shots[1], shots[2])
    others = s["left"] + s["up"] + s["down"]
    say(s["right"] > 50 and s["right"] > 5 * (others + 1),
        "marks: drawn at the crosshair's right (redder ring pixels: right %(right)d left %(left)d up %(up)d down %(down)d)" % s)
    mark = lines(log, "flathitmark")
    a = nums(mark[0])[1] if mark else 0.0
    say(mark and 60 < a < 120, "marks: the probe's mark at %.1f deg (right: 90)" % a)
    knock = lines(log, "flathitknock")
    k = nums(knock[1]) if len(knock) > 1 else [0, 0, 0]
    say(k[0] > 0.05 and k[2] > 0.99, "knock: hipfire the hands' knock is all of itself (%.4f -> %.4f)" % (k[0], k[1]))


def section_b(log):
    knock = lines(log, "flathitknock")
    k = nums(knock[-1]) if knock else [0, 0, 0]
    ratio = k[1] / k[0] if k[0] > 0 else 0
    say(k[0] > 0.05 and 0.19 < ratio < 0.21 and 0.19 < k[2] < 0.21,
        "knock: sighted the hands' knock is 0.2 of itself (%.4f -> %.4f, %.3f)" % (k[0], k[1], ratio))
    punch = lines(log, "flathitpunch")
    p = nums(punch[-1]) if punch else [0, 0, 0]
    say(p[2] > 0.1 and 0.49 < p[1] / p[2] < 0.51, "punch: sighted it is 0.5 of itself (%.3f of %.3f deg)" % (p[1], p[2]))


def section_c(log, shots):
    size = lines(log, "blood size")
    say(size and "health 0.500" in size[0], "vignette: the flatscreen size is used (%s)" % (size[0] if size else "no line"))
    if len(shots) < 2:
        say(False, "vignette: screenshots (%d)" % len(shots))
        return
    edge, mid = edge_and_middle(shots[0], shots[1])
    say(edge > 8 and abs(mid) < 2, "vignette: the edges redder at 10 health, the middle not (edge %.2f, middle %.2f)" % (edge, mid))


if __name__ == "__main__":
    which, log, shots = sys.argv[1], sys.argv[2], sys.argv[3:]
    if which == "a":
        section_a(log, shots)
    elif which == "b":
        section_b(log)
    else:
        section_c(log, shots)
