"""toolgun_physgun_ragdoll_test.py <agent> [--parts <part>] [--knock] [--raw <file>] -- the physgun on a ragdoll
(docs/vr-port/ROUND22.md, "The physgun on ragdolls").

vrfiringrange, a grunt killed 110 units ahead (vr_test_kill; --knock: knocked down alive instead, vr_knockdown_test 0),
the toolgun in the main hand on the physgun: the hand aimed at a part of the ragdoll (vr_mock_hand_aim ... ragdoll <part>),
the trigger held, the target moved along a path by the off stick (64 u/s aside, then up, then 256 u/s back, then
still), let go. Each frame vr_toolgun_status's "ragdoll held" line: the held part's and the root part's places.
Prints per part: the held part's and the root's frame-to-frame jitter (the RMS of the second difference, units), their
largest jump in a frame against the target's, how far the held part ends from the target, and the release's speed.
(--old, vr_toolgun_physgun_ragdoll 0, the entity dragged by its origin, went with that setting in round 23.)
PASS: no jump in a frame over twice the target's fastest plus 4 units, the held part within 12 units of its
target at the end, the root's jitter under 1 unit.
"""
import math
import os
import re
import subprocess
import sys

agent = sys.argv[1] if len(sys.argv) > 1 else "gorefx"
knock = "--knock" in sys.argv
parts = [0, 5]
if "--parts" in sys.argv:
    parts = [int(p) for p in sys.argv[sys.argv.index("--parts") + 1].split(",")]
KIT = "C:/OHWorkspace/qvr-kit"

s = []
a = s.append
# The path: the off stick (the physgun's up, down and aside while it holds) pushed for so many frames at so many
# units a second (vr_toolgun_move_speed), the gun hand still (its aim steady: the target moves only as the sticks
# say). (The script's length is limited, about 8k characters: one part a run.)
path = [(1, 0, 50, 64), (0, 1, 50, 64), (-1, 0, 40, 256), (0, 0, 40, 64)]
parts = parts[:1]
for part in parts:
    a("sv_random_seed 7;map vrfiringrange;wait60;god;notarget;vr_crate_health 0;vr_debug_ragdoll 1")
    a("impulse 169;vr_weapon_grip_mode 1;wait10;vr_toolgun_tool 2")
    a("vr_mock_hand main 0.0 1.3 -0.3 20 0 0;wait5")
    a("vr_test_spawn 0;vr_test_spawn_dist 110;impulse 241;wait40")
    a("vr_knockdown_test 0;wait40" if knock else "vr_test_kill 1;wait400")
    # (walked up to him, the beam steep; his gun lying by him, a prop, gone: not in its way. Knocked down: soon, before
    # he gets up)
    a("vr_scene_clean props;wait5;vr_mock_walk_to 190 -556 8;wait%d;vr_mock_walk_to off;vr_mock_look 0 180;wait5"
      % (60 if knock else 150))
    a("vr_mock_hand_aim main ragdoll %d;wait30;vr_mock_hand_aim main off;wait2" % part)
    a("echo PART%d;vr_toolgun_status" % part)
    a("vr_mock_button main trigger 1;wait3")
    for (x, y, frames, speed) in path:
        a("vr_toolgun_move_speed %d;vr_mock_stick off %d %d" % (speed, x, y))
        a(";".join(["wait1;vr_toolgun_status"] * frames))
    a("vr_mock_stick off 0 0;vr_toolgun_move_speed 64;vr_mock_button main trigger 0;wait3;vr_toolgun_status;wait60;echo DONE%d;vr_ragdoll_list 1" % part)
a("toggleconsole;quit")
script = ";".join(s)

flt = r"toolgun|ragdoll:|^PART|^DONE|held by|ENGINE|CRASH|exit=|TIMEOUT"
BASH = os.environ.get("QVR_BASH", "C:/Program Files/Git/bin/bash.exe")  # (not WSL's)
out = subprocess.run([BASH, KIT + "/run.sh", agent, "-Script", script, "-Timeout", "900", "-Filter", flt],
    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True).stdout
if "--raw" in sys.argv:
    open(sys.argv[sys.argv.index("--raw") + 1], "w").write(out)

line = re.compile(r"toolgun: rag (\w+) ([-\d.]+) (\d+) ([-\d.]+) ([-\d.]+) ([-\d.]+) r ([-\d.]+) ([-\d.]+) ([-\d.]+) "
    r"g ([-\d.]+) ([-\d.]+) ([-\d.]+) e ([-\d.]+)")
blocks = re.split(r"^PART(\d+)", out, flags=re.M)
ok = len(blocks) > 1
for i in range(1, len(blocks), 2):
    part = int(blocks[i])
    body = blocks[i + 1]
    rows = [m for m in (line.search(l) for l in body.splitlines()) if m]
    for l in body.splitlines():
        if "taken by the physgun" in l or "ragdoll by part" in l or "let go" in l or "lost" in l:
            print("  " + l.strip())
    if len(rows) < 10:
        print("PART %d: only %d samples" % (part, len(rows)))
        ok = False
        continue
    t = [float(m.group(2)) for m in rows]
    P = [tuple(float(m.group(k)) for k in (4, 5, 6)) for m in rows]
    R = [tuple(float(m.group(k)) for k in (7, 8, 9)) for m in rows]
    T = [tuple(float(m.group(k)) for k in (10, 11, 12)) for m in rows]

    def d(u, v):
        return math.sqrt(sum((x - y) ** 2 for x, y in zip(u, v)))

    def jitter(X):
        # second difference: a smooth move's is small, a shake's or a jump's large
        acc = [d(tuple(2 * b - a_ - c for a_, b, c in zip(X[k - 1], X[k], X[k + 1])), (0, 0, 0)) for k in range(1, len(X) - 1)]
        return math.sqrt(sum(x * x for x in acc) / len(acc)), max(acc)

    def jumps(X):
        return max(d(X[k], X[k - 1]) for k in range(1, len(X)))

    jp, jpmax = jitter(P)
    jr, jrmax = jitter(R)
    tj = jumps(T)
    lag = [float(m.group(13)) for m in rows]
    end = lag[-1]
    mode = rows[0].group(1)
    print("PART %d (%s, %d frames): held part jitter %.2f (worst %.2f), root %.2f (worst %.2f); biggest jump a frame: "
          "part %.2f, root %.2f, target %.2f; the held point %.1f behind its target at most, %.1f at the end; frames %.4f s" % (part, mode,
          len(rows), jp, jpmax, jr, jrmax, jumps(P), jumps(R), tj, max(lag), end, (t[-1] - t[0]) / (len(t) - 1)))
    bad = jumps(P) > 2 * tj + 4 or jumps(R) > 2 * tj + 4 or jr > 1.0 or end > 12.0
    if bad:
        print("  FAIL: jumps or jitter")
        ok = False
for l in out.splitlines():
    if "held by" in l or "ENGINE" in l or "CRASH" in l or "TIMEOUT" in l:
        print("  " + l.strip())
print("RESULT", "PASS" if ok else "FAIL")
