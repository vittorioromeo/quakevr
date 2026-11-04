#!/usr/bin/env python3
# vr_movement_mode (Move Towards), on the mock: with the head and the hands turned away from each other and from the torso,
# each mode must walk along what it says. Without body tracking (the torso estimated) and with the mock's synthetic tracked
# body (the head twisted away from the hips). A counted walk per mode; the run ends on the last report.
#   python Misc/quakevr/move_towards_test.py <worktree name>
import os
import re
import sys

import qvrtest

TOLERANCE = 3.0  # degrees between where he walks and what steers
APART = 8.0      # degrees the torso must be from the head and the hands, or the case shows nothing


def walk(label, mode):
    # (Noclip: no wall stops a walk, so the hands level: their pitch would take speed into climbing. And back to the start.)
    return (f"vr_movement_mode {mode};vr_mock_stick off 0 1;wait40;vr_move_report {label};"
            "vr_mock_stick off 0 -1;wait40;vr_mock_stick off 0 0;wait30;")


SETUP = ("map vrfiringrange;wait30;god;notarget;noclip;vr_gunangle 0;vr_offhandpitch 0;vr_snap_turn 0;vr_stick_swap 0;vr_move_snap 15;"
         "vr_mock_look 0 50;vr_mock_hand off -0.2 1.2 -0.3 0 -60 0;vr_mock_hand main 0.2 1.2 -0.3 0 -20 0;wait90;")
TRACKED = "vr_bodytrack 1;vr_bodytrack_synth 1;vr_bodytrack_synth_twist 50;wait90;"
CASES = [("head", 1, "head"), ("hand", 0, "left"), ("left", 2, "left"), ("right", 3, "right"), ("torso", 4, "torso")]
script = SETUP + "".join(walk(label, mode) for label, mode, _ in CASES)
script += TRACKED + walk("tracked", 4) + walk("trackedhead", 1) + "vr_move_report end;toggleconsole;quit"

out = qvrtest.run_sh(sys.argv[1], ["-Filter", "^move report", "-QuitOn", "move report end", "-Timeout", "180",
                                   "-Script", script], env=dict(os.environ, KIT=qvrtest.KIT, QVR_AUTHOR_WAIT_ALWAYS="1"))

rows = {}
for m in re.finditer(r"move report (\w+): mode (-?\d+) head (\S+) torso (\S+) left (\S+) right (\S+) walk (\S+) speed (\S+)", out):
    rows[m.group(1)] = dict(zip(("mode", "head", "torso", "left", "right", "walk", "speed"), map(float, m.groups()[1:])))


def off(a, b):
    return abs((a - b + 180.0) % 360.0 - 180.0)


tally = qvrtest.Tally("move_towards_test")
for label, steer in [(c[0], c[2]) for c in CASES] + [("tracked", "torso"), ("trackedhead", "head")]:
    r = rows.get(label)
    if not r:
        tally.check(False, f"{label}: no report")
        continue
    tally.check(r["speed"] > 100.0 and off(r["walk"], r[steer]) <= TOLERANCE,
                f"{label}: mode {r['mode']:.0f} walk {r['walk']:.1f} along {steer} {r[steer]:.1f} "
                f"(head {r['head']:.1f} torso {r['torso']:.1f} left {r['left']:.1f} right {r['right']:.1f}, speed {r['speed']:.0f})")
for label, others in [("torso", ("head", "left", "right")), ("tracked", ("head",))]:
    r = rows.get(label)
    if r:
        tally.check(all(off(r["torso"], r[o]) >= APART for o in others),
                    f"{label}: the torso {r['torso']:.1f} stands apart from {', '.join(others)}")
sys.exit(tally.verdict())
