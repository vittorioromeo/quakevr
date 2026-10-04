"""grid.py -- compose the eyeshots into one labelled grid and print pixel differences between pairs.

    python grid.py <dir> <n0> <n1> ...      (composes <dir>/start_<n>_L.png, prints a diff matrix, writes grid.png)
"""
import itertools
import sys

from PIL import Image, ImageChops, ImageDraw

args = sys.argv[1:]
d = args[0]
nums = args[1:]
labels = ["1 gates yaw-20", "2 gates yaw+40", "3 nogate yaw-20", "4 nogate yaw+40", "5 novis yaw-20", "6 novis yaw+40"]

imgs = []
for i, n in enumerate(nums):
    p = f"{d}/start_{n}_L.png"
    im = Image.open(p).convert("RGB")
    im.thumbnail((420, 420))
    dr = ImageDraw.Draw(im)
    dr.rectangle([0, 0, 150, 18], fill=(0, 0, 0))
    dr.text((3, 3), labels[i] if i < len(labels) else n, fill=(255, 255, 0))
    imgs.append(im)

w, h = imgs[0].size
cols = 2
rows = (len(imgs) + cols - 1) // cols
out = Image.new("RGB", (w * cols, h * rows), (20, 20, 20))
for i, im in enumerate(imgs):
    out.paste(im, ((i % cols) * w, (i // cols) * h))
out.save("grid.png")

# differences: fraction of pixels that changed, and the mean change, for the pairs that matter
full = Image.new("RGB", imgs[0].size, (0, 0, 0))
for a, b in itertools.combinations(range(len(imgs)), 2):
    diff = ImageChops.difference(imgs[a], imgs[b])
    hist = diff.convert("L").histogram()
    tot = sum(hist)
    changed = sum(hist[8:])  # a visible change
    mean = sum(i * c for i, c in enumerate(hist)) / max(tot, 1)
    if changed:
        box = diff.convert("L").point(lambda v: 255 if v > 8 else 0).getbbox()
        print(f"{labels[a]} vs {labels[b]}: changed {100.0*changed/tot:5.2f}%  mean {mean:5.2f}  bbox {box}")
print("grid.png", out.size)
