"""Impacts and manhandling (QC vr_ragimpact.qc; ROUND22.md, "Impacts and manhandling"), headless with the mock headset.

Usage: python Misc/quakevr/foegrab/ragimpact_test.py <agent worktree name> [case ...]
Cases (all by default):
  wall      a knocked-down grunt 48 units off a wall flung at it at 7 m/s and 3 m/s up (vr_knockdown_test 58, 0, 51; as
            your throw): its strikes deal 15 or more; Impacts off: none.
  slam      a grunt knocked down where it stands, its ragdoll's head taken in the main hand (vr_mock_hand_to ... ragdoll
            head), turned palm down, lifted 0.6 m and slammed down 1.2 m in 0.2 s ten times (the hand kept above the floor: the head about 2.5 m/s): killed, its head popped.
  crush     you dropped 256 units onto a knocked-down grunt (vr_knockdown_test 52): the part under your feet crushed for
            20 or more; 96 units (53): nothing.
  grab      one hand grips a standing grunt: staggered a moment ago (vr_knockdown_test 54) or at a fifth of its health
            (55): knocked down, held as a ragdoll ('manhandle: ... staggered' / 'weak'); healthy and steady: held standing,
            not knocked down; staggering as a ragdoll (a one-hand shove, vr_knockdown_test 21) and taken by a hand: it
            goes down ('manhandle: ... staggering', 'staggered and fell').
Prints PASS/FAIL per check and the FAIL count; each run's lines go to scratch/ragimpact_<case>.txt (overwritten).
"""
import os
import re
import subprocess
import sys

KIT = "C:/OHWorkspace/qvr-kit"
AGENT = sys.argv[1] if len(sys.argv) > 1 else "voxelopt"
TREE = f"C:/OHWorkspace/qvr-agents/{AGENT}"
CASES = sys.argv[2:] or ["wall", "slam", "crush", "grab"]
MOTIONS = os.path.join(TREE, "quakevr", "motions")  # (git-ignored)
SCRATCH = os.path.join(TREE, "scratch")
PRE = ("sv_random_seed 7;map vrfiringrange;wait30;god;notarget;developer 1;vr_foegrab_debug 1;vr_foegrab_leniency 6;"
       "vr_test_spawn 0;vr_test_spawn_dist 36;impulse 241;wait40;vr_knockdown_debug 1;vr_knockdown_test 56")
FILTER = "ragimpact|manhandle|limb taken|limb lost|let go of by|foegrab: .*(takes|lifted|held on)|staggered and fell|decap:|limb:|rror|CRASH"
SLAMS = 10
UP, DOWN = float(os.environ.get("SLAM_UP", "0.35")), float(os.environ.get("SLAM_DOWN", "-0.5"))  # (m, from where it took the head)
GAP, NEAR = 0.09, -1.15  # (the judo test's: a hand beside the grunt's middle, on its near side; tracking space, m)

FAILS = 0


def verdict(ok, what):
    global FAILS
    print(f"  {'PASS' if ok else 'FAIL'} {what}")
    if not ok:
        FAILS += 1


def run(name, script, timeout=300):
    cmd = ["C:/Program Files/Git/bin/bash.exe", f"{KIT}/run.sh", AGENT, "-Script", script + ";toggleconsole;quit",
           "-Filter", FILTER, "-Timeout", str(timeout)]
    out = subprocess.run(cmd, capture_output=True, text=True, errors="replace").stdout.replace("\r", "")
    with open(os.path.join(SCRATCH, f"ragimpact_{name}.txt"), "w") as f:
        f.write(out)
    return " ".join(out.split())


def motion(name, keys):
    os.makedirs(MOTIONS, exist_ok=True)
    with open(os.path.join(MOTIONS, name + ".mock"), "w") as f:
        for t, line in keys:
            f.write(f"{t:.3f} {line}\n")
    return f"quakevr/motions/{name}.mock"


def state(flat):
    """The last 'ragimpact state' line: (health, knockdown, headless, head worn, strikes, damage) or None."""
    found = re.findall(r"ragimpact state: \S+ health ([-\d.]+) knockdown (\d) headless (\d) head worn ([\d.]+), most on a "
                       r"limb [\d.]+; strikes (\d+), ([\d.]+) damage", flat)
    return tuple(float(g) for g in found[-1]) if found else None


def case_wall():
    for what, extra in [("on", ""), ("off", "vr_ragimpact 0;")]:
        flat = run(f"wall_{what}", f"{PRE};{extra}vr_test_spawn_dist 64;vr_knockdown_test 58;wait5;vr_knockdown_test 0;"
                   "wait3;vr_test_throw_up_speed 7;vr_knockdown_test 51;wait150;vr_knockdown_test 50")
        s = state(flat)
        bad = "rror" in flat or "CRASH" in flat or not s or "flung at 7.0" not in flat
        if what == "on":
            verdict(not bad and s[5] >= 15, f"thrown into a wall: {s[4]:.0f} strikes, {s[5]:.1f} damage, health {s[0]:.0f}"
                    if s else "no state")
        else:
            verdict(not bad and s[4] == 0, f"Impacts off: {s[4]:.0f} strikes, health {s[0]:.0f}" if s else "no state")


def case_slam():
    # (The hand turned palm down once it holds the head (rolled 90 degrees): the head under the palm meets the floor, the
    # hand kept above it.)
    keys = [(0.0, "cmd vr_mock_hand_to main ragdoll head 0"), (0.03, "rel main 0 0 0"), (0.15, "cmd +grabmain"),
            (0.3, "cmd vr_mock_hand_turn main 0 0 90")]
    t = 0.5
    for _ in range(SLAMS):
        keys += [(t, "rel main 0 %.2f 0" % DOWN), (t + 0.5, "rel main 0 %.2f 0" % UP), (t + 1.0, "rel main 0 %.2f 0" % UP),
                 (t + 1.15, "rel main 0 %.2f 0" % DOWN)]
        t += 1.5
    keys += [(t, "cmd vr_knockdown_test 50"), (t + 0.2, "cmd -grabmain")]
    path = motion("ragimpact_slam", keys)
    flat = run("slam", f"{PRE};vr_knockdown_push 0;vr_knockdown_time_min 30;vr_knockdown_time_max 30;vr_knockdown_test 0;"
               f"wait60;vr_mock_walk_to nearest monster_army 24;wait200;vr_mock_walk_to off;wait90;vr_mock_play {path};"
               "wait2500", timeout=600)
    s = state(flat)
    took = "limb taken by hand" in flat or "ragimpact" in flat
    heads = len(re.findall(r"to its head|into its head", flat))
    popped = "its head pops" in flat or "head popped" in flat or (s and s[2] == 1)
    bad = "rror" in flat or "CRASH" in flat or not s
    verdict(not bad and s[0] <= 0 and popped,
            f"{SLAMS} head slams: {heads} head strikes, health {s[0]:.0f}, headless {s[2]:.0f}, head worn {s[3]:.1f}"
            if s else f"no state (hand took it: {took})")


def case_crush():
    for what, mode in [("high", 52), ("low", 53)]:
        flat = run(f"crush_{what}", f"{PRE};vr_knockdown_push 0;vr_knockdown_time_min 12;vr_knockdown_time_max 12;"
                   f"vr_knockdown_test 0;wait90;vr_knockdown_test {mode};wait150;vr_knockdown_test 50")
        m = re.search(r"crushed, you landing at ([\d.]+) m/s: ([\d.]+) damage to its (\S+ ?\d*)", flat)
        bad = "rror" in flat or "CRASH" in flat
        if what == "high":
            verdict(not bad and m and float(m.group(2)) >= 20,
                    f"256-unit drop onto it: {m.group(2)} damage to its {m.group(3)} at {m.group(1)} m/s" if m else
                    "256-unit drop onto it: no crush")
        else:
            verdict(not bad and not m, "96-unit drop: " + ("crushed (wrong)" if m else "nothing"))


def grip_motion(name):
    keys = [(0.0, "main %.3f 0.510 %.3f" % (GAP, NEAR - 0.08)), (0.0, "off -0.400 0.800 -0.200"), (0.3, "cmd +grabmain"),
            (1.2, "cmd vr_knockdown_test 50"), (1.6, "cmd -grabmain")]
    return motion(name, keys)


def case_grab():
    runs = [("staggered", "vr_knockdown_test 54", "staggered"), ("weak", "vr_knockdown_test 55", "weak"),
            ("healthy", "", None)]
    for what, setup, want in runs:
        path = grip_motion(f"ragimpact_grab_{what}")
        flat = run(f"grab_{what}", f"{PRE};{setup};vr_mock_play {path};wait200")
        m = re.search(r"manhandle: monster_army \(\d+\) taken by the main hand (\w+): knocked down", flat)
        took = "hand takes hold of monster_army" in flat
        s = state(flat)
        bad = "rror" in flat or "CRASH" in flat or not s
        if want:
            ok = not bad and took and m and m.group(1) == want and s[1] == 1
            verdict(ok, f"{what}: {'manhandled, ' + m.group(1) if m else 'not manhandled'} (held: {took}), knockdown "
                        f"{s[1] if s else '?'}")
        else:
            ok = not bad and took and not m and s[1] == 0
            verdict(ok, f"{what}: {'manhandled (wrong)' if m else 'held standing'} (held: {took}), knockdown "
                        f"{s[1] if s else '?'}")
    # Staggering as a ragdoll (a one-hand shove), its chest taken by the main hand: it goes down.
    keys = [(0.0, "cmd vr_mock_hand_to main ragdoll near 0"), (0.03, "rel main 0 0 0"), (0.1, "cmd +grabmain"),
            (0.8, "cmd vr_knockdown_test 50"), (1.2, "cmd -grabmain")]
    path = motion("ragimpact_grab_staggering", keys)
    flat = run("grab_staggering", f"{PRE};vr_knockdown_chance 0.01;vr_ragdoll_active_stagger_travel 0.03;"
               f"vr_knockdown_test 21;wait3;vr_mock_play {path};wait200")
    went = "staggering: it goes down" in flat
    fell = "staggered and fell" in flat
    bad = "rror" in flat or "CRASH" in flat
    verdict(not bad and went and fell, f"staggering, taken by a hand: {'goes down' if went else 'not manhandled'}, "
                                       f"{'fell' if fell else 'did not fall'}")


def main():
    os.makedirs(SCRATCH, exist_ok=True)
    for c in CASES:
        print(f"== {c}")
        globals()[f"case_{c}"]()
    print(f"FAILS {FAILS}")
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
