#!/usr/bin/env python3
"""sa_shockarcs_diff.py -- what the arcs on your own arms change on screen (sa_shockarcs_test.sh; no image libraries here).

  sa_shockarcs_diff.py <with.png> <without.png> [--out side_by_side.png]

Prints `blue=<n> lowblue=<m> centre=<x>,<y> box=<x0>,<y0>,<x1>,<y1> of <w>x<h>`: the pixels made brighter and bluer by more than
waterzap_diff.THRESHOLD (the arcs' colour), those in the view's lower half (where the arms are), and where the lower ones
lie (shares of the width and height): hands locked to the view keep them in one place however the look goes.
"""
import sys

from waterzap_diff import THRESHOLD, read_png, write_png


def main():
    args = sys.argv[1:]
    out = None
    if "--out" in args:
        i = args.index("--out")
        out = args[i + 1]
        del args[i:i + 2]
    w, h, ca, a = read_png(args[0])
    w2, h2, cb, b = read_png(args[1])
    if (w, h) != (w2, h2):
        raise SystemExit("the frames differ in size")
    blue = low = sx = sy = 0
    x0, y0, x1, y1 = w, h, 0, 0
    mask = []
    for y in range(h):
        ra, rb = a[y], b[y]
        line = bytearray(w * 3)
        for x in range(w):
            pa, pb = x * ca, x * cb
            dr, db = ra[pa] - rb[pb], ra[pa + 2] - rb[pb + 2]
            if db > THRESHOLD and db >= dr:
                blue += 1
                line[x * 3:x * 3 + 3] = b"\xff\xff\xff"
                if y >= h // 2:
                    low += 1
                    sx += x
                    sy += y
                    x0, y0, x1, y1 = min(x0, x), min(y0, y), max(x1, x), max(y1, y)
        mask.append(line)
    print("blue=%d lowblue=%d centre=%.2f,%.2f box=%.2f,%.2f,%.2f,%.2f of %dx%d" % (
        blue, low, sx / max(low, 1) / w, sy / max(low, 1) / h, x0 / w, y0 / h, x1 / w, y1 / h, w, h))
    if out:
        rows = []
        for y in range(h):
            row = bytearray()
            for src, c in ((a, ca), (b, cb)):
                row += b"".join(bytes(src[y][x * c:x * c + 3]) for x in range(w))
            rows.append(row + mask[y])
        write_png(out, w * 3, h, rows)


if __name__ == "__main__":
    main()
