#!/usr/bin/env python3
# make_bloody_hands.py -- adds the damage skins to the hand models (hand_base.mdl and the five
# finger_*.mdl): skin 0 is the model's own, skins 1..3 add more and more blood, as the body's
# skins do (make_vrbody.py). vr_view.cpp picks the skin from the player's health (vr_body_state).
#
# Usage: python Misc/quakevr/make_bloody_hands.py [progs folder]
#
# Idempotent: skin 0 is kept, any other skins are replaced. The blood only goes where the model's
# triangles use the skin. Run it again after changing a model's skin (taper_hand.py keeps skins).

import math
import os
import struct
import sys

from mdlgen import HEADER, read_skins

MODELS = ["hand_base", "finger_thumb", "finger_index", "finger_middle", "finger_ring", "finger_pinky"]

# Quake palette reds (64..79 go from black to (127, 0, 0)); none of them fullbright.
BLOOD_DARK = 72
BLOOD = 75
BLOOD_EDGE = 78
SCRATCH = 76


def read_mdl(path):
    data = open(path, "rb").read()
    h = list(HEADER.unpack_from(data, 0))
    num_skins, w, hgt, num_verts, num_tris = h[12], h[13], h[14], h[15], h[16]
    skins, off = read_skins(data, HEADER.size, num_skins, w, hgt)
    rest = data[off:]
    stverts = [struct.unpack_from("<3i", rest, 12 * i) for i in range(num_verts)]
    tris = [struct.unpack_from("<4i", rest, 12 * num_verts + 16 * i) for i in range(num_tris)]
    return h, skins, rest, stverts, tris, w, hgt


def coverage(stverts, tris, w, hgt):
    """The texels the triangles use (back faces of seam vertices take the right half)."""
    used = bytearray(w * hgt)
    for front, a, b, c in tris:
        pts = []
        for v in (a, b, c):
            onseam, s, t = stverts[v]
            if onseam and not front:
                s += w // 2
            pts.append((s + 0.5, t + 0.5))
        (x0, y0), (x1, y1), (x2, y2) = pts
        area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        if abs(area) < 1e-6:
            continue
        for y in range(max(0, int(min(y0, y1, y2)) - 1), min(hgt, int(max(y0, y1, y2)) + 2)):
            for x in range(max(0, int(min(x0, x1, x2)) - 1), min(w, int(max(x0, x1, x2)) + 2)):
                px, py = x + 0.5, y + 0.5
                l0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / area
                l1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) / area
                if l0 >= -0.02 and l1 >= -0.02 and 1 - l0 - l1 >= -0.02:
                    used[y * w + x] = 1
    return used


def marks_for(name, used, w):
    """Blood blotches and scratches per damage level (each level adds to the previous)."""
    rng = sum(ord(c) * 31 ** i for i, c in enumerate(name)) & 0x7FFFFFFF

    def rand():
        nonlocal rng
        rng = (rng * 1103515245 + 12345) & 0x7FFFFFFF
        return (rng >> 8) / float(1 << 23)

    cells = [i for i, u in enumerate(used) if u]
    # Size the marks to the area the model uses: the fingers use far less of the skin than the hand.
    scale = max(1.2, math.sqrt(len(cells)) / 24.0)
    levels = []
    for level in (1, 2, 3):
        marks = []
        for _ in range(2 + 3 * level):
            i = cells[int(rand() * len(cells))]
            marks.append(("blood", i % w, i // w, scale * (0.7 + 0.8 * rand()), rand()))
        for _ in range(2 * level):
            i = cells[int(rand() * len(cells))]
            angle = rand() * math.pi
            length = scale * (2.0 + 3.0 * rand())
            marks.append(("scratch", i % w, i // w, math.cos(angle) * length, math.sin(angle) * length))
        levels.append(marks)
    return levels


def mark_texel(x, y, marks):
    for kind, a, b, c, d in marks:
        if kind == "scratch":
            px, py = x - a, y - b
            k = max(0.0, min(1.0, (px * c + py * d) / (c * c + d * d)))
            if math.hypot(px - k * c, py - k * d) < 0.7:
                return SCRATCH
        else:
            dx, dy = x - a, y - b
            r = c * (1.0 + 0.25 * math.sin(7.0 * math.atan2(dy, dx) + d * 6.0))  # ragged edge
            dist = math.hypot(dx, dy)
            if dist < r:
                return BLOOD_DARK if dist < r * 0.5 else BLOOD if dist < r * 0.8 else BLOOD_EDGE
            if abs(dx) < max(0.6, c * 0.2) and 0 < dy < c * (1.2 + 1.5 * d):  # a drip
                return BLOOD
    return None


def process(path, name):
    h, skins, rest, stverts, tris, w, hgt = read_mdl(path)
    base = skins[0]
    (group,) = struct.unpack_from("<i", base, 0)
    if group != 0:
        raise SystemExit("%s: skin 0 is a group; not supported" % path)
    pixels = base[4 : 4 + w * hgt]
    used = coverage(stverts, tris, w, hgt)
    levels = marks_for(name, used, w)

    out_skins = [base]
    marks = []
    for level_marks in levels:
        marks = marks + level_marks
        skin = bytearray(pixels)
        for y in range(hgt):
            for x in range(w):
                if not used[y * w + x]:
                    continue
                m = mark_texel(x + 0.5, y + 0.5, marks)
                if m is not None:
                    skin[y * w + x] = m
        out_skins.append(struct.pack("<i", 0) + bytes(skin))

    h[12] = len(out_skins)
    with open(path, "wb") as f:
        f.write(HEADER.pack(*h))
        for s in out_skins:
            f.write(s)
        f.write(rest)
    print("%s: %d skins, %d texels used" % (name, len(out_skins), sum(used)))


def main():
    folder = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", "quakevr", "progs")
    for name in MODELS:
        process(os.path.join(folder, name + ".mdl"), name)


if __name__ == "__main__":
    main()
