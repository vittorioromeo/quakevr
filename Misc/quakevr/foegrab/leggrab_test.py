#!/usr/bin/env python3
"""leggrab_test.py <agent> [regex] -- the leg grabs with the mock headset's hands (ROUND22.md, "Leg grabs").

Each case is one game on vrfiringrange (batched: testbatch.py): a humanoid spawned ahead facing you (vr_test_spawn 0 a
grunt, 6 a death knight; 36 units), both hands put on its legs (vr_mock_hand_to <hand> nearest <class> <dz>: dz units
under its origin, then 'rel' keys aside in tracking space: x right, y up, z towards you), gripped at 0.15 s, then:
  pull_one     both hands on its right shin, pulled 0.45 m towards you in 0.25 s (1.8 m/s)
  pull_both    a hand on each shin, the same pull
  thigh        a hand on each thigh, the same pull (Thigh Grip 0: no leg pull)
  thigh_half   Thigh Grip 0.5 and a 3 m/s pull (counts, the speed needed doubled: pulled)
  thigh_half_slow  Thigh Grip 0.5, the 1.8 m/s pull (under the doubled need: nothing)
  slow         a hand on each shin, pulled 0.45 m in 1 s (0.45 m/s: nothing)
  pull_solo    the main hand alone on its right shin (the off hand free), pulled 0.45 m in 0.25 s (1.8 m/s: pulled,
               One-Hand Pull 1.3 needs 1.56); pull_solo_weak in 0.32 s (1.4 m/s: not); pull_both_weak the same with
               both hands (pulled: two hands need 1.2)
  lift         a hand on each shin, pulled up 1.1 m in 0.6 s (1.8 m/s): lifted by the legs, its head below its feet,
               then swung away 0.5 m in 0.12 s and let go: thrown (its pelvis carried on away)
  lift_gentle  grunt, pulled up 0.55 m in 0.5 s (1.1 m/s, Leg Lift Speed 1.0): lifted
  heavy_gentle death knight (150 kg: x1.14, needs 1.14 m/s), the same 1.1 m/s: not lifted
  heavy        death knight, the 1.8 m/s lift: lifted
PASS per case as above; the logs go to <agent>/scratch/leggrab_<case>.txt.
"""
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
       "vr_foegrab_leggrab_debug 1;vr_knockdown_debug 1;vr_test_spawn_facing %d;vr_test_spawn %d;vr_test_spawn_dist 36;impulse 241;wait40")
FILTER = "foegrab|leg diag|legpull trace|leglift trace|MARK|[Ee]rror|CRASH"
SHIN, THIGH = -21, -9  # units under its origin (its feet 24 under it; a grunt's knees 10-12 over them, the rig)
SIDE = 0.17            # m: each leg this far from its middle (its left on your right: it faces you)
BACK = 0.38            # m: and its left foot this far back (the rig: a grunt stands with it 12 units behind its right)
FAILS = 0
EXTRA = os.environ.get("LG_EXTRA", "")  # (more commands for every case: experiments)


def motion(name, keys):
    os.makedirs(MOTIONS, exist_ok=True)
    with open(os.path.join(MOTIONS, name + ".mock"), "w") as f:
        for t, line in keys:
            f.write(f"{t:.3f} {line}\n")
    return f"quakevr/motions/{name}.mock"


def keys_for(cls, kind, dz, way, dist, dur, swing=False, start=0.6):
    """Both hands on `cls`'s legs (`kind`: "one" both on its right leg, "both" one each), dz under its origin, gripped,
    moved `dist` m `way` ("towards" or "up") in `dur` s from 0.6 s; swing: then swung away and let go at 1.42 s."""
    if kind == "one":
        main, off = (-SIDE, 0.06, 0.0), (-SIDE, -0.06, 0.0)  # (x, dy, dz: its right leg, one hand above the other)
    elif kind == "back":  # (it faces away: its right leg on your right)
        main, off = (SIDE, 0.06, 0.0), (SIDE, -0.06, 0.0)
    elif kind == "near":  # (its side to you: the leg nearer you)
        main, off = (0.0, 0.06, SIDE), (0.0, -0.06, SIDE)
    elif kind == "solo":  # (the main hand alone on its right shin; the off hand left where it was, not gripped)
        main, off = (-SIDE, 0.0, 0.0), None
    else:
        back = BACK if dz == SHIN else 0.0  # (its left foot stands back; its thighs side by side)
        main, off = (SIDE, -0.15, -back), (-SIDE, 0.0, 0.0)  # (the main hand's point sits higher)
    k = [(0.0, f"cmd vr_mock_hand_to main nearest {cls} {dz}"), (0.03, "rel main 0 0 0")]
    if off:
        k += [(0.0, f"cmd vr_mock_hand_to off nearest {cls} {dz}"), (0.03, "rel off 0 0 0")]

    def at(t, d, back=0.0):
        dy, dzt = (d, 0.0) if way == "up" else (0.0, d)
        return [(t, "rel %s %.3f %.3f %.3f" % (n, h[0], h[1] + dy, h[2] + dzt - back))
                for n, h in (("main", main), ("off", off)) if h]
    k += at(0.1, 0.0) + [(0.15, "cmd +grabmain")] + ([(0.15, "cmd +graboff")] if off else [])
    k += at(start, 0.0) + at(start + dur, dist)
    if swing:
        k += at(1.3, dist) + at(1.42, dist, 0.5) + [(1.42, "cmd -grabmain"), (1.42, "cmd -graboff")]
    k += [(1.2, "cmd echo MARK1"), (1.22, "cmd vr_foegrab_status")]
    return k


def verdict(ok, what):
    global FAILS
    print(f"  {'PASS' if ok else 'FAIL'} {what}")
    if not ok:
        FAILS += 1


# (case, spawn, class, kind, dz, way, metres, seconds, extra commands, swing)
CASES = [
    ("pull_one", 0, "monster_army", "one", SHIN, "towards", 0.45, 0.25, "", False),
    ("pull_both", 0, "monster_army", "both", SHIN, "towards", 0.45, 0.25, "", False),
    ("pull_back", 0, "monster_army", "back", SHIN, "towards", 0.45, 0.25, "", False),
    ("pull_back_ai", 0, "monster_army", "back", SHIN, "towards", 0.45, 0.25, "notarget", False),
    ("pull_left", 0, "monster_army", "near", SHIN, "towards", 0.45, 0.25, "", False),
    ("pull_right", 0, "monster_army", "near", SHIN, "towards", 0.45, 0.25, "", False),
    ("thigh", 0, "monster_army", "both", THIGH, "towards", 0.45, 0.25, "", False),
    ("thigh_half", 0, "monster_army", "both", THIGH, "towards", 0.6, 0.2, "vr_foegrab_leggrab_thigh 0.5", False),
    ("thigh_half_slow", 0, "monster_army", "both", THIGH, "towards", 0.45, 0.25, "vr_foegrab_leggrab_thigh 0.5", False),
    ("slow", 0, "monster_army", "both", SHIN, "towards", 0.45, 1.0, "", False),
    # One hand alone (One-Hand Pull 1.3: a grunt needs 1.56 m/s): 1.8 m/s pulls; 1.4 m/s (enough for two hands) doesn't.
    ("pull_solo", 0, "monster_army", "solo", SHIN, "towards", 0.45, 0.25, "", False),
    ("pull_solo_weak", 0, "monster_army", "solo", SHIN, "towards", 0.45, 0.32, "", False),
    ("pull_both_weak", 0, "monster_army", "both", SHIN, "towards", 0.45, 0.32, "", False),
    ("lift", 0, "monster_army", "both", SHIN, "up", 1.1, 0.6, "", True),
    ("lift_gentle", 0, "monster_army", "both", SHIN, "up", 0.55, 0.5, "", False),
    ("heavy_gentle", 6, "monster_hell_knight", "both", SHIN, "up", 0.55, 0.5, "", False),
    ("heavy", 6, "monster_hell_knight", "both", SHIN, "up", 1.1, 0.6, "", False),
]
# Its facing (vr_test_spawn_facing: 0 towards you, 2 away, 4 its left side to you, 5 its right) and the way a pull
# should fell it (by its own facing).
START = {"pull_back_ai": 2.0}  # (s: the pull starts then: held this long first)
FACING = {"pull_back": 2, "pull_back_ai": 2, "pull_left": 4, "pull_right": 5}
FELL = {"pull_one": "backward", "pull_both": "backward", "pull_solo": "backward", "pull_both_weak": "backward", "thigh_half": "backward", "pull_back": "forward", "pull_back_ai": "forward",
        "pull_left": "to its right", "pull_right": "to its left"}


def main():
    os.makedirs(SCRATCH, exist_ok=True)
    b = testbatch.Batch(AGENT, "leggrab")
    handles = {}
    for name, spawn, cls, kind, dz, way, dist, dur, extra, swing in CASES:
        if ONLY and not re.search(ONLY, name):
            continue
        path = motion("leggrab_" + name, keys_for(cls, kind, dz, way, dist, dur, swing, START.get(name, 0.6)))
        script = (f"{PRE % (FACING.get(name, 0), spawn)};{extra};{EXTRA};vr_mock_play {path};wait500;-grabmain;-graboff;wait5;toggleconsole;quit")
        handles[name] = b.case(["-Script", script, "-Filter", FILTER, "-Timeout", "200"])
    b.run()
    for name, spawn, cls, kind, dz, way, dist, dur, extra, swing in CASES:
        if name not in handles:
            continue
        out = b.output(handles[name]).replace("\r", "")
        with open(os.path.join(SCRATCH, f"leggrab_{name}.txt"), "w") as f:
            f.write(out)
        flat = " ".join(out.split())
        bad = bool(re.search(r"\b[Ee]rror|CRASH", flat))
        tries = re.findall(r"leg diag: \S+ \(\d+\) (pull|lift): ([\d.]+) m/s \(needs ([\d.]+)[^)]*\), the hands on (\S+) "
                           r"and (\S+), came ([-\d.]+) cm towards you \(needs \d+\): ([^\n]*?)(?= foegrab| leg| MARK|$)",
                           flat)
        ended = re.search(r"leg diag: \S+ \(\d+\): both hands on it [\d.]+ s \(([^,]+), ([^;]+); grip ([\d.]+)\), its "
                          r"fastest pull towards you ([\d.]+) m/s \(pulls at ([\d.]+)\), up ([\d.]+) m/s \(lifts at "
                          r"([\d.]+)\)", flat)
        pulled = any(t[0] == "pull" and "PULLED DOWN" in t[6] for t in tries)
        lifted = any(t[0] == "lift" and "LIFTED BY THE LEGS" in t[6] for t in tries)
        tried = "; ".join(f"{t[0]} {t[1]} (needs {t[2]}) on {t[3]}/{t[4]}, {t[5]} cm: {t[6][:24]}" for t in tries)
        info = tried or (f"untried: {ended.group(1)}/{ended.group(2)} grip {ended.group(3)}, best pull "
                         f"{ended.group(4)}/{ended.group(5)}, up {ended.group(6)}/{ended.group(7)}" if ended else
                         "no leg grip seen")
        if name in FELL:
            # Its feet towards you (along the pull) and its head away (against it).
            done = re.search(r"legpull trace done: tilt at 0.5 s (\d+) deg, most \d+, turned \d+ in all; feet moved "
                             r"[\d.]+ at most, [\d.]+ at the end \(([-\d.]+) along the legpull\).*?lies \w+ it \(head "
                             r"([-\d.]+) along the legpull\)", flat)
            feet, head = (float(done.group(2)), float(done.group(3))) if done else (0.0, 0.0)
            # (And by its own facing as it stood: pulled from its front it falls backward, from its back forward, from
            # a side to its other side.)
            by = re.search(r"legpull trace done: by its facing, its head ([-\d.]+) ahead of its feet, ([-\d.]+) to its "
                           r"left: fell (\w+(?: \w+ \w+)?)", flat)
            fell = by.group(3) if by else "-"
            ok = pulled and done and feet > 2 and head < -15 and not lifted and fell == FELL[name]
            verdict(ok and not bad, f"{name}: {'pulled down' if pulled else 'NOT pulled'}; feet {feet:.1f} towards you, "
                                    f"its head {head:.1f} along the pull from its feet; fell {fell} (should: "
                                    f"{FELL[name]}){' by ' + by.group(1) + ' ahead, ' + by.group(2) + ' left' if by else ''} "
                                    f"({info})")
        elif name in ("thigh", "thigh_half_slow", "slow", "pull_solo_weak"):
            ok = not pulled and not lifted
            verdict(ok and not bad, f"{name}: {'PULLED (wrong)' if pulled else 'not pulled'} ({info})")
        elif name in ("lift", "lift_gentle", "heavy"):
            ups = [float(x) for x in re.findall(r"leglift trace: [\d.]+ s tilt .*? ([-\d.]+) up; pelvis", flat)]
            flails = "flailing" in flat and re.search(r"lifted, held on as a ragdoll by 2 hands", flat)
            down = min(ups) if ups else 0.0
            ok = lifted and flails and down < -20
            what = f"{name}: {'lifted' if lifted else 'NOT lifted'}, its head {down:.1f} under its feet at most"
            if swing:
                let = re.search(r"let go of after ([\d.]+) s held on", flat)
                # (Its pelvis along the lift's way (away from you) after the swing and the let-go at 1.42 s, from the
                # trace's last line before it.)
                pat = r"leglift trace: [\d.]+ s tilt .*?pelvis ([-\d.]+) along"
                cut = flat.find("let go of after")
                before = re.findall(pat, flat[:cut]) if cut >= 0 else []
                after = re.findall(pat, flat[cut:]) if cut >= 0 else []
                gone = max(float(x) for x in after) - float(before[-1]) if before and after else 0.0
                ok = ok and let and gone > 12
                what += f"; let go of after {let.group(1) if let else '-'} s, thrown {gone:.1f} units on"
            verdict(ok and not bad, f"{what} ({info})")
        else:  # heavy_gentle
            ok = not lifted
            verdict(ok and not bad, f"{name}: {'LIFTED (wrong)' if lifted else 'not lifted'} ({info})")
    print(f"{FAILS} failed")
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
