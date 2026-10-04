#!/c/Python313/python.exe
# boxascii.py <png> <xlo> <xhi> <ylo> <yhi> [cols] -- a box in NDC (-1..1, y up) as ASCII art (brightness ramp),
# scaled to `cols` columns. (test aid: what a region of an eye image actually shows)
import sys
from PIL import Image

path, xlo, xhi, ylo, yhi = sys.argv[1], *(float(v) for v in sys.argv[2:6])
cols = int(sys.argv[6]) if len(sys.argv) > 6 else 60
im = Image.open(path).convert("L")
w, h = im.size
c0, c1 = int((xlo + 1) / 2 * w), int((xhi + 1) / 2 * w)
r1, r0 = int((1 - (ylo + 1) / 2) * h), int((1 - (yhi + 1) / 2) * h)
c0, c1 = max(0, c0), min(w, c1)
r0, r1 = max(0, r0), min(h, r1)
crop = im.crop((c0, r0, c1, r1))
rows = max(1, int(cols * crop.height / max(crop.width, 1) * 0.5))
crop = crop.resize((cols, rows))
ramp = " .:-=+*#%@"
mx = crop.getextrema()[1]
scale = int(sys.argv[7]) if len(sys.argv) > 7 else 255 # a fixed scale, so the same art means the same brightness
print("%s  %dx%d px -> %dx%d, max luma %d, scale %d" % (path.split("/")[-1], crop.width, crop.height, cols, rows, mx, scale))
for y in range(rows):
    print("".join(ramp[min(9, crop.getpixel((x, y)) * 9 // scale)] for x in range(cols)))
