"""Writes a mock: both hands (rel keys) chop down into the floor at a few speeds, held there 1 s, then back up slowly.
Usage: python gen_chop.py <out.mock> [onehand]"""
import sys

hands = ["main"] if len(sys.argv) > 2 and sys.argv[2] == "onehand" else ["main", "off"]
lines = ["0.000 cmd echo STEP setup"]
t = 0.5
y = 0.0


def key(tt, yy, zz=0.0):
    for h in hands:
        lines.append("%.3f rel %s 0 %.4f %.4f" % (tt, h, yy, zz))


def move(to, speed, fwd=0.0):
    global t, y
    dist = abs(to - y)
    total = max(dist / speed, 0.02)
    n = max(2, int(total / 0.01))
    y0 = y
    for i in range(1, n + 1):
        u = i / n
        key(t + u * total, y0 + (to - y0) * u, -fwd * (u if to < y0 else 1 - u))
    t += total
    y = to


def pause(d):
    global t
    key(t + d, y)
    t += d


key(t, y)
pause(0.5)
for v in (2.0, 3.5, 5.0):
    lines.append("%.3f cmd echo STEP chop_%.1f" % (t, v))
    move(0.3, 0.5)
    pause(0.4)
    move(-1.3, v)  # (straight on into the floor: the hand is stopped by it)
    pause(1.0)
    move(0.0, 0.4)
    pause(0.5)
lines.append("%.3f cmd echo STEP end" % t)
open(sys.argv[1], "w").write("\n".join(lines) + "\n")
print("%.1f s" % t)
