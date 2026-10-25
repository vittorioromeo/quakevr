"""env_trace.py <label> < run output -- one line summing up what a dying body did with what was round it (env_test.sh):
the game's `ragenv` lines (vr_ragdoll_active_trace with vr_ragdoll_active_env) and `ragenv:` events (vr_debug_ragdoll).

  <label> env: pelvis from (x y z) to (x y z) | per hand: the modes it was in (catch/brace/lean/ledge/curl) and for how
  long, held s, most load N, slid units, closest to its goal units | events: began/let go and why | FAIL ...

ragenv line: ragenv <num> <t> p <x> <y> <z> m <modeL><modeR> h <holdL><holdR> g <gapL> <gapR> ld <loadL> <loadR>
             sl <slidL> <slidR> c <curl>
"""
import re
import sys

MODES = {0: '', 1: 'catch', 2: 'brace', 3: 'lean', 4: 'ledge', 5: 'curl'}
label = sys.argv[1] if len(sys.argv) > 1 else "?"
rows = []
events = []
fails = []
for line in sys.stdin:
    line = line.strip()
    if line.startswith('ragenv') and 'nan' in line.lower():
        fails.append('NaN')
    if re.search(r'CRASH|ENGINE ERROR|TIMEOUT', line):
        fails.append(line[:60])
    m = re.match(r'ragenv: \d+ (.*)', line)
    if m:
        events.append(m.group(1))
        continue
    m = re.match(r'ragenv (\d+) ([-\d.]+) p ([-\d.]+) ([-\d.]+) ([-\d.]+) m (\d)(\d) h (\d)(\d) g ([-\d.]+) ([-\d.]+) '
                 r'ld ([-\d.]+) ([-\d.]+) sl ([-\d.]+) ([-\d.]+) c ([-\d.]+)', line)
    if m:
        g = m.groups()
        rows.append(dict(t=float(g[1]), p=(float(g[2]), float(g[3]), float(g[4])), mode=(int(g[5]), int(g[6])),
                         hold=(int(g[7]), int(g[8])), gap=(float(g[9]), float(g[10])), load=(float(g[11]), float(g[12])),
                         slid=(float(g[13]), float(g[14])), curl=float(g[15])))
if not rows:
    print('%s env: no ragenv lines%s%s' % (label, ' | ' + '; '.join(events[:6]) if events else '',
                                         ' | FAIL ' + '; '.join(fails) if fails else ''))
    sys.exit(0)
dt = (rows[-1]['t'] - rows[0]['t']) / max(len(rows) - 1, 1)
hands = []
for h in range(2):
    modes = {}
    for r in rows:
        if r['mode'][h]:
            modes[r['mode'][h]] = modes.get(r['mode'][h], 0.0) + dt
    if not modes:
        hands.append('-')
        continue
    held = sum(dt for r in rows if r['hold'][h])
    load = max(r['load'][h] for r in rows)
    slid = max(r['slid'][h] for r in rows)
    gaps = [r['gap'][h] for r in rows if r['mode'][h] and r['gap'][h] >= 0]
    hands.append('%s%s%s%s' % ('+'.join('%s %.2fs' % (MODES[k], v) for k, v in sorted(modes.items())),
                               ' held %.2fs load %.0fN' % (held, load) if held else '',
                               ' slid %.0fu' % slid if slid > 0.5 else '',
                               ' gap %.1fu' % min(gaps) if gaps else ''))
p0, p1 = rows[0]['p'], rows[-1]['p']
curl = max(r['curl'] for r in rows)
print('%s env: pelvis (%.0f %.0f %.0f) -> (%.0f %.0f %.0f) | L %s | R %s%s | %s%s'
      % (label, p0[0], p0[1], p0[2], p1[0], p1[1], p1[2], hands[0], hands[1], ' | curl %.2f' % curl if curl > 0 else '',
         '; '.join(events[:8]) if events else 'no events', ' | FAIL ' + '; '.join(fails) if fails else ''))
