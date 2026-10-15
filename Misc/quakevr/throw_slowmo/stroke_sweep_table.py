# stroke_sweep_table.py <prefix> <lob|toss> -- stroke_sweep_test.sh's table from its logs (<prefix>_<tag>.log).
import sys, re
P, KIND = sys.argv[1], sys.argv[2]
TAGS = ("full", "bt1", "bt1.5", "bt2", "bt2.5", "bt3.3")
def load(tag):
    d = {}; name = None; text = open(f"{P}_{tag}.log", errors="replace").read()
    # the console wraps long lines: join every line not starting a record
    lines = []
    for l in text.splitlines():
        if lines and not re.match(r'THROW|throw main|  slow motion', l): lines[-1] = lines[-1].rstrip() + ' ' + l
        else: lines.append(l)
    for l in lines:
        if l.startswith('THROW'): name = l.split()[1]; continue
        m = re.match(r'throw main: ([\d.]+) m/s \(\S+ \S+ (\S+)\)', l)
        if m and name: d[name] = [float(m.group(1)), float(m.group(2)), None, None]; last = name; name = None; continue
        m = re.search(r'tempo ([\d.]+) s: gain x([\d.]+)', l)
        if m and d: d[last][2] = float(m.group(2)); d[last][3] = float(m.group(1))
    return d
runs = {t: load(t) for t in TAGS}
def far(v, vz):
    return v * v / 9.81 if KIND == "lob" else vz * vz / (2 * 9.81) * 100  # metres at 45 degrees / cm risen
unit = "m at 45 deg" if KIND == "lob" else "cm risen"
print(f"{KIND}: {unit}; bullet time (0.3x) made at 1x .. 3.3x slower: over full speed (gain, tempo s)")
print(f"{'throw':7s} {'full':>6s} " + " ".join(f"{t:>20s}" for t in TAGS[1:]))
for k, (v, vz, _, _) in runs["full"].items():
    f = far(v, vz); row = f"{k:7s} {f:6.2f} "
    for t in TAGS[1:]:
        r = runs[t].get(k)
        if not r: row += f"{'-':>20s} "; continue
        g = far(r[0], r[1]); row += f"{g:6.2f} x{g / max(f, 1e-4):4.2f} ({r[2] or 0:4.2f},{r[3] or 0:5.3f}) "
    print(row)
