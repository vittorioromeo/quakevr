#!/usr/bin/env python3
"""perf_report.py -- the perf recorder's CSV (vr_perf_record, Quake/vr/vr_perfrec.hpp) read back: the hitches ranked by
cause, and the sustained costs by pass.

Usage: python perf_report.py <perf_*.csv> [--budget-ms N] [--top N] [--map NAME]

A hitch: a frame whose period is over 1.5 display periods (a refresh missed), or one the runtime says missed display
periods. Its cost: the time over one display period. Its cause, in order:
  1. the frame's own events (or the frame before's): shader compiles, binary-cache programs, texture/model/sound loads,
     runtime lightmaps, a map's load, a save, decal bakes, a navmesh job's wait (the kind with the most milliseconds;
     "work" (VR_TimeAdd's timed work, often inside a load) only when nothing else is there);
  2. else the CPU (our work over the budget: its costliest CPU phase), or the GPU (its span over the budget: its
     costliest GPU phase), or "runtime/unknown" (neither: the compositor, the OS, a GPU stall not timed).
The sustained costs: each phase's mean, the 95th/99th percentiles of the frame, its CPU work and the GPU's span, and the
seconds whose frame rate fell the most.
"""

import argparse
import csv
import sys
from collections import defaultdict

KINDS = ["shader", "shaderbin", "texture", "model", "lightmap", "sound", "map", "save", "decal", "nav", "qc", "phys",
         "work"]
EVENT_KINDS = ["shader", "shaderbin", "texture", "model", "lightmap", "sound", "map", "save", "decal", "nav"]


def num(s):
    try:
        return float(s) if s else 0.0
    except ValueError:
        return 0.0


def pct(values, p):
    if not values:
        return 0.0
    v = sorted(values)
    return v[min(len(v) - 1, int(round(p / 100.0 * (len(v) - 1))))]


def read(path):
    header, rows, comments = None, [], []
    with open(path, newline="", encoding="utf-8", errors="replace") as f:
        for line in f:
            if line.startswith("#"):
                comments.append(line.rstrip("\n"))
                continue
            if header is None:
                header = next(csv.reader([line]))
                continue
            vals = next(csv.reader([line]), None)
            if not vals or len(vals) < len(header) - 1:
                continue  # (a last line cut off)
            rows.append(dict(zip(header, vals)))
    return header or [], rows, comments


UNTIMED = ("map", "lightmap")  # events whose cost lands elsewhere (the frames' periods, the driver's later work)
CPU_WAITS = ("xr_wait", "xrWaitFrame", "swap", "xr_acquire", "xr_release", "xr_submit")
CPU_CONTAINERS = ("screen", "3D", "eye_L", "eye_R", "server")  # phases holding others
GPU_CONTAINERS = ("3D", "eye_L", "eye_R")


def top_phase(r, prefix, skip, containers):
    """The costliest phase (a leaf one if it holds 40% of the costliest container's time) and the top three."""
    ph = sorted(((k[len(prefix):], num(v)) for k, v in r.items() if k.startswith(prefix) and k[len(prefix):] not in skip),
                key=lambda kv: -kv[1])
    if not ph:
        return "?", ""
    leaf = next((kv for kv in ph if kv[0] not in containers), ph[0])
    pick = leaf if leaf[1] >= 0.4 * ph[0][1] else ph[0]
    return pick[0], ", ".join("%s %.1f" % kv for kv in ph[:3])


def frame_cause(r, prev, budget):
    """(cause, detail) for a hitch frame."""
    excess = num(r["period"]) - budget

    def events(row):
        best, best_ms = None, -1.0
        for k in EVENT_KINDS:
            n = num(row.get("ev_%s_n" % k))
            if n <= 0:
                continue
            ms = num(row.get("ev_%s_ms" % k))
            # a cause if its time explains a good part of the excess, or if its cost isn't timed here
            if (ms >= 0.3 * excess or k in UNTIMED) and (ms > best_ms or best in UNTIMED):
                best, best_ms = k, ms
        return best, best_ms

    for row, where in ((r, ""), (prev, " (frame before)")):
        if row is None:
            continue
        k, ms = events(row)
        if k:
            names = [e for e in row.get("events", "").split("|") if e.startswith(k + ":")]
            return "event:" + k, "%.1f ms%s %s" % (ms, where, " ".join(names[:3])[:160])
    if num(r.get("ev_work_ms")) >= 0.3 * excess:
        return "event:work", r.get("events", "")[:160]
    busy, span = num(r["busy"]), num(r["gpu_span"])
    evs = (" | events: " + r["events"][:80]) if r.get("events") else ""
    if busy > budget and busy >= span:
        c, top = top_phase(r, "c_", CPU_WAITS, CPU_CONTAINERS)
        return "cpu:" + c, "busy %.1f ms (%s)%s" % (busy, top, evs)
    if span > budget:
        g, top = top_phase(r, "g_", (), GPU_CONTAINERS)
        return "gpu:" + g, "span %.1f ms (%s)%s" % (span, top, evs)
    if not r.get("gpu_span"):
        return "unknown (gpu not read back)", "busy %.1f ms" % busy
    return "runtime/unknown", "busy %.1f, gpu %.1f ms" % (busy, span)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--budget-ms", type=float, default=0.0, help="the frame budget (default: the runtime's period)")
    ap.add_argument("--top", type=int, default=12)
    ap.add_argument("--map", default="")
    a = ap.parse_args()

    header, rows, comments = read(a.csv)
    if a.map:
        rows = [r for r in rows if r.get("map") == a.map]
    if not rows:
        print("no frames")
        return 1
    for c in comments[:2]:
        print(c[:300])
    periods = [num(r["xr_period"]) for r in rows if num(r["xr_period"]) > 0]
    budget = a.budget_ms or (pct(periods, 50) if periods else 1000.0 / 72)
    secs = num(rows[-1]["t"]) - num(rows[0]["t"])
    maps = defaultdict(int)
    for r in rows:
        maps[r["map"]] += 1
    print("frames %d over %.1f s (%.1f FPS), budget %.2f ms (%.0f Hz); maps: %s" % (len(rows), secs,
          len(rows) / secs if secs > 0 else 0, budget, 1000.0 / budget,
          ", ".join("%s %d" % kv for kv in sorted(maps.items(), key=lambda kv: -kv[1]))))

    # ---- the frame, sustained ----
    per = [num(r["period"]) for r in rows]
    busy = [num(r["busy"]) for r in rows]
    span = [num(r["gpu_span"]) for r in rows if r.get("gpu_span")]
    missed = sum(int(num(r["xr_missed"])) for r in rows)
    print("\n%-14s %8s %8s %8s %8s %8s" % ("ms", "mean", "p50", "p95", "p99", "max"))
    for name, v in (("period", per), ("cpu work", busy), ("gpu span", span)):
        if v:
            print("%-14s %8.2f %8.2f %8.2f %8.2f %8.2f" % (name, sum(v) / len(v), pct(v, 50), pct(v, 95), pct(v, 99), max(v)))
    print("display periods missed (runtime): %d; frames over budget: cpu %d (%.1f%%), gpu %d (%.1f%%)" % (missed,
          sum(1 for b in busy if b > budget), 100.0 * sum(1 for b in busy if b > budget) / len(rows),
          sum(1 for s in span if s > budget), 100.0 * sum(1 for s in span if s > budget) / max(1, len(span))))

    # ---- the hitches by cause ----
    hitches = []
    for i, r in enumerate(rows):
        p = num(r["period"])
        if p > 1.5 * budget or num(r["xr_missed"]) > 0:
            cause, detail = frame_cause(r, rows[i - 1] if i else None, budget)
            hitches.append((p - budget, cause, detail, r))
    by = defaultdict(lambda: [0, 0.0, 0.0, None])
    for excess, cause, detail, r in hitches:
        b = by[cause]
        b[0] += 1
        b[1] += excess
        if excess > b[2]:
            b[2], b[3] = excess, (r["t"], r["map"], detail)
    print("\nhitches: %d (frames over 1.5 budgets or with missed periods), %.0f ms over budget in all" % (len(hitches),
          sum(h[0] for h in hitches)))
    print("%-28s %6s %10s %9s  %s" % ("cause", "count", "excess ms", "worst ms", "worst: t, map, detail"))
    for cause, (n, ex, worst, w) in sorted(by.items(), key=lambda kv: -kv[1][1])[:a.top * 2]:
        print("%-28s %6d %10.0f %9.1f  %s" % (cause, n, ex, worst, "%ss %s %s" % (w[0], w[1], w[2][:110]) if w else ""))

    # ---- the events, all of them (hitch or not) ----
    print("\nevents (all frames): kind, count, ms total, worst ms")
    for k in KINDS:
        ns = [num(r.get("ev_%s_n" % k)) for r in rows]
        ms = [num(r.get("ev_%s_ms" % k)) for r in rows]
        if sum(ns):
            print("  %-10s %7d %10.1f %8.2f" % (k, sum(ns), sum(ms), max(ms)))

    # ---- the passes, sustained ----
    print("\nCPU phases, mean ms a frame (top %d):" % a.top)
    cpu = [(k[2:], sum(num(r[k]) for r in rows) / len(rows), pct([num(r[k]) for r in rows], 99)) for k in header
           if k.startswith("c_")]
    for name, m, p99 in sorted(cpu, key=lambda x: -x[1])[:a.top]:
        print("  %-16s %7.2f  (p99 %.2f)" % (name, m, p99))
    gpu_rows = [r for r in rows if r.get("gpu_span")]
    if gpu_rows:
        print("GPU phases, mean ms a frame over %d frames read back (top %d; nested: 3D holds the eyes, the eyes their passes):"
              % (len(gpu_rows), a.top))
        gpu = [(k[2:], sum(num(r[k]) for r in gpu_rows) / len(gpu_rows), pct([num(r[k]) for r in gpu_rows], 99))
               for k in header if k.startswith("g_")]
        for name, m, p99 in sorted(gpu, key=lambda x: -x[1])[:a.top]:
            print("  %-16s %7.2f  (p99 %.2f)" % (name, m, p99))

    # ---- the worst seconds ----
    win = defaultdict(list)
    for r in rows:
        win[int(num(r["t"]))].append(r)
    worst = sorted(win.items(), key=lambda kv: len(kv[1]))[:min(a.top, len(win))]
    print("\nthe slowest seconds (frames in it; cpu work / gpu span means; dynamic resolution's scale; draws; dlights):")
    for s, rs in sorted(worst, key=lambda kv: len(kv[1])):
        g = [num(r["gpu_span"]) for r in rs if r.get("gpu_span")]
        print("  t=%5ds %-6s %3d frames  cpu %5.1f  gpu %5.1f  scale %.2f  draws %5.0f  dlights %3.0f" % (s, rs[0]["map"],
              len(rs), sum(num(r["busy"]) for r in rs) / len(rs), sum(g) / len(g) if g else 0,
              sum(num(r["scale"]) for r in rs) / len(rs), sum(num(r["draws"]) for r in rs) / len(rs),
              sum(num(r["dlights"]) for r in rs) / len(rs)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
