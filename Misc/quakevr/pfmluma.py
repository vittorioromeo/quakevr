#!/c/Python313/python.exe
# pfmluma.py <pfm> <xlo> <xhi> <ylo> <yhi> ... -- the same as boxluma.py for a .pfm (the eye's raw scene, as
# vr_eyeshot 2 writes it): mean and max luma in 0..255 units over a box in NDC, and the near-black fraction.
import struct, sys

def read_pfm(path):
    with open(path, "rb") as f:
        header = f.readline().strip()
        assert header == b"PF", header
        w, h = (int(v) for v in f.readline().split())
        scale = float(f.readline())
        data = struct.unpack("<%df" % (w * h * 3), f.read(w * h * 12))
    return data, w, h, scale

def stats(path, xlo, xhi, ylo, yhi):
    data, w, h, scale = read_pfm(path)
    c0, c1 = int((xlo + 1) / 2 * w), int((xhi + 1) / 2 * w)
    # PFM's first row is the bottom row of the image
    r0, r1 = int((ylo + 1) / 2 * h), int((yhi + 1) / 2 * h)
    c0, c1 = max(0, c0), min(w, c1)
    r0, r1 = max(0, r0), min(h, r1)
    n = 0
    total = 0.0
    mx = 0.0
    black = 0
    for r in range(r0, r1):
        for c in range(c0, c1):
            i = (r * w + c) * 3
            l = (0.299 * data[i] + 0.587 * data[i + 1] + 0.114 * data[i + 2]) * 255.0
            total += l
            mx = max(mx, l)
            black += 1 if l < 12.0 else 0
            n += 1
    print("%-46s raw scene %4dx%-4d mean %6.1f max %6.1f black%% %5.1f" %
          (path.split("/")[-1], c1 - c0, r1 - r0, total / max(n, 1), mx, 100.0 * black / max(n, 1)))

args = sys.argv[1:]
i = 0
while i < len(args):
    stats(args[i], *(float(v) for v in args[i + 1:i + 5]))
    i += 5
