#!/usr/bin/env python3
# slipgate_stuck_fuzz.py <agent> [runs] [trials per run] [seed] [extra commands] (runs x trials trials in all): the softlock fuzz (ROUND21.md,
# "Slipgates: shooting yourself, stuck behind a gate"). Each trial loads vrslipgates, spawns a crate in front of a gate
# (the flush player gate, the large or the wide one, or the loop's west gate), walks the player at it with randomized
# offsets, stick directions, stops, back-steps, sidesteps and jumps (pushing the crate in, following it, backing out
# while it is half through), then lets go and tries to walk back out. vr_portals_stuck (the engine's probe) is asked
# after the walk and after the walk back: "stuck 1" (his box in the world's solid where he stands, a gate's split body
# included) at the end of a trial is a softlock. Prints each stuck trial's script and the totals. FUZZ_ONLY=<n> runs
# trial n of the seed's sequence alone (and prints its script).
import os, random, re, shutil, subprocess, sys

AGENT = sys.argv[1] if len(sys.argv) > 1 else "tpshoot"
RUNS = int(sys.argv[2]) if len(sys.argv) > 2 else 4
TRIALS = int(sys.argv[3]) if len(sys.argv) > 3 else 10
SEED = int(sys.argv[4]) if len(sys.argv) > 4 else 1
EXTRA = sys.argv[5] if len(sys.argv) > 5 else ""
KIT = "C:/OHWorkspace/qvr-kit"
LOG = f"{KIT}/bases/{AGENT}/qbase/qconsole.log"
BASH = shutil.which("bash") or "bash" # (Git's: a bare "bash" from Python on Windows is System32's, WSL)

# (name, plane coordinate, axis (1: y, 0: x), the way into the gate (+1/-1), lateral centre, yaw facing the gate)
GATES = [
    ("flush", 640, 1, +1, -256, 90),
    ("large", 640, 1, +1, 0, 90),
    ("wide", 640, 1, +1, 352, 90),
    ("loop", -1600, 0, -1, 560, 180),
]

def place(g, back, side):
    name, plane, axis, way, centre, yaw = g
    a = plane - way * back
    return (centre + side, a) if axis == 1 else (a, centre + side)

def trial(rng, n):
    g = rng.choice(GATES)
    crate = rng.choice([107, 108])
    x, y = place(g, rng.uniform(40, 70), rng.uniform(-14, 14))
    s = f"echo fuzz trial {n} {g[0]} crate {crate};developer 1;vr_mock_eye_size 160;map vrslipgates;wait60;god;notarget;{EXTRA}"
    s += f";setpos {x:.0f} {y:.0f} 24 0 {g[5]} 0;wait5;noclip 0;vr_test_spawn {crate};vr_test_spawn_dist {rng.randint(36, 56)};impulse 241;wait20"
    x, y = place(g, rng.uniform(70, 120), rng.uniform(-10, 10))
    s += f";setpos {x:.0f} {y:.0f} 24 0 {g[5]} 0;wait5;noclip 0"
    if rng.random() < 0.5:  # a bounce: in to the plane (and through), then short bursts back and on again
        s += f";vr_mock_stick off {rng.uniform(-0.15, 0.15):.2f} 1;wait{rng.randint(12, 30)}"
        for _ in range(rng.randint(3, 7)):
            s += f";vr_mock_stick off {rng.uniform(-0.15, 0.15):.2f} -{rng.uniform(0.6, 1.0):.2f};wait{rng.randint(4, 14)}"
            s += f";vr_mock_stick off {rng.uniform(-0.15, 0.15):.2f} {rng.uniform(0.6, 1.0):.2f};wait{rng.randint(4, 14)}"
    for _ in range(rng.randint(3, 8)):
        kind = rng.random()
        if kind < 0.4:
            st = f"{rng.uniform(-0.3, 0.3):.2f} {rng.uniform(0.6, 1.0):.2f}"   # on into it
        elif kind < 0.7:
            st = f"{rng.uniform(-0.3, 0.3):.2f} {rng.uniform(-1.0, -0.6):.2f}"  # straight back out
        elif kind < 0.8:
            st = f"{rng.choice([-1, 1]) * rng.uniform(0.5, 1.0):.2f} {rng.uniform(-0.2, 0.4):.2f}"  # sideways
        elif kind < 0.85:
            st = "0 0"                                                           # stop
        elif kind < 0.92:
            st = f"0 {rng.uniform(0.6, 1.0):.2f};+jump;wait2;-jump"              # a jump in
        else:  # the crate flung into the gate, or back out of it into him
            yaw = g[5] + rng.choice([0, 180]) + rng.uniform(-20, 20)
            st = f"0 {rng.uniform(0, 1.0):.2f};vr_physics_fling nearest {rng.uniform(60, 300):.0f} {yaw:.0f}"
        s += f";vr_mock_stick off {st};wait{rng.randint(3, 25)}"
    s += ";vr_mock_stick off 0 0;wait30;vr_portals_stuck;vr_mock_stick off 0 -1;wait60;vr_mock_stick off 0 0;wait10;vr_portals_stuck"
    return s

def main():
    rng = random.Random(SEED)
    total = stuck = mid = 0
    unstuck = 0
    kinds, biggest = {}, [0] # the engine's ways out of a gate's wall (its developer lines), the longest move (units)
    only = int(os.environ.get("FUZZ_ONLY", "-1")) # FUZZ_ONLY=<n>: that trial of the seed's sequence alone
    every = [trial(rng, n) for n in range(RUNS * TRIALS)]
    if only >= 0:
        print("script: " + every[only])
    # Runs of trials whose script stays under 7000 characters: a longer -Script is cut short on its way to the game
    # (Windows' command line), the run then waits out its timeout.
    runs, cur = [], []
    for n, t in enumerate(every):
        if only >= 0 and n != only:
            continue
        if cur and sum(len(every[i]) + 1 for i in cur) + len(t) > 7000:
            runs.append(cur); cur = []
        cur.append(n)
    if cur:
        runs.append(cur)
    for run in runs:
        script = ";".join(every[n] for n in run) + ";toggleconsole;quit"
        subprocess.run([BASH, f"{KIT}/run.sh", AGENT, "-Filter", "x", "-Timeout", "900", "-Script", script],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        cur = None
        probes = {}
        for line in open(LOG, errors="replace"):
            m = re.match(r"fuzz trial (\d+)", line)
            if m:
                cur = int(m.group(1)); probes[cur] = []
            m = re.match(r"VR portal: unstuck edict 1: (carried on through|back out of) side \d+ \((\d+)", line)
            if m:
                kinds[m.group(1)] = kinds.get(m.group(1), 0) + 1
                biggest[0] = max(biggest[0], int(m.group(2)))
            m = re.match(r"portal stuck: (\d) at (.*), unstuck (\d+)", line)
            if m and cur is not None:
                probes[cur].append((int(m.group(1)), m.group(2), int(m.group(3))))
        runmax = 0
        for t in run:
            p = probes.get(t, [])
            total += 1
            if len(p) < 2:
                print(f"trial {t}: probes missing ({len(p)})"); continue
            if p[-1][2] > runmax: # (the engine's count is the process's: each trial's own the rise)
                print(f"trial {t}: got out of a gate's wall {p[-1][2] - runmax} times")
            runmax = max(runmax, p[-1][2])
            if p[0][0]: mid += 1
            if p[-1][0]:
                stuck += 1
                print(f"trial {t}: STUCK at the end ({p[-1][1]}); after the walk {p[0][1]}")
                print("  script: " + every[t])
        unstuck += runmax
    print(f"fuzz: {total} trials in {len(runs)} runs, stuck after the walk {mid}, stuck at the end {stuck} (0), "
          f"got out of a gate's wall by the engine {unstuck} times ({kinds}, the longest move {biggest[0]} units)")

main()
