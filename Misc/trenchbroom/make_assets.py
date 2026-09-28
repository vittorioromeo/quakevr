#!/usr/bin/env python3
"""Makes the TrenchBroom assets that are ours: the game's icon and a small texture WAD.

    python Misc/trenchbroom/make_assets.py [--quake <Quake folder>]

- Misc/trenchbroom/QuakeVR/Icon.png: the icon in TrenchBroom's game list (32x32).
- quakevr/wads/quakevr_dev.wad: the example map's textures, drawn here (grids at Quake VR's scale, trims, panels, a
  glowing strip, and the compiler's special textures). Neither id's textures nor any other game's data: a map made
  of them can be committed and shipped, and so can its .bsp (which embeds its textures).

The WAD's pixels are indices into Quake's palette, which is id's data: it is read from the player's own
id1/pak0.pak (the colours are matched to it), never copied into the repository.
"""

import argparse
import math
import os
import random
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
ICON = os.path.join(HERE, "QuakeVR", "Icon.png")
WAD = os.path.join(ROOT, "quakevr", "wads", "quakevr_dev.wad")
DEFAULT_QUAKE = r"C:\Program Files (x86)\Steam\steamapps\common\Quake"


def read_palette(quake):
    for name in os.listdir(os.path.join(quake, "id1")):
        if name.lower() != "pak0.pak":
            continue
        with open(os.path.join(quake, "id1", name), "rb") as f:
            magic, off, size = struct.unpack("<4sii", f.read(12))
            f.seek(off)
            d = f.read(size)
            for i in range(0, size, 64):
                n = d[i:i + 56].split(b"\0")[0].decode("latin1").lower()
                if n == "gfx/palette.lmp":
                    pos, ln = struct.unpack("<ii", d[i + 56:i + 64])
                    f.seek(pos)
                    p = f.read(ln)
                    return [tuple(p[j:j + 3]) for j in range(0, 768, 3)]
    sys.exit("no gfx/palette.lmp in %s/id1/pak0.pak" % quake)


class Painter:
    """Draws in RGB, then maps to the palette: 0..223 for lit colours, 224..254 for fullbright ones."""

    def __init__(self, palette):
        self.pal = palette

    def index(self, rgb, fullbright=False):
        lo, hi = (224, 255) if fullbright else (0, 224)
        best, bd = 0, 1e9
        for i in range(lo, hi):
            p = self.pal[i]
            d = (p[0] - rgb[0]) ** 2 * 0.3 + (p[1] - rgb[1]) ** 2 * 0.59 + (p[2] - rgb[2]) ** 2 * 0.11
            if d < bd:
                best, bd = i, d
        return best


def clamp(v):
    return max(0, min(255, int(round(v))))


def shade(c, k):
    return tuple(clamp(x * k) for x in c)


def noise_field(w, h, seed, amp):
    rnd = random.Random(seed)
    return [[1.0 + (rnd.random() - 0.5) * amp for _ in range(w)] for _ in range(h)]


def tex_grid(w, h, base, line, major, step=16, major_step=64, seed=1, amp=0.10, bevel=True):
    """A surface with a line every step units and a stronger one every major_step (the scale grid)."""
    n = noise_field(w, h, seed, amp)
    img = []
    for y in range(h):
        row = []
        for x in range(w):
            c = shade(base, n[y][x])
            if x % major_step == 0 or y % major_step == 0:
                c = major
            elif x % step == 0 or y % step == 0:
                c = line
            elif bevel and (x % major_step == 1 or y % major_step == 1):
                c = shade(c, 1.12)
            row.append((c, False))
        img.append(row)
    return img


def tex_trim(w, h, base, stripe, seed=2):
    """Hazard trim: diagonal stripes between two dark edges."""
    n = noise_field(w, h, seed, 0.08)
    img = []
    for y in range(h):
        row = []
        for x in range(w):
            if y < 2 or y >= h - 2:
                c = (40, 38, 36)
            elif y == 2 or y == h - 3:
                c = (90, 86, 80)
            else:
                c = stripe if ((x + y) // 8) % 2 == 0 else base
                c = shade(c, n[y][x])
            row.append((c, False))
        img.append(row)
    return img


def tex_panel(w, h, face, frame, seed=3):
    n = noise_field(w, h, seed, 0.08)
    img = []
    for y in range(h):
        row = []
        for x in range(w):
            e = min(x, y, w - 1 - x, h - 1 - y)
            if e < 3:
                c = shade(frame, 0.7 + 0.1 * e)
            elif e == 3:
                c = (30, 28, 26)
            else:
                c = shade(face, n[y][x])
                # rivets
                for cx, cy in ((7, 7), (w - 8, 7), (7, h - 8), (w - 8, h - 8)):
                    if (x - cx) ** 2 + (y - cy) ** 2 <= 2:
                        c = shade(frame, 1.2)
            row.append((c, False))
        img.append(row)
    return img


def tex_light(w, h, glow, rim):
    """A glowing strip: fullbright in the middle, a lit metal rim."""
    img = []
    for y in range(h):
        row = []
        for x in range(w):
            e = min(x, y, w - 1 - x, h - 1 - y)
            if e < 3:
                row.append((shade(rim, 0.8 + 0.1 * e), False))
            else:
                k = 0.85 + 0.15 * math.cos((y - h / 2) / h * math.pi)
                row.append((shade(glow, k), True))
        img.append(row)
    return img


def tex_button(w, h, face):
    img = []
    for y in range(h):
        row = []
        for x in range(w):
            d = math.hypot(x - (w - 1) / 2, y - (h - 1) / 2)
            if d < w * 0.30:
                row.append((shade(face, 1.0 - d / w), True))
            elif d < w * 0.36:
                row.append(((50, 46, 42), False))
            else:
                row.append((shade((110, 104, 96), 1.0 - 0.3 * (d / w)), False))
        img.append(row)
    return img


def tex_special(w, h, a, b, label_rows=None):
    """Checkers for the compiler's special textures (never drawn in the game)."""
    img = []
    for y in range(h):
        row = []
        for x in range(w):
            row.append((a if ((x // 8) + (y // 8)) % 2 == 0 else b, False))
        img.append(row)
    return img


def tex_water(w, h, seed=5):
    img = []
    for y in range(h):
        row = []
        for x in range(w):
            v = 0.5 + 0.25 * math.sin(x / w * 2 * math.pi * 2 + math.sin(y / h * 2 * math.pi) * 1.5) \
                + 0.25 * math.sin(y / h * 2 * math.pi * 3)
            row.append(((clamp(20 + 30 * v), clamp(60 + 50 * v), clamp(70 + 60 * v)), False))
        img.append(row)
    return img


TEXTURES = [
    # name, builder
    ("qvr_floor", lambda: tex_grid(64, 64, (74, 72, 70), (60, 58, 56), (96, 88, 70), seed=11)),
    ("qvr_wall", lambda: tex_grid(64, 64, (98, 102, 108), (84, 88, 94), (70, 74, 80), seed=12, amp=0.06)),
    ("qvr_ceiling", lambda: tex_grid(64, 64, (56, 58, 62), (46, 48, 52), (40, 42, 46), seed=13, amp=0.06)),
    ("qvr_trim", lambda: tex_trim(64, 16, (40, 38, 36), (200, 150, 30))),
    ("qvr_panel", lambda: tex_panel(64, 64, (60, 64, 70), (150, 110, 60))),
    ("qvr_light", lambda: tex_light(32, 32, (255, 240, 200), (120, 116, 110))),
    ("qvr_button", lambda: tex_button(32, 32, (255, 90, 40))),
    ("*qvr_water", lambda: tex_water(64, 64)),
    ("clip", lambda: tex_special(64, 64, (120, 60, 140), (90, 40, 110))),
    ("skip", lambda: tex_special(64, 64, (200, 80, 160), (160, 60, 130))),
    ("trigger", lambda: tex_special(64, 64, (220, 130, 40), (180, 100, 30))),
    ("hint", lambda: tex_special(64, 64, (60, 140, 200), (40, 110, 170))),
    ("origin", lambda: tex_special(64, 64, (60, 160, 90), (40, 120, 70))),
]


def mips(indices, w, h, pal):
    """Mip levels 0..3 by averaging colours (fullbright pixels stay fullbright)."""
    levels = [bytes(indices)]
    cur = [[pal[indices[y * w + x]] for x in range(w)] for y in range(h)]
    fb = [[indices[y * w + x] >= 224 for x in range(w)] for y in range(h)]
    cw, ch = w, h
    for _ in range(3):
        nw, nh = cw // 2, ch // 2
        nxt, nfb, out = [], [], []
        for y in range(nh):
            row, frow = [], []
            for x in range(nw):
                px = [cur[2 * y + j][2 * x + i] for j in (0, 1) for i in (0, 1)]
                f = sum(fb[2 * y + j][2 * x + i] for j in (0, 1) for i in (0, 1)) >= 2
                row.append(tuple(sum(p[k] for p in px) / 4 for k in range(3)))
                frow.append(f)
            nxt.append(row)
            nfb.append(frow)
        cur, fb, cw, ch = nxt, nfb, nw, nh
        levels.append((cur, fb))
    return levels


def build_wad(pal):
    painter = Painter(pal)
    cache = {}

    def idx(rgb, full):
        key = (rgb, full)
        if key not in cache:
            cache[key] = painter.index(rgb, full)
        return cache[key]

    lumps = []
    for name, make in TEXTURES:
        img = make()
        h, w = len(img), len(img[0])
        base = [idx(c, full) for row in img for (c, full) in row]
        data = [bytes(base)]
        levels = mips(base, w, h, pal)
        for cur, fb in levels[1:]:
            data.append(bytes(idx(tuple(clamp(v) for v in c), f) for row, frow in zip(cur, fb) for c, f in zip(row, frow)))
        header = struct.pack("<16sII", name.encode("latin1"), w, h)
        offs, pos = [], 40
        for d in data:
            offs.append(pos)
            pos += len(d)
        lumps.append((name, header + struct.pack("<4I", *offs) + b"".join(data)))

    out = bytearray(b"WAD2" + struct.pack("<ii", len(lumps), 0))
    entries = []
    for name, blob in lumps:
        entries.append((len(out), len(blob), name))
        out += blob
        while len(out) % 4:
            out += b"\0"
    dir_ofs = len(out)
    for pos, size, name in entries:
        out += struct.pack("<iiibbh16s", pos, size, size, 0x44, 0, 0, name.encode("latin1"))
    struct.pack_into("<i", out, 8, dir_ofs)
    return bytes(out)


def build_icon():
    from PIL import Image, ImageDraw

    s = 256
    im = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((8, 8, s - 8, s - 8), radius=48, fill=(52, 30, 20, 255), outline=(150, 90, 40, 255), width=10)
    # A headset: the visor, its strap and two lenses.
    d.rounded_rectangle((34, 86, s - 34, 182), radius=34, fill=(196, 120, 48, 255))
    d.rectangle((20, 118, 40, 142), fill=(150, 90, 40, 255))
    d.rectangle((s - 40, 118, s - 20, 142), fill=(150, 90, 40, 255))
    d.ellipse((62, 104, 120, 162), fill=(40, 22, 14, 255))
    d.ellipse((s - 120, 104, s - 62, 162), fill=(40, 22, 14, 255))
    d.polygon([(112, 182), (144, 182), (128, 158)], fill=(52, 30, 20, 255))
    im.resize((32, 32), Image.LANCZOS).save(ICON)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--quake", default=DEFAULT_QUAKE, help="the Quake folder (for id1/pak0.pak's palette)")
    args = ap.parse_args()
    pal = read_palette(args.quake)
    os.makedirs(os.path.dirname(WAD), exist_ok=True)
    with open(WAD, "wb") as f:
        f.write(build_wad(pal))
    print("wrote %s (%d textures)" % (os.path.relpath(WAD, ROOT), len(TEXTURES)))
    build_icon()
    print("wrote %s" % os.path.relpath(ICON, ROOT))


if __name__ == "__main__":
    main()
