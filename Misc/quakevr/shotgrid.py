#!/c/Python313/python.exe
# shotgrid.py <out.png> <in...> -- compose screenshots in a row (labelled), and print per-image stats:
# mean luma and the fraction of near-black pixels. (test aid)
import sys
from PIL import Image, ImageDraw

out = sys.argv[1]
ins = sys.argv[2:]
imgs = [Image.open(p).convert("RGB") for p in ins]
w = max(i.width for i in imgs)
h = max(i.height for i in imgs)
pad, lab = 8, 22
sheet = Image.new("RGB", (w * len(imgs) + pad * (len(imgs) + 1), h + lab), (40, 40, 40))
d = ImageDraw.Draw(sheet)
for n, im in enumerate(imgs):
    x = pad + n * (w + pad)
    sheet.paste(im, (x, lab))
    d.text((x, 4), ins[n].split("/")[-1], fill=(255, 255, 0))
    px = list(im.getdata())
    lum = [(3 * r + 6 * g + b) // 10 for r, g, b in px]
    black = sum(1 for v in lum if v < 12)
    print("%-40s %dx%d mean %6.1f black%% %5.1f" % (ins[n], im.width, im.height, sum(lum) / len(lum), 100.0 * black / len(lum)))
sheet.save(out)
print("wrote", out)
