# make_bigfont.py -- writes Quake/vr/vr_bigfont_glyphs.inc: the main menu's lettering as a font (vr_bigfont.cpp).
#
#   python Misc/quakevr/make_bigfont.py --pak <id1/pak0.pak> [--preview out.png] ["text" ...]
#
# The letters are not shipped: the .inc only says where each one is in id's menu pictures (gfx/mainmenu.lmp,
# sp_menu.lmp, mp_menu.lmp, Quake 1.06), with a hash of each piece, and the engine cuts them from the player's own
# pak at run time (a piece whose hash differs, a mod's own picture, is not used: the menu keeps its picture then).
#
# A glyph is a list of operations into a cell CELL rows high, whose row 15 is the small capitals' bottom:
#   C(band, x0, x1, y0, y1, dx, dy, flip): copies the band's rows y0..y1 (columns x0..x1) to column dx, down by the
#     band's own offset plus dy (flip: the rows upside down);
#   X(x0, x1, y0, y1): clears that part of the cell (a neighbour's pixels that reach into a letter's columns).
# The pictures have no V, R, C or b (VR Calibration): they are made from v (Save), r (Player), G (Game) and p (Options),
# stretched or mirrored. "ay" is one glyph: the two letters overlap in the pictures.
# --preview draws the given texts (the main menu's rows by default) with the pak's letters, 3 times the size.
import argparse
import os
import struct

BANDS = {  # name: picture, first row, rows, offset down into the cell
    'M0': ('gfx/mainmenu.lmp', 0, 20, 1), 'M1': ('gfx/mainmenu.lmp', 20, 20, 0), 'M2': ('gfx/mainmenu.lmp', 40, 20, 0),
    'M3': ('gfx/mainmenu.lmp', 60, 20, 0), 'M4': ('gfx/mainmenu.lmp', 80, 24, 0), 'S0': ('gfx/sp_menu.lmp', 0, 20, 1),
    'S1': ('gfx/sp_menu.lmp', 20, 20, 0), 'S2': ('gfx/sp_menu.lmp', 40, 20, 0), 'J0': ('gfx/mp_menu.lmp', 0, 20, 1),
    'J2': ('gfx/mp_menu.lmp', 40, 20, 0),
}
PICS = ['gfx/mainmenu.lmp', 'gfx/sp_menu.lmp', 'gfx/mp_menu.lmp']
CELL = 24
SPACE = 9


def C(band, x0, x1, y0=0, y1=None, dx=0, dy=0, flip=False):
    return ('C', band, x0, x1, y0, BANDS[band][2] if y1 is None else y1, dx, dy, flip)


def X(x0, x1, y0, y1):
    return ('X', x0, x1, y0, y1)


GLYPHS = {}  # text: (width, advance, ops)


def glyph(text, width, ops, adv=None):
    GLYPHS[text] = (width, width if adv is None else adv, ops)


def cut(text, band, x0, x1, *extra, adv=None):
    glyph(text, x1 - x0, [C(band, x0, x1)] + list(extra), adv)


# Cut as they are.
cut('S', 'M0', 2, 20); cut('i', 'M0', 22, 30); cut('n', 'M0', 32, 50); cut('g', 'M0', 50, 69)
cut('l', 'M0', 70, 87); cut('e', 'M0', 87, 105); cut('P', 'M0', 114, 134); cut('r', 'M0', 200, 221)
cut('a', 'J0', 71, 90); cut('M', 'M1', 1, 23); cut('ay', 'M1', 122, 153)
cut('u', 'J2', 59, 79); cut('t', 'J2', 40, 58)
cut('p', 'M2', 22, 41, X(0, 1, 7, 11), X(18, 19, 3, 6))  # without O's edge and t's bar
cut('O', 'M2', 0, 23, X(22, 23, 0, 7), X(22, 23, 11, 20), adv=22)  # its right edge is p's column
cut('o', 'M2', 67, 88); cut('s', 'M2', 106, 123); cut('H', 'M3', 1, 22); cut('/', 'M3', 76, 91)
cut('d', 'S1', 58, 77); cut('Q', 'M4', 0, 23)
# Made from others.
glyph('R', 21, [C('M0', 200, 221, 2, 6, dy=-1), C('M0', 200, 221, 5, 12), C('M0', 200, 221, 11, 15, dy=1)])  # r, 2 rows taller
glyph('V', 19, [C('S2', 32, 51, 3, 7, dy=-2), C('S2', 32, 51, 6, 11, dy=-1), C('S2', 32, 51, 10, 17),  # v, 2 rows taller,
                X(0, 1, 4, 7), X(0, 2, 7, 8), X(0, 3, 8, 11), X(0, 4, 11, 12), X(0, 5, 12, 13), X(0, 6, 13, 14),
                X(0, 7, 14, 16)])  # without a's leg
glyph('C', 19, [C('S0', 71, 82, 0, 17), C('S0', 83, 90, 0, 6, dx=12),  # G's curve and its top end,
                C('S0', 83, 90, 0, 6, dx=12, dy=10, flip=True)])  # the end again at the bottom
glyph('b', 19, [C('M2', 22, 31), X(0, 1, 7, 11),  # p's stem, and its bowl twice (rows 3-6 and 10-12 of it)
                C('M2', 31, 41, 3, 7, dx=9), C('M2', 31, 41, 10, 13, dx=9, dy=-3),
                C('M2', 31, 41, 3, 7, dx=9, dy=6), C('M2', 31, 41, 10, 13, dx=9, dy=3), X(18, 19, 3, 6), X(18, 19, 9, 12)])


def read_pak(path):
    d = open(path, 'rb').read()
    _, off, ln = struct.unpack('<4sii', d[:12])
    files = {}
    for i in range(ln // 64):
        n, o, l = struct.unpack('<56sii', d[off + i * 64:off + i * 64 + 64])
        files[n.split(b'\0')[0].decode()] = d[o:o + l]
    return files


def fnv1a(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & 0xffffffff
    return h


class Pics:
    def __init__(self, files):
        self.pal = files['gfx/palette.lmp']
        self.pics = {}
        for n in PICS:
            b = files[n]
            w, h = struct.unpack('<ii', b[:8])
            self.pics[n] = (w, h, b[8:8 + w * h])

    def rect(self, band, x0, x1, y0, y1):
        name, top = BANDS[band][0], BANDS[band][1]
        w, h, px = self.pics[name]
        assert 0 <= x0 < x1 <= w and top + y1 <= h, (band, x0, x1, y0, y1)
        return bytes(px[(top + y) * w + x] for y in range(y0, y1) for x in range(x0, x1))

    def glyph(self, text):
        width, _, ops = GLYPHS[text]
        img = [[255] * width for _ in range(CELL)]
        for op in ops:
            if op[0] == 'X':
                _, x0, x1, y0, y1 = op
                for y in range(y0, y1):
                    for x in range(x0, x1):
                        img[y][x] = 255
                continue
            _, band, x0, x1, y0, y1, dx, dy, flip = op
            src = self.rect(band, x0, x1, y0, y1)
            for y in range(y0, y1):
                sy = y0 + y1 - 1 - y if flip else y
                ty = y + BANDS[band][3] + dy
                for x in range(x0, x1):
                    p = src[(sy - y0) * (x1 - x0) + x - x0]
                    tx = x - x0 + dx
                    if p != 255 and 0 <= ty < CELL and 0 <= tx < width:
                        img[ty][tx] = p
        return img


def tokens(text):
    out, i = [], 0
    while i < len(text):
        for n in (2, 1):
            if text[i:i + n] in GLYPHS or n == 1:
                out.append(text[i:i + n])
                i += n
                break
    return out


def write_inc(pics, path):
    bands = list(BANDS)
    lines = ['// vr_bigfont_glyphs.inc -- written by Misc/quakevr/make_bigfont.py: do not edit (edit the script).',
             '// Where each letter of the main menu\'s lettering is in id\'s menu pictures (no pixels: vr_bigfont.cpp cuts',
             '// them from the pak at run time, each piece only when its hash matches).', '',
             'constexpr Band bands[]{']
    for b in bands:
        name, top, rows, dy = BANDS[b]
        lines.append('    {%d, %d, %d, %d}, // %s: %s' % (PICS.index(name), top, rows, dy, b, name))
    lines += ['};', '', 'constexpr Op ops[]{']
    glyph_lines, first = [], 0
    for text, (width, adv, ops) in GLYPHS.items():
        for op in ops:
            if op[0] == 'X':
                _, x0, x1, y0, y1 = op
                lines.append('    {Op::Clear, 0, %d, %d, %d, %d, 0, 0, false, 0u},' % (x0, x1, y0, y1))
            else:
                _, band, x0, x1, y0, y1, dx, dy, flip = op
                lines.append('    {Op::Copy, %d, %d, %d, %d, %d, %d, %d, %s, 0x%08xu},' % (
                    bands.index(band), x0, x1, y0, y1, dx, dy, 'true' if flip else 'false', fnv1a(pics.rect(band, x0, x1, y0, y1))))
        glyph_lines.append('    {"%s", %d, %d, %d, %d},' % (text, width, adv, first, len(ops)))
        first += len(ops)
    lines += ['};', '', 'constexpr GlyphDef glyphDefs[]{'] + glyph_lines + ['};', '',
              'constexpr int cellRows = %d;' % CELL, 'constexpr int spaceWidth = %d;' % SPACE, '']
    with open(path, 'w', newline='\n') as f:
        f.write('\n'.join(lines))


def preview(pics, texts, path, scale=3):
    from PIL import Image, ImageDraw
    rows = []
    for t in texts:
        x, row = 0, [[255] * 400 for _ in range(CELL)]
        for tok in tokens(t):
            if tok == ' ':
                x += SPACE
                continue
            img = pics.glyph(tok)
            for y in range(CELL):
                for gx, p in enumerate(img[y]):
                    if p != 255:
                        row[y][x + gx] = p
            x += GLYPHS[tok][1]
        rows.append(row)
    im = Image.new('RGB', (404 * scale, len(rows) * (CELL + 4) * scale), (0, 60, 60))
    d = ImageDraw.Draw(im)
    for r, row in enumerate(rows):
        for y, line in enumerate(row):
            for x, p in enumerate(line):
                if p != 255:
                    X0, Y0 = (x + 2) * scale, (r * (CELL + 4) + y + 2) * scale
                    d.rectangle([X0, Y0, X0 + scale - 1, Y0 + scale - 1], fill=tuple(pics.pal[p * 3:p * 3 + 3]))
    im.save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--pak', required=True, help="Quake 1.06's id1/pak0.pak")
    ap.add_argument('--preview', help='a PNG of the texts drawn with the font (not written with --no-inc alone)')
    ap.add_argument('--no-inc', action='store_true', help='do not write the .inc')
    ap.add_argument('texts', nargs='*')
    a = ap.parse_args()
    pics = Pics(read_pak(a.pak))
    if not a.no_inc:
        out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'Quake', 'vr', 'vr_bigfont_glyphs.inc')
        write_inc(pics, os.path.normpath(out))
        print('wrote', os.path.normpath(out))
    if a.preview:
        preview(pics, a.texts or ['VR Calibration', 'Single Player', 'Multiplayer', 'Options', 'Mods', 'Help/Ordering', 'Quit'], a.preview)


if __name__ == '__main__':
    main()
