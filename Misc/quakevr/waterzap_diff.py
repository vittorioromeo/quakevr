#!/usr/bin/env python3
"""waterzap_diff.py -- what the electrified water changes on screen (waterzap_test.sh; no image libraries here).

  waterzap_diff.py <with.png> <without.png> [--out side_by_side.png]

Prints `changed=<n> blue=<m> of <total>`: pixels that differ by more than THRESHOLD on a channel between the two
frames, and of those the ones made brighter and bluer: the arcs' colour, not a splash's or a smoke's moving. With --out:
the two frames and the changed pixels (white) side by side.
"""
import struct
import sys
import zlib

THRESHOLD = 40


def read_png(path):
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(path + ": not a PNG")
    pos = 8
    idat = []
    width = height = channels = 0
    while pos < len(data):
        size, kind = struct.unpack(">I4s", data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + size]
        pos += 12 + size
        if kind == b"IHDR":
            width, height, depth, color = struct.unpack(">IIBB", chunk[:10])
            if depth != 8 or color not in (2, 6):
                raise SystemExit(path + ": only 8-bit RGB or RGBA")
            channels = 3 if color == 2 else 4
        elif kind == b"IDAT":
            idat.append(chunk)
    raw = zlib.decompress(b"".join(idat))
    stride = width * channels
    rows = []
    prev = bytearray(stride)
    at = 0
    for _ in range(height):
        kind = raw[at]
        line = bytearray(raw[at + 1:at + 1 + stride])
        at += 1 + stride
        if kind == 1:
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 255
        elif kind == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 255
        elif kind == 3:
            for i in range(stride):
                left = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 255
        elif kind == 4:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                b = prev[i]
                c = prev[i - channels] if i >= channels else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else (b if pb <= pc else c))) & 255
        rows.append(line)
        prev = line
    return width, height, channels, rows


def write_png(path, width, height, rows):
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))

    raw = b"".join(b"\x00" + bytes(r) for r in rows)
    out = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    out += chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b"")
    open(path, "wb").write(out)


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
    changed = blue = 0
    mask = []
    for y in range(h):
        ra, rb = a[y], b[y]
        line = bytearray(w * 3)
        for x in range(w):
            pa, pb = x * ca, x * cb
            dr, dg, db = ra[pa] - rb[pb], ra[pa + 1] - rb[pb + 1], ra[pa + 2] - rb[pb + 2]
            if max(abs(dr), abs(dg), abs(db)) > THRESHOLD:
                changed += 1
                line[x * 3:x * 3 + 3] = b"\x60\x60\x60"
                if db > THRESHOLD and db >= dr:
                    blue += 1
                    line[x * 3:x * 3 + 3] = b"\xff\xff\xff"
        mask.append(line)
    print("changed=%d blue=%d of %d" % (changed, blue, w * h))
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
