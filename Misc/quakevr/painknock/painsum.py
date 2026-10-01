import re, sys, math
# painsum.py qconsole.log...: from vr_debug_pain 2's painview lines, each RUN_<name> block's knock of each drawn hand
# (or the weapon in it): its peak against the eye (the player may be thrown about by the hit), and the part of it seen as
# a move (across the line from the eye to the hand: what is along it only nears or recedes), with that as degrees.
U = 32.67 / 100  # units a cm (mock, world scale 1)
def sub(a, b): return [x - y for x, y in zip(a, b)]
def dot(a, b): return sum(x * y for x, y in zip(a, b))
for f in sys.argv[1:]:
    runs = {}; cur = None; base = {}
    for line in open(f, encoding='utf-8', errors='replace'):
        if line.startswith('RUN'):
            cur = line.split()[0]; runs[cur] = []; base = {}; continue
        m = re.match(r'painview (\S+) (\w+) weapon (\d) (\S+) (\S+) (\S+) hand \d (\S+) (\S+) (\S+)(?: eye (\S+) (\S+) (\S+))?\s*$', line)
        if not m or cur is None:
            continue
        g = m.groups()
        part = [float(x) for x in (g[3:6] if g[2] == '1' else g[6:9])]  # the weapon when drawn, else the hand
        eye = [float(x) for x in g[9:12]] if g[9] else [0.0, 0.0, 0.0]
        rel = sub(part, eye)
        base.setdefault(g[1], rel)
        b = base[g[1]]
        d = sub(rel, b)
        los = [x / math.hypot(*b) for x in b] if math.hypot(*b) > 0 else [1.0, 0.0, 0.0]
        along = dot(d, los)
        across = math.sqrt(max(dot(d, d) - along * along, 0.0))
        deg = math.degrees(math.acos(max(-1.0, min(1.0, dot(rel, b) / max(math.hypot(*rel) * math.hypot(*b), 1e-6)))))
        runs[cur].append((float(g[0]), g[1], math.hypot(*d) / U, g[2], across / U, deg))
    for r, rows in runs.items():
        out = []
        for h in ('main', 'off'):
            rr = [x for x in rows if x[1] == h]
            start = next((x[0] for x in rr if x[2] > 0.05), None)
            if start is None:
                out.append(f"{h}: none"); continue
            pk = max(x[2] for x in rr); tp = next(x[0] for x in rr if x[2] == pk)
            half = max(x[0] for x in rr if x[2] >= pk / 2); end = max(x[0] for x in rr if x[2] > 0.05)
            seen = max(x[4] for x in rr); deg = max(x[5] for x in rr)
            what = 'weapon' if rr[0][3] == '1' else 'hand'
            out.append(f"{h} {what}: peak {pk:.1f} cm (seen across {seen:.1f} cm, {deg:.1f} deg) at {tp-start:.2f} s, "
                       f">=half to {half-start:.2f} s, gone {end-start:.2f} s")
        print(f"{r}: " + "; ".join(out))
