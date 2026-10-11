"""hitreact_ai.py (hitreact_test.sh ai) <ref.txt> <other.txt>... -- `hitwatch:` timelines as their animation frames' sequence (each change of frame
once, with its time): how many of the reference's first changes the other run makes the same, in the same order, and
how far apart in time (the runs' own jitter: a frame or two)."""
import re
import sys

pat = re.compile(r'hitwatch: ([\d.]+) \S+ frame (\S+) at')


def changes(p):
    out = []
    for line in open(p, encoding='utf-8', errors='replace'):
        m = pat.search(line)
        if m and (not out or out[-1][1] != m.group(2)):
            out.append((float(m.group(1)), m.group(2)))
    return out


ref = changes(sys.argv[1])
t0 = ref[0][0]
print(sys.argv[1], len(ref), 'frame changes over %.2f s' % (ref[-1][0] - t0))
for p in sys.argv[2:]:
    o = changes(p)
    o0 = o[0][0]
    same = 0
    drift = 0.0
    for a, b in zip(ref, o):
        if a[1] != b[1]:
            break
        same += 1
        drift = max(drift, abs((a[0] - t0) - (b[0] - o0)))
    at = ref[same][0] - t0 if same < len(ref) else None
    print(p, len(o), 'changes; the same as the reference for its first', same,
          '(to %.2f s after the first hit)' % at if at is not None else '(all)', '; most time apart %.3f s' % drift)
