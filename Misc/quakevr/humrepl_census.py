"""humrepl_census.py <run0.txt> <run1.txt>: humrepl_census.sh's two runs (vr_agrunt_repl_variants 0, 1) compared map by
map: the monster counts must match (the maps' flow), the setting on must leave no variant AD's and replace some.
Prints a table and "humrepl_census: N passed, M failed"."""
import re
import sys

LINE = re.compile(r"humcensus: (\S+) monsters (\d+) armed (\d+) variants (\d+) left (\d+)")


def read(path):
    out = {}
    order = []
    for line in open(path, encoding="utf-8", errors="replace"):
        m = LINE.search(line)
        if m:
            name = m.group(1)
            if name not in out:
                order.append(name)
            out[name] = tuple(int(v) for v in m.groups()[1:])
    return out, order


off, order = read(sys.argv[1])
on, order1 = read(sys.argv[2])
for n in order1:
    if n not in order:
        order.append(n)
passed = failed = 0
tot_made = 0
print("%-14s %9s %9s %7s %7s %6s %5s" % ("map", "monsters0", "monsters1", "armed0", "armed1", "made", "left"))
for n in order:
    a = off.get(n)
    b = on.get(n)
    ok = a is not None and b is not None and a[0] == b[0] and b[3] == 0
    passed += ok
    failed += not ok
    if b:
        tot_made += b[2]
    print("%-14s %9s %9s %7s %7s %6s %5s %s" % (n, a[0] if a else "-", b[0] if b else "-", a[1] if a else "-",
                                               b[1] if b else "-", b[2] if b else "-", b[3] if b else "-",
                                               "" if ok else "FAIL"))
print("variants replaced in all: %d" % tot_made)
print("humrepl_census: %d passed, %d failed" % (passed, failed))
