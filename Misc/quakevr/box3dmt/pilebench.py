#!/usr/bin/env python3
"""Box3D on the pool against the Physics Stress piles (ROUND21.md, "Physics threads benchmarked").

gen: writes <worktree>/quakevr/pilebench.cfg: for each repeat, for each pile (kind:count), for each setting
(interleaved, so that drift and turbo hit every setting alike): the setting's cvars, vr_physics_bigpile <kind> <count>,
then vr_physics_steptime bins after the fall (FALL frames) and after the settle (SETTLE frames), then
vr_physics_clearpiles. Physics Threads From is 0 throughout (a setting may set it again).
parse: reads the run's console (stdin) and prints, for each pile, phase and setting, the median over the repeats of
the mean, median, 95th and 99th percentiles and worst step (ms), the worst of all, the frames over 2 ms (all
repeats) and the awake bodies; then every setting's step by the awake
bodies as it began (all piles and phases).

  python pilebench.py gen <worktree> --piles rocks:300,mixed:500 --sets "t0=vr_box3d_threads 0" "w4=..." --reps 5
  bash <kit>/run.sh --exclusive <name> -Script "map vrfiringrange;wait60;exec pilebench.cfg;toggleconsole;quit" \\
      -Filter "PB |steptime" -Timeout 1800 > out.txt
  python pilebench.py parse < out.txt
"""
import argparse
import re
import statistics
import sys

FALL = 144  # frames (2 s at 72 Hz): the columns topple and land
SETTLE = 288  # frames after that (4 s): sliding, rolling, falling asleep


def waits(n):
    out = []
    for big, name in ((60, "w60"), (10, "w10"), (1, "wait")):
        while n >= big:
            out.append(name)
            n -= big
    return ";".join(out)


def gen(a):
    piles = [p.split(":") for p in a.piles.split(",")]
    sets = [s.split("=", 1) for s in a.sets]
    lines = ['alias w10 "wait;wait;wait;wait;wait;wait;wait;wait;wait;wait"', 'alias w60 "w10;w10;w10;w10;w10;w10"',
             "developer 0"]
    for r in range(a.reps):
        for kind, count in piles:
            for tag, cvars in sets:
                lines.append("vr_box3d_threads_bodies 0;" + cvars)
                lines.append("wait;wait")
                lines.append(f"vr_physics_bigpile {kind} {count} {a.distance}")
                lines.append("wait;wait;vr_physics_steptime")  # (the spawn frame not counted)
                lines.append(waits(FALL))
                lines.append(f"echo PB {kind} {count} {tag} {r} fall")
                lines.append("vr_physics_steptime bins")
                lines.append(waits(SETTLE))
                lines.append(f"echo PB {kind} {count} {tag} {r} settle")
                lines.append("vr_physics_steptime bins")
                lines.append("vr_physics_clearpiles;wait;wait;wait;wait;wait")
    lines.append("echo PB done")
    with open(f"{a.worktree}/quakevr/pilebench.cfg", "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"{len(lines)} lines, {a.reps * len(piles) * len(sets)} piles")


STEP = re.compile(r"vr_physics_steptime: (\d+) frames, step ([\d.]+) ms \(median ([\d.]+), p95 ([\d.]+), p99 ([\d.]+), "
                  r"worst ([\d.]+); (\d+) over 2 ms\), ([\d.]+) bodies awake, (\d+) bodies, (\d+) workers")
BIN = re.compile(r"steptime bin (\d+) (\d+): (\d+) frames, median ([\d.]+), p95 ([\d.]+)")


def parse(a):
    joined = []  # (the console wraps long lines: a line not starting a message continues the last)
    for line in sys.stdin:
        line = line.rstrip("\r\n")
        if joined and not re.match(r"(PB |vr_|exit=|$)", line):
            joined[-1] += line
        else:
            joined.append(line)
    rows = {}
    order = []
    bins = {}  # (set, lo) -> [(frames, median, p95)]
    pending = None
    last = None
    for line in joined:
        line = line.strip()
        b = BIN.search(line)
        if b and last:
            bins.setdefault((last, int(b.group(1))), []).append(tuple(float(x) for x in b.group(3, 4, 5)))
            continue
        if line.startswith("PB ") and line != "PB done":
            pending = line.split()[1:]
            continue
        m = STEP.search(line)
        if m and pending:
            kind, count, tag, rep, phase = pending
            last = tag
            if tag not in order:
                order.append(tag)
            rows.setdefault((kind, int(count), phase), {}).setdefault(tag, []).append(tuple(float(x) for x in m.groups()))
            pending = None
    med = statistics.median
    print(f"{'pile':<14}{'phase':<8}{'set':<8}{'n':>3}{'mean':>8}{'median':>8}{'p95':>8}{'p99':>8}{'worst':>8}{'max':>8}"
          f"{'>2ms':>6}{'awake':>7}{'wk':>4}{'vs 1st':>8}")
    for key in sorted(rows, key=lambda k: (k[0], k[1], k[2] != "fall")):
        base = None
        for tag in order:
            if tag not in rows[key]:
                continue
            v = rows[key][tag]
            mean, p50, p95, p99, worst, awake = (med(x[i] for x in v) for i in (1, 2, 3, 4, 5, 7))
            most = max(x[5] for x in v)
            over = int(sum(x[6] for x in v))
            wk = int(med(x[9] for x in v))
            base = base or mean
            print(f"{key[0] + ' ' + str(key[1]):<14}{key[2]:<8}{tag:<8}{len(v):>3}{mean:>8.3f}{p50:>8.3f}{p95:>8.3f}"
                  f"{p99:>8.3f}{worst:>8.3f}{most:>8.2f}{over:>6}{awake:>7.0f}{wk:>4}{mean / base:>8.2f}")
    if not bins:
        return
    print("\nby awake bodies as the step began (all piles, phases, repeats): median of the runs' medians (and p95s), ms")
    los = sorted({k[1] for k in bins})
    print(f"{'set':<8}" + "".join(f"{lo:>14}" for lo in los))
    for tag in order:
        cells = []
        for lo in los:
            v = bins.get((tag, lo))
            if not v or sum(x[0] for x in v) < 20:
                cells.append(f"{'':>14}")
                continue
            cells.append(f"{med(x[1] for x in v):>7.3f}({med(x[2] for x in v):>5.2f})")
        print(f"{tag:<8}" + "".join(cells))
    print(f"{'frames':<8}" + "".join(f"{int(sum(x[0] for x in bins.get((order[0], lo), []))):>14}" for lo in los))


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    sub = p.add_subparsers(dest="cmd", required=True)
    g = sub.add_parser("gen")
    g.add_argument("worktree")
    g.add_argument("--piles", default="rocks:300")
    g.add_argument("--sets", nargs="+", default=["t0=vr_box3d_threads 0", "w4=vr_box3d_threads 1;vr_box3d_workers 4"])
    g.add_argument("--reps", type=int, default=3)
    g.add_argument("--distance", type=int, default=96)
    sub.add_parser("parse")
    a = p.parse_args()
    gen(a) if a.cmd == "gen" else parse(a)
