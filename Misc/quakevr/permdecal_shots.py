#!/usr/bin/env python3
"""permdecal_shots.py <screenshot dir> <n> <i:j> [<i:j> ...] - permdecal_test.sh's image checks: of the newest n
screenshots (oldest first, numbered from 0), the peak difference (per channel) between shots i and j for each pair,
the bottom-left 96 x 96 corner left out (a HUD element that changes between frames). Prints "i:j peak P" lines.
"""
import glob
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pixdiff_ab import read_png  # noqa: E402


def peak(a, b):
    w, h, pa = a
    _, _, pb = b
    m = 0
    for y in range(h):
        x0 = 96 if y >= h - 96 else 0
        row = (y * w) * 3
        for i in range(row + x0 * 3, row + w * 3):
            d = pa[i] - pb[i]
            if d < 0:
                d = -d
            if d > m:
                m = d
    return m


def main():
    files = sorted(glob.glob(os.path.join(sys.argv[1], "*.png")), key=os.path.getmtime)[-int(sys.argv[2]):]
    shots = [read_png(f) for f in files]
    for pair in sys.argv[3:]:
        i, j = (int(v) for v in pair.split(":"))
        print("%d:%d peak %d" % (i, j, peak(shots[i], shots[j])))


if __name__ == "__main__":
    main()
