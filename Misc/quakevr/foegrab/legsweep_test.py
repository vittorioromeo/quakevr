#!/usr/bin/env python3
"""legsweep_test.py <agent> [regex] -- the leg sweep with the mock headset's main hand (ROUND22.md, "Leg sweep").

Each case is one game on vrfiringrange: a grunt spawned ahead (vr_test_spawn 0, 36 units: its origin 1.17 m ahead in
tracking space), a weapon or a prop put in the main hand, then a vr_mock_play motion sweeps the hand level through its
lower legs (tracking space: x right, y up, -z forward), the thing pointing forward and down 35 degrees:
  lr / rl      in front of it, left to right / right to left (6 m/s)
  away / toward  beside it (the hand on its right, the thing pointing left across its shins), from its front to its back
               / back to front (6 m/s): it faces you, so away is along its back, towards along its front
  slow         lr at 0.8 m/s
  hilt         a sword held blade up and back, hand low: its pommel sweeps
PASS: down or not as expected; down, its feet slid along the sweep (legsweep fell: ... feet slid > 0) and its head lies
against the sweep from its feet (toppled the other way); the side sweeps: away falls forward, toward backward (its own
facing). The logs go to <agent>/scratch/legsweep_<case>.txt.
"""
import os
import re
import subprocess
import sys

KIT = "C:/OHWorkspace/qvr-kit"
BASH = "C:/Program Files/Git/bin/bash.exe"
AGENT = sys.argv[1] if len(sys.argv) > 1 else "fastship"
ONLY = sys.argv[2] if len(sys.argv) > 2 else ""
TREE = f"C:/OHWorkspace/qvr-agents/{AGENT}"
MOTIONS = os.path.join(TREE, "quakevr", "motions")
SCRATCH = os.path.join(TREE, "scratch")
PRE = ("sv_random_seed 7;vr_weapon_grip_mode 1;map vrfiringrange;wait30;god;notarget;developer 1;vr_test_spawn 0;vr_test_spawn_dist 36;"
       "impulse 241;wait40")
PITCH = 10

# What the main hand holds: the console commands that put it there.
# (impulse 150 + its weapon id: one lying beside you; vr_test_weaponinst 4: the nearest lying into the main hand)
W = "impulse %d;wait10;vr_test_weaponinst 4;impulse 120"
HOLD = {
    "shotgun": W % 154, "supershotgun": W % 155, "grenadel": W % 158, "rocketl": W % 160,
    "lightning": W % 161, "laser": W % 162, "axe": W % 152, "crowbar": W % 167,
    "sword": W % 163,
    "plank": "vr_test_spawn_hold 1;vr_test_spawn_dist 20;vr_test_spawn 114;impulse 241",
    "crate": "vr_test_spawn_hold 1;vr_test_spawn_dist 20;vr_test_spawn 109;impulse 241",
    "brick": "vr_test_spawn_hold 1;vr_test_spawn_dist 20;vr_test_spawn 115;impulse 241",
}

# (case, held, motion, expected down)
CASES = [
    ("shotgun_lr", "shotgun", "lr", True), ("shotgun_rl", "shotgun", "rl", True),
    ("shotgun_away", "shotgun", "away", True), ("shotgun_toward", "shotgun", "toward", True),
    ("shotgun_slow", "shotgun", "slow", False),
    ("laser_lr", "laser", "lr", True), ("rocketl_lr", "rocketl", "lr", True), ("grenadel_lr", "grenadel", "lr", True),
    ("crowbar_lr", "crowbar", "lr", True), ("plank_lr", "plank", "lr", True),
    ("axe_lr", "axe", "lr", False), ("supershotgun_lr", "supershotgun", "lr", False),
    ("lightning_lr", "lightning", "lr", False), ("sword_lr", "sword", "lr", False), ("sword_hilt", "sword", "hilt", True),
    ("crate_lr", "crate", "lr", False), ("brick_lr", "brick", "lr", False),
]

FAILS = 0


def motion(name, keys):
    os.makedirs(MOTIONS, exist_ok=True)
    with open(os.path.join(MOTIONS, name + ".mock"), "w") as f:
        for t, line in keys:
            f.write(f"{t:.3f} {line}\n")
    return f"quakevr/motions/{name}.mock"


def sweep(kind):
    """The hand's turn (pitch, yaw: the gun's line is about 48 degrees under the controller's) and its (t, line) keys
    relative to the grunt's origin (rel: the hand is put there first, vr_mock_hand_to): still at its start for 0.8 s,
    then swept to its end."""
    if kind in ("lr", "rl", "slow"):
        x0, x1 = (-0.75, 0.75) if kind != "rl" else (0.75, -0.75)
        a, b, turn = (x0, 0.0, 0.35), (x1, 0.0, 0.35), (PITCH, 0)
        dur = 0.25 if kind != "slow" else 1.9
    elif kind in ("away", "toward"):
        z0, z1 = (0.75, -0.75) if kind == "away" else (-0.75, 0.75)
        a, b, turn = (0.45, 0.0, z0), (0.45, 0.0, z1), (PITCH, 90)
        dur = 0.25
    else:  # hilt: the blade up and back past the vertical, the hand low: the pommel forward and down
        a, b, turn = (-0.75, -0.2, 0.55), (0.75, -0.2, 0.55), (168, 0)
        dur = 0.25
    line = "rel main %.3f %.3f %.3f"
    return turn, [(0.0, line % a), (0.8, line % a), (0.8 + dur, line % b), (2.5, line % b)]


def verdict(ok, what):
    global FAILS
    print(f"  {'PASS' if ok else 'FAIL'} {what}")
    if not ok:
        FAILS += 1


def main():
    os.makedirs(SCRATCH, exist_ok=True)
    for name, held, kind, down in CASES:
        if ONLY and not re.search(ONLY, name):
            continue
        turn, keys = sweep(kind)
        path = motion("legsweep_" + name, keys)
        script = (f"{PRE};{HOLD[held]};wait40;vr_mock_hand main 0 0.8 -0.4 {turn[0]} {turn[1]} 0;wait5;"
                  f"vr_mock_hand_to main nearest monster_army 0;wait2;vr_legsweep_debug 1;vr_mock_play {path};wait400;"
                  "toggleconsole;quit")
        r = subprocess.run([BASH, f"{KIT}/run.sh", AGENT, "-RealTime", "-Script", script, "-Filter",
                            "legsweep|not put|rror|CRASH", "-Timeout", "300"], capture_output=True, text=True)
        out = r.stdout.replace("\r", "")
        with open(os.path.join(SCRATCH, f"legsweep_{name}.txt"), "w") as f:
            f.write(out)
        flat = " ".join(out.split())
        swept = "SWEPT DOWN" in flat
        fell = re.search(r"its head ([-\d.]+) along the sweep from its feet \([^)]*\), ([-\d.]+) ahead of them by its "
                         r"facing, ([-\d.]+) to its left: fell (\w+(?: \w+ \w+)?); feet slid ([-\d.]+) along", flat)
        notes = re.findall(r"legsweep: the main hand's thing on \S+ \(\d+\)'s legs: ([^:]+: [^;]*?)(?= legsweep|$)", flat)
        last = notes[-1][:150] if notes else "no contact"
        if down:
            ok = swept and fell is not None and float(fell.group(5)) > 0 and float(fell.group(1)) < 0
            if ok and kind == "away":
                ok = fell.group(4) == "forward"
            if ok and kind == "toward":
                ok = fell.group(4) == "backward"
            what = (f"{name}: swept, its head {fell.group(1)} along the sweep from its feet, fell {fell.group(4)}, feet "
                    f"slid {fell.group(5)}" if fell else f"{name}: {'swept, no trace' if swept else 'not swept'} ({last})")
        else:
            ok = not swept and "rror" not in flat
            what = f"{name}: {'SWEPT' if swept else 'not swept'} ({last})"
        verdict(ok and "CRASH" not in flat, what)
    print(f"{FAILS} failed")
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
