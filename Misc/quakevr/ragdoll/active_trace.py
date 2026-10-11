"""active_trace.py <label> < run output -- one line summing up an active ragdoll's death (active_test.sh): the game's
`ragtrace` lines (vr_ragdoll_active_trace), its `limp at frame` and `active death` lines (vr_debug_ragdoll).

  <label>: limp at frame F (P%) | <what its death does> | err legs/torso/arms/head while strong (deg) |
  floor at T s | driven until T s | after 1.5 s: energy, spin at most | pelvis travel | step/wound/crawl seen | FAIL ...
"""
import math
import re
import sys

label = sys.argv[1] if len(sys.argv) > 1 else "?"
limp = what = kill = None
rows = []
fails = []
for line in sys.stdin:
    line = line.strip()
    if 'nan' in line.lower() and 'ragtrace' in line:
        fails.append('NaN')
    if re.search(r'CRASH|ENGINE ERROR|TIMEOUT', line):
        fails.append(line[:60])
    m = re.search(r'limp at frame (\d+) \((\d+)% of its death', line)
    if m:
        limp = 'frame %s (%s%%)' % (m.group(1), m.group(2))
    m = re.search(r'active death \d+ from frame \d+ \(([\d.]+) s\): (.*)', line)
    if m:
        what = m.group(2)
    if line.startswith('test kill:'):
        kill = line
    m = re.match(r'ragtrace (\d+) ([-\d.]+) p ([-\d.]+) ([-\d.]+) ([-\d.]+) e (\d+) (\d+) (\d+) (\d+) '
                 r's ([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+) ke ([\d.]+) w ([\d.]+) pin ([\d.]+) f (\d)(\d)(\d) ([\d.]+)', line)
    if m:
        g = m.groups()
        rows.append(dict(t=float(g[1]), p=(float(g[2]), float(g[3]), float(g[4])), err=[int(x) for x in g[5:9]],
                         str=[float(x) for x in g[9:13]], ke=float(g[13]), spin=float(g[14]), pin=float(g[15]),
                         step=int(g[16]), wound=int(g[17]), crawl=int(g[18]), crawled=float(g[19])))
if not rows:
    print('%s: no ragtrace (limp at %s) %s' % (label, limp, ' '.join(fails)))
    sys.exit(0)
errs = []
for ch in range(4):
    e = [r['err'][ch] for r in rows if r['str'][ch] > 0.5]
    errs.append('%d' % (sum(e) / len(e)) if e else '-')
zend = rows[-1]['p'][2]
floor = next((r['t'] for r in rows if r['p'][2] <= zend + 3.0), rows[-1]['t'])
driven = [r['t'] for r in rows if max(r['str']) > 0.005 or r['pin'] > 0]
late = [r for r in rows if r['t'] > 1.5]
ke = max((r['ke'] for r in late), default=0.0)
spin = max((r['spin'] for r in late), default=0.0)
ke_all = max(r['ke'] for r in rows)
p0, p1 = rows[0]['p'], rows[-1]['p']
travel = math.hypot(p1[0] - p0[0], p1[1] - p0[1])
seen = '/'.join(k for k in ('step', 'wound', 'crawl') if any(r[k] for r in rows))
crawled = max(r['crawled'] for r in rows)
if ke > 50.0:
    fails.append('energy %.0f J after 1.5 s' % ke)
if spin > 25.0:
    fails.append('spin %.0f rad/s after 1.5 s' % spin)
if ke_all > 5000.0:
    fails.append('energy %.0f J (explosion?)' % ke_all)
print('%s: limp at %s | %s | err %s | floor %.2f s | driven %.2f s | late ke %.1f J spin %.1f (max ke %.0f) | travel %.1f u%s%s%s'
      % (label, limp, what or 'limp', '/'.join(errs), floor, max(driven) if driven else 0.0, ke, spin, ke_all, travel,
         ' | ' + seen if seen else '', ' crawled %.1f u' % crawled if crawled > 0 else '',
         ' | FAIL ' + '; '.join(fails) if fails else ''))
