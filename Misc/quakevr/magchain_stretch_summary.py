"""magchain_stretch_summary.py <old log> <new log> -- magchain_stretch_test.sh's verdict. Each log has the simulated
tether's per-step "magpiece" and "magsway" lines (vr_magholster_chain_log) and its "magphase" marks: per phase the worst
piece off its length (the longest's stretch, the shortest's squeeze, %), how the middle point hangs under the line
between the ends (mean mm, the most it differs a step) and the points' jerk (95th percentile, mm). Passes when the new
chain's worst piece in the fast phases is at most 15 % off its length, every value is finite, no piece explodes, the
run ended by itself, and the slow swing hangs as before (its sag within 5 mm every step of the run with the limit off)."""
import re
import sys

LINE = re.compile(r"magpiece \d stretch (-?[\d.]+) squeeze (-?[\d.]+) sag (-?[\d.]+) rest ([\d.]+) total ([\d.]+) length ([\d.]+) home ([\d.]+) gun ([\d.]+) finite (\d)")
SWAY = re.compile(r"magsway \d move ([\d.]+) jerk ([\d.]+)")
LIMIT = 15.0
SAG_MM = 5.0


def load(path):
    phases = {}
    jerks = {}
    order = []
    phase = "pre"
    bad = 0
    ended = False
    for line in open(path, encoding="utf-8", errors="replace"):
        if "exit=0" in line:
            ended = True
        if "magphase" in line:
            phase = line.split("magphase", 1)[1].strip()
            continue
        if "magpiece" in line and ("nan" in line or "inf" in line or " finite 0" in line):
            bad += 1
            continue
        s = SWAY.search(line)
        if s:
            jerks.setdefault(phase, []).append(float(s.group(2)))
        m = LINE.search(line)
        if not m:
            continue
        if phase not in phases:
            phases[phase] = []
            order.append(phase)
        phases[phase].append([float(g) for g in m.groups()])
    return phases, jerks, order, bad, ended


def worst(rows, i):
    return max((r[i] for r in rows), default=0.0)


def p95(values):
    values = sorted(values)
    return values[int(0.95 * (len(values) - 1))] if values else 0.0


old, oldJerk, order, oldBad, oldEnded = load(sys.argv[1])
new, newJerk, _, newBad, newEnded = load(sys.argv[2])
print("phase      steps | off: stretch squeeze jerk | on: stretch squeeze jerk | sag off on (mean mm) | sag differs most mm")
fails = []
fastOld = fastNew = 0.0
for name in order:
    a = old.get(name, [])
    b = new.get(name, [])
    if not a or not b:
        fails.append("phase %s has no steps" % name)
        continue
    sagA = sum(r[2] for r in a) / len(a)
    sagB = sum(r[2] for r in b) / len(b)
    off = max(abs(x[2] - y[2]) for x, y in zip(a, b))
    print("%-10s %5d | %12.1f %7.1f %4.0f | %11.1f %7.1f %4.0f | %6.1f %6.1f | %6.1f" % (
        name, len(b), worst(a, 0), worst(a, 1), p95(oldJerk.get(name, [])), worst(b, 0), worst(b, 1), p95(newJerk.get(name, [])), sagA, sagB, off))
    if name.startswith("fast"):
        fastOld = max(fastOld, worst(a, 0), worst(a, 1))
        fastNew = max(fastNew, worst(b, 0), worst(b, 1))
    if name == "slow" and off > SAG_MM:
        fails.append("the slow swing's sag moved %.1f mm" % off)
    if worst(b, 0) > 300.0:
        fails.append("%s: exploded (a piece %.0f %% long)" % (name, worst(b, 0)))
if "slow" not in old:
    fails.append("no slow phase")
if not any(n.startswith("fast") for n in order):
    fails.append("no fast phase")
if fastNew > LIMIT:
    fails.append("worst piece %.1f %% off its length" % fastNew)
if newBad:
    fails.append("%d steps not finite" % newBad)
if not oldEnded or not newEnded:
    fails.append("a run did not end by itself")
print("magchain_stretch: worst piece on fast swings %.1f %% (limit off: %.1f %%), %d not finite: %s%s" % (
    fastNew, fastOld, newBad, "FAIL" if fails else "PASS", (" (" + "; ".join(fails) + ")") if fails else ""))
sys.exit(1 if fails else 0)
