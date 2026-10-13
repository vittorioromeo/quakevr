#!/usr/bin/env python3
"""judo_test.py <agent> [cases...] -- the two-hand judo throw with the mock headset's hands (ROUND22.md, "Judo: the
throw's direction, held throws and lifts").

Each case is one game on vrfiringrange: a grunt spawned 60 units ahead (vr_test_spawn 0), both mock hands put on it and
gripped (+grabmain, +graboff), then a vr_mock_play motion pulls them (tracking space: x right, y up, -z forward).

  dir      pulls left, right, towards you and away (vr_foegrab_throw_away 1) with the hands at its shoulders, at its box's
           middle, low on it, and uneven (the main hand higher and faster aside): PASS each when the throw goes the
           pulled way ("foegrab: both hands turn ... to your left") and its ragdoll lies that side of where it stood
           ("throw trace done: ... lies <f> ahead, <l> to your left").

Prints PASS/FAIL per case and the FAIL count; the logs' lines go to scratch/judo_test_<case>.txt (overwritten).
"""
import os
import re
import subprocess
import sys

KIT = "C:/OHWorkspace/qvr-kit"
AGENT = sys.argv[1] if len(sys.argv) > 1 else "foegrab3"
TREE = f"C:/OHWorkspace/qvr-agents/{AGENT}"
CASES = sys.argv[2:] or ["dir"]
MOTIONS = os.path.join(TREE, "quakevr", "motions")  # (git-ignored: quakevr/.gitignore)
SCRATCH = os.path.join(TREE, "scratch")
PRE = ("sv_random_seed 7;map vrfiringrange;wait30;god;notarget;developer 1;vr_foegrab_debug 1;vr_foegrab_leniency 6;"
       "vr_test_spawn 0;vr_test_spawn_dist 36;impulse 241;wait40" +
       (";" + os.environ["JUDO_EXTRA"] if os.environ.get("JUDO_EXTRA") else ""))  # (JUDO_EXTRA: more commands)

# The grunt's box (its origin 1.93 m ahead; the hand's point 0.896 m up in tracking space is 20 units over its origin,
# 31 units a metre): its shoulders, its box's middle (origin + 8), low (origin - 4).
HEIGHTS = {"high": 1.02, "mid": 0.51, "low": 0.35}
NEAR = -1.15   # the hands' depth (on its near side)
GAP = 0.09     # each hand this far from the middle, aside
PULL = 0.45    # metres pulled in PULL_T seconds (3 m/s)
PULL_T = 0.15
WAYS = {"left": (-1, 0), "right": (1, 0), "towards": (0, 1), "away": (0, -1)}  # (x, z) in tracking space
WORDS = {"left": "to your left", "right": "to your right", "towards": "towards you", "away": "away from you"}

FAILS = 0
ONLY = os.environ.get("JUDO_ONLY", "")  # a regex: only the cases whose names (judo_<height>_<way>) match


def verdict(ok, what):
    global FAILS
    print(f"  {'PASS' if ok else 'FAIL'} {what}")
    if not ok:
        FAILS += 1


def run(script, filt, timeout=300, extra=()):
    cmd = ["C:/Program Files/Git/bin/bash.exe", f"{KIT}/run.sh", AGENT, "-Script", script, "-Filter", filt, "-Timeout", str(timeout), *extra]
    out = subprocess.run(cmd, capture_output=True, text=True, errors="replace").stdout
    return out.replace("\r", "")


def motion(name, keys):
    """Writes quakevr/motions/<name>.mock from (t, line) pairs; returns its path for vr_mock_play."""
    os.makedirs(MOTIONS, exist_ok=True)
    with open(os.path.join(MOTIONS, name + ".mock"), "w") as f:
        for t, line in keys:
            f.write(f"{t:.3f} {line}\n")
    return f"quakevr/motions/{name}.mock"


def hands_at(t, main, off):
    return [(t, "main %.3f %.3f %.3f" % main), (t, "off %.3f %.3f %.3f" % off)]


def pull_motion(name, way, height, uneven=False, grab_t=0.3, pull_at=0.8, extra_keys=()):
    """Both hands on the grunt, gripped at grab_t, pulled `way` at pull_at; uneven: the main hand 0.25 m higher and
    moving 1.6 times as far aside (the hands' different speeds at different heights: the old turn's fragile term)."""
    y = HEIGHTS[height]
    dx, dz = WAYS[way]
    main0 = (GAP, y + (0.25 if uneven else 0.0), NEAR - 0.08)  # (its fist sits a little nearer: deeper)
    off0 = (-GAP, y, NEAR)
    k = 1.6 if uneven else 1.0
    main1 = (main0[0] + dx * PULL * k, main0[1], main0[2] + dz * PULL)
    off1 = (off0[0] + dx * PULL, off0[1], off0[2] + dz * PULL)
    keys = hands_at(0.0, main0, off0)
    keys += [(grab_t, "cmd +grabmain"), (grab_t, "cmd +graboff")]
    keys += hands_at(pull_at, main0, off0)
    keys += hands_at(pull_at + PULL_T, main1, off1)
    keys += list(extra_keys)
    return motion(name, keys)


def case_dir():
    for height in ["high", "mid", "low", "uneven"]:
        for way in ["left", "right", "towards", "away"]:
            name = f"judo_{height}_{way}"
            if ONLY and not re.search(ONLY, name):
                continue
            path = pull_motion(name, way, "high" if height == "uneven" else height, uneven=height == "uneven")
            out = run(f"{PRE};vr_foegrab_throw_away 1;vr_mock_play {path};wait300;-grabmain;-graboff;wait5;"
                      "toggleconsole;quit",
                      "foegrab|throw trace|rror|CRASH")
            with open(os.path.join(SCRATCH, f"judo_test_{name}.txt"), "w") as f:
                f.write(out)
            flat = " ".join(out.split())
            m = re.search(r"both hands turn \S+ \(\d+\) (\d+) deg/s at ([\d.]+) m/s, (to your left|to your right|"
                          r"towards you|away from you)", flat)
            lie = re.search(r"lies ([-\d.]+) ahead, ([-\d.]+) to your left", flat)
            tip = re.search(r"throw trace: 0\.[12]\d s tilt \d+ deg.*?head ([-\d.]+) along the throw", flat)
            got = m.group(3) if m else "no throw"
            ahead, left = (float(lie.group(1)), float(lie.group(2))) if lie else (0.0, 0.0)
            early = float(tip.group(1)) if tip else 0.0
            # (Its start too, its head along the throw at 0.1-0.3 s; pulled towards you it passes through you:
            # vr_foegrab_throw_grace.)
            side_ok = {"left": left > 8, "right": left < -8, "towards": ahead < -8, "away": ahead > 8}[way]
            ok = got == WORDS[way] and early > 8 and side_ok and "rror" not in flat and "CRASH" not in flat
            verdict(ok, f"{height} {way}: thrown {got}" + (f" ({m.group(1)} deg/s, {m.group(2)} m/s)" if m else "") +
                    (f", tips {early:.0f} along at 0.2 s, lies {ahead:.0f} ahead, {left:.0f} left" if lie else ", no trace"))


def status(flat, after):
    """The 'holds ... on as a ragdoll' status line printed after `after` in the log: (hands, ahead, left, up) or None."""
    i = flat.find(after)
    m = re.search(r"on as a ragdoll \((?:thrown|lifted)\) with (\d) hand\(s\), [\d.]+ s: its pelvis ([-\d.]+) ahead, "
                  r"([-\d.]+) to his left, ([-\d.]+) up", flat[i:] if i >= 0 else "")
    return (int(m.group(1)), float(m.group(2)), float(m.group(3)), float(m.group(4))) if m else None


def case_hold():
    """hold: thrown left (hands at its shoulders) and held on (vr_foegrab_throw_hold 1): handed over to both hands, it
    still falls to the left, then the hands, lowered to it, drag it 0.5 m towards you over a second: its pelvis comes 8+
    units nearer.
    Held on off: let go as thrown, no ragdoll hold."""
    y = HEIGHTS["high"]
    for on in [1, 0]:
        name = f"judo_hold{on}"
        main1 = (GAP - PULL, y, NEAR - 0.08)
        off1 = (-GAP - PULL, y, NEAR)
        drag = [(1.55, "cmd echo MARK1"), (1.6, "cmd vr_foegrab_status")]
        # (Down to it first, crouched: from your chest the pull on it is mostly up.)
        low = HEIGHTS["low"]
        drag += hands_at(1.7, main1, off1)
        drag += hands_at(2.2, (main1[0], low, main1[2]), (off1[0], low, off1[2]))
        drag += hands_at(3.2, (main1[0], low, main1[2] + 0.5), (off1[0], low, off1[2] + 0.5))
        drag += [(3.45, "cmd echo MARK2"), (3.5, "cmd vr_foegrab_status")]
        path = pull_motion(name, "left", "high", extra_keys=drag)
        out = run(f"{PRE};vr_debug_ragdoll 1;vr_foegrab_throw_hold {on};vr_mock_play {path};wait500;-grabmain;-graboff;"
                  "wait5;toggleconsole;quit",
                  "foegrab|throw trace|held on by|limb let go|limb lost|MARK|rror|CRASH")
        with open(os.path.join(SCRATCH, f"judo_test_{name}.txt"), "w") as f:
            f.write(out)
        flat = " ".join(out.split())
        thrown = "both hands turn monster_army" in flat and "to your left" in flat
        handed = re.search(r"thrown, held on as a ragdoll by (\d) hands?", flat)
        lie = re.search(r"lies ([-\d.]+) ahead, ([-\d.]+) to your left", flat)
        left = float(lie.group(2)) if lie else 0.0
        s1, s2 = status(flat, "MARK1"), status(flat, "MARK2")
        bad = "rror" in flat or "CRASH" in flat
        if on:
            pulled = s1 and s2 and s1[1] - s2[1] > 8
            ok = thrown and handed and handed.group(1) == "2" and left > 8 and pulled and not bad
            verdict(ok, f"held on: thrown left {thrown}, held by {handed.group(1) if handed else 0} hands, lies {left:.0f} "
                        f"left; pelvis ahead {s1[1] if s1 else '?'} -> {s2[1] if s2 else '?'} "
                        f"(hands {s1[0] if s1 else 0}, {s2[0] if s2 else 0})")
        else:
            ok = thrown and not handed and s1 is None and left > 8 and not bad
            verdict(ok, f"held on off: thrown left {thrown}, not held ({'held' if handed else 'let go'}), lies {left:.0f} left")


def main():
    os.makedirs(SCRATCH, exist_ok=True)
    for c in CASES:
        print(f"== {c}")
        globals()[f"case_{c}"]()
    print(f"FAILS {FAILS}")
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
