# adposekind's retarget: the AD Knight kind's keys (as in effect: the knight's fitted) corrected where test 231 found a hand
# out of reach: each failing key's right target moved by the wrist's shortfall (where the IK brought it less its goal), the
# left's for classes whose left target is its own (fists, two weapons); both rows' shortfalls averaged (one shared key).
# usage (Misc/quakevr/adpose_retarget.py): <dump of posekey lines> <adkey run> <out rows> [<previous rows file to fold in>]
import re, sys, collections

NP = 11  # agpose::NumParts
dump, run, out = sys.argv[1], sys.argv[2], sys.argv[3]
prev = sys.argv[4] if len(sys.argv) > 4 else None
TOL = 0.9   # correct keys whose hand misses by more than this
GAIN = 1.15 # a little past the shortfall (inside reach)

keys = {}
for line in open(dump, encoding="utf-8", errors="replace"):
    m = re.search(r"posekey (\d+)((?: [-\d.e]+)+)", line)
    if m and int(m.group(1)) // 100000 == 4:
        v = [float(x) for x in m.group(2).split()]
        keys[int(m.group(1))] = (v + [0.0] * (NP * 3))[:NP * 3]
prevCodes = set()
if prev:
    for line in open(prev, encoding="utf-8"):
        m = re.match(r"\s*\{(\d+), \{([^}]*)\}\},", line)
        if m:
            keys[int(m.group(1))] = [float(x.strip().rstrip("f")) for x in m.group(2).split(",")]
            prevCodes.add(int(m.group(1)))

fix = collections.defaultdict(lambda: [[0.0, 0.0, 0.0], [0.0, 0.0, 0.0], 0])
for line in open(run, encoding="utf-8", errors="replace"):
    m = re.search(r"adkey (\d+) (\d+) (\d+) (\d+) " + r" ".join([r"([-\d.e]+)"] * 9), line)
    if not m:
        continue
    row, cls, pp = int(m.group(2)), int(m.group(3)), int(m.group(4))
    v = [float(m.group(i)) for i in range(5, 14)]
    pose, phase = pp // 10, pp % 10
    code = 4 * 100000 + (7 if pose == 3 else cls) * 1000 + pose * 10 + (0 if pose == 3 else phase)
    f = fix[code]
    f[2] += 1
    if v[0] > TOL:
        f[0] = [a + b for a, b in zip(f[0], v[3:6])]
    if v[1] > TOL and (cls in (0, 5, 6) or pose == 20) and pose != 3:
        f[1] = [a + b for a, b in zip(f[1], v[6:9])]

rows = {}
for code, (r, l, n) in fix.items():
    if r == [0.0, 0.0, 0.0] and l == [0.0, 0.0, 0.0]:
        continue
    k = list(keys.get(code, [0.0] * (NP * 3)))
    for i in range(3):
        k[0 + i] += GAIN * r[i] / n  # (the shortfalls averaged over every sample of the key: both rows, every class sharing it)
        k[6 + i] += GAIN * l[i] / n
    rows[code] = k
with open(out, "w", encoding="utf-8", newline="\n") as o:
    for code in sorted(set(rows) | prevCodes):  # (the keys fixed before and not now: kept)
        k = rows.get(code, keys[code])
        cls, pose, ph = (code // 1000) % 100, (code // 10) % 100, code % 10
        o.write("    // AD knight, class %d, pose %d, phase %d (the knight's retargeted: its reach brought in)\n" % (cls, pose, ph))
        o.write("    {%d, {%s}},\n" % (code, ", ".join(("%.2ff" % x).replace("-0.00f", "0.f").replace(".00f", ".f") for x in k)))
print("rows", len(rows), "of", len(fix), "keys seen")
