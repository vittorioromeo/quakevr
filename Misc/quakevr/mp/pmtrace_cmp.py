"""B11b: two runs' pmtrace lines compared frame by frame (vr_pmove_own 0 against 1).
usage: python pmtrace_cmp.py old.txt new.txt [label]
Prints: lines each, the first differing frame (both lines and the 2 before), the largest origin/velocity gaps,
and a verdict line "b11b_cmp <label>: identical" or "b11b_cmp <label>: differs ...".
"""
import re
import sys

PAT = re.compile(r"pmtrace: e=(\d+) t=([\d.]+) o=(\S+) (\S+) (\S+) v=(\S+) (\S+) (\S+) fl=(-?\d+) wl=(\d+) ge=(\d+) m=(\d)")


def load(path):
    rows = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = PAT.search(line)
            if m:
                g = m.groups()
                rows.append((int(g[0]), g[1], tuple(float(x) for x in g[2:5]), tuple(float(x) for x in g[5:8]),
                             int(g[8]), int(g[9]), int(g[10]), int(g[11]), line.strip()))
    return rows


def main():
    a, b = load(sys.argv[1]), load(sys.argv[2])
    label = sys.argv[3] if len(sys.argv) > 3 else "run"
    n = min(len(a), len(b))
    first = None
    omax = vmax = 0.0
    omax_at = vmax_at = -1
    flag_diffs = 0
    for i in range(n):
        ra, rb = a[i], b[i]
        same = ra[:7] == rb[:7]
        if not same and first is None:
            first = i
        if ra[1] != rb[1]:
            continue  # (frames out of step: the gaps would mean nothing)
        do = max(abs(x - y) for x, y in zip(ra[2], rb[2]))
        dv = max(abs(x - y) for x, y in zip(ra[3], rb[3]))
        if do > omax:
            omax, omax_at = do, i
        if dv > vmax:
            vmax, vmax_at = dv, i
        if ra[4:7] != rb[4:7]:
            flag_diffs += 1
    print("lines: old %d new %d; new walked through the moves: %d of %d" % (len(a), len(b), sum(r[7] for r in b), len(b)))
    if first is not None:
        print("first difference at line %d:" % first)
        for j in range(max(0, first - 2), min(n, first + 3)):
            print("  old %s" % a[j][8])
            print("  new %s" % b[j][8])
        print("largest origin gap %.4f at line %d (t=%s), velocity gap %.3f at line %d; frames with other flags/water/ground: %d"
              % (omax, omax_at, a[omax_at][1] if omax_at >= 0 else "-", vmax, vmax_at, flag_diffs))
    if first is None and len(a) == len(b) and n > 0:
        print("b11b_cmp %s: identical (%d frames)" % (label, n))
    else:
        print("b11b_cmp %s: differs (first at %s of %d, origin gap at most %.4f)" % (label, first, n, omax))


main()
