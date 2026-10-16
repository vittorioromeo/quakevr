#!/usr/bin/env python3
"""gripcrowd_test.py <agent> <tag> -- foegrab's grip search in a ring of 10 grunts: both mock hands grip at 30 places
each (vr_foegrab_debug 2's 'foegrab: search' lines: the result and its time). Writes scratch/gripcrowd_<tag>.txt."""
import os
import re
import subprocess
import sys

KIT = "C:/OHWorkspace/qvr-kit"
AGENT = sys.argv[1]
TAG = sys.argv[2]
TREE = f"C:/OHWorkspace/qvr-agents/{AGENT}"
MOTIONS = os.path.join(TREE, "quakevr", "motions")
os.makedirs(MOTIONS, exist_ok=True)

keys = []
n = 30
for i in range(n):
    t = 0.5 * i
    # a deterministic spread over the ring's near side: x across, y up, z forward (tracking space, metres)
    x = -0.9 + 1.8 * ((i * 7) % n) / (n - 1)
    y = 0.35 + 0.75 * ((i * 11) % n) / (n - 1)
    z = -1.0 - 0.7 * ((i * 13) % n) / (n - 1)
    keys.append((t, "main %.3f %.3f %.3f" % (x, y, z)))
    keys.append((t, "off %.3f %.3f %.3f" % (-x, y, z + 0.1)))
    keys.append((t + 0.05, "cmd +grabmain"))
    keys.append((t + 0.05, "cmd +graboff"))
    keys.append((t + 0.35, "cmd -grabmain"))
    keys.append((t + 0.35, "cmd -graboff"))
with open(os.path.join(MOTIONS, "gripcrowd.mock"), "w") as f:
    for t, line in keys:
        f.write(f"{t:.3f} {line}\n")

spawn = ""
for k in range(10):
    spawn += f"vr_mock_turn_to {k * 36};wait40;impulse 241;wait5;"
script = ("sv_random_seed 7;map vrfiringrange;wait30;god;notarget;developer 1;vr_foegrab_debug 2;vr_foegrab_leniency 6;"
          "vr_test_spawn 0;vr_test_spawn_dist 56;" + spawn +
          "vr_mock_turn_to 0;wait40;vr_mock_turn_to off;wait10;vr_mock_play quakevr/motions/gripcrowd.mock;wait1300;"
          "toggleconsole;quit")
extra = sys.argv[3:]
out = subprocess.run(["C:/Program Files/Git/bin/bash.exe", f"{KIT}/run.sh", *extra, AGENT, "-Script", script, "-Filter", "foegrab: search|takes hold|rror|CRASH",
                      "-Timeout", "400"], capture_output=True, text=True).stdout.replace("\r", "")
lines = [l for l in out.splitlines() if "foegrab: search" in l]
with open(os.path.join(TREE, "scratch", f"gripcrowd_{TAG}.txt"), "w") as f:
    f.write(out)
ms = [float(m.group(1)) for l in lines if (m := re.search(r", ([0-9.]+) ms", l))]
old = sorted(float(m.group(1)) for l in lines if (m := re.search(r"\(the old ([0-9.]+)\)", l)))
same = sum(1 for l in lines if ", same:" in l)
models = [int(m.group(1)) for l in lines if (m := re.search(r": (\d+) models", l))]
takes = sum(1 for l in out.splitlines() if "takes hold" in l)
ms.sort()
if ms:
    print(f"{TAG}: {len(ms)} searches, {takes} takes, models avg {sum(models) / len(models):.1f} max {max(models)}; "
          f"ms avg {sum(ms) / len(ms):.3f} median {ms[len(ms) // 2]:.3f} p95 {ms[int(len(ms) * 0.95)]:.3f} max {ms[-1]:.3f}" + (f"; old ms avg {sum(old) / len(old):.3f} median {old[len(old) // 2]:.3f} p95 {old[int(len(old) * 0.95)]:.3f} max {old[-1]:.3f}; same {same}/{len(lines)}" if old else ""))
else:
    print(f"{TAG}: no searches; tail:\n" + "\n".join(out.splitlines()[-8:]))
