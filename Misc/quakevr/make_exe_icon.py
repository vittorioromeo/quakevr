"""Builds the engine's Windows icon (Windows/QuakeVR.ico) from the square Quake VR logo.

Usage: python make_exe_icon.py <logo_square.(webp|png)> [out.ico] [--preview preview.png]

Sizes 16..256. Each is a premultiplied-alpha Lanczos downscale of the full logo (alpha kept as the
downscale gives it, never thresholded); sizes up to 64 get a mild unsharp mask on colour only, so the
small taskbar / title-bar icons stay crisp. Sizes below 256 are stored as 32-bit BMP entries (every
Windows icon API reads them), 256 as PNG (the usual Vista+ layout). Needs Pillow with WebP support.
"""
import io
import struct
import sys

from PIL import Image, ImageFilter

SIZES = (16, 24, 32, 48, 64, 128, 256)
# (radius, percent, threshold) per size: stronger at the smallest sizes, none from 128 up
SHARPEN = {16: (0.6, 80, 1), 24: (0.6, 70, 1), 32: (0.7, 60, 1), 48: (0.8, 50, 1), 64: (0.8, 40, 1)}


def make_frame(src, size):
    # Downscale in steps of at most 2x down to 4x the target, then Lanczos to size (Pillow
    # premultiplies RGBA while resampling, so transparent edges don't darken).
    im = src
    while im.width >= size * 8:
        im = im.resize((im.width // 2, im.height // 2), Image.Resampling.LANCZOS)
    im = im.resize((size, size), Image.Resampling.LANCZOS)
    if size in SHARPEN:
        r, p, t = SHARPEN[size]
        alpha = im.getchannel("A")
        rgb = im.convert("RGB").filter(ImageFilter.UnsharpMask(radius=r, percent=p, threshold=t))
        im = rgb.convert("RGBA")
        im.putalpha(alpha)
    return im


def bmp_entry(im):
    w, h = im.size
    px = im.tobytes("raw", "BGRA")
    rows = [px[y * w * 4:(y + 1) * w * 4] for y in range(h)]
    xor = b"".join(reversed(rows))  # bottom-up
    mask_stride = ((w + 31) // 32) * 4
    alpha = im.getchannel("A").tobytes()
    mask_rows = []
    for y in range(h - 1, -1, -1):
        row = bytearray(mask_stride)
        for x in range(w):
            if alpha[y * w + x] == 0:
                row[x >> 3] |= 0x80 >> (x & 7)
        mask_rows.append(bytes(row))
    header = struct.pack("<IiiHHIIiiII", 40, w, h * 2, 1, 32, 0, len(xor) + len(mask_rows) * mask_stride, 0, 0, 0, 0)
    return header + xor + b"".join(mask_rows)


def png_entry(im):
    buf = io.BytesIO()
    im.save(buf, "PNG", optimize=True)
    return buf.getvalue()


def write_ico(frames, path):
    blobs = [png_entry(f) if f.width >= 256 else bmp_entry(f) for f in frames]
    out = struct.pack("<HHH", 0, 1, len(frames))
    offset = 6 + 16 * len(frames)
    for f, b in zip(frames, blobs):
        dim = 0 if f.width >= 256 else f.width
        out += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(b), offset)
        offset += len(b)
    with open(path, "wb") as fh:
        fh.write(out + b"".join(blobs))


def main():
    args = sys.argv[1:]
    preview = None
    if "--preview" in args:
        i = args.index("--preview")
        preview = args[i + 1]
        del args[i:i + 2]
    src = Image.open(args[0]).convert("RGBA")
    if src.width != src.height:
        side = max(src.size)
        sq = Image.new("RGBA", (side, side), (0, 0, 0, 0))
        sq.paste(src, ((side - src.width) // 2, (side - src.height) // 2))
        src = sq
    out = args[1] if len(args) > 1 else "Windows/QuakeVR.ico"
    frames = [make_frame(src, s) for s in SIZES]
    write_ico(frames, out)
    print("wrote", out, [f.size[0] for f in frames])
    if preview:
        # each size at 1x and 4x nearest-neighbour, on a light and a dark background
        tiles = []
        for bg in ((240, 240, 240, 255), (32, 32, 32, 255)):
            row = Image.new("RGBA", (sum(s + 8 for s in SIZES) + sum(s * 3 + 8 for s in SIZES[:3]), 256 + 8), bg)
            x = 4
            for f in frames:
                row.alpha_composite(f, (x, 4)); x += f.width + 8
            for f in frames[:3]:
                big = f.resize((f.width * 3, f.height * 3), Image.Resampling.NEAREST)
                row.alpha_composite(big, (x, 4)); x += big.width + 8
            tiles.append(row)
        sheet = Image.new("RGBA", (tiles[0].width, tiles[0].height * 2))
        sheet.paste(tiles[0], (0, 0)); sheet.paste(tiles[1], (0, tiles[0].height))
        sheet.convert("RGB").save(preview)


if __name__ == "__main__":
    main()
