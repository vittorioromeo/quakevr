"""pngdiff.py -- compare PNGs without any image library.

    python pngdiff.py a.png b.png            # how much they differ
    python pngdiff.py --region x0 y0 x1 y1 a.png b.png
    python pngdiff.py --stats a.png          # one image: mean luma, and the same over a region

Prints a mean absolute difference over the RGB bytes (0..255), the count of pixels that changed by more than 8, and
the mean luma of each image. Used to tell whether a view changed between two frames.
"""
import struct
import sys
import zlib


def read_png(path):
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", path
    pos = 8
    idat = b""
    w = h = depth = color = None
    while pos < len(data):
        ln = struct.unpack_from(">i", data, pos)[0]
        kind = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + ln]
        pos += 12 + ln
        if kind == b"IHDR":
            w, h, depth, color = struct.unpack_from(">iiBBB", body, 0)[:4]
        elif kind == b"IDAT":
            idat += body
    raw = zlib.decompress(idat)
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color]
    assert depth == 8, (path, depth)
    stride = w * channels
    out = bytearray(h * stride)
    prev = bytearray(stride)
    i = 0
    for y in range(h):
        ft = raw[i]
        i += 1
        line = bytearray(raw[i:i + stride])
        i += stride
        if ft == 0:
            pass
        elif ft == 1:  # Sub
            for k in range(channels, stride):
                line[k] = (line[k] + line[k - channels]) & 0xFF
        elif ft == 2:  # Up
            for k in range(stride):
                line[k] = (line[k] + prev[k]) & 0xFF
        elif ft == 3:  # Average
            for k in range(stride):
                a = line[k - channels] if k >= channels else 0
                line[k] = (line[k] + ((a + prev[k]) >> 1)) & 0xFF
        elif ft == 4:  # Paeth
            for k in range(stride):
                a = line[k - channels] if k >= channels else 0
                b = prev[k]
                c = prev[k - channels] if k >= channels else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[k] = (line[k] + pr) & 0xFF
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return w, h, channels, out


def luma(data, channels, x0, y0, x1, y1, w):
    s = 0
    n = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            i = (y * w + x) * channels
            s += 0.299 * data[i] + 0.587 * data[i + 1] + 0.114 * data[i + 2]
            n += 1
    return s / max(n, 1)


def main():
    args = sys.argv[1:]
    region = None
    if args[0] == "--region":
        region = [int(v) for v in args[1:5]]
        args = args[5:]
    if args[0] == "--stats":
        args = args[1:]
        w, h, c, d = read_png(args[0])
        print(f"{args[0]}: {w}x{h} mean luma {luma(d, c, 0, 0, w, h, w):.2f}")
        return
    wa, ha, ca, da = read_png(args[0])
    wb, hb, cb, db = read_png(args[1])
    assert (wa, ha) == (wb, hb), (wa, ha, wb, hb)
    x0, y0, x1, y1 = region if region else (0, 0, wa, ha)
    tot = 0
    changed = 0
    worst = 0
    n = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            ia = (y * wa + x) * ca
            ib = (y * wb + x) * cb
            d3 = max(abs(da[ia + k] - db[ib + k]) for k in range(3))
            tot += d3
            worst = max(worst, d3)
            changed += d3 > 8
            n += 1
    print(f"{args[0]} vs {args[1]}: {n} px  mean abs diff {tot / max(n, 1):.3f}  changed>8 {changed} ({100.0 * changed / max(n, 1):.2f}%)  max {worst}")
    print(f"   luma {luma(da, ca, x0, y0, x1, y1, wa):.2f} vs {luma(db, cb, x0, y0, x1, y1, wb):.2f}")


main()
