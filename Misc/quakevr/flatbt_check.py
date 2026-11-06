#!/usr/bin/env python
"""flatbt_check.py <off.png> <on.png> -- flatbt_test.sh's pixel check: tile 0 of each run's sheet (480x270). The on frame has
less colour than the off one, its corners are darker relative to its middle, and its middle is tinted (blue over red).
Prints `flatbt: PASS ...` or `flatbt: FAIL ...`."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pixdiff_ab import read_png  # noqa: E402

TW, TH = 480, 270


def stats(path):
    w, h, px = read_png(path)

    def region(x0, y0, x1, y1):
        n, r, g, b, sat = 0, 0, 0, 0, 0
        for y in range(y0, y1, 2):
            for x in range(x0, x1, 2):
                o = (y * w + x) * 3
                pr, pg, pb = px[o], px[o + 1], px[o + 2]
                r += pr
                g += pg
                b += pb
                sat += max(pr, pg, pb) - min(pr, pg, pb)
                n += 1
        return r / n, g / n, b / n, sat / n

    mid = region(TW // 4, TH // 4, 3 * TW // 4, 3 * TH // 4)
    cor = region(0, 0, TW // 8, TH // 8)
    return mid, cor


def lum(c):
    return 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]


offm, offc = stats(sys.argv[1])
onm, onc = stats(sys.argv[2])
ratio_off = lum(offc) / max(lum(offm), 1e-3)
ratio_on = lum(onc) / max(lum(onm), 1e-3)
ok_sat = onm[3] < offm[3] * 0.8
ok_vig = ratio_on < ratio_off * 0.9
ok_tint = (onm[2] / max(onm[0], 1e-3)) != (offm[2] / max(offm[0], 1e-3)) and abs(onm[2] / max(onm[0], 1e-3) - offm[2] / max(offm[0], 1e-3)) > 0.03
msg = ("saturation %.1f -> %.1f, corner/middle luma %.2f -> %.2f, mid blue/red %.2f -> %.2f"
       % (offm[3], onm[3], ratio_off, ratio_on, offm[2] / max(offm[0], 1e-3), onm[2] / max(onm[0], 1e-3)))
print("flatbt: %s (%s)" % ("PASS" if ok_sat and ok_vig and ok_tint else "FAIL sat=%s vig=%s tint=%s" % (ok_sat, ok_vig, ok_tint), msg))
