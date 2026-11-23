#!/usr/bin/env python3
"""gles_render_check.py <pc dir> <es dir> -- the PC run's and the -glescompat run's render outputs compared, as
gles_render_test.sh collects them in each folder: eye.png (the left eye), envmap.tga (vr_envmap_dump: the weapon
reflections' cube), imagehash.txt (every loaded texture as the GPU holds it, the PBR companions among them).
Prints one line each (numbers only; the shell test judges them):
  eye <mean abs diff per channel> <share of pixels moved by more than 60 of 765> <pc mean> <es mean>
  envmap <mean abs diff> <share moved> <pc mean> <es mean>
  imagehash <textures> <differing> (the sorted lines compared; -1 -1 when a file is missing)
A missing image prints -1 for its numbers. Standard library only (pixdiff_ab.read_png)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pixdiff_ab import read_png  # noqa: E402


def read_tga(path):
    """(width, height, RGB bytes) of vr_envmap_dump's uncompressed 24-bit TGA (BGR, bottom row first)."""
    with open(path, "rb") as f:
        data = f.read()
    w = data[12] | data[13] << 8
    h = data[14] | data[15] << 8
    px = bytearray(data[18:18 + w * h * 3])
    px[0::3], px[2::3] = px[2::3], px[0::3]  # (BGR to RGB; the row order is the same in both files)
    return w, h, px


def compare(a, b):
    (wa, ha, pa), (wb, hb, pb) = a, b
    if (wa, ha) != (wb, hb) or not pa:
        return "-1 -1 -1 -1"
    n = wa * ha
    total = moved = 0
    for p in range(0, n * 3, 3):
        d = abs(pa[p] - pb[p]) + abs(pa[p + 1] - pb[p + 1]) + abs(pa[p + 2] - pb[p + 2])
        total += d
        moved += d > 60
    return f"{total / (n * 3):.3f} {moved / n:.4f} {sum(pa) / (n * 3):.1f} {sum(pb) / (n * 3):.1f}"


def image(reader, a, b):
    if not (os.path.exists(a) and os.path.exists(b)):
        return "-1 -1 -1 -1"
    return compare(reader(a), reader(b))


def main():
    pc, es = sys.argv[1], sys.argv[2]
    print("eye", image(read_png, os.path.join(pc, "eye.png"), os.path.join(es, "eye.png")))
    # (the eyes' sizes, and the pixels differing at all: an exact comparison; -1 when either is missing)
    ea, eb = os.path.join(pc, "eye.png"), os.path.join(es, "eye.png")
    if os.path.exists(ea) and os.path.exists(eb):
        (wa, ha, pa), (wb, hb, pb) = read_png(ea), read_png(eb)
        same = (wa, ha) == (wb, hb)
        exact = sum(pa[p:p + 3] != pb[p:p + 3] for p in range(0, wa * ha * 3, 3)) if same else -1
        print("eyesize", wa, ha, wb, hb, exact)
    else:
        print("eyesize -1 -1 -1 -1 -1")
    print("envmap", image(read_tga, os.path.join(pc, "envmap.tga"), os.path.join(es, "envmap.tga")))
    hashes = [os.path.join(d, "imagehash.txt") for d in (pc, es)]
    if all(os.path.exists(h) for h in hashes):
        a, b = (sorted(open(h, encoding="utf-8", errors="replace").read().splitlines()) for h in hashes)
        print("imagehash", len(a), max(len(set(a) - set(b)), len(set(b) - set(a))))  # (changed or missing, either side)
    else:
        print("imagehash -1 -1")


if __name__ == "__main__":
    main()
