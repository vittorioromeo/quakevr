#!/usr/bin/env python3
"""Compares two runs of vr_grasp_sweep (their console output: the "gsweep" lines, wrapped or not).

usage: grasp_compare.py <before.txt> <after.txt>

Prints the cases compared, how many are identical to the last bit, the largest difference of a finger's curl (curl
frames) and of the palm's move (hand units), and every case whose thumb choice or finger flags differ.
"""
import re
import sys


def records(path):
    """The sweep's records by (hand, model, variant, place): their fields."""
    text = open(path, encoding="utf-8", errors="replace").read().splitlines()
    joined = []
    for line in text:
        if line.startswith("gsweep h"):
            joined.append(line)
        elif joined and not line.startswith("gsweep") and not re.search(r"places\s+\d+\s*$", joined[-1]):
            joined[-1] += " " + line
    out = {}
    for r in joined:
        t = r.split()
        r = " ".join(t)
        key = tuple(t[1:5])
        f = {}
        i = 5
        f["palm"] = [float(x) for x in t[i + 1:i + 4]]
        f["turn"] = [float(x) for x in t[i + 5:i + 9]]
        f["thumb"] = int(t[i + 10])
        f["thumbTurn"] = [float(x) for x in t[i + 11:i + 15]]
        fingers = []
        for m in re.finditer(r"f(\d) (\S+) (\S+) (\S+) (\d{4})", r):
            fingers.append(([float(m.group(2)), float(m.group(3)), float(m.group(4))], m.group(5)))
        f["fingers"] = fingers
        f["probes"] = int(re.search(r"probes (\d+)", r).group(1))
        f["places"] = int(re.search(r"places (\d+)", r).group(1))
        out[key] = (f, " ".join(t[5:]))
    return out


def main():
    a, b = records(sys.argv[1]), records(sys.argv[2])
    keys = sorted(set(a) & set(b))
    only = len(set(a) ^ set(b))
    same = 0
    curl = palm = 0.0
    worst = None
    discrete = []
    for k in keys:
        fa, ta = a[k]
        fb, tb = b[k]
        if ta == tb:
            same += 1
            continue
        for (ca, fla), (cb, flb) in zip(fa["fingers"], fb["fingers"]):
            d = max(abs(x - y) for x, y in zip(ca, cb))
            if d > curl:
                curl, worst = d, k
            if fla != flb:
                discrete.append((k, "flags"))
        palm = max(palm, max(abs(x - y) for x, y in zip(fa["palm"], fb["palm"])))
        if fa["thumb"] != fb["thumb"]:
            discrete.append((k, "thumb %d -> %d" % (fa["thumb"], fb["thumb"])))
    print("cases %d (unmatched %d): identical %d; max curl difference %.6g%s; max palm difference %.6g" %
          (len(keys), only, same, curl, " (%s)" % " ".join(worst) if worst else "", palm))
    for k, what in discrete[:20]:
        print("  differs:", " ".join(k), what)
    return 0 if same == len(keys) and not only else 1


if __name__ == "__main__":
    sys.exit(main())
