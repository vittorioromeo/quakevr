"""Impacts and manhandling (QC vr_ragimpact.qc; ROUND22.md, "Impacts and manhandling"), headless with the mock headset.

Usage: python Misc/quakevr/foegrab/ragimpact_test.py <agent worktree name> [case ...]
Cases (all by default):
  wall      a grunt 48 units off a wall knocked down where it stands and flung at it at 7 m/s and 3 m/s up (vr_knockdown_test 58, 0, 51; as
            your throw): its strikes deal 15 or more; Impacts off: none.
  slam      a grunt knocked down where it stands, its ragdoll's head taken in the main hand (vr_mock_hand_to ... ragdoll
            head), turned palm down, lifted 0.6 m and slammed down 1.2 m in 0.2 s ten times (the hand kept above the floor: the head about 2.5 m/s): killed, its head popped.
  crush     you dropped 256 units onto a knocked-down grunt (vr_knockdown_test 52): the part under your feet crushed for
            20 or more; 16 units over the floor by its head (60): nothing; a standing grunt's head from 64 and 128
            units (59): crushed, harder the higher, popped at 128; 16: nothing; a standing ogre from 256: knocked down.
  stompwear landings wear heads and limbs: same-level jumps (46 units over the floor, 60) on a down grunt's head pop it
            in 2-4; 112 units pop it at once (a down grunt; a corpse's head, then its shin, 61); standing grunt,
            knight, enforcer from 112 over their top: the head pops; ogre, shambler: hurt only.
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
CASES = sys.argv[2:] or ["wall", "slam", "crush", "stompwear", "grab"]
MOTIONS = os.path.join(TREE, "quakevr", "motions")  # (git-ignored)
SCRATCH = os.path.join(TREE, "scratch")
PRE = ("sv_random_seed 7;map vrfiringrange;wait30;god;notarget;developer 1;vr_foegrab_debug 1;vr_foegrab_leniency 6;"
       "vr_test_spawn 0;vr_test_spawn_dist 36;impulse 241;wait40;vr_knockdown_debug 1;vr_knockdown_test 56")
FILTER = "ragimpact|manhandle|limb taken|limb lost|let go of by|foegrab: .*(takes|lifted|held on)|staggered and fell|decap:|limb:|rror|CRASH"
SLAMS = 10
UP, DOWN = float(os.environ.get("SLAM_UP", "0.35")), float(os.environ.get("SLAM_DOWN", "-0.5"))  # (m, from where it took the head)
GAP, NEAR = 0.09, -1.15  # (the judo test's: a hand beside the grunt's middle, on its near side; tracking space, m)

CRUSHED = (r"crushed \([\w ,]+\), you landing at ([\d.]+) m/s, wearing [\d.]+: ([\d.]+) damage to its (head|torso|limb \d+) "
           r"\(worn [\d.]+ of [\d.]+\)(, its head pops|, the limb pops)?")

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
    dealt = []
    for what, extra, speed in [("on5", "", 5), ("on", "", 7), ("on9", "", 9), ("off", "vr_ragimpact 0;", 7)]:
        flat = run(f"wall_{what}", f"{PRE};{extra}vr_knockdown_push 0;vr_knockdown_test 58;wait5;vr_knockdown_test 0;"
                   f"wait3;vr_test_throw_up_speed {speed};vr_knockdown_test 51;wait150;vr_knockdown_test 50")
        s = state(flat)
        bad = "rror" in flat or "CRASH" in flat or not s or f"flung at {speed}.0" not in flat
        if what == "off":
            verdict(not bad and s[4] == 0, f"Impacts off: {s[4]:.0f} strikes, health {s[0]:.0f}" if s else "no state")
            continue
        first = re.search(r"struck at ([\d.]+) m/s \(([\d.]+) raw\), ([\d.]+) kg behind it: ([\d.]+) damage", flat)
        dealt.append(s[5] if s else -1)
        verdict(not bad and (speed < 7 or s[5] >= 15),
                f"thrown into a wall at {speed} m/s: {s[4]:.0f} strikes, {s[5]:.1f} damage, health {s[0]:.0f}"
                + (f" (first strike {first.group(4)} at {first.group(1)} m/s, {first.group(3)} kg)" if first else "")
                if s else "no state")
    verdict(dealt == sorted(dealt) and dealt[0] >= 0, f"damage rises with the throw's speed: {dealt}")


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
    # A knocked-down grunt: 256 units onto its middle (vr_knockdown_test 52) crushes; your feet 16 units over the floor
    # by its head (60: 160 u/s, under vr_ragimpact_crush_speed 200) nothing.
    for what, setup in [("high", "vr_knockdown_test 52"), ("low", "vr_test_drop_height 16;vr_knockdown_test 60")]:
        flat = run(f"crush_{what}", f"{PRE};vr_knockdown_push 0;vr_knockdown_time_min 12;vr_knockdown_time_max 12;"
                   f"vr_knockdown_test 0;wait90;{setup};wait150;vr_knockdown_test 50")
        m = re.search(CRUSHED, flat)
        bad = "rror" in flat or "CRASH" in flat
        if what == "high":
            verdict(not bad and m and float(m.group(2)) >= 20,
                    f"256-unit drop onto it: {m.group(2)} damage to its {m.group(3)} at {m.group(1)} m/s" if m else
                    "256-unit drop onto it: no crush")
        else:
            verdict(not bad and not m, "16-unit drop: " + ("crushed (wrong)" if m else "nothing"))
    # Standing grunts: you dropped onto its head from 64 and 128 units (vr_knockdown_test 59: the higher pops it), then
    # 16 units (too slow).
    got = []
    for up in (64, 128, 16):
        flat = run(f"crush_standing{up}", f"{PRE};vr_test_drop_height {up};vr_knockdown_test 59;wait150;vr_knockdown_test 50")
        m = re.search(CRUSHED, flat)
        s = state(flat)
        bad = "rror" in flat or "CRASH" in flat or not s
        if up == 16:
            verdict(not bad and not m, "standing, 16-unit drop: " + ("crushed (wrong)" if m else "nothing"))
            continue
        pops = m.group(4) or "" if m else ""
        got.append(float(m.group(2)) if m else -1)
        verdict(not bad and m and m.group(3).startswith("head") and (up < 128 or "head pops" in pops),
                f"standing, {up}-unit drop onto its head: {m.group(2)} damage to its {m.group(3)} at {m.group(1)} m/s, "
                f"health {s[0]:.0f}, headless {s[2]:.0f} {pops}" if m and s else f"standing, {up}-unit drop: no crush")
    verdict(len(got) == 2 and 0 < got[0] < got[1], f"the higher drop crushes harder: {got}")
    # A standing ogre, 256 units: it lives through the crush (big: hurt only) and is knocked down by the stomp
    # (vr_ragimpact_stomp), you bouncing off it.
    pre = PRE.replace("vr_test_spawn 0;vr_test_spawn_dist 36", "vr_test_spawn 1;vr_test_spawn_dist 80")
    flat = run("crush_stomp_ogre", f"{pre};vr_test_drop_height 256;vr_knockdown_test 59;wait150;vr_knockdown_test 50")
    s = state(flat)
    st = re.search(r"ragimpact stomp: \S+ \(\d+ kg\) knocked down, health [-\d.]+: landed at (\d+) u/s", flat)
    verdict("rror" not in flat and st and s and s[1] == 1 and s[0] > 0,
            f"standing ogre, 256-unit drop: knocked down by the stomp at {st.group(1)} u/s, health {s[0]:.0f}"
            if st and s else "standing ogre, 256-unit drop: not knocked down")
    # Cushioned: a 400-unit drop onto a knocked-down grunt (no god mode): no fall damage with vr_ragimpact_crush_cushion 1,
    # Quake's 5 without it.
    hp = {}
    for cushion in (1, 0):
        flat = run(f"crush_cushion{cushion}", f"{PRE};god;vr_ragimpact_crush_cushion {cushion};vr_knockdown_push 0;"
                   "vr_knockdown_time_min 12;vr_knockdown_time_max 12;vr_knockdown_test 0;wait90;vr_test_drop_height 400;"
                   "vr_knockdown_test 52;wait150;vr_knockdown_test 50")
        you = re.findall(r"ragimpact you: health ([-\d.]+)", flat)
        hp[cushion] = float(you[-1]) if you else None
        if cushion == 1:
            verdict(re.search(CRUSHED, flat) is not None, "400-unit drop onto a down one: crushed")
    verdict(hp[1] is not None and hp[0] is not None and hp[1] > hp[0],
            f"a crushing landing breaks your fall: your health {hp[1]} cushioned, {hp[0]} not")


def pops(flat):
    """(decaps, limb pops) from the last 'ragimpact state' line, or None."""
    found = re.findall(r"decaps (\d+), limb pops (\d+)", flat)
    return tuple(int(g) for g in found[-1]) if found else None


DOWN = "vr_knockdown_push 0;vr_knockdown_time_min 30;vr_knockdown_time_max 30;vr_knockdown_test 0;wait90"


def case_stompwear():
    # Jumps from the same level (your feet 46 units over the floor: ~270 u/s) onto a knocked-down grunt's head, again
    # and again: its head pops within a few (alive, then a corpse: the wear carries over).
    stomps = "".join("vr_knockdown_test 60;wait60;" for _ in range(6))
    flat = run("stomp_jumps", f"{PRE};{DOWN};vr_test_drop_height 46;{stomps}vr_knockdown_test 50")
    lines = re.findall(r"crushed \((?:a ragdoll|a corpse)\), you landing at ([\d.]+) m/s|(head popped|its head pops)", flat)
    count, popped_at = 0, None
    for speed, pop in lines:
        if speed:
            count += 1
        elif popped_at is None:
            popped_at = count
    speeds = re.findall(r"you landing at ([\d.]+) m/s", flat)
    verdict("rror" not in flat and popped_at is not None and 2 <= popped_at <= 4,
            f"same-level jumps on a down grunt's head (at {speeds[:1]} m/s): popped at stomp {popped_at} of {count}")
    # Two heights (112 units over the floor) onto a down grunt's head: popped at once.
    flat = run("stomp_down112", f"{PRE};{DOWN};vr_test_drop_height 112;vr_knockdown_test 60;wait60;vr_knockdown_test 50")
    p = pops(flat)
    verdict(p is not None and p[0] == 1 and len(re.findall(r"crushed \(", flat)) == 1,
            f"112-unit drop on a down grunt's head: decaps {p[0] if p else '?'} in one landing")
    # A corpse (knocked down, then killed: a ragdoll): 112 units onto its head, then onto its shin: each pops at once.
    flat = run("stomp_corpse", f"{PRE};{DOWN};vr_knockdown_test 62;wait60;vr_test_drop_height 112;vr_knockdown_test 60;wait60;"
               "vr_knockdown_test 61;wait60;vr_knockdown_test 50")
    p = pops(flat)
    ragdoll = "ragdoll 1 knockdown 0 health -" in flat
    verdict(ragdoll and p == (1, 1), f"corpse (a ragdoll: {ragdoll}): 112 units on its head, then its shin: decaps, "
                                     f"limb pops {p}")
    # Standing humanoids, 112 units over their top: the head pops. Big ones: hurt only (alive, nothing popped).
    for spawn, name, big in [(0, "grunt", False), (5, "knight", False), (8, "enforcer", False), (1, "ogre", True),
                             (3, "shambler", True)]:
        pre = PRE.replace("vr_test_spawn 0;vr_test_spawn_dist 36", f"vr_test_spawn {spawn};vr_test_spawn_dist 80")
        flat = run(f"stomp_standing_{name}", f"{pre};vr_test_drop_height 112;vr_knockdown_test 59;wait90;"
                   "vr_knockdown_test 50")
        p = pops(flat)
        s = state(flat)
        m = re.search(r"crushed \(standing[^)]*\), you landing at ([\d.]+) m/s, wearing ([\d.]+): ([\d.]+) damage", flat)
        what = (f"{m.group(3)} damage at {m.group(1)} m/s, wearing {m.group(2)}" if m else "no crush") + \
               f", health {s[0] if s else '?'}, decaps/limb pops {p}"
        if big:
            verdict(m and s and s[0] > 0 and p == (0, 0), f"standing {name}, 112-unit drop: hurt only: {what}")
        else:
            verdict(m and p is not None and p[0] == 1, f"standing {name}, 112-unit drop: head pops: {what}")


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
