"""magchain_sway_summary.py <log> -- sums magchain_sway_test.sh's magsway lines (vr_magholster_chain_log: the simulated
tether's points in the play space) by phase: frames, the points' most movement a frame (mm: p95, max), their jerk (mm:
p95, max) and the middle point's horizontal excursion from where it hung in the still phase (mm: max, at the phase's
end), and the chains started afresh."""
import math
import re
import sys

rows = {}
order = []
phase = "pre"
afresh = {}
for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
    if "magphase" in line:
        phase = line.split("magphase", 1)[1].strip()
        continue
    if "exit=" in line or "ERROR" in line or "CRASH" in line:
        print(line.strip())
    if re.search(r"magchain \d (now|jump .*the chains)", line):
        afresh[phase] = afresh.get(phase, 0) + 1
        continue
    m = re.search(r"magsway (\d) move ([\d.]+) jerk ([\d.]+) mid (-?\d+) (-?\d+) (-?\d+) n (\d+)", line)
    if not m:
        continue
    if phase not in rows:
        rows[phase] = []
        order.append(phase)
    rows[phase].append((float(m.group(2)), float(m.group(3)), float(m.group(4)), float(m.group(5))))


def pct(v, q):
    v = sorted(v)
    return v[int(q * (len(v) - 1))] if v else 0.0


still = rows.get("still", [])
rest = (sum(r[2] for r in still) / len(still), sum(r[3] for r in still) / len(still)) if still else (0.0, 0.0)
print("phase    frames move.p95 move.max jerk.p95 jerk.max  exc.max  exc.end afresh")
for ph in order:
    r = rows[ph]
    exc = [math.hypot(x[2] - rest[0], x[3] - rest[1]) for x in r]
    move = [x[0] for x in r]
    jerk = [x[1] for x in r[2:]] or [0.0]
    print("%-8s %6d %8.1f %8.1f %8.1f %8.1f %8.0f %8.0f %6d" % (ph, len(r), pct(move, 0.95), max(move), pct(jerk, 0.95),
          max(jerk), max(exc), exc[-1], afresh.get(ph, 0)))
