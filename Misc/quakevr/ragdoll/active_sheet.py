"""active_sheet.py <screenshots dir> <out.png> [count [x0,y0,x1,y1]] -- the last `count` (9) screenshots, oldest first, cropped round
the body (active_test.sh's camera: it lies right of the middle) into a 3-wide grid (active_test.sh EYES=1)."""
import glob
import os
import sys

from PIL import Image

count = int(sys.argv[3]) if len(sys.argv) > 3 else 9
box = [float(x) for x in sys.argv[4].split(",")] if len(sys.argv) > 4 else [0.3, 0.3, 0.95, 0.9]
files = sorted(glob.glob(sys.argv[1] + '/*.png'), key=os.path.getmtime)[-count:]
tw, th, cols = 360, 220, 3
rows = (len(files) + cols - 1) // cols
sheet = Image.new('RGB', (tw * cols, th * rows))
for i, f in enumerate(files):
    im = Image.open(f).convert('RGB')
    w, h = im.size
    tile = im.crop((int(w * box[0]), int(h * box[1]), int(w * box[2]), int(h * box[3]))).resize((tw, th))
    sheet.paste(tile, ((i % cols) * tw, (i // cols) * th))
sheet.save(sys.argv[2])
