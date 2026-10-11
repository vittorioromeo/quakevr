"""hitreact_sheet.py (hitreact_test.sh eyes) <out.png> <count> <crop x0,y0,x1,y1 (fractions)> <dir>... -- a row per directory, its newest `count`
screenshots oldest first, cropped round the monster. Directories in pairs (reaction on, off): each frame's changed
pixels between the pair printed, and the on row's frames tinted where they differ."""
import glob
import os
import sys

from PIL import Image, ImageChops, ImageDraw

out, count, crop, dirs = sys.argv[1], int(sys.argv[2]), [float(v) for v in sys.argv[3].split(',')], sys.argv[4:]
tw, th, label = 220, 260, 18


def tiles(d):
    files = sorted(glob.glob(d + '/*.png'), key=os.path.getmtime)[-count:]
    out = []
    for f in files:
        im = Image.open(f).convert('RGB')
        w, h = im.size
        out.append(im.crop((int(w * crop[0]), int(h * crop[1]), int(w * crop[2]), int(h * crop[3]))))
    return out


sheet = Image.new('RGB', (tw * count, (th + label) * len(dirs)), (24, 24, 24))
draw = ImageDraw.Draw(sheet)
rows = [tiles(d) for d in dirs]
for r, d in enumerate(dirs):
    y = r * (th + label)
    draw.text((4, y + 3), os.path.basename(d), fill=(230, 230, 230))
    for i, t in enumerate(rows[r]):
        sheet.paste(t.resize((tw, th)), (i * tw, y + label))
for r in range(0, len(dirs) - 1, 2):
    counts = []
    for a, b in zip(rows[r], rows[r + 1]):
        mask = ImageChops.difference(a, b).convert('L').point(lambda v: 255 if v > 24 else 0)
        counts.append(sum(1 for v in mask.get_flattened_data() if v))
    print(os.path.basename(dirs[r]), 'pixels differing from', os.path.basename(dirs[r + 1]), counts)
sheet.save(out)
