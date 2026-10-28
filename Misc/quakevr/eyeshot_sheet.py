# eyeshot_sheet.py -- a contact sheet of eyeshots (pure Python: no PIL or numpy needed), e.g. the shove test's (vr_agrunt_test
# 84; ROUND22.md, "Armed humanoids' shove"): each image cropped (its middle cx,cy, side size px), shrunk k times. Usage:
#   python Misc/quakevr/eyeshot_sheet.py <out.png> <cols> <k> <cx,cy,size> <img1.png> [<img2.png> ...]
import struct, sys, zlib

def read_png(path):
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    pos, idat, w = 8, b"", 0
    while pos < len(data):
        n, = struct.unpack(">I", data[pos:pos + 4]); kind = data[pos + 4:pos + 8]; body = data[pos + 8:pos + 8 + n]
        if kind == b"IHDR":
            w, h, depth, ctype, _, _, inter = struct.unpack(">IIBBBBB", body)
            assert depth == 8 and inter == 0 and ctype in (2, 6)
            bpp = 3 if ctype == 2 else 4
        elif kind == b"IDAT":
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    stride = w * bpp
    rows, prev, i = [], bytearray(stride), 0
    for y in range(h):
        f = raw[i]; line = bytearray(raw[i + 1:i + 1 + stride]); i += 1 + stride
        if f == 1:
            for x in range(bpp, stride): line[x] = (line[x] + line[x - bpp]) & 255
        elif f == 2:
            for x in range(stride): line[x] = (line[x] + prev[x]) & 255
        elif f == 3:
            for x in range(stride): line[x] = (line[x] + ((line[x - bpp] if x >= bpp else 0) + prev[x]) // 2) & 255
        elif f == 4:
            for x in range(stride):
                a = line[x - bpp] if x >= bpp else 0; b = prev[x]; c = prev[x - bpp] if x >= bpp else 0
                p = a + b - c; pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line); prev = line
    return w, h, bpp, rows

def write_png(path, w, h, rows):
    raw = b"".join(b"\x00" + bytes(r) for r in rows)
    def chunk(k, b): return struct.pack(">I", len(b)) + k + b + struct.pack(">I", zlib.crc32(k + b) & 0xffffffff)
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                           chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))

out, cols, k = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
cx, cy, size = (int(v) for v in sys.argv[4].split(","))  # the crop: its middle and its side (px)
imgs = []
for p in sys.argv[5:]:
    w, h, bpp, rows = read_png(p)
    x0, y0 = max(0, cx - size // 2), max(0, cy - size // 2)
    sw, sh = min(size, w - x0) // k, min(size, h - y0) // k
    small = [bytearray(sw * 3) for _ in range(sh)]
    for y in range(sh):
        src = rows[y0 + y * k]
        for x in range(sw):
            o = (x0 + x * k) * bpp
            small[y][x * 3:x * 3 + 3] = src[o:o + 3]
    imgs.append((sw, sh, small))
cw, ch = imgs[0][0], imgs[0][1]
nrows = (len(imgs) + cols - 1) // cols
W, H = cols * (cw + 4), nrows * (ch + 4)
sheet = [bytearray(W * 3) for _ in range(H)]
for i, (sw, sh, small) in enumerate(imgs):
    ox, oy = (i % cols) * (cw + 4), (i // cols) * (ch + 4)
    for y in range(min(sh, ch)):
        sheet[oy + y][ox * 3:ox * 3 + min(sw, cw) * 3] = small[y][:min(sw, cw) * 3]
write_png(out, W, H, sheet)
print("sheet", out, W, "x", H, "from", len(imgs), "images of", cw, "x", ch)
