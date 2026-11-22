#!/usr/bin/env python3
# make_gun_variants.py -- one model per gun (ROUND25.md, "onemodel"; BACKLOG.md 8): a gun's other-ammo look is its
# base model with another skin, declared in Quake/vr/vr_gunvariants.inc (QVR_GUN_VARIANT(variant, base, skin)). The
# skin is an indexed PNG (the Quake palette) on the BASE model's skin layout, kept beside the base's .blend
# (Misc/quakevr/src_models/blend/<base>/<base>_skin_<variant>.png): paint it on the base model as its own skin.
#
# Usage (plain Python 3, from anywhere; no Blender):
#   python Misc/quakevr/make_gun_variants.py build [--check]
#       Writes each variant's shipped files from its base's: quakevr/progs/<variant>.mdl (the base's model, its skin the
#       variant's PNG), the base's parts drawn in its place (vr_mag_on_<base>.mdl, vr_magwell_on_<base>.mdl: the same
#       skin layout) as the variant's, and their normal maps (the base's, byte for byte: the same surface). Run it
#       after exporting a base model (blend_export.py) or painting a variant skin. --check writes nothing and fails
#       (exit 1) if a shipped file differs from what build would write. Ends "gun_variants: ok, N files ...".
#   python Misc/quakevr/make_gun_variants.py transfer <variant> <old variant .mdl>
#       Once per variant: its old model's skin carried onto the base's layout (each base texel takes the colour of the
#       old model's surface nearest its point on the base, on a face turned the same way) -> the variant's PNG.
import os
import re
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(HERE, "blender", "addons", "quakevr_models"))
import mdl as mdlmod  # noqa: E402
import qpal  # noqa: E402

TABLE = os.path.join(ROOT, "Quake", "vr", "vr_gunvariants.inc")
PROGS = os.path.join(ROOT, "quakevr", "progs")
PARTS = ("vr_mag_on_", "vr_magwell_on_")


def variants():
    """[(variant stem, base stem, skin png path)] from vr_gunvariants.inc."""
    out = []
    with open(TABLE, encoding="utf-8") as f:
        for m in re.finditer(r'^QVR_GUN_VARIANT\(\s*"progs/(\w+)\.mdl"\s*,\s*"progs/(\w+)\.mdl"\s*,\s*"([^"]+)"\s*\)',
                             f.read(), re.M):
            out.append((m.group(1), m.group(2), os.path.join(ROOT, m.group(3))))
    return out


# ----------------------------------------------------------------------------- PNG (indexed 8-bit; RGB/RGBA read)

def write_png(path, w, h, idx):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)
    raw = b"".join(b"\0" + bytes(idx[y * w:(y + 1) * w]) for y in range(h))
    pal = b"".join(bytes(c) for c in qpal.PALETTE)
    data = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 3, 0, 0, 0)) +
            chunk(b"PLTE", pal) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(data)


def read_png(path):
    """(w, h, indices or None, rgb list or None): palette indices when the file is indexed with the Quake palette."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("gun_variants: REFUSED: %s is not a PNG" % path)
    off, idat, pal = 8, b"", None
    while off < len(data):
        n, tag = struct.unpack_from(">I4s", data, off)
        body = data[off + 8:off + 8 + n]
        if tag == b"IHDR":
            w, h, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
        elif tag == b"PLTE":
            pal = [tuple(body[3 * i:3 * i + 3]) for i in range(n // 3)]
        elif tag == b"IDAT":
            idat += body
        off += 12 + n
    if depth != 8 or interlace or ctype not in (2, 3, 6):
        raise SystemExit("gun_variants: REFUSED: %s: save it as 8-bit indexed, RGB or RGBA, not interlaced" % path)
    bpp = {2: 3, 3: 1, 6: 4}[ctype]
    raw, stride = zlib.decompress(idat), w * bpp
    rows, prev = [], bytearray(stride)
    for y in range(h):
        ft, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b, c = prev[i], prev[i - bpp] if i >= bpp else 0
            if ft == 1:
                line[i] = (line[i] + a) & 255
            elif ft == 2:
                line[i] = (line[i] + b) & 255
            elif ft == 3:
                line[i] = (line[i] + ((a + b) >> 1)) & 255
            elif ft == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line)
        prev = line
    flat = b"".join(rows)
    if ctype == 3:
        if pal is not None and pal[:256] == qpal.PALETTE[:len(pal)]:
            return w, h, bytes(flat), None
        return w, h, None, [pal[i] for i in flat]
    return w, h, None, [tuple(flat[i:i + 3]) for i in range(0, len(flat), bpp)]


def part_png(png, prefix, base, var):
    """A part's variant skin, beside the gun's: <folder>/<prefix><base>_skin_<variant>.png (absent: the base part's)."""
    return os.path.join(os.path.dirname(png), "%s%s_skin_%s.png" % (prefix, base, var))


def skin_indices(png, base_skin, w, h):
    pw, ph, idx, rgb = read_png(png)
    if (pw, ph) != (w, h):
        raise SystemExit("gun_variants: REFUSED: %s is %dx%d, the base's skin %dx%d" % (png, pw, ph, w, h))
    if idx is not None:
        return idx
    return bytes(qpal.Quantizer().indices(rgb, base_skin))  # (an RGB repaint: glowing where the base glows)


# ----------------------------------------------------------------------------- build

def build(check):
    written, differ = 0, []
    for var, base, png in variants():
        bm = mdlmod.Model.read(os.path.join(PROGS, base + ".mdl"))
        sw, sh = bm.h[13], bm.h[14]
        skin = skin_indices(png, bm.skins[0][2][0], sw, sh)
        outs = []
        for prefix in ("",) + PARTS:
            src = os.path.join(PROGS, prefix + base + ".mdl")
            if not os.path.exists(src):
                continue
            m = mdlmod.Model.read(src)
            if prefix:  # a part has a skin of its own: the variant's (an ammo's magazine) or else the base part's
                ppng = part_png(png, prefix, base, var)
                if os.path.exists(ppng):
                    m.skins = [[0, b"", [skin_indices(ppng, m.skins[0][2][0], m.h[13], m.h[14])]]]
            else:
                m.skins = [[0, b"", [skin]]]
            outs.append((os.path.join(PROGS, prefix + var + ".mdl"), m.to_bytes()))
            norm = src + "_0_norm.png"
            if os.path.exists(norm):
                with open(norm, "rb") as f:
                    outs.append((os.path.join(PROGS, prefix + var + ".mdl_0_norm.png"), f.read()))
        for path, data in outs:
            old = open(path, "rb").read() if os.path.exists(path) else None
            if old == data:
                continue
            if check:
                differ.append(os.path.relpath(path, ROOT))
            else:
                with open(path, "wb") as f:
                    f.write(data)
                written += 1
    if differ:
        print("gun_variants: FAILED: out of date (run build): " + ", ".join(differ))
        sys.exit(1)
    print("gun_variants: ok, %d files written, %d variants" % (written, len(variants())))


# ----------------------------------------------------------------------------- transfer

def triangles(m):
    """[(3 points, 3 (s, t) texel coordinates, normal)] of pose 0 (a back face reads an onseam vertex's s half a skin
    to the right)."""
    sc, org = m.h[2:5], m.h[5:8]
    fr = m.frames[0]
    vb = fr[2] if fr[0] == "simple" else fr[3][0][1]
    pts = [tuple(vb[4 * i + k] * sc[k] + org[k] for k in range(3)) for i in range(len(m.st))]
    half = m.h[13] / 2
    out = []
    for front, a, b, c in m.tris:
        p = [pts[a], pts[b], pts[c]]
        uv = []
        for v in (a, b, c):
            onseam, s, t = m.st[v]
            uv.append((s + (half if onseam and not front else 0) + 0.5, t + 0.5))
        e1 = [p[1][k] - p[0][k] for k in range(3)]
        e2 = [p[2][k] - p[0][k] for k in range(3)]
        n = (e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0])
        ln = (n[0] ** 2 + n[1] ** 2 + n[2] ** 2) ** 0.5 or 1.0
        out.append((p, uv, (n[0] / ln, n[1] / ln, n[2] / ln)))
    return out


def closest_bary(p, a, b, c):
    """The point of triangle abc (3D) nearest p: (squared distance, its barycentric weights)."""
    q, w = mdlmod.closest_on_triangle(p, a, b, c)
    return sum((q[k] - p[k]) ** 2 for k in range(3)), w


def transfer(var, old_path):
    row = [r for r in variants() if r[0] == var]
    if not row:
        raise SystemExit("gun_variants: REFUSED: %s is not in vr_gunvariants.inc" % var)
    _, base, png = row[0]
    bm = mdlmod.Model.read(os.path.join(PROGS, base + ".mdl"))
    om = mdlmod.Model.read(old_path)
    sw, sh = bm.h[13], bm.h[14]
    osw, osh = om.h[13], om.h[14]
    oskin = om.skins[0][2][0]
    btris, otris = triangles(bm), triangles(om)
    obox = [([min(p[i][k] for i in range(3)) for k in range(3)], [max(p[i][k] for i in range(3)) for k in range(3)])
            for p, _, _ in otris]
    out = bytearray(bm.skins[0][2][0])  # texels no face covers keep the base's
    covered = bytearray(sw * sh)
    far, worst = 0, 0.0
    for p, uv, n in btris:
        lo = [min(p[i][k] for i in range(3)) for k in range(3)]
        hi = [max(p[i][k] for i in range(3)) for k in range(3)]
        for margin, facing in ((0.5, 0.3), (1.5, 0.3), (4.0, 0.3), (1e9, 0.3), (1e9, -2.0)):
            cand = [j for j, (ol, oh) in enumerate(obox)
                    if all(ol[k] <= hi[k] + margin and oh[k] >= lo[k] - margin for k in range(3))
                    and sum(otris[j][2][k] * n[k] for k in range(3)) > facing]
            if cand:
                break
        x0, x1 = int(min(u[0] for u in uv)) - 1, int(max(u[0] for u in uv)) + 2
        y0, y1 = int(min(u[1] for u in uv)) - 1, int(max(u[1] for u in uv)) + 2
        for y in range(max(0, y0), min(sh, y1)):
            for x in range(max(0, x0), min(sw, x1)):
                # the texel's middle on the face's skin triangle, or within 0.75 texel of it (a bleed edge)
                d2, cw = closest_bary((x + 0.5, y + 0.5, 0.0), *[(u[0], u[1], 0.0) for u in uv])
                if d2 > 0.5625:
                    continue
                inside = d2 < 1e-9
                if covered[y * sw + x] == 2 or (covered[y * sw + x] == 1 and not inside):
                    continue
                q = [sum(cw[i] * p[i][k] for i in range(3)) for k in range(3)]
                best, bw, bj = 1e30, None, -1
                for j in cand:
                    d2, w = closest_bary(q, *otris[j][0])
                    if d2 < best:
                        best, bw, bj = d2, w, j
                if best > 0.25:
                    far += 1
                worst = max(worst, best ** 0.5)
                ouv = otris[bj][1]
                ox = int(sum(bw[i] * ouv[i][0] for i in range(3)))
                oy = int(sum(bw[i] * ouv[i][1] for i in range(3)))
                out[y * sw + x] = oskin[min(osh - 1, max(0, oy)) * osw + min(osw - 1, max(0, ox))]
                covered[y * sw + x] = 2 if inside else 1
    write_png(png, sw, sh, out)
    # Its parts (cut from the gun with the same skin layout as the base's parts): an old part's skin as it is, where it
    # differs from the base part's (an ammo's magazine); the same skin (the well) needs none.
    for prefix in PARTS:
        old_part = os.path.join(os.path.dirname(old_path), prefix + os.path.basename(old_path))
        base_part = os.path.join(PROGS, prefix + base + ".mdl")
        if not (os.path.exists(old_part) and os.path.exists(base_part)):
            continue
        op, bp = mdlmod.Model.read(old_part), mdlmod.Model.read(base_part)
        if op.st != bp.st or op.tris != bp.tris or op.h[13:15] != bp.h[13:15]:
            raise SystemExit("gun_variants: REFUSED: %s is not laid out as %s" % (old_part, base_part))
        if op.skins[0][2][0] != bp.skins[0][2][0]:
            write_png(part_png(png, prefix, base, var), op.h[13], op.h[14], op.skins[0][2][0])
            print("gun_variants: transfer %s%s: its own skin" % (prefix, var))
    print("gun_variants: transfer %s -> %s: %d texels covered, %d over 0.5 units from the old surface (worst %.3f)" %
          (var, os.path.relpath(png, ROOT), sum(1 for c in covered if c), far, worst))


def main():
    a = sys.argv[1:]
    if a and a[0] == "build":
        build("--check" in a)
    elif len(a) == 3 and a[0] == "transfer":
        transfer(a[1], a[2])
    else:
        print(__doc__ or "usage: make_gun_variants.py build [--check] | transfer <variant> <old .mdl>")
        sys.exit(2)


if __name__ == "__main__":
    main()
