# axe_stick_rates.py -- the thrown axe stick-rate test (ROUND21.md, "Thrown axes: the blade decides"; axe_stick_rates.sh):
#   gen <vr_test_axe mode> <throws> <seed> <cfg>: a cfg of synthetic hand throws (impulse 209) at 3-7 m, 7-12 m/s
#   parse <log>: stuck / bounced / never sticks (Box3D touched first) / none, and the reasons
import sys, random, re, collections
if sys.argv[1] == "gen":
    mode, n, seed, path = int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), sys.argv[5]
    r = random.Random(seed)
    L = [f"vr_test_axe {mode}", "vr_test_axe_loft 1", "vr_test_axe_damage 0"]
    for i in range(n):
        L.append(f"echo AXETHROW {i}; vr_test_axe_dist {r.uniform(120,280):.0f}; vr_test_axe_speed {r.uniform(7,12):.1f}; vr_test_axe_side {r.uniform(-40,40):.0f}; impulse 209"); L.extend(["wait"] * 110)
    L.append("echo AXEDONE")
    open(path, "w", newline="\n").write("\n".join(L) + "\n")
else:
    res = collections.Counter(); why = collections.Counter(); cur = None
    def close():
        if cur is not None: res[cur] += 1
    for line in open(sys.argv[2], encoding="utf-8", errors="replace"):
        if "AXETHROW" in line:
            close(); cur = "none"; continue
        if "AXEDONE" in line:
            close(); cur = None; continue
        if cur is None or "axestick:" not in line: continue
        m = re.search(r"\(([0-9.]+) m/s\)", line)
        if m and float(m.group(1)) > 40: cur = "invalid"; continue  # (a test throw gone wild: not counted)
        if " stuck in " in line and cur in ("none", "bounce"): cur = "stuck"
        elif "never sticks now" in line and cur == "none":
            cur = "out"; m = re.search(r"with (.*?) \(", line); why["out: " + (m.group(1) if m else "?")] += 1
        elif "bounces (" in line and cur == "none":
            cur = "bounce"; m = re.search(r"bounces \(([^)]*)\)", line); why[m.group(1) if m else "?"] += 1
    bad = res.pop("invalid", 0); n = sum(res.values())
    print(f"n={n} stuck={res['stuck']} ({100*res['stuck']/max(n,1):.0f}%) bounce={res['bounce']} out={res['out']} none={res['none']} (not counted: {bad})")
    print("  " + "; ".join(f"{k} {v}" for k, v in why.most_common(6)))
