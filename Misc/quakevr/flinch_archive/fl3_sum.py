import re, sys
# Summarise fl3_seq.sh output: per class, the third hit (test 5) outcome, time and steps; also test 1/2 outcomes.
txt = open(sys.argv[1]).read().replace("\n", " ")
runs = re.split(r"== spawn ", txt)[1:]
res = {}
for r in runs:
    cls = r.split()[0]
    parts = re.split(r"flinch test (\d): ", r)
    out = {}
    for i in range(1, len(parts), 2):
        t = parts[i]
        body = parts[i + 1]
        m = re.search(r"(recovered|toppled) at ([0-9.]+) s.*?(\d+)\s+steps", body)
        out[t] = (m.group(1)[0], float(m.group(2)), int(m.group(3))) if m else ("-", 0, 0)
    res.setdefault(cls, []).append(out)
for cls, lst in res.items():
    for t in ("1", "2", "5"):
        v = [o.get(t, ("?", 0, 0)) for o in lst]
        rec = [x for x in v if x[0] == "r"]
        top = [x for x in v if x[0] == "t"]
        shuffle = [x for x in v if x[2] >= 5 or (x[0] == "t" and x[1] > 1.5)]
        print("class %s test %s: n=%d rec %d top %d shuffle(>=5 steps or late topple) %d | %s" % (
            cls, t, len(v), len(rec), len(top), len(shuffle), " ".join("%s%.2f/%d" % x for x in v)))
