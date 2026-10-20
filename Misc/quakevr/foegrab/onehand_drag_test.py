#!/usr/bin/env python3
"""onehand_drag_test.py <agent> [regex] -- the one-hand drag by part (vr_foegrab_drag_onehand; ROUND22.md, "Mantle
height, one-hand drag by part, shallower dents").

Each case is one game on vrfiringrange (batched: testbatch.py): a grunt spawned ahead facing you (36 units), the main
hand alone put on it (vr_mock_hand_to main nearest monster_army <dz>), gripped at 0.15 s, then pulled 0.4 m towards you
in 1 s (0.4 m/s: far under any leg pull) and held. vr_foegrab_status before the pull (0.55 s) and after (1.75 s): the
grunt's origin and the hold's part.
  shin   dz -21: by its leg, doesn't drag (moves under 6 units, or the hold breaks: pulled too far)
  chest  dz 8: by its torso, drags (comes 6 units or more towards you)
  head   dz 23: by its head, doesn't drag (as the shin)
  shin_any  the shin with One Hand Drags By: Anywhere (0): drags, as before
PASS per case; the logs go to <agent>/scratch/onehanddrag_<case>.txt.
"""
import math
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "testbatch"))
import testbatch  # noqa: E402

os.environ.setdefault("TB_NODRAW", "1")
AGENT = sys.argv[1] if len(sys.argv) > 1 else "fastship"
ONLY = sys.argv[2] if len(sys.argv) > 2 else ""
TREE = f"C:/OHWorkspace/qvr-agents/{AGENT}"
MOTIONS = os.path.join(TREE, "quakevr", "motions")
SCRATCH = os.path.join(TREE, "scratch")
PRE = ("sv_random_seed 7;map vrfiringrange;wait30;god;notarget;developer 1;vr_foegrab_debug 1;vr_foegrab_leniency 12;"
       "vr_test_spawn_facing 0;vr_test_spawn 0;vr_test_spawn_dist 36;impulse 241;wait40")
FILTER = "foegrab|leg diag|MARK|[Ee]rror|CRASH"
# (case, dz under/over its origin, extra commands, drags)
CASES = [
    ("shin", -21, "", False),
    ("chest", 8, "", True),
    ("head", 23, "", False),
    ("shin_any", -21, "vr_foegrab_drag_onehand 0", True),
]


def keys(dz):
    k = [(0.0, f"cmd vr_mock_hand_to main nearest monster_army {dz}"), (0.03, "rel main 0 0 0"),
         (0.1, "rel main 0 0 0"), (0.15, "cmd +grabmain"), (0.5, "cmd echo MARK0"), (0.55, "cmd vr_foegrab_status"),
         (0.6, "rel main 0 0 0"), (1.6, "rel main 0 0 0.4"), (1.7, "cmd echo MARK1"), (1.75, "cmd vr_foegrab_status")]
    os.makedirs(MOTIONS, exist_ok=True)
    path = os.path.join(MOTIONS, f"onehanddrag_{dz}.mock")
    with open(path, "w") as f:
        for t, line in k:
            f.write(f"{t:.3f} {line}\n")
    return f"quakevr/motions/onehanddrag_{dz}.mock"


def main():
    os.makedirs(SCRATCH, exist_ok=True)
    b = testbatch.Batch(AGENT, "onehanddrag")
    handles = {}
    for name, dz, extra, drags in CASES:
        if ONLY and not re.search(ONLY, name):
            continue
        script = f"{PRE};{extra};vr_mock_play {keys(dz)};wait300;-grabmain;wait5;toggleconsole;quit"
        handles[name] = b.case(["-Script", script, "-Filter", FILTER, "-Timeout", "200"])
    b.run()
    fails = 0
    for name, dz, extra, drags in CASES:
        if name not in handles:
            continue
        out = b.output(handles[name]).replace("\r", "")
        with open(os.path.join(SCRATCH, f"onehanddrag_{name}.txt"), "w") as f:
            f.write(out)
        st = re.findall(r"foegrab status: player 1 main hand holds \S+ \(\d+\) at \(([-\d.]+) ([-\d.]+) ([-\d.]+)\):.*?; "
                        r"by ([^:]+): (drags|doesn't drag)", out)
        pulled = "PULLED DOWN" in out
        broke = re.search(r"main hand lets go of \S+ \(\d+\): pulled too far from the spot \(held ([\d.]+) s", out)
        if len(st) == 1 and broke and not drags:
            # (Not dragged, the hand pulled on past the break: let go, the grunt where it stood.)
            ok = st[0][4] == "doesn't drag" and not pulled
            what = f"by {st[0][3]}: {st[0][4]}; the hold broke at {broke.group(1)} s (pulled too far), not dragged"
        elif len(st) < 2:
            ok, what = False, f"{len(st)} status lines"
        else:
            a, z = st[0], st[-1]
            moved = math.dist((float(a[0]), float(a[1])), (float(z[0]), float(z[1])))
            ok = (moved >= 6.0) == drags and (z[4] == "drags") == drags and not pulled
            what = f"by {z[3]}: {z[4]}, moved {moved:.1f} units{' PULLED DOWN' if pulled else ''}"
        print(f"  {'PASS' if ok else 'FAIL'} {name}: {what}")
        fails += 0 if ok else 1
    print("ALL PASS" if fails == 0 else f"{fails} FAILED")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
