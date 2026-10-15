"""dismember_trace.py <label> < run output -- one line summing up an active ragdoll's reaction to a limb cut
(dismember_test.sh; ROUND22.md, "Active ragdolls: dismemberment"): the game's `ragdis` and `ragtrace` lines
(vr_ragdoll_active_trace), its `lost his` / `loses his balance` / `beheaded` lines (vr_debug_ragdoll) and `test cut:`.

  <label>: <the cut> | <what he does> | beat: on one leg until T s (lean L deg), the pelvis's drop then | hand to stump
  (units) at 0.5/1/2/3/4 s, least, mean while held | writhe: stir (rad/s) while on, before | driven until T s | after the
  drive (1 s on): energy, spin at most | crawled | FAIL ...
Fails on NaN, a crash, energy over 5 kJ, energy over 50 J or spin over 25 rad/s once the drive let go (1 s on), a cut
not made.
"""
import re
import sys

label = sys.argv[1] if len(sys.argv) > 1 else "?"
cut = plan = broke = beheaded = None
dis = []
tr = []
fails = []
for line in sys.stdin:
    line = line.strip()
    if 'nan' in line.lower() and line.startswith('rag'):
        fails.append('NaN')
    if re.search(r'CRASH|ENGINE ERROR|TIMEOUT', line):
        fails.append(line[:60])
    if line.startswith('test cut:'):
        cut = re.sub(r'^test cut: ', '', line)
        if 'NOT cut' in line or 'no ' in line:
            fails.append('not cut')
    m = re.search(r'ragdoll: \d+ lost his (.*?) [\d.]+ s into his death:(.*)', line)
    if m:
        plan = m.group(1) + ':' + m.group(2).split(', writhe')[0]
    m = re.search(r'loses his balance after ([\d.]+) s, leaning (\d+) deg', line)
    if m:
        broke = (float(m.group(1)), int(m.group(2)))
    if 'beheaded' in line and 'collapse' in line:
        beheaded = line
    m = re.match(r'ragdis \d+ ([-\d.]+) gap ([-\d.]+) lean ([-\d.]+) beat (\d) ([-\d.]+) hold ([\d.]+) writhe ([\d.]+) '
                 r'stir ([\d.]+) lost (\d+)', line)
    if m:
        g = m.groups()
        dis.append(dict(t=float(g[0]), gap=float(g[1]), lean=float(g[2]), beat=int(g[3]), hold=float(g[5]),
                        writhe=float(g[6]), stir=float(g[7]), lost=int(g[8])))
    m = re.match(r'ragtrace \d+ ([-\d.]+) p ([-\d.]+) ([-\d.]+) ([-\d.]+) e .* s ([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+) '
                 r'ke ([\d.]+) w ([\d.]+) pin ([\d.]+) f (\d)(\d)(\d) ([\d.]+)', line)
    if m:
        g = m.groups()
        tr.append(dict(t=float(g[0]), z=float(g[3]), s=[float(x) for x in g[4:8]], ke=float(g[8]), spin=float(g[9]),
                       pin=float(g[10]), crawl=int(g[13]), crawled=float(g[14])))
out = [label + ':', cut or 'no cut line']
if beheaded:
    out.append('beheaded: a collapse')
if plan:
    out.append(plan.strip())
if tr:
    driven = [r for r in tr if max(r['s']) > 0.004 or r['pin'] > 0]
    until = driven[-1]['t'] if driven else 0.0
    strong = max(max(r['s']) for r in tr[:3]) if tr else 0.0
    if broke:
        z0 = tr[0]['z']
        zb = min((r for r in tr if r['t'] >= broke[0]), key=lambda r: r['t'] - broke[0], default=tr[0])['z']
        out.append('beat until %.2f s (lean %d deg), pelvis %.1f -> %.1f' % (broke[0], broke[1], z0, zb))
    if dis:
        def gap_at(t):
            near = min(dis, key=lambda r: abs(r['t'] - t))
            return '%.0f' % near['gap'] if abs(near['t'] - t) < 0.2 and near['gap'] >= 0 else '-'
        held = [r['gap'] for r in dis if r['hold'] > 0.9 and r['gap'] >= 0]
        if held:
            out.append('hand to stump %s (least %.1f, mean %.1f held)' % ('/'.join(gap_at(t) for t in (0.5, 1, 2, 3, 4)),
                                                                          min(held), sum(held) / len(held)))
        else:
            out.append('no hand held')
        on = [r['stir'] for r in dis if r['writhe'] > 0.5]
        before = [r['stir'] for r in dis if r['writhe'] == 0 and r['beat'] != 1 and r['t'] > 0.6]
        if on:
            out.append('writhe stir %.2f (%.2f before)' % (sum(on) / len(on), sum(before) / max(len(before), 1)))
    out.append('strength %.2f at first, driven until %.1f s' % (strong, until))
    after = [r for r in tr if r['t'] > until + 1.0]
    if after:
        ke = max(r['ke'] for r in after)
        spin = max(r['spin'] for r in after)
        out.append('after: %.0f J, %.1f rad/s' % (ke, spin))
        if ke > 50 or spin > 25:
            fails.append('restless after')
    if max(r['ke'] for r in tr) > 5000:
        fails.append('energy %.0f J' % max(r['ke'] for r in tr))
    crawled = max(r['crawled'] for r in tr)
    if crawled > 0:
        out.append('crawled %.0f units' % crawled)
else:
    out.append('no ragtrace')
if fails:
    out.append('FAIL ' + ' '.join(sorted(set(fails))))
print(' | '.join(out))
