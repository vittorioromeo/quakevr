#!/c/Python313/python.exe
# boxgrid.py <out.png> <xlo> <xhi> <ylo> <yhi> <scale> <in1> [<in2> ...] -- the same NDC box cropped from each
# image, scaled up and laid side by side in one image (a before/after pair in a single view).
import sys
from PIL import Image

out, xlo, xhi, ylo, yhi, scale = sys.argv[1], *(float(v) for v in sys.argv[2:7])
scale = int(scale)
ims = []
for path in sys.argv[7:]:
    im = Image.open(path).convert("RGB")
    w, h = im.size
    c0, c1 = int((xlo + 1) / 2 * w), int((xhi + 1) / 2 * w)
    r1, r0 = int((1 - (ylo + 1) / 2) * h), int((1 - (yhi + 1) / 2) * h)
    c0, c1 = max(0, c0), min(w, c1)
    r0, r1 = max(0, r0), min(h, r1)
    crop = im.crop((c0, r0, c1, r1)).resize(((c1 - c0) * scale, (r1 - r0) * scale), Image.NEAREST)
    ims.append(crop)
w = sum(i.width for i in ims) + 8 * (len(ims) - 1)
h = max(i.height for i in ims)
out_im = Image.new("RGB", (w, h), (40, 40, 40))
x = 0
for i in ims:
    out_im.paste(i, (x, 0))
    x += i.width + 8
out_im.save(out)
print("%s  %d crops %dx%d -> %dx%d" % (out, len(ims), ims[0].width, ims[0].height, w, h))
