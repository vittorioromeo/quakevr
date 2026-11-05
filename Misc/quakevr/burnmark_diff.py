#!/usr/bin/env python3
"""burnmark_diff.py -- what a lightning bolt left on the wall (burnmark_test.sh).

  burnmark_diff.py <before.png> <after.png> [--out side_by_side.png]

Prints `dark=<n> changed=<m> of <total>`: pixels of the frame's upper part (the HUD left out) darker by more than DARK on every
channel in the second frame, and those different by more than that at all. The frames have the entities hidden, so what is
left is the world. With --out: both frames and the darkened pixels (white) side by side.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import waterzap_diff as d  # noqa: E402

DARK = 18
HUD_SHARE = 0.72  # the rows over the HUD
LIST_SHARE = 0.88  # the columns left of the weapons' list


def main():
    args = sys.argv[1:]
    out = None
    if "--out" in args:
        i = args.index("--out")
        out = args[i + 1]
        del args[i:i + 2]
    w, h, ca, a = d.read_png(args[0])
    w2, h2, cb, b = d.read_png(args[1])
    if (w, h) != (w2, h2):
        raise SystemExit("the frames differ in size")
    dark = changed = 0
    mask = []
    for y in range(h):
        line = bytearray(w * 3)
        if y < h * HUD_SHARE:
            for x in range(int(w * LIST_SHARE)):
                pa, pb = x * ca, x * cb
                dr, dg, db = a[y][pa] - b[y][pb], a[y][pa + 1] - b[y][pb + 1], a[y][pa + 2] - b[y][pb + 2]
                if max(abs(dr), abs(dg), abs(db)) > DARK:
                    changed += 1
                    line[x * 3:x * 3 + 3] = b"\x60\x60\x60"
                    if min(dr, dg, db) > DARK:
                        dark += 1
                        line[x * 3:x * 3 + 3] = b"\xff\xff\xff"
        mask.append(line)
    print("dark=%d changed=%d of %d" % (dark, changed, w * h))
    if out:
        rows = []
        for y in range(h):
            row = bytearray()
            for src, c in ((a, ca), (b, cb)):
                row += b"".join(bytes(src[y][x * c:x * c + 3]) for x in range(w))
            rows.append(row + mask[y])
        d.write_png(out, w * 3, h, rows)


main()
