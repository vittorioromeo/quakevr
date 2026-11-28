"""reloadstudio_strip.py <log> <screenshots dir> <out.png> <gun> -- reloadstudio3: lays a strip run's screenshots out a row a key,
the studio's lock left, the game's real reload right (reloadstudio_strip.sh). The log's "reloadstudio_strip: key N lock|game
(name)" lines say which shot is which, in the order taken (the shots sorted by time); "game skipped": no shot (a blank cell)."""
import os
import re
import sys

from PIL import Image, ImageDraw

W, H = 560, 480  # each cell
LABEL = 22
CROP = (0.18, 0.04, 0.82, 0.96)  # the shot's middle (left, top, right, bottom shares): the player, the gun and the hands


def cell(path):
    im = Image.open(path).convert("RGB")
    w, h = im.size
    return im.crop((int(CROP[0] * w), int(CROP[1] * h), int(CROP[2] * w), int(CROP[3] * h))).resize((W, H))


def main():
    log, shots_dir, out, gun = sys.argv[1:5]
    lines = open(log, encoding="utf-8", errors="replace").read().splitlines()
    order = []  # (key, side, name)
    for ln in lines:
        m = re.search(r"reloadstudio_strip: key (\d+) (lock|game)( skipped)? \((.*)\)", ln)
        if m:
            order.append((int(m.group(1)), m.group(2), m.group(4), m.group(3) is not None))
    files = []
    if shots_dir and os.path.isdir(shots_dir):
        files = sorted((os.path.join(shots_dir, f) for f in os.listdir(shots_dir) if f.lower().endswith(".png")),
                       key=os.path.getmtime)
    shots = [o for o in order if not o[3]]
    if len(files) != len(shots):
        print(f"reloadstudio_strip.py: {gun}: {len(files)} screenshots for {len(shots)} shots; laid out as many as match")
    cells = {}
    names = {}
    for (key, side, name, _), f in zip(shots, files):
        cells[(key, side)] = f
        names[key] = name
    for key, side, name, skipped in order:
        names.setdefault(key, name)
    keys = sorted(names)
    img = Image.new("RGB", (2 * W, LABEL + len(keys) * (H + LABEL)), (24, 24, 24))
    d = ImageDraw.Draw(img)
    d.text((8, 4), f"{gun}: Reload Studio (lock)", fill=(255, 255, 255))
    d.text((W + 8, 4), f"{gun}: game (a real reload held at the key's end)", fill=(255, 255, 255))
    for row, key in enumerate(keys):
        y = LABEL + row * (H + LABEL)
        d.text((8, y + 4), f"key {key}: {names[key]}", fill=(255, 220, 120))
        for col, side in enumerate(("lock", "game")):
            f = cells.get((key, side))
            if f:
                img.paste(cell(f), (col * W, y + LABEL))
            else:
                d.text((col * W + 8, y + LABEL + H // 2), "(not shot: the game skipped this key)", fill=(200, 120, 120))
    img.save(out)
    print(f"reloadstudio_strip.py: {gun}: {len(keys)} keys into {out}")


if __name__ == "__main__":
    main()
