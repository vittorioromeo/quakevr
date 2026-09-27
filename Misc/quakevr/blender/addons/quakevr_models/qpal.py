# qpal.py -- Quake's palette, colours to palette indices, and the body's .tga skins. Plain Python.

import struct

# id's gfx/palette.lmp. Indices 224..255 are fullbright: they glow in the dark (the sights, screens, lights).
PALETTE_HEX = (
    "0000000f0f0f1f1f1f2f2f2f3f3f3f4b4b4b5b5b5b6b6b6b7b7b7b8b8b8b9b9b9babababbbbbbbcbcbcbdbdbdbebebeb"
    "0f0b07170f0b1f170b271b0f2f2313372b173f2f174b371b533b1b5b431f634b1f6b531f73571f7b5f238367238f6f23"
    "0b0b0f13131b1b1b272727332f2f3f37374b3f3f574747674f4f735b5b7f63638b6b6b977373a37b7baf8383bb8b8bcb"
    "0000000707000b0b001313001b1b002323002b2b072f2f073737073f3f074747074b4b0b53530b5b5b0b63630b6b6b0f"
    "0700000f00001700001f00002700002f00003700003f00004700004f00005700005f00006700006f00007700007f0000"
    "1313001b1b002323002f2b00372f004337004b3b075743075f47076b4b0b77530f8357138b5b13975f1ba3631faf6723"
    "2313072f170b3b1f0f4b2313572b17632f1f7337237f3b2b8f43339f4f33af632fbf772fcf8f2bdfab27efcb1ffff31b"
    "0b07001b13002b230f372b1347331b533723633f2b6f47337f533f8b5f479b6b53a77b5fb7876bc3937bd3a38be3b397"
    "ab8ba39f7f979373878b677b7f5b6f7753636b4b575f3f4b5737434b2f3743272f371f232b171b231313170b0b0f0707"
    "bb739faf6b8fa35f839757778b4f6b7f4b5f7343536b3b4b5f333f532b3747232b3b1f232f171b231313170b0b0f0707"
    "dbc3bbcbb3a7bfa39baf978ba3877b977b6f876f5f7b63536b57475f4b3b533f33433327372b1f271f171b130f0f0b07"
    "6f837b677b6f5f7367576b5f4f6357475b4f3f5347374b3f2f43372b3b2f2333271f2b1f1723170f1b130b130b070b07"
    "fff31befdf17dbcb13cbb70fbba70fab970b9b83078b73077b63076b53005b47004b37003b2b002b1f001b0f000b0700"
    "0000ff0b0bef1313df1b1bcf2323bf2b2baf2f2f9f2f2f8f2f2f7f2f2f6f2f2f5f2b2b4f23233f1b1b2f13131f0b0b0f"
    "2b00003b00004b07005f07006f0f007f1707931f07a3270bb7330fc34b1bcf632bdb7f3be3974fe7ab5fefbf77f7d38b"
    "a77b3bb79b37c7c337e7e3577fbfffabe7ffd7ffff6700008b0000b30000d70000ff0000fff393fff7c7ffffff9f5b53"
)
FULLBRIGHT = 224

PALETTE = [tuple(bytes.fromhex("".join(PALETTE_HEX))[3 * i:3 * i + 3]) for i in range(256)]


class Quantizer:
    """Colours to palette indices. A texel whose colour is still its old index's keeps that index (so an unedited skin
    comes back byte for byte). Any other colour becomes the nearest palette colour: among the ordinary ones (0..223),
    or, where the old texel glowed (a sight, a screen), among all 256, a glowing one winning a tie (so a glowing texel
    can be repainted in another glow, or made ordinary, and an ordinary one never starts glowing)."""

    def __init__(self):
        self.cache = {}

    def nearest(self, rgb, glow):
        key = (rgb, glow)
        c = self.cache.get(key)
        if c is None:
            r, g, b = rgb
            best, c = 1 << 30, 0
            order = list(range(FULLBRIGHT, 256)) + list(range(FULLBRIGHT)) if glow else range(FULLBRIGHT)
            for i in order:
                p = PALETTE[i]
                d = (p[0] - r) ** 2 + (p[1] - g) ** 2 + (p[2] - b) ** 2
                if d < best:
                    best, c = d, i
            self.cache[key] = c
        return c

    def indices(self, rgb, old=None):
        """rgb: a flat list of (r, g, b) 0..255; old: the indices they came from (same length) or None."""
        out = bytearray(len(rgb))
        for i, c in enumerate(rgb):
            o = old[i] if old is not None else None
            if o is not None and PALETTE[o] == c:
                out[i] = o
            else:
                out[i] = self.nearest(c, o is not None and o >= FULLBRIGHT)
        return out


# ----------------------------------------------------------------------------
# TGA (the body's skins: uncompressed true colour, 24 or 32 bits)


class TgaError(Exception):
    pass


def read_tga(path):
    """(header bytes, width, height, [(r, g, b)] rows top to bottom, alpha list or None, top_first)."""
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < 18:
        raise TgaError("%s: too short for a TGA" % path)
    idlen, cmap, itype = data[0], data[1], data[2]
    w, h = struct.unpack_from("<HH", data, 12)
    bpp, desc = data[16], data[17]
    if itype != 2 or cmap != 0 or bpp not in (24, 32):
        raise TgaError("%s: not an uncompressed 24- or 32-bit TGA (type %d, %d bits); save it uncompressed" %
                       (path, itype, bpp))
    off = 18 + idlen
    n = bpp // 8
    if len(data) < off + w * h * n:
        raise TgaError("%s: the file ends early" % path)
    top_first = bool(desc & 0x20)
    rows = []
    alpha = [] if n == 4 else None
    for y in range(h):
        src = y if top_first else h - 1 - y
        o = off + src * w * n
        for x in range(w):
            b, g, r = data[o + x * n:o + x * n + 3]
            rows.append((r, g, b))
            if alpha is not None:
                alpha.append(data[o + x * n + 3])
    return data[:off], w, h, rows, alpha, top_first


def write_tga(path, header, w, h, rgb, alpha=None, top_first=False):
    """header: the original's first bytes (its type, size, bits and origin are kept), rgb rows top to bottom."""
    n = header[16] // 8
    out = bytearray(header)
    for y in range(h):
        src = y if top_first else h - 1 - y
        for x in range(w):
            r, g, b = rgb[src * w + x]
            out += bytes((b, g, r))
            if n == 4:
                out.append(alpha[src * w + x] if alpha is not None else 255)
    with open(path, "wb") as f:
        f.write(out)
