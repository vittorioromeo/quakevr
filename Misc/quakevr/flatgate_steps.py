#!/usr/bin/env python3
"""flatgate_steps.py <log> [--turn DEG]: from vr_flatplay_debug 2's flatview lines, what a flat-screen teleporter does to the
view, frame by frame (flatgate_test.sh). Prints key=value words.

  pitch     the largest frame-to-frame change of the view's pitch, the frames a setpos moved him left out
  gun       the gun's largest frame-to-frame step in the camera's frame at a crossing's frame or the next two
  crossings how many crossings the log has
  gatestep  with --turn: at each crossing the yaw's step less the gate's turn less the frames around it (0: the turn alone)
"""
import re
import sys

LINE = re.compile(r"^flatview: t (\S+) local (\S+) (\S+) (\S+) yaw \S+ pitch \S+ roll \S+ view (\S+) (\S+)")


def off(a, b):
    return (a - b + 180.0) % 360.0 - 180.0


def main():
    path = sys.argv[1]
    turn = float(sys.argv[sys.argv.index("--turn") + 1]) if "--turn" in sys.argv else None
    frames = []
    crossAt = []
    for raw in open(path, encoding="latin-1"):
        if raw.startswith("VR portal: carried"):
            crossAt.append(len(frames))
            continue
        m = LINE.match(raw)
        if m:
            frames.append(tuple(float(x) for x in m.groups()))
    pitch = 0.0
    gun = 0.0
    gatestep = 0.0
    for at in crossAt:
        lo = max(1, at - 1)
        hi = min(len(frames), at + 4)
        best = 0.0
        for i in range(lo, hi):
            a, b = frames[i - 1], frames[i]
            pitch = max(pitch, abs(b[4] - a[4]))
            gun = max(gun, ((b[1] - a[1]) ** 2 + (b[2] - a[2]) ** 2 + (b[3] - a[3]) ** 2) ** 0.5)
            step = off(b[5], a[5])
            if abs(step) > abs(best):
                best = step
        if turn is not None:
            # (what a frame of the turn itself is: the frames either side of the crossing)
            around = [off(frames[i][5], frames[i - 1][5]) for i in range(max(1, lo - 4), lo)]
            own = sum(around) / len(around) if around else 0.0
            gatestep = max(gatestep, abs(abs(best - own) - abs(turn)))
    words = ["pitch=%.3f" % pitch, "gun=%.3f" % gun, "crossings=%d" % len(crossAt), "frames=%d" % len(frames)]
    if turn is not None:
        words.append("gatestep=%.3f" % gatestep)
    print(" ".join(words))


main()
