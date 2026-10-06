# quakeimage.py -- reading the image files Quake VR's engine reads (Image_LoadImage: .png, .tga, .jpg), for the
# scripts here (relight_maps.py: the glow images of replacement textures, QRP's <name>_luma and <name>_glow, and the
# external material maps' glow images, quakevr/textures_quetoo/<name>_luma.jpg). Python's standard library only.
#
# read_rgb(data, ext) gives (width, height, rgb), rgb a bytes object of 3 bytes a pixel (alpha dropped), or None for
# what it cannot read: PNG of any colour type at 8 or 16 bits (and palette or grey at 1, 2 and 4 bits), not interlaced;
# uncompressed and RLE true-colour and grey TGA; baseline and extended sequential JPEG (Huffman), not progressive.
# None also says why (`last_error`).

import struct
import zlib
import math

last_error = ""


def _fail(why):
    global last_error
    last_error = why
    return None


# --- TGA -------------------------------------------------------------------------------------------------------------


def read_tga(data):
    try:
        idlen, cmap, kind = data[0], data[1], data[2]
        width, height, bits = struct.unpack_from("<HHB", data, 12)
    except (IndexError, struct.error):
        return _fail("truncated TGA")
    if cmap or kind not in (2, 3, 10, 11) or bits not in (8, 24, 32) or (kind in (3, 11)) != (bits == 8):
        return _fail("TGA type %d at %d bits not read" % (kind, bits))
    size = bits // 8
    n = width * height
    pos = 18 + idlen
    if kind in (2, 3):
        raw = data[pos : pos + n * size]
    else:
        out = bytearray()
        while len(out) < n * size and pos < len(data):
            head = data[pos]
            pos += 1
            count = (head & 0x7F) + 1
            if head & 0x80:
                out += data[pos : pos + size] * count
                pos += size
            else:
                out += data[pos : pos + count * size]
                pos += count * size
        raw = bytes(out)
    if len(raw) < n * size:
        return _fail("truncated TGA")
    rgb = bytearray(n * 3)
    if size == 1:
        rgb[0::3] = rgb[1::3] = rgb[2::3] = raw[:n]
    else:
        rgb[0::3] = raw[2::size][:n]
        rgb[1::3] = raw[1::size][:n]
        rgb[2::3] = raw[0::size][:n]
    return width, height, bytes(rgb)


# --- PNG -------------------------------------------------------------------------------------------------------------

_MASK7F = {}
_MASK80 = {}


def _add_bytes(a, b):
    """a + b, byte by byte, modulo 256 (PNG's Up filter), on whole rows at once."""
    n = len(a)
    if n not in _MASK7F:
        _MASK7F[n] = int.from_bytes(b"\x7f" * n, "little")
        _MASK80[n] = int.from_bytes(b"\x80" * n, "little")
    x = int.from_bytes(a, "little")
    y = int.from_bytes(b, "little")
    s = ((x & _MASK7F[n]) + (y & _MASK7F[n])) ^ ((x ^ y) & _MASK80[n])
    return s.to_bytes(n, "little")


def _unfilter(raw, height, stride, bpp):
    out = bytearray(height * stride)
    prev = bytes(stride)
    pos = 0
    for y in range(height):
        kind = raw[pos]
        line = bytearray(raw[pos + 1 : pos + 1 + stride])
        pos += 1 + stride
        if kind == 1:
            for x in range(bpp, stride):
                line[x] = (line[x] + line[x - bpp]) & 255
        elif kind == 2:
            line = bytearray(_add_bytes(line, prev))
        elif kind == 3:
            for x in range(stride):
                left = line[x - bpp] if x >= bpp else 0
                line[x] = (line[x] + ((left + prev[x]) >> 1)) & 255
        elif kind == 4:
            for x in range(stride):
                if x >= bpp:
                    a, c = line[x - bpp], prev[x - bpp]
                else:
                    a = c = 0
                b = prev[x]
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        elif kind != 0:
            return None
        out[y * stride : (y + 1) * stride] = line
        prev = line
    return out


def read_png(data):
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        return _fail("not a PNG")
    pos = 8
    idat = []
    header = palette = None
    while pos + 8 <= len(data):
        length, kind = struct.unpack_from(">I4s", data, pos)
        body = data[pos + 8 : pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body[:13])
        elif kind == b"PLTE":
            palette = body
        elif kind == b"IDAT":
            idat.append(body)
        elif kind == b"IEND":
            break
    if not header:
        return _fail("PNG without a header")
    width, height, depth, colour, _, _, interlace = header
    if interlace:
        return _fail("interlaced PNG not read")
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(colour)
    if not channels or depth not in (1, 2, 4, 8, 16) or (depth < 8 and colour not in (0, 3)):
        return _fail("PNG colour type %d at %d bits not read" % (colour, depth))
    try:
        raw = zlib.decompress(b"".join(idat))
    except zlib.error:
        return _fail("PNG data damaged")
    bits = channels * depth
    stride = (width * bits + 7) // 8
    if len(raw) < height * (stride + 1):
        return _fail("truncated PNG")
    pix = _unfilter(raw, height, stride, max(1, bits // 8))
    if pix is None:
        return _fail("PNG filter not known")
    n = width * height
    if depth == 16:
        pix = pix[0::2]  # (the high bytes)
        depth = 8
    elif depth < 8:
        per = 8 // depth
        mask = (1 << depth) - 1
        rows = []
        for y in range(height):
            row = pix[y * stride : (y + 1) * stride]
            vals = [(byte >> (8 - depth * (k + 1))) & mask for byte in row for k in range(per)][:width]
            if colour == 0:
                vals = [v * 255 // mask for v in vals]
            rows.append(bytes(vals))
        pix = b"".join(rows)
    rgb = bytearray(n * 3)
    if colour == 3:
        if not palette:
            return _fail("PNG without a palette")
        pal = palette + bytes(768 - len(palette)) if len(palette) < 768 else palette
        for j in range(3):
            rgb[j::3] = bytes(pix[:n]).translate(bytes(pal[k * 3 + j] for k in range(256)))
    elif colour in (0, 4):
        rgb[0::3] = rgb[1::3] = rgb[2::3] = pix[0 : n * channels : channels]
    else:
        for j in range(3):
            rgb[j::3] = pix[j : n * channels : channels]
    return width, height, bytes(rgb)


# --- JPEG ------------------------------------------------------------------------------------------------------------

_ZIGZAG = (0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14,
           21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53,
           60, 61, 54, 47, 55, 62, 63)
# The IDCT's basis: _COS[x][u] = C(u) / 2 * cos((2x + 1) u pi / 16).
_COS = [[(math.sqrt(0.5) if u == 0 else 1.0) / 2 * math.cos((2 * x + 1) * u * math.pi / 16) for u in range(8)]
        for x in range(8)]


def _huffman(counts, symbols):
    """{(length, code): symbol} of a DHT table."""
    table = {}
    code = 0
    k = 0
    for length in range(1, 17):
        for _ in range(counts[length - 1]):
            table[(length, code)] = symbols[k]
            k += 1
            code += 1
        code <<= 1
    return table


class _Bits:
    def __init__(self, data, pos):
        self.data = data
        self.pos = pos
        self.acc = 0
        self.n = 0
        self.marker = None

    def bit(self):
        if self.n == 0:
            byte = 0
            if self.marker is None and self.pos < len(self.data):
                byte = self.data[self.pos]
                self.pos += 1
                if byte == 0xFF:
                    nxt = self.data[self.pos] if self.pos < len(self.data) else 0
                    if nxt == 0:
                        self.pos += 1
                    else:
                        self.marker = nxt  # (a restart or the end: zeros from here)
                        self.pos += 1
                        byte = 0
            self.acc = byte
            self.n = 8
        self.n -= 1
        return (self.acc >> self.n) & 1

    def bits(self, count):
        v = 0
        for _ in range(count):
            v = (v << 1) | self.bit()
        return v

    def decode(self, table):
        code = 0
        for length in range(1, 17):
            code = (code << 1) | self.bit()
            sym = table.get((length, code))
            if sym is not None:
                return sym
        raise ValueError("bad Huffman code")

    def restart(self):
        self.n = 0
        if self.marker is not None and 0xD0 <= self.marker <= 0xD7:
            self.marker = None
            return
        # (the marker not met yet: find it)
        while self.pos + 1 < len(self.data):
            if self.data[self.pos] == 0xFF and 0xD0 <= self.data[self.pos + 1] <= 0xD7:
                self.pos += 2
                break
            self.pos += 1
        self.marker = None


def _extend(v, t):
    return v - (1 << t) + 1 if t and v < (1 << (t - 1)) else v


def _idct(coef):
    """8x8 samples (0-255, row-major) of a block's dequantised coefficients (natural order)."""
    if not any(coef[1:]):
        v = int(round(coef[0] / 8 + 128))
        return [min(255, max(0, v))] * 64
    tmp = [0.0] * 64
    cos = _COS
    for y in range(8):  # rows: the vertical frequencies' sums
        row = coef[y * 8 : y * 8 + 8]
        if not any(row):
            continue
        for x in range(8):
            cx = cos[x]
            tmp[y * 8 + x] = sum(cx[u] * row[u] for u in range(8) if row[u])
    out = [0] * 64
    for x in range(8):
        col = [tmp[v * 8 + x] for v in range(8)]
        for y in range(8):
            cy = cos[y]
            val = int(round(sum(cy[v] * col[v] for v in range(8)) + 128))
            out[y * 8 + x] = 0 if val < 0 else 255 if val > 255 else val
    return out


def read_jpeg(data):
    if data[:2] != b"\xff\xd8":
        return _fail("not a JPEG")
    pos = 2
    quant = {}
    tables = {}
    frame = None
    restart = 0
    while pos + 4 <= len(data):
        if data[pos] != 0xFF:
            pos += 1
            continue
        marker = data[pos + 1]
        if marker in (0xD8, 0x01) or 0xD0 <= marker <= 0xD7:
            pos += 2
            continue
        if marker == 0xD9:
            break
        (length,) = struct.unpack_from(">H", data, pos + 2)
        body = data[pos + 4 : pos + 2 + length]
        pos += 2 + length
        if marker == 0xDB:
            k = 0
            while k < len(body):
                precision, ident = body[k] >> 4, body[k] & 15
                k += 1
                if precision:
                    vals = struct.unpack_from(">64H", body, k)
                    k += 128
                else:
                    vals = body[k : k + 64]
                    k += 64
                q = [0] * 64
                for i, v in enumerate(vals):
                    q[_ZIGZAG[i]] = v
                quant[ident] = q
        elif marker == 0xC4:
            k = 0
            while k < len(body):
                cls, ident = body[k] >> 4, body[k] & 15
                counts = body[k + 1 : k + 17]
                total = sum(counts)
                tables[(cls, ident)] = _huffman(counts, body[k + 17 : k + 17 + total])
                k += 17 + total
        elif marker in (0xC0, 0xC1):
            precision, height, width, ncomp = struct.unpack_from(">BHHB", body, 0)
            if precision != 8:
                return _fail("%d-bit JPEG not read" % precision)
            comps = []
            for i in range(ncomp):
                cid, sampling, tq = body[6 + i * 3], body[7 + i * 3], body[8 + i * 3]
                comps.append({"id": cid, "h": sampling >> 4, "v": sampling & 15, "q": tq})
            frame = (width, height, comps)
        elif 0xC2 <= marker <= 0xCF and marker not in (0xC4, 0xC8, 0xCC):
            return _fail("progressive or arithmetic-coded JPEG not read")
        elif marker == 0xDD:
            (restart,) = struct.unpack_from(">H", body, 0)
        elif marker == 0xDA:
            if not frame:
                return _fail("JPEG scan before its frame")
            return _jpeg_scan(data, pos, body, frame, quant, tables, restart)
    return _fail("JPEG without a scan")


def _jpeg_scan(data, pos, scan, frame, quant, tables, restart):
    width, height, comps = frame
    order = []
    for i in range(scan[0]):
        cid, sel = scan[1 + i * 2], scan[2 + i * 2]
        for c in comps:
            if c["id"] == cid:
                c["dc"], c["ac"] = tables.get((0, sel >> 4)), tables.get((1, sel & 15))
                order.append(c)
    if len(order) != len(comps):
        return _fail("JPEG with several scans not read")
    hmax = max(c["h"] for c in comps)
    vmax = max(c["v"] for c in comps)
    mcux = (width + 8 * hmax - 1) // (8 * hmax)
    mcuy = (height + 8 * vmax - 1) // (8 * vmax)
    planes = []
    for c in comps:
        c["w"] = mcux * c["h"] * 8
        c["plane"] = bytearray(c["w"] * mcuy * c["v"] * 8)
        c["pred"] = 0
        planes.append(c)
    bits = _Bits(data, pos)
    try:
        for my in range(mcuy):
            for mx in range(mcux):
                index = my * mcux + mx
                if restart and index and index % restart == 0:
                    bits.restart()
                    for c in comps:
                        c["pred"] = 0
                for c in order:
                    q = quant[c["q"]]
                    for by in range(c["v"]):
                        for bx in range(c["h"]):
                            coef = [0] * 64
                            t = bits.decode(c["dc"])
                            c["pred"] += _extend(bits.bits(t), t)
                            coef[0] = c["pred"] * q[0]
                            k = 1
                            while k < 64:
                                rs = bits.decode(c["ac"])
                                r, s = rs >> 4, rs & 15
                                if s == 0:
                                    if r != 15:
                                        break
                                    k += 16
                                    continue
                                k += r
                                if k > 63:
                                    break
                                z = _ZIGZAG[k]
                                coef[z] = _extend(bits.bits(s), s) * q[z]
                                k += 1
                            block = _idct(coef)
                            x0 = (mx * c["h"] + bx) * 8
                            y0 = (my * c["v"] + by) * 8
                            plane, w = c["plane"], c["w"]
                            for y in range(8):
                                plane[(y0 + y) * w + x0 : (y0 + y) * w + x0 + 8] = bytes(block[y * 8 : y * 8 + 8])
    except (ValueError, IndexError, KeyError, TypeError):
        return _fail("JPEG data damaged")

    def full(c):
        """The component's plane at the image's size (chroma repeated, as nearest-neighbour upsampling)."""
        sx, sy = hmax // c["h"], vmax // c["v"]
        rows = []
        for y in range(height):
            row = c["plane"][(y // sy) * c["w"] : (y // sy) * c["w"] + c["w"]]
            if sx > 1:
                row = bytes(v for v in row for _ in range(sx))
            rows.append(bytes(row[:width]))
        return b"".join(rows)

    n = width * height
    rgb = bytearray(n * 3)
    if len(comps) == 1:
        rgb[0::3] = rgb[1::3] = rgb[2::3] = full(comps[0])
        return width, height, bytes(rgb)
    y, cb, cr = (full(c) for c in comps[:3])
    out = rgb
    for i in range(n):
        yy, b, r = y[i], cb[i] - 128, cr[i] - 128
        rv = int(yy + 1.402 * r + 0.5)
        gv = int(yy - 0.344136 * b - 0.714136 * r + 0.5)
        bv = int(yy + 1.772 * b + 0.5)
        out[i * 3] = 0 if rv < 0 else 255 if rv > 255 else rv
        out[i * 3 + 1] = 0 if gv < 0 else 255 if gv > 255 else gv
        out[i * 3 + 2] = 0 if bv < 0 else 255 if bv > 255 else bv
    return width, height, bytes(rgb)


def read_rgb(data, ext):
    """(width, height, rgb bytes) of an image file's contents, by its extension (png, tga, jpg), or None."""
    ext = ext.lower().lstrip(".")
    try:
        if ext == "png":
            return read_png(data)
        if ext == "tga":
            return read_tga(data)
        if ext in ("jpg", "jpeg"):
            return read_jpeg(data)
    except (IndexError, struct.error, ValueError):
        return _fail("damaged %s" % ext)
    return _fail("%s not read" % ext)
