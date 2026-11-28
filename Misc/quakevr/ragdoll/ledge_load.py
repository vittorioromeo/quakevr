"""ledge_load.py <log>... : a ledge grab's load against its cap, per climb_test run (ROUND26.md "ledgegrab").

The log: climb_test.sh with CLIMB_LOG=<log>, XFILTER="^ragenv " and EXTRA="vr_ragdoll_active_trace 1" (each run under an
"== run <class> seed <n>" header; a batched suite's collect pass leaves empty headers, skipped). Per run, each ledge hold
in turn: its hand, when (s from the trace's start), the fall speed and the hand's distance at the grab (the "holds on"
line), its cap (body weights), its load at most (of the cap), the frames at 0.95 of the cap or more, the hand's distance
from its hold at most, and how it let go; then the run's summary (holds pulled away; the catch's line, if any).
"""
import re
import sys

HOLD = re.compile(r'ragenv: \d+ (left|right) hand holds on \(([a-z]+)\) at .*?at most (\d+) N \(([\d.]+) body weights\).*?'
                  r'the hand (\d+) units off it(?:, falling at ([-\d.]+) m/s)?')
LETGO = re.compile(r'ragenv: \d+ (left|right) hand lets go \(([a-z]+), (.*?)\) after ([\d.]+) s')
TRACE = re.compile(r'ragenv \d+ ([-\d.]+) p .*? h (\d)(\d) g ([-\d.]+) ([-\d.]+) ld ([-\d.]+) ([-\d.]+) .*? vz ([-\d.]+) '
                   r'cap ([-\d.]+) ([-\d.]+)')
CATCH = re.compile(r'climb: \d+ (caught its fall .*)')


def runs(paths):
    for path in paths:
        name, lines = None, []
        with open(path, errors='replace') as f:
            for line in f:
                line = line.rstrip('\n')
                if line.startswith('== run '):
                    if name and lines:
                        yield path, name, lines
                    name, lines = line[7:], []
                elif name and lines and line and not re.match(r'(ragenv|climb|monster_|run\.ps1|exit=)', line):
                    lines[-1] += line  # (a run alone wraps its long lines)
                elif name:
                    lines.append(line)
        if name and lines:
            yield path, name, lines


def report(path, name, lines):
    grabs, live = [], {}  # every ledge hold in turn; side -> the one holding
    t = 0.0
    caught = ''
    for line in lines:
        m = TRACE.search(line)
        if m:
            t = float(m.group(1))
            for i, side in enumerate(('left', 'right')):
                g = live.get(side)
                if not g or m.group(2 + i) != '1':
                    continue
                load = float(m.group(6 + i))
                g['peak'] = max(g['peak'], load)
                g['over'] += load >= 0.95 * g['cap']
                g['gapmax'] = max(g['gapmax'], float(m.group(4 + i)))
            continue
        m = HOLD.search(line)
        if m and m.group(2) == 'ledge':
            g = dict(side=m.group(1), t=t, cap=float(m.group(3)), bw=float(m.group(4)), gap0=float(m.group(5)),
                     fall=float(m.group(6)) if m.group(6) else float('nan'), peak=0.0, over=0, gapmax=0.0,
                     why='held to the end')
            grabs.append(g)
            live[g['side']] = g
            continue
        m = LETGO.search(line)
        if m and m.group(2) == 'ledge' and m.group(1) in live:
            live.pop(m.group(1))['why'] = '%s after %s s' % (m.group(3), m.group(4))
            continue
        m = CATCH.search(line)
        if m:
            caught = m.group(1)
    print('%s | %s' % (path.replace('\\', '/').split('/')[-1], name))
    for g in grabs:
        print('    %-5s %.2f s: fall %4.1f m/s, %2.0f u off, cap %.2f bw, load at most %.2f of it, %2d frames >= 0.95, '
              '%4.1f u off at most; %s' % (g['side'], g['t'], g['fall'], g['gap0'], g['bw'], g['peak'] / max(g['cap'], 1.0),
                                          g['over'], g['gapmax'], g['why']))
    if not grabs:
        print('    no ledge grab')
    pulled = sum(1 for g in grabs if g['why'].startswith('pulled away'))
    print('    summary: %d holds, %d pulled away, worst load %.2f of the cap%s' % (
        len(grabs), pulled, max([g['peak'] / max(g['cap'], 1.0) for g in grabs] or [0.0]), '; ' + caught if caught else ''))


def main():
    for path, name, lines in runs(sys.argv[1:]):
        report(path, name, lines)


if __name__ == '__main__':
    main()
