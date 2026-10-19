#!/usr/bin/env python3
"""legsweep_test.py <agent> [regex] -- the leg sweep with the mock headset's main hand (ROUND22.md, "Leg sweep").

Each case is one game on vrfiringrange (fixed frames, vr_fixed_frames 1: the mock plays its motions on the game's clock,
the same ticks every run; LS_REALTIME=1 runs on the wall clock, as before 2026-10-19, when which cases failed changed
from run to run): a grunt spawned ahead
(vr_test_spawn 0, 36 units: its origin 1.2 m ahead in tracking space, GRUNT), a weapon or a prop put in the main hand
(vr_weapon_grip_mode 1: it stays there without the grip held), then a vr_mock_play motion sweeps the hand level through
its lower legs (tracking space: x right, y up, -z forward), the thing pointing forward and down about 38 degrees (a gun's
line is about 48 degrees under the controller's pitch):
  lr / rl      in front of it, left to right / right to left (1.5 m in 0.25 s; the hand arrives at 2-3 m/s)
  away / toward  beside it (the hand on its right, the thing pointing left across its shins), from its front to its back
               / back to front: it faces you, so away is along its back, towards along its front
  slow         lr over 1.9 s (under 1 m/s)
  hilt         a sword held blade up and back past the vertical, the hand low: its pommel sweeps
PASS: down or not as expected; down, its feet slid along the sweep (legsweep fell: ... feet slid > 0) and its head lies
against the sweep from its feet (toppled the other way); the side sweeps: away falls forward, toward backward (its own
facing). The logs go to <agent>/scratch/legsweep_<case>.txt.
"""
import os
import re
import subprocess
import sys
import time

KIT = "C:/OHWorkspace/qvr-kit"
BASH = "C:/Program Files/Git/bin/bash.exe"
AGENT = sys.argv[1] if len(sys.argv) > 1 else "fastship"
ONLY = sys.argv[2] if len(sys.argv) > 2 else ""
TREE = f"C:/OHWorkspace/qvr-agents/{AGENT}"
MOTIONS = os.path.join(TREE, "quakevr", "motions")
SCRATCH = os.path.join(TREE, "scratch")
# vr_fixed_frames 1: every frame 1/72 s of game time with a server tick, and vr_mock_play's clock (realtime) advances
# by the same frames, so the sweep's poses land on the same ticks every run (on the wall clock they beat against the
# server's ticks and the machine's load: which frame met the legs, if any, changed from run to run); with
# sv_random_seed the grunt's AI plays the same too. LS_REALTIME=1: the old wall-clock runs, for comparison.
REALTIME = os.environ.get("LS_REALTIME", "") == "1"
# LS_RATE=<Hz>: the headset's frames at that rate (vr_fixed_frames_rate; the server's ticks stay 72 Hz); LS_STUTTER=<n>:
# every n-th frame dropped (vr_fixed_frames_stutter). The sweep's frame-rate independence (ROUND22.md).
RATE = os.environ.get("LS_RATE", "")
STUTTER = os.environ.get("LS_STUTTER", "")
PRE = ("" if REALTIME else "vr_fixed_frames 1;") + \
    (f"vr_fixed_frames_rate {RATE};" if RATE else "") + (f"vr_fixed_frames_stutter {STUTTER};" if STUTTER else "") + \
    "sv_random_seed 7;vr_weapon_grip_mode 1;map vrfiringrange;wait30;god;notarget;developer 1"
SPAWN = "vr_test_spawn 0;vr_test_spawn_dist 36;impulse 241;wait5"  # (the grunt last: a grunt left standing wanders)
GRUNT = (0.0, 0.72, -1.2)  # its origin in tracking space (vr_mock_hand_to main nearest monster_army 0)

# What the main hand holds: the console commands that put it there, and the controller's pitch that points it forward
# and down about 38 degrees (a gun's line is about 48 degrees under the controller's, a melee weapon's and a prop's along
# it). (impulse 150 + its weapon id: in the main hand (vr_test_weaponinst 4 then finds the hand full: a no-op kept
# for older builds). A prop: in the empty main hand, gripped: vr_test_spawn_hold, +grabmain.)
W = "impulse %d;wait10;vr_test_weaponinst 4;impulse 120"
P = "+grabmain;vr_test_spawn_hold 1;vr_test_spawn_dist 20;vr_test_spawn %d;impulse 241"
GUN, MELEE = 10, -40
# The laser cannon's line (hand to muzzle) is longer than the other guns' (42 units) and steeper: at GUN its muzzle rested
# on the floor (77 degrees down; vr_legsweep_debug 2), dragged along it stuck, then jumped past the legs in one frame
# (ignored as a teleport: the case's "no contact"). Held 40 degrees higher: its line about 40 degrees down.
LASER = GUN + 40
HOLD = {
    "shotgun": (W % 154, GUN), "supershotgun": (W % 155, GUN), "grenadel": (W % 158, GUN), "rocketl": (W % 160, GUN),
    "lightning": (W % 161, GUN), "nailgun": (W % 156, GUN), "laser": (W % 162, LASER), "axe": (W % 152, MELEE), "crowbar": (W % 167, MELEE),
    "sword": (W % 163, MELEE), "plank": (P % 114, MELEE), "crate": (P % 107, MELEE), "brick": (P % 115, MELEE),
}
# Metres lower and further forward the hand sweeps (else at the grunt's origin's height, 0.35 m short of it: where a held
# line pointing forward and down reaches its shins): a crate or a brick in the hand reaches nothing beyond it, so it
# swept over and in front of its legs and met nothing (the cases' "no contact"). (The crate: the small one, 107.)
SHIFT = {"crate": (0.3, 0.3), "brick": (0.3, 0.3)}
PROPS = ("crate", "brick")  # (put in the hand at the sweep's start; the plank as before: taken there, it is held at
# another angle and met nothing)

# (case, held, motion, expected down)
CASES = [
    ("shotgun_lr", "shotgun", "lr", True), ("shotgun_rl", "shotgun", "rl", True),
    ("shotgun_away", "shotgun", "away", True), ("shotgun_toward", "shotgun", "toward", True),
    ("shotgun_slow", "shotgun", "slow", False),
    ("laser_lr", "laser", "lr", True), ("rocketl_lr", "rocketl", "lr", True), ("grenadel_lr", "grenadel", "lr", True),
    ("crowbar_lr", "crowbar", "lr", True), ("plank_lr", "plank", "lr", True),
    ("axe_lr", "axe", "lr", False), ("supershotgun_lr", "supershotgun", "lr", True),
    ("lightning_lr", "lightning", "lr", True), ("nailgun_lr", "nailgun", "lr", True),
    ("sword_lr", "sword", "lr", False), ("sword_hilt", "sword", "hilt", True),
    ("crate_lr", "crate", "lr", False), ("brick_lr", "brick", "lr", False),
    # (a gun held still, walked into its legs; pushed slowly into them: no sweep)
    ("shotgun_walk", "shotgun", "walk", False), ("shotgun_brush", "shotgun", "brush", False),
]
SPAWN_DIST = {"walk": 48}  # units (36: GRUNT's place); walk: 12 further, walked up to

FAILS = 0
EXTRA = os.environ.get("LS_EXTRA", "") or "echo"  # (more commands for every case: experiments)


def motion(name, keys):
    os.makedirs(MOTIONS, exist_ok=True)
    with open(os.path.join(MOTIONS, name + ".mock"), "w") as f:
        for t, line in keys:
            f.write(f"{t:.3f} {line}\n")
    return f"quakevr/motions/{name}.mock"


def sweep(kind, pitch, shift=(0.0, 0.0)):
    """The hand's (t, line) keys, placed from the grunt's origin (`shift`: m lower, m further forward): still at its start
    for 0.3 s, then swept to its end."""
    if kind in ("lr", "rl", "slow"):
        x0, x1 = (-0.75, 0.75) if kind != "rl" else (0.75, -0.75)
        a, b, turn = (x0, 0.0, 0.35), (x1, 0.0, 0.35), (pitch, 0)
        dur = 0.25 if kind != "slow" else 1.9
    elif kind in ("walk", "brush"):
        # walk: the hand held still where lr's sweep crosses its shins, the player walked into its legs (the stick
        # pushed from 0.3 s to 1.3 s; the grunt spawned further: SPAWN_DIST); brush: the hand pushed slowly forward
        # into them (0.45 m in 0.6 s: 0.75 m/s)
        a, b, turn = (-0.17, 0.0, 0.75 if kind == "brush" else 0.35), (-0.17, 0.0, 0.3 if kind == "brush" else 0.35), (pitch, 0)
        dur = 0.6
    elif kind in ("away", "toward"):
        z0, z1 = (0.75, -0.75) if kind == "away" else (-0.75, 0.75)
        a, b, turn = (0.45, 0.0, z0), (0.45, 0.0, z1), (pitch, 90)
        dur = 0.25
    else:  # hilt: a melee weapon's blade up and back, 30 degrees past the vertical, the hand low: its pommel forward, down
        a, b, turn = (-0.75, -0.2, 0.3), (0.75, -0.2, 0.3), (118, 0)
        dur = 0.25
    line = "main %.3f %.3f %.3f %.1f %.1f 0"
    a, b = [(p[0] + GRUNT[0], p[1] + GRUNT[1] - shift[0], p[2] + GRUNT[2] - shift[1]) + turn for p in (a, b)]
    keys = [(0.0, line % a), (0.3, line % a), (0.3 + dur, line % b), (2.0, line % b)]
    if kind == "walk":
        keys += [(0.3, "cmd vr_mock_stick off 0 1"), (1.3, "cmd vr_mock_stick off 0 0")]
    return keys


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
        hold, pitch = HOLD[held]
        keys = sweep(kind, pitch, SHIFT.get(held, (0.0, 0.0)))
        path = motion("legsweep_" + name, keys)
        spawn = SPAWN.replace("vr_test_spawn_dist 36", f"vr_test_spawn_dist {SPAWN_DIST.get(kind, 36)}")
        # (A prop is put in the hand where it already is, at the sweep's start: a carried crate jumped there with the hand
        # was let go, "carry: stopped at the body, let go".)
        first = f"vr_mock_hand {keys[0][1]};wait5;{hold};wait40" if held in PROPS else \
            f"{hold};wait40;vr_mock_hand {keys[0][1]};wait5"
        script = (f"{PRE};{first};{spawn};"
                  f"vr_legsweep_debug 1;{EXTRA};vr_mock_play {path};wait400;"
                  "toggleconsole;quit")
        keep = "legsweep|not put|let go|rror|CRASH|TIMEOUT"
        log = f"{KIT}/bases/{AGENT}/qbase/qconsole.log"
        started = time.time()
        r = subprocess.run([BASH, f"{KIT}/run.sh", AGENT] + (["-RealTime"] if REALTIME else []) +
                           ["-Script", script, "-Filter", keep, "-Timeout", "300"], capture_output=True, text=True)
        # (The game's own log, written to the end: the game has quit. run.sh's output wraps long lines. A log older than
        # the run is an earlier case's: this run never started the game.)
        if not os.path.exists(log) or os.path.getmtime(log) < started - 1:
            verdict(False, f"{name}: no game log from this run ({r.stdout.strip()[-200:]})")
            continue
        with open(log, errors="replace") as f:
            out = "\n".join(line for line in f.read().splitlines() if re.search(keep, line))
        out += "\n" + r.stdout.replace("\r", "")[-300:]
        with open(os.path.join(SCRATCH, f"legsweep_{name}.txt"), "w") as f:
            f.write(out)
        flat = " ".join(out.split())
        swept = "SWEPT DOWN" in flat
        fell = re.search(r"its head ([-\d.]+) along the sweep from its feet, ([-\d.]+) across it \([^)]*\), ([-\d.]+) "
                         r"ahead of them by its facing, ([-\d.]+) to its left: fell (\w+(?: \w+ \w+)?) \(swept towards "
                         r"(its \w+)\); feet slid ([-\d.]+) along", flat)
        notes = re.findall(r"legsweep: the main hand's thing on \S+ \(\d+\)'s legs: ([^:]+: [^;]*?)(?= legsweep|$)", flat)
        dropped = re.search(r"(test spawn: \S+ not put in the main hand|carry: [^:]*let go)", flat)
        last = notes[-1][:150] if notes else "no contact" + (f"; {dropped.group(1)}" if dropped else "")
        if down:
            # Its feet slid along the sweep and its head lies against it from them (toppled the other way, more than
            # aside); from its side (along its facing), it fell the other way of its facing from where the sweep went.
            along, across, slid = (float(fell.group(1)), float(fell.group(2)), float(fell.group(7))) if fell else (0, 0, 0)
            ok = swept and fell is not None and slid > 0 and along < 0 and abs(across) < -along
            opposite = {"its front": "backward", "its back": "forward", "its left": "to its right",
                        "its right": "to its left"}
            if ok and kind in ("away", "toward"):
                ok = fell.group(6) in ("its front", "its back") and fell.group(5) == opposite[fell.group(6)]
            what = (f"{name}: swept towards {fell.group(6)}, fell {fell.group(5)}; its head {along:.1f} along the sweep "
                    f"from its feet, {across:.1f} across; feet slid {slid:.1f}" if fell else
                    f"{name}: {'swept, no trace' if swept else 'not swept'} ({last})")
        else:
            # (it met the legs, and said why not; "rror" alone matched the shader log's "vr mirror")
            ok = not swept and bool(notes) and not re.search(r"\b[Ee]rror", flat)
            what = f"{name}: {'SWEPT' if swept else 'not swept'} ({last})"
        verdict(ok and "CRASH" not in flat, what)
    print(f"{FAILS} failed")
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
