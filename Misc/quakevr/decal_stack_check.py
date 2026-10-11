#!/usr/bin/env python3
"""decal_stack_check.py <before.png> <after.png> [<control.png>] [--mask out.png] - marks laid over others never
lighten a pixel.

The marks on the world multiply the texture (each by its colour where it covers, 1 elsewhere): more marks only darken.
A pixel lighter after more marks were laid over the older ones is an older mark gone there (a grid bucket that dropped
it: the square, cell-shaped holes of decal_stack_test.sh). Counts the pixels lighter by more than the threshold (luma),
prints where they are on a 16 x 9 map of the image, and exits 1 when there are more than ALLOWANCE. With a control shot
(the scene again with nothing laid since the first: a monster idling, a flicker), the pixels it changed and those within
MARGIN of them are left out. --mask writes the lighter pixels white over the after shot darkened (to look at once).
"""
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pixdiff_ab import read_png  # noqa: E402

THRESHOLD = 12  # luma levels
ALLOWANCE = 50  # pixels
MARGIN = 8  # pixels round those the control changed


def load(path):
    w, h, rgb = read_png(path)
    rows = []
    for y in range(h):
        o = 3 * w * y
        rows.append([(rgb[o + 3 * x] * 299 + rgb[o + 3 * x + 1] * 587 + rgb[o + 3 * x + 2] * 114) // 1000
                     for x in range(w)])
    return w, h, rgb, rows


def dilate(rows, w, h):
    """Each set pixel's neighbourhood (MARGIN each way) set too: along the rows, then the columns."""
    out = [[False] * w for _ in range(h)]
    for y in range(h):
        last = -10 * MARGIN
        for x in range(w):
            last = x if rows[y][x] else last
            out[y][x] = x - last <= MARGIN
        last = 10 * w
        for x in range(w - 1, -1, -1):
            last = x if rows[y][x] else last
            out[y][x] = out[y][x] or last - x <= MARGIN
    final = [[False] * w for _ in range(h)]
    for x in range(w):
        last = -10 * MARGIN
        for y in range(h):
            last = y if out[y][x] else last
            final[y][x] = y - last <= MARGIN
        last = 10 * h
        for y in range(h - 1, -1, -1):
            last = y if out[y][x] else last
            final[y][x] = final[y][x] or last - y <= MARGIN
    return final


def write_mask(path, w, h, rgb, hit):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        o = 3 * w * y
        for x in range(w):
            raw += b"\xff\xff\xff" if hit[y][x] else bytes(v // 3 for v in rgb[o + 3 * x:o + 3 * x + 3])

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(bytes(raw), 6)) + chunk(b"IEND", b""))


def compare(before, after, control=None, mask=None):
    w, h, _, la = load(before)
    wb, hb, rgb, lb = load(after)
    if (w, h) != (wb, hb):
        raise SystemExit(f"sizes differ: {w}x{h} {wb}x{hb}")
    skip = None
    if control:
        _, _, _, lc = load(control)
        skip = dilate([[abs(lc[y][x] - la[y][x]) > THRESHOLD for x in range(w)] for y in range(h)], w, h)
    cols, rws = 16, 9
    grid = [[0] * cols for _ in range(rws)]
    hit = [[False] * w for _ in range(h)]
    n = darker = skipped = 0
    for y in range(h):
        for x in range(w):
            if skip and skip[y][x]:
                skipped += 1
                continue
            d = lb[y][x] - la[y][x]
            if d > THRESHOLD:
                n += 1
                hit[y][x] = True
                grid[y * rws // h][x * cols // w] += 1
            elif d < -THRESHOLD:
                darker += 1
    if mask:
        write_mask(mask, w, h, rgb, hit)
    return n, darker, skipped, grid, w * h


def main():
    args = sys.argv[1:]
    mask = None
    if "--mask" in args:
        i = args.index("--mask")
        mask = args[i + 1]
        args = args[:i] + args[i + 2:]
    if len(args) < 2:
        print(__doc__)
        return 2
    n, darker, skipped, grid, total = compare(args[0], args[1], args[2] if len(args) > 2 else None, mask)
    print(f"lighter after the new marks: {n} pixels of {total} (darker: {darker}; left out, moving in the control: "
          f"{skipped})")
    if n > ALLOWANCE:
        for row in grid:
            print("  " + " ".join(f"{v:5d}" if v else "    ." for v in row))
        print("FAIL: older marks gone under newer ones")
        return 1
    print("PASS: every older mark still under the newer")
    return 0


if __name__ == "__main__":
    sys.exit(main())
