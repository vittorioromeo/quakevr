#!/usr/bin/env python3
"""pixdiff_ab.py <before.png> [after.png] - the mean and peak absolute difference between two frames
(per channel), so a visual claim can be checked by numbers instead of looking at both. With one file it
compares that file's left and right halves (a composed before/after pair, as run.sh -Out writes).

Only the standard library: a small PNG reader (8-bit, non-interlaced; RGB, RGBA, gray or palette).
  python Misc/quakevr/pixdiff_ab.py C:/OHWorkspace/qvr-kit/scratch/ambientlight_ab.png
"""
import struct
import sys
import zlib


def read_png(path):
    """(width, height, rows of RGB bytes)."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    pos, idat, plte = 8, bytearray(), b""
    width = height = color = None
    while pos + 8 <= len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        kind, chunk = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + length]
        if kind == b"IHDR":
            width, height, depth, color, comp, filt, interlace = struct.unpack(">IIBBBBB", chunk)
            if depth != 8 or interlace != 0 or comp != 0 or filt != 0:
                raise ValueError(f"unsupported PNG (depth {depth}, colour {color})")
        elif kind == b"PLTE":
            plte = chunk
        elif kind == b"IDAT":
            idat += chunk
        elif kind == b"IEND":
            break
        pos += 12 + length
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color]
    raw = zlib.decompress(bytes(idat))
    stride = width * channels
    out = bytearray()
    previous = bytearray(stride)
    i = 0
    for _ in range(height):
        ft, line = raw[i], bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        if ft == 1:
            for x in range(channels, stride):
                line[x] = (line[x] + line[x - channels]) & 0xFF
        elif ft == 2:
            for x in range(stride):
                line[x] = (line[x] + previous[x]) & 0xFF
        elif ft == 3:
            for x in range(stride):
                left = line[x - channels] if x >= channels else 0
                line[x] = (line[x] + ((left + previous[x]) >> 1)) & 0xFF
        elif ft == 4:
            for x in range(stride):
                a, b = previous[x], line[x - channels] if x >= channels else 0
                c = previous[x - channels] if x >= channels else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if (pa <= pb and pa <= pc) else (b if pb <= pc else c))) & 0xFF
        previous = line  # the filters work on the row as stored, before it is made RGB
        if color == 2:
            out += line
        elif color == 6:
            for x in range(width):
                out += bytes(line[4 * x:4 * x + 3])
        elif color == 3:
            for x in range(width):
                out += plte[line[x] * 3:line[x] * 3 + 3]
        else: # 0 (gray) and 4 (gray and alpha): the grey as a colour
            for x in range(width):
                g = line[channels * x]
                out += bytes((g, g, g))
    return width, height, bytes(out)


def main():
    if len(sys.argv) > 2:
        a, b = read_png(sys.argv[1]), read_png(sys.argv[2])
        if a[:2] != b[:2]:
            raise ValueError(f"different sizes: {a[:2]} and {b[:2]}")
        width, height, stride = a[0], a[1], a[0] * 3
        pa, pb, off = a[2], b[2], 0
        desc = f"{width}x{height}, two frames"
    else:
        full, height, pixels = read_png(sys.argv[1])
        width, stride = full // 2, full * 3
        pa = pb = pixels
        off = 3 * (full - width) # the same pixel in the composed pair's right half
        desc = f"{full}x{height}: its halves {width}x{height}"
    total = peak = 0
    luma_a = luma_b = 0.0
    dark_a = dark_b = 0
    count = 0
    for y in range(height):
        base = y * stride
        for x in range(width):
            ia = base + 3 * x
            u, v = pa[ia:ia + 3], pb[ia + off:ia + off + 3]
            d = [abs(u[k] - v[k]) for k in range(3)]
            total += sum(d)
            peak = max(peak, max(d))
            la, lb = (u[0] + u[1] + u[2]) / 3.0, (v[0] + v[1] + v[2]) / 3.0
            luma_a += la
            luma_b += lb
            dark_a += 1 if la < 40 else 0
            dark_b += 1 if lb < 40 else 0
            count += 1
    print(f"{desc}: mean absolute difference per channel {total / count / 3.0:.2f}, peak {peak}")
    print(f"mean luma before {luma_a / count:.1f}, after {luma_b / count:.1f}; pixels under luma 40: {dark_a} -> {dark_b}")


if __name__ == "__main__":
    main()
