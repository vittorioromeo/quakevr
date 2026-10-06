import sys, re, math, glob
NUM = r"(-?[\d.]+)"
PAT = re.compile(r"^(\d) (\S+) ([LR]) S %s %s %s E %s %s %s E0 %s %s %s W %s %s %s elbow %s swivel %s" % ((NUM,) * 14))
def sub(a, b): return [x - y for x, y in zip(a, b)]
def dot(a, b): return sum(x * y for x, y in zip(a, b))
def norm(a): return math.sqrt(dot(a, a))
def swing_from_ideal(S, E, W, side):
    ax = sub(W, S); n = norm(ax); ax = [x / n for x in ax]
    e = sub(E, S); e = sub(e, [x * dot(e, ax) for x in ax])
    lat = 1 if side == "L" else -1
    ideal = [-0.8, 0.35 * lat, -1.0]; ideal = sub(ideal, [x * dot(ideal, ax) for x in ax])
    c = dot(e, ideal) / max(1e-6, norm(e) * norm(ideal))
    return math.degrees(math.acos(max(-1, min(1, c))))
def load(path, side="R"):
    out = []
    for line in open(path, errors="replace"):
        m = PAT.match(line)
        if m and m.group(3) == side:
            v = [float(x) for x in m.groups()[3:]]
            out.append(dict(S=v[0:3], E=v[3:6], W=v[9:12], angle=v[12], swivel=v[13]))
    return out
def summary(path, side="R"):
    fr = load(path, side)
    if len(fr) < 2: return None
    jumps = [norm(sub(b["E"], a["E"])) for a, b in zip(fr, fr[1:])]
    hand = [norm(sub(b["W"], a["W"])) for a, b in zip(fr, fr[1:])]
    sw = [swing_from_ideal(f["S"], f["E"], f["W"], side) for f in fr]
    i = max(range(len(jumps)), key=lambda k: jumps[k])
    out = [(f["E"][1] - f["S"][1]) * (1 if side == "L" else -1) for f in fr]
    moves = [(j, h) for j, h in zip(jumps, hand) if h > 0.2]
    ratio = max(j / h for j, h in moves) if moves else 0
    big = sum(1 for j, h in moves if j > 3 * h + 1.0)
    return dict(ratio=ratio, big=big, nm=len(moves), n=len(fr), maxjump=jumps[i], handAt=hand[i], meanhand=sum(hand) / len(hand), p99=sorted(jumps)[int(0.99 * len(jumps))],
                maxdev=max(sw), meandev=sum(sw) / len(sw), minangle=min(f["angle"] for f in fr), outmin=min(out), outmax=max(out),
                maxback=max(f["S"][0] - f["E"][0] for f in fr))
if __name__ == "__main__":
  tags = sys.argv[1:]
  names = sorted(set(re.sub(r"trace_[^_]+_", "", p.split("\\")[-1].split("/")[-1])[:-4] for p in glob.glob("trace_%s_*.txt" % tags[0])))
  print("%-12s %-4s %5s %6s %4s %7s %6s %6s %6s %6s %6s %11s %6s" % ("sweep", "tag", "moves", "ratio", "big", "maxjmp", "p99", "hand", "maxdev", "mean", "minang", "out range", "back"))
  for nm in names:
    for t in tags:
        s = summary("trace_%s_%s.txt" % (t, nm))
        if s: print("%-12s %-4s %5d %6.1f %4d %7.2f %6.2f %6.2f %6.0f %6.0f %6.0f %5.1f..%5.1f %6.1f" % (nm, t, s["nm"], s["ratio"], s["big"], s["maxjump"], s["p99"], s["meanhand"], s["maxdev"], s["meandev"], s["minangle"], s["outmin"], s["outmax"], s["maxback"]))
