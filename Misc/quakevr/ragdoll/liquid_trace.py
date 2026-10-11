"""liquid_trace.py <label> < run output -- one line summing up an active ragdoll's death in a liquid
(liquid_active_test.sh): the game's `ragswim` and `ragtrace` lines (vr_ragdoll_active_trace), its `ragdoll: N in
<liquid> ...` / `out of the` lines (vr_debug_ragdoll 1), drowning and burning lines.

  <label>: <what started> | swum T s (seen) | limbs m/s first second / last second | head under % first / second half,
  lift N at most | pelvis z at the start, lowest, last | driven until T s | after: energy, spin at most | FAIL ...
"""
import re
import sys

label = sys.argv[1] if len(sys.argv) > 1 else "?"
what = stop = None
notes = []
swim = []
rows = []
fails = []
for line in sys.stdin:
    line = line.strip()
    if 'nan' in line.lower() and ('ragtrace' in line or 'ragswim' in line):
        fails.append('NaN')
    if re.search(r'CRASH|ENGINE ERROR|TIMEOUT', line):
        fails.append(line[:60])
    m = re.search(r'ragdoll: \d+ (in \w+ [\d.]+ s after its death: .*)', line)
    if m:
        what = m.group(1)
    m = re.search(r'ragdoll: \d+ (out of the \w+ after [\d.]+ s)', line)
    if m:
        stop = m.group(1)
    if 'drowned for' in line and len(notes) < 1:
        notes.append('drowned')
    if re.search(r'burnt through|gibbed', line) and 'gibbed' not in notes:
        notes.append('gibbed')
    m = re.match(r'ragswim (\d+) ([-\d.]+) l (\d) v ([\d.]+) head (\d) ([-\d.]+) lift ([\d.]+) limbs ([\d.]+)', line)
    if m:
        g = m.groups()
        swim.append(dict(t=float(g[1]), l=int(g[2]), v=float(g[3]), head=int(g[4]), hz=float(g[5]), lift=float(g[6]),
                         limbs=float(g[7])))
    m = re.match(r'ragtrace (\d+) ([-\d.]+) p ([-\d.]+) ([-\d.]+) ([-\d.]+) e (\d+) (\d+) (\d+) (\d+) '
                 r's ([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+) ke ([\d.]+) w ([\d.]+) pin ([\d.]+)', line)
    if m:
        g = m.groups()
        rows.append(dict(t=float(g[1]), z=float(g[4]), str=[float(x) for x in g[9:13]], ke=float(g[13]),
                         spin=float(g[14]), pin=float(g[15])))
if not rows:
    print('%s: no ragtrace %s' % (label, ' '.join(fails)))
    sys.exit(0)
out = [label + ':', what or 'no swim']
if swim:
    t0, t1 = swim[0]['t'], swim[-1]['t']
    first = [s['limbs'] for s in swim if s['t'] < t0 + 1.0]
    last = [s['limbs'] for s in swim if s['t'] > t1 - 1.0]
    half = t0 + (t1 - t0) / 2
    h1 = [s['head'] for s in swim if s['t'] <= half]
    h2 = [s['head'] for s in swim if s['t'] > half]
    out.append('swum %.2f s%s' % (t1 - t0, ' (' + stop + ')' if stop else ''))
    out.append('limbs %.2f / %.2f m/s' % (sum(first) / len(first), sum(last) / len(last)))
    out.append('head under %d%% / %d%%, lift %.0f N' % (100 * sum(h1) / max(len(h1), 1), 100 * sum(h2) / max(len(h2), 1),
                                                        max(s['lift'] for s in swim)))
    zs = [r['z'] for r in rows if r['t'] >= t0]
    if zs:
        out.append('pelvis z %.0f, low %.0f, last %.0f' % (zs[0], min(zs), zs[-1]))
driven = [r['t'] for r in rows if max(r['str']) > 0.005 or r['pin'] > 0]
quiet = max(1.5, (max(driven) + 0.5) if driven else 0.0)
late = [r for r in rows if r['t'] > quiet]
ke = max((r['ke'] for r in late), default=0.0)
spin = max((r['spin'] for r in late), default=0.0)
ke_all = max(r['ke'] for r in rows)
out.append('driven %.2f s' % (max(driven) if driven else 0.0))
out.append('after: ke %.1f J spin %.1f (max ke %.0f)' % (ke, spin, ke_all))
if notes:
    out.append(', '.join(notes))
if ke > 50.0:
    fails.append('energy %.0f J after the drive' % ke)
if spin > 25.0:
    fails.append('spin %.0f rad/s after the drive' % spin)
ke0 = max((r['ke'] for r in rows if r['t'] < rows[0]['t'] + 0.06), default=0.0)
if ke_all > max(3.0 * ke0, 20000.0):
    fails.append('energy %.0f J, made with %.0f (explosion?)' % (ke_all, ke0))
print(' | '.join(out) + (' | FAIL ' + '; '.join(fails) if fails else ''))
