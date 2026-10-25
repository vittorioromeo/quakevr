"""env_sheet.py <out.png> <count> <dir>... -- one contact sheet (env_test.sh EYES=1): a row per directory (a case and a
monster), its newest `count` screenshots oldest first, each cropped to the middle of the view and labelled with the
directory's name."""
import glob
import os
import sys

from PIL import Image, ImageDraw

out, count, dirs = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
tw, th, label = 300, 200, 18
sheet = Image.new('RGB', (tw * count, (th + label) * len(dirs)), (24, 24, 24))
draw = ImageDraw.Draw(sheet)
for r, d in enumerate(dirs):
    files = sorted(glob.glob(d + '/*.png'), key=os.path.getmtime)[-count:]
    y = r * (th + label)
    draw.text((4, y + 3), os.path.basename(d).replace('eyes_', ''), fill=(230, 230, 230))
    for i, f in enumerate(files):
        im = Image.open(f).convert('RGB')
        w, h = im.size
        tile = im.crop((int(w * 0.15), int(h * 0.12), int(w * 0.85), int(h * 0.88))).resize((tw, th))
        sheet.paste(tile, (i * tw, y + label))
sheet.save(out)
