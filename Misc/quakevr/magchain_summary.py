"""magchain_summary.py <log> -- sums magchain_test.sh's per-frame chain lines (vr_magholster_chain_log) by phase and
chain: frames, the springs' worst stretch (% of relaxed: max, mean), the most movement off the anchor a frame (mm: max)
and the jerk (mm: 95th percentile, max), and the frames it started afresh (a jerk past 20 mm counts as a spaz)."""
import re
import sys

rows = {}
order = []
phase = "pre"
for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
    if "magphase" in line:
        phase = line.split("magphase", 1)[1].strip()
        continue
    if "exit=" in line or "ERROR" in line or "CRASH" in line:
        print(line.strip())
    m = re.search(r"magchain (\d) (\w) err ([\d.]+) move ([\d.]+) jerk ([\d.]+) frames (\d+)(?: dt ([\d.]+))?", line)
    if not m:
        continue
    key = (phase, m.group(2))
    if key not in rows:
        rows[key] = []
        order.append(key)
    rows[key].append((float(m.group(3)), float(m.group(4)), float(m.group(5)), int(m.group(6)), float(m.group(7) or 0)))

names = {"L": "lanyard", "S": "stub", "T": "tether"}
print("phase    chain    frames  err%max err%mean move.max jerk.p95 jerk.max restarts spazzes dt.ms")
for key in order:
    r = rows[key]
    err = [x[0] for x in r]
    move = [x[1] for x in r]
    jerk = sorted(x[2] for x in r if x[3] >= 3)
    restarts = sum(1 for x in r if x[3] == 1)
    p95 = jerk[int(0.95 * (len(jerk) - 1))] if jerk else 0.0
    print("%-8s %-8s %6d %8.1f %8.1f %8.1f %8.1f %8.1f %8d %7d %5.1f" % (key[0], names.get(key[1], key[1]), len(r), max(err),
          sum(err) / len(err), max(move), p95, jerk[-1] if jerk else 0.0, restarts, sum(1 for j in jerk if j > 20.0), sum(x[4] for x in r) / len(r)))
