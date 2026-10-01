import re, sys, math
U = 32.67 / 100  # units a cm (mock, world scale 1)
for f in sys.argv[1:]:
    runs = {}; cur = None; base = {}
    for line in open(f, encoding='utf-8', errors='replace'):
        if line.startswith('RUN'):
            cur = line.split()[0]; runs[cur] = []; base = {}; continue
        m = re.match(r'painview (\S+) (\w+) weapon (\d) (\S+) (\S+) (\S+) hand \d (\S+) (\S+) (\S+)\s*$', line)
        if not m or cur is None:
            continue
        g = m.groups()
        part = [float(x) for x in (g[3:6] if g[2] == '1' else g[6:9])]  # the weapon when drawn, else the hand
        base.setdefault(g[1], part)
        runs[cur].append((float(g[0]), g[1], math.dist(part, base[g[1]]) / U, g[2]))
    for r, rows in runs.items():
        out = []
        for h in ('main', 'off'):
            rr = [x for x in rows if x[1] == h]
            start = next((t for t, _, d, _ in rr if d > 0.05), None)
            if start is None:
                out.append(f"{h}: none"); continue
            pk = max(d for _, _, d, _ in rr); tp = next(t for t, _, d, _ in rr if d == pk)
            half = max(t for t, _, d, _ in rr if d >= pk / 2); end = max(t for t, _, d, _ in rr if d > 0.05)
            what = 'weapon' if rr[0][3] == '1' else 'hand'
            out.append(f"{h} {what}: peak {pk:.1f} cm at {tp-start:.2f} s, >=half to {half-start:.2f} s, gone {end-start:.2f} s")
        print(f"{r}: " + "; ".join(out))
