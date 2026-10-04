#!/c/Python313/python.exe
# boxluma.py <png> <xlo> <xhi> <ylo> <yhi> ... -- the luma statistics of a box in NDC (-1..1, y up) in each image:
# mean, max, and the fraction of near-black pixels. (test aid for "is that region showing something lit")
import sys
from PIL import Image

args = sys.argv[1:]
i = 0
while i < len(args):
    path = args[i]
    xlo, xhi, ylo, yhi = (float(v) for v in args[i + 1 : i + 5])
    i += 5
    im = Image.open(path).convert("L")
    w, h = im.size
    px = im.load()
    c0, c1 = int((xlo + 1) / 2 * w), int((xhi + 1) / 2 * w)
    r1, r0 = int((1 - (ylo + 1) / 2) * h), int((1 - (yhi + 1) / 2) * h)
    c0, c1 = max(0, c0), min(w, c1)
    r0, r1 = max(0, r0), min(h, r1)
    vals = [px[x, y] for y in range(r0, r1) for x in range(c0, c1)]
    n = len(vals)
    black = sum(1 for v in vals if v < 12)
    print("%-46s %4dx%-4d mean %6.1f max %3d black%% %5.1f" % (path.split("/")[-1], c1 - c0, r1 - r0, sum(vals) / max(n, 1), max(vals) if vals else 0, 100.0 * black / max(n, 1)))
