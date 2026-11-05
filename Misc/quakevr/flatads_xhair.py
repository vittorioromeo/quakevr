"""flatads_xhair.py <screenshots dir>: the last six screenshots, hipfire then sighted, each as: the crosshairs on, off, on again
(flatads_test.sh section e). Prints how many pixels differ in the middle fifth of the screen (any channel more than 8
apart) between on and off, and between on and on again (the scene's own change from frame to frame: the control).
Hipfire on/off must differ well past the control (the crosshairs are drawn), sighted not past it (faded out)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pixdiff_ab import read_png  # noqa: E402

d = sys.argv[1]
shots = sorted(f for f in os.listdir(d) if f.lower().endswith(".png"))[-6:]
if len(shots) < 6:
    print("flatxhair: fewer than 6 screenshots")
    sys.exit(1)
imgs = [read_png(os.path.join(d, f)) for f in shots]


def differ(a, b):
    w, h, pa = a
    pb = b[2]
    n = 0
    for y in range(h * 2 // 5, h * 3 // 5):  # the middle fifth: where the crosshairs are
        for x in range(w * 2 // 5, w * 3 // 5):
            o = 3 * (w * y + x)
            if max(abs(pa[o + c] - pb[o + c]) for c in range(3)) > 8:
                n += 1
    return n


print("flatxhair: hipfire %d control %d sighted %d control %d pixels differ" %
      (differ(imgs[0], imgs[1]), differ(imgs[0], imgs[2]), differ(imgs[3], imgs[4]), differ(imgs[3], imgs[5])))
