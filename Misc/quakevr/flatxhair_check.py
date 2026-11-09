#!/usr/bin/env python3
"""flatxhair_check.py <log> <screenshots dir> -- flatxhair_test.sh's verdicts (see there) from vr_flat_xhair_probe's lines and
the run's screenshots (in the order taken: open, wall, dual, styles 0..7, none). Standard library only."""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pixdiff_ab import read_png  # noqa: E402

log, shots_dir = sys.argv[1], sys.argv[2]
text = open(log, encoding="utf-8", errors="replace").read().splitlines()

# Sections: the probes after each "flatxhair_test: <name>".
sections, current = {}, None
for line in text:
    m = re.match(r"flatxhair_test: (\w+)", line)
    if m:
        current = m.group(1)
        sections[current] = []
        continue
    if current is None or not line.startswith("flatxhair: "):
        continue
    if line.startswith("flatxhair: on "):
        sections[current].append({"head": line})
    elif sections[current]:
        block = sections[current][-1]
        m = re.match(r"flatxhair: (view|why|main|off) ?(\w*)", line)
        if m and m.group(1) in ("view", "why"):
            block["head"] += " " + line
        elif m:
            key = m.group(1) + ("" if m.group(2) == "shown" else "_" + m.group(2))
            block[key] = line

passed = failed = 0


def check(ok, what):
    global passed, failed
    print(("PASS " if ok else "FAIL ") + what)
    if ok:
        passed += 1
    else:
        failed += 1


def num(line, key, n=1):
    """The n numbers after `key` in the line (floats)."""
    if not line:
        return [float("nan")] * n
    m = re.search(re.escape(key) + r" " + r" ".join([r"(-?[\d.]+)"] * n), line)
    return [float(g) for g in m.groups()] if m else [float("nan")] * n


def block(name, i):
    b = sections.get(name, [])
    return b[i] if i < len(b) else {}


def hand(b, h, key, n=1):
    return num(b.get(h, ""), key, n)


head0 = block("open", 0).get("head", "")
view = num(head0, "view", 2)
scale = view[1] / 1080.0 if view[1] == view[1] else 0.5
snap = num(head0, "snap", 1)[0]
tol = 8.0 * scale  # pellets' mean against the drawn point (the shotgun's spread over six pellets: under 2 px at 1080, sd)
centre = num(head0, "centre", 2)
print(f"view {view} centre {centre} snap {snap:.1f} px, tolerance {tol:.1f} px")

# open
b0, b1 = block("open", 0), block("open", 1)
check(hand(b0, "main", "shown")[0] == 1 and hand(b0, "main", "snapped")[0] == 1 and hand(b0, "main", "drawn", 2) == centre,
      f"open: the shotgun's crosshair on the centre ({b0.get('main', '?')[11:80]})")
p = b1.get("main_pellets", "")
check(num(p, "from drawn")[0] <= tol, f"open: its pellets land on it ({num(p, 'from drawn')[0]} px from the drawn crosshair)")

# wall
b0, b1 = block("wall", 0), block("wall", 1)
off = hand(b0, "main", "off")[0]
check(hand(b0, "main", "snapped")[0] == 0 and off > snap, f"wall: off the centre, past the snap ({off} px > {snap})")
p = b1.get("main_pellets", "")
fd, fc = num(p, "from drawn")[0], num(p, "from centre")[0]
check(fd <= tol and fc > snap, f"wall: its pellets land on the drawn crosshair ({fd} px), not on the centre ({fc} px)")

# dual
b0, b1 = block("dual", 0), block("dual", 1)
merged = num(b0.get("head", ""), "merged")[0]
mx, ox = hand(b0, "main", "drawn", 2)[0], hand(b0, "off", "drawn", 2)[0]
check(hand(b0, "off", "shown")[0] == 1 and merged == 0 and mx > centre[0] and ox < centre[0],
      f"dual: two crosshairs, the right gun's right of the centre ({mx}), the left's left ({ox}; centre {centre[0]})")
for h in ("main", "off"):
    p = b1.get(h + "_pellets", "")
    check(num(p, "from drawn")[0] <= tol, f"dual: the {h} gun's pellets land on its own crosshair ({num(p, 'from drawn')[0]} px)")

# merged, menu, visor
b0 = block("merged", 0)
check(num(b0.get("head", ""), "merged")[0] == 1 and hand(b0, "main", "snapped")[0] == 1 and hand(b0, "off", "snapped")[0] == 1,
      "merged: both guns at the point: one crosshair, on the centre")
check(num(block("menu", 0).get("head", ""), "on")[0] == 0, "menu: not drawn with the menu up")
b0 = block("visor", 0).get("head", "")
check(num(b0, "on")[0] == 1 and num(b0, "drawn")[0] == 1, "visor: drawn with the visor off (vr_flat_visor 0)")

# Pixels. The crosshair's green: well greener than red and blue.
shots = sorted((os.path.join(shots_dir, f) for f in os.listdir(shots_dir) if f.lower().endswith(".png")), key=os.path.getmtime)
check(len(shots) == 12, f"screenshots: 12 taken ({len(shots)})")
if len(shots) == 12:
    imgs = [read_png(s) for s in shots]
    w, h, _ = imgs[0]
    k = w / view[0] if view[0] == view[0] else 1.0

    def green(img, cx, cy, r):
        iw, ih, data = img
        n = sx = sy = 0
        for y in range(max(0, int(cy - r)), min(ih, int(cy + r) + 1)):
            for x in range(max(0, int(cx - r)), min(iw, int(cx + r) + 1)):
                o = (y * iw + x) * 3
                rr, gg, bb = data[o], data[o + 1], data[o + 2]
                if gg > rr + 40 and gg > bb + 30:
                    n, sx, sy = n + 1, sx + x, sy + y
        return n, (sx / n if n else -1), (sy / n if n else -1)

    def at(name, i, hnd):
        return [v * k for v in hand(block(name, i), hnd, "drawn", 2)]

    r = 16 * scale * k
    c = [v * k for v in centre]
    n, gx, gy = green(imgs[0], c[0], c[1], r)
    check(n > 4 and abs(gx - c[0]) <= 1.5 and abs(gy - c[1]) <= 1.5, f"pixels open: drawn on the centre ({n} px, centroid {gx:.1f} {gy:.1f} vs {c[0]:.1f} {c[1]:.1f})")
    d = at("wall", 0, "main")
    n, gx, gy = green(imgs[1], d[0], d[1], r)
    n0, _, _ = green(imgs[1], c[0], c[1], r) if abs(d[0] - c[0]) > 2 * r or abs(d[1] - c[1]) > 2 * r else (0, 0, 0)
    check(n > 4 and abs(gx - d[0]) <= 1.5 and abs(gy - d[1]) <= 1.5 and n0 == 0,
          f"pixels wall: drawn at the hit ({n} px, centroid {gx:.1f} {gy:.1f} vs {d[0]:.1f} {d[1]:.1f}), none on the centre ({n0})")
    for hnd in ("main", "off"):
        d = at("dual", 0, hnd)
        n, gx, gy = green(imgs[2], d[0], d[1], r)
        # (Its side mark pulls the centroid toward its side: a few pixels at most.)
        check(n > 4 and abs(gx - d[0]) <= 4 * scale * k + 1.5 and abs(gy - d[1]) <= 1.5,
              f"pixels dual: the {hnd} gun's crosshair drawn at its hit ({n} px, centroid {gx:.1f} {gy:.1f} vs {d[0]:.1f} {d[1]:.1f})")
    # Styles: each draws at the centre (against the last shot, vr_flat_xhair 0), and each differs from the one before.
    none = imgs[11]
    rr = int(24 * scale * k)

    def crop(img):
        iw, _, data = img
        out = []
        for y in range(int(c[1]) - rr, int(c[1]) + rr + 1):
            o = (y * iw + int(c[0]) - rr) * 3
            out.append(data[o:o + (2 * rr + 1) * 3])
        return b"".join(out)

    def diff(a, b):
        return sum(1 for i in range(0, len(a), 3) if abs(a[i] - b[i]) + abs(a[i + 1] - b[i + 1]) + abs(a[i + 2] - b[i + 2]) > 30)

    base = crop(none)
    prev = None
    names = ["dot", "cross", "cross and dot", "circle", "circle and dot", "T", "chevron", "brackets"]
    for s in range(8):
        cur = crop(imgs[3 + s])
        dn = diff(cur, base)
        dp = diff(cur, prev) if prev is not None else dn
        check(dn > 3 and dp > 0, f"style {s} ({names[s]}): drawn ({dn} px changed against none, {dp} against the last style)")
        prev = cur

print(f"flatxhair_test: {passed} passed, {failed} failed")
sys.exit(1 if failed else 0)
