#!/usr/bin/env python3
# flatvisuals_check.py <screenshots dir> <log>: judges flatvisuals_test.sh's frames (in the order taken):
#   0 vr_crosshair 0, 1 dot, 2 laser, 3 soft laser; 4 dead (Immersive), 5 dead (Third Person: Quake's view).
# Prints a line per check and `flatvisuals: N passed, M failed`.
import glob, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import quakeimage

# (the headset's sizes: the dot is vr_crosshair_size units a 100 units off, about 6 pixels across at the far wall; the laser
# 0.3 units wide, under a pixel past a few metres; the soft laser full only from 15% to 70% of the way: far, thin)
DOT_MIN, DOT_MAX = 4, 2000   # red pixels a dot adds
DOT_NEAR = 0.12              # ... their middle within this share of the width of the screen's middle
LASER_MIN = 150              # red pixels a laser adds
SOFT_MIN = 10                # ... a soft laser (fewer than the laser)
DEATH_DIFF = 0.25            # share of pixels the head's view differs from Quake's floor view by
EYES_NEAR = 2.0              # units the camera may be off the head's eyes
results = []

def check(ok, what):
    results.append(ok)
    print("flatvisuals: %s: %s" % ("PASS" if ok else "FAIL", what))

def load(path):
    return quakeimage.read_png(open(path, "rb").read())

def redAdded(base, img):
    # pixels red in img (the crosshair's hue 0) that were not in base, and their box
    w, h, a = base
    _, _, b = img
    n, xs, ys = 0, [], []
    for y in range(0, h):
        row = y * w * 3
        for x in range(0, w):
            p = row + x * 3
            r, g, bl = b[p], b[p + 1], b[p + 2]
            if r > 120 and r > 2 * g and r > 2 * bl and (abs(r - a[p]) > 40 or abs(g - a[p + 1]) > 40):
                n += 1
                xs.append(x)
                ys.append(y)
    return n, xs, ys

def diffShare(A, B):
    w, h, a = A
    _, _, b = B
    n = sum(1 for p in range(0, w * h * 3, 3) if abs(a[p] - b[p]) + abs(a[p + 1] - b[p + 1]) + abs(a[p + 2] - b[p + 2]) > 60)
    return n / float(w * h)

shots = sorted(glob.glob(os.path.join(sys.argv[1], "*.png")))
if len(shots) < 6:
    check(False, "6 frames taken (got %d)" % len(shots))
else:
    imgs = [load(s) for s in shots[:6]]
    w, h = imgs[0][0], imgs[0][1]
    n, xs, ys = redAdded(imgs[0], imgs[1])
    mx = sum(xs) / len(xs) if xs else -1
    my = sum(ys) / len(ys) if ys else -1
    check(DOT_MIN <= n <= DOT_MAX and abs(mx - w / 2) < DOT_NEAR * w and abs(my - h / 2) < DOT_NEAR * w,
          "dot: %d red pixels added, their middle %.0f %.0f (the screen's %d %d)" % (n, mx, my, w // 2, h // 2))
    n2, xs2, ys2 = redAdded(imgs[0], imgs[2])
    spanOk = bool(xs2) and min(xs2) < w * 0.6 and max(ys2) > h * 0.6 and min(ys2) < h * 0.55
    check(n2 >= LASER_MIN and spanOk, "laser: %d red pixels added, x %s..%s, y %s..%s (from the gun, low right, to the middle)" %
          (n2, min(xs2) if xs2 else "-", max(xs2) if xs2 else "-", min(ys2) if ys2 else "-", max(ys2) if ys2 else "-"))
    n3, xs3, ys3 = redAdded(imgs[0], imgs[3])
    check(SOFT_MIN <= n3 < n2 and bool(ys3) and min(ys3) < h * 0.55,
          "soft laser: %d red pixels added (the laser %d), reaching y %s" % (n3, n2, min(ys3) if ys3 else "-"))
    d = diffShare(imgs[4], imgs[5])
    check(d >= DEATH_DIFF, "dead: the head's view against Quake's floor view: %.1f%% of the pixels differ" % (100 * d))

log = open(sys.argv[2], "rb").read().decode("latin-1").replace("\r", "").replace("\n", "") # (run.sh wraps long lines at a space)
st = re.findall(r"deathview: mode (\d+) health (-?\d+) doll (\d+) ragdoll (\d) head (\d).*?immersive (\d).*?camera (\S+) (\S+) (\S+) \((-?[\d.]+) from the eyes\).*?camangles (-?[\d.]+) (-?[\d.]+) (-?[\d.]+)", log)
if len(st) < 2:
    check(False, "two death view statuses printed (got %d)" % len(st))
else:
    m, health, doll, rag, head, imm, cx, cy, cz, off, pa, ya, ra = st[0]
    check(m == "2" and int(health) <= 0 and doll != "0" and rag == "1" and imm == "1" and 0 <= float(off) <= EYES_NEAR and abs(float(ra)) < 0.5,
          "dead, Immersive: body %s ragdoll %s immersive %s, the camera %s from the head's eyes, roll %s" % (doll, rag, imm, off, ra))
    imm1 = st[1][5]
    check(st[1][0] == "1" and imm1 == "0", "Third Person after: immersive %s" % imm1)

passed = sum(1 for r in results if r)
print("flatvisuals: %d passed, %d failed" % (passed, len(results) - passed))
