#!/usr/bin/env python3
# flathud_check.py <shots.png> <report.log> <bases> -- flathud_test.sh's pixel reader: finds flatscreen play's status bar pieces
# (Quake/sbar.c Sbar_DrawFlatHud) in run.ps1's composed screenshots (480x270 tiles, two to a row, in the order taken)
# and prints one line per piece and shot: "flathudpx: <shot> <piece> changed <share> mean <r> <g> <b>".
# The canvas (gl_draw.c CANVAS_SBAR2: the screen over the bar's scale) comes from vr_flathud_report's "flathud: canvas"
# line. A piece is "changed" by the share of its pixels that differ from its base shot (`<bases>`: per shot, 1-based,
# the shot it is read against, 0 none -- the same frame at viewsize 120, no status bar at all), so the scene behind it
# cancels out.
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pixdiff_ab import read_png  # noqa: E402

TW, TH = 480, 270


class Tile:
    """One shot of the composed sheet: getpixel((x, y)) -> (r, g, b)."""

    def __init__(self, sheet, i):
        self.px, self.w, self.x0, self.y0 = sheet.px, sheet.w, (i % 2) * TW, (i // 2) * TH

    def getpixel(self, p):
        o = ((self.y0 + p[1]) * self.w + self.x0 + p[0]) * 3
        return self.px[o], self.px[o + 1], self.px[o + 2]

    def blank(self):
        for y in range(0, TH, 9):
            o = ((self.y0 + y) * self.w + self.x0) * 3
            if any(self.px[o:o + TW * 3]):
                return False
        return True


class Sheet:
    def __init__(self, path):
        self.w, self.h, self.px = read_png(path)
        self.size = (self.w, self.h)


def tile(img, i):
    return Tile(img, i)


def main():
    img = Sheet(sys.argv[1])
    log = open(sys.argv[2], encoding="utf-8", errors="replace").read()
    m = re.search(r"flathud: canvas (\d+) (\d+) scale ([0-9.]+)", log)
    if not m:
        print("flathudpx: no canvas line")
        return 1
    w, h, s = int(m.group(1)), int(m.group(2)), float(m.group(3))
    cw, ch = w / s, h / s  # the canvas, in its own units
    cx = int(cw * 0.5 + 0.5)
    rowtop = int(ch - 9 - 4 - 24 + 0.5)
    yc = int(ch - 10 - 24 + 0.5)
    pieces = {  # canvas rects (x0, y0, x1, y1), as Sbar_DrawFlatHud places them
        "face": (cx - 52, rowtop, cx - 28, rowtop + 24),
        "health": (cx - 20, rowtop, cx + 52, rowtop + 24),
        "timebar": (cx - 52, rowtop - 7, cx + 52, rowtop - 3),
        "lefthand": (16, yc, 16 + 104, yc + 24),
        "righthand": (int(cw - 16 - 24 + 0.5) - 80, yc, int(cw - 16 + 0.5), yc + 24),
        "oldface": (16, yc, 16 + 24, yc + 24),  # Ironwail's own place for the portrait (vr_flat_hud 0)
    }
    bases = [int(b) for b in sys.argv[3].split(",")]
    n = min((img.size[1] // TH) * 2, len(bases))
    for i in range(n):
        if bases[i] <= 0:
            continue
        t, base = tile(img, i), tile(img, bases[i] - 1)
        if t.blank():
            continue
        for name, (x0, y0, x1, y1) in pieces.items():
            px0, py0 = int(x0 * TW / cw), int(y0 * TH / ch)
            px1, py1 = max(px0 + 1, int(x1 * TW / cw + 0.999)), max(py0 + 1, int(y1 * TH / ch + 0.999))
            changed, total, acc = 0, 0, [0, 0, 0]
            for y in range(py0, min(py1, TH)):
                for x in range(px0, min(px1, TW)):
                    a, b = t.getpixel((x, y)), base.getpixel((x, y))
                    total += 1
                    if sum(abs(a[k] - b[k]) for k in range(3)) > 45:
                        changed += 1
                        for k in range(3):
                            acc[k] += a[k]
            share = changed / total if total else 0.0
            mean = [round(v / changed) if changed else 0 for v in acc]
            print(f"flathudpx: {i + 1} {name} changed {share:.2f} mean {mean[0]} {mean[1]} {mean[2]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
