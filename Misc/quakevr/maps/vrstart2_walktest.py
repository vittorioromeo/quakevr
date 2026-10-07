# vrstart2_walktest.py -- walks vrstart2's path headless, one leg at a time (setpos at the leg's start facing its end,
# the mock stick forward for the leg's length at running speed, viewpos at its end), and checks where each leg ended.
#
#   python Misc/quakevr/maps/vrstart2_walktest.py script        # writes quakevr/vs2walk.cfg (git-ignored)
#   bash <kit>/run.sh <agent> -Script "exec vs2walk.cfg" -Filter "Player pos|WP" | python Misc/quakevr/maps/vrstart2_walktest.py check
#
# A leg passes when it ends within 48 units of its end (and 24 in height). (One cfg exec'd, the waits as aliases: the
# kit's -Script expands waitN into N lines, which overflows the command buffer for a walk this long.)
import math
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
# the path's points: (x, y, the player's origin z there)
W = [(-1200, -1310, 48), (-1200, -870, 48), (-1200, -660, 40), (-1200, -470, 136), (-1180, -400, 136), (-1160, -150, 136),
     (-900, -150, 136), (-650, -150, 150), (-420, -150, 136), (-330, -110, 136), (-200, -96, 152), (60, -96, 152),
     (240, -96, 80), (380, -40, 80), (390, -260, 80), (520, -470, 84), (760, -640, 82), (980, -676, 80), (1150, -640, 80)]


def wait(n):
    return ["w10"] * (n // 10) + ["wait"] * (n % 10)


def script():
    out = ['alias w10 "wait;wait;wait;wait;wait;wait;wait;wait;wait;wait"', "map vrstart2"] + wait(30)
    for (ax, ay, az), (bx, by, bz) in zip(W, W[1:]):
        yaw = math.degrees(math.atan2(by - ay, bx - ax))
        frames = max(1, int(math.hypot(bx - ax, by - ay) / 320 * 72))
        out += ["setpos %d %d %d 0 %.1f 0" % (ax, ay, az + 20, yaw), "noclip"] + wait(10) + ["vr_mock_look 0 0"] + wait(2)
        out += ["vr_mock_stick off 0 1"] + wait(frames) + ["vr_mock_stick off 0 0"] + wait(10)
        out += ["echo WP %d %d %d" % (bx, by, bz), "viewpos"]
    out += ["toggleconsole", "quit"]
    path = os.path.join(ROOT, "quakevr", "vs2walk.cfg")
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    print("wrote %s: %d legs" % (path, len(W) - 1))


def check():
    wp, ok, n = None, 0, 0
    for line in sys.stdin.read().splitlines():
        m = re.match(r"\s*WP (-?\d+) (-?\d+) (-?\d+)", line)
        if m:
            wp = tuple(map(int, m.groups()))
            continue
        m = re.search(r"Player pos: \((-?[\d.]+) (-?[\d.]+) (-?[\d.]+)\)", line)
        if m and wp:
            p = tuple(float(v) for v in m.groups())
            miss = math.hypot(p[0] - wp[0], p[1] - wp[1])
            good = miss < 48 and abs(p[2] - wp[2]) < 24
            ok += good
            n += 1
            print("%-18s -> %-16s miss %4.0f dz %4.0f %s" % (wp, "%d %d %d" % p, miss, p[2] - wp[2], "OK" if good else "FAIL"))
            wp = None
    print("%d of %d legs" % (ok, n))


if __name__ == "__main__":
    {"script": script, "check": check}[sys.argv[1] if len(sys.argv) > 1 else "script"]()
