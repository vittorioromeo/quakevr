"""Flinch balance suite (agent/flinch4): per terrain x case, N runs (jittered spot and stand phase), one mode.
Usage: python fl4_suite.py <label> <vr_flinch> [runs] [extra console commands]
Writes scratch/fl4_<label>.csv and prints a summary per case."""
import os, re, sys, statistics as st

TREE = "C:/OHWorkspace/qvr-agents/fastship"
sys.path.insert(0, TREE + "/Misc/quakevr/testbatch")
import testbatch  # noqa: E402

label, mode = sys.argv[1], sys.argv[2]
runs = int(sys.argv[3]) if len(sys.argv) > 3 else 8
extra = sys.argv[4] if len(sys.argv) > 4 else ""
only = os.environ.get("FL4_ONLY", "")

TERRAINS = [("flat", -240, 100, 24), ("slope", -160, 540, 60), ("stairs", 200, 592, 140)]
CASES = [("sg64", 1, 64), ("sg192", 1, 192), ("blow", 5, 0), ("upper", 10, 0)]

b = testbatch.Batch("fastship", "fl4_" + label)
meta = []
for tn, x, y, z in TERRAINS:
    for cn, test, dist in CASES:
        if only and only not in (tn, cn, tn + "/" + cn):
            continue
        for k in range(runs):
            jx = (k % 4) * 8 - 12
            jw = 90 + 9 * k
            s = (f"map vrtesthall; wait30; god; notarget; developer 1; vr_flinch_debug 1; vr_flinch {mode}; {extra}; "
                 f"setpos {x} {y - 100} 24 0 90 0; vr_flinch_test_x {x + jx}; vr_flinch_test_y {y}; vr_flinch_test_z {z}; "
                 f"vr_flinch_test 20; wait{jw}; vr_flinch_test_dist {dist}; vr_flinch_test {test}; "
                 f"vr_waitprint 400 result; wait10; toggleconsole; quit")
            n = b.case(["-Script", s, "-Filter", r"flinch: [0-9]+ (result|monster_army struck)|flinch: monster_army by|ENGINE",
                        "-Timeout", "120"])
            meta.append((n, tn, cn))
b.run()

rows = []
for n, tn, cn in meta:
    out = b.output(n)
    dv = re.search(r"struck \(\d+ N s, ([0-9.]+) m/s", out)
    r = re.search(r"result (\w+) at ([0-9.]+) s: (\d+) steps, along the hit ([-0-9.]+), stepped (\d+) units, rise ([-0-9.]+)", out)
    down = "down at once" in out or "toppled (no stagger)" in out
    crash = "ENGINE" in out
    if r:
        rows.append((tn, cn, float(dv.group(1)) if dv else -1, r.group(1), float(r.group(2)), int(r.group(3)),
                     float(r.group(4)), int(r.group(5)), float(r.group(6))))
    else:
        rows.append((tn, cn, float(dv.group(1)) if dv else -1, "crash" if crash else "down" if down else "none", 0, 0, 0, 0, 0))

with open(f"{TREE}/scratch/fl4_{label}.csv", "w") as f:
    f.write("terrain,case,dv,outcome,t,steps,along,stepped_u,rise_u\n")
    for r in rows:
        f.write(",".join(str(v) for v in r) + "\n")

print(f"{label} (vr_flinch {mode}) {extra}")
print("case          dv    rec fell  steps   along  stepped  rise   rec_t")
for tn, _, _, _ in TERRAINS:
    for cn, _, _ in CASES:
        rs = [r for r in rows if r[0] == tn and r[1] == cn]
        if not rs:
            continue
        rec = [r for r in rs if r[3] == "recovered"]
        fell = [r for r in rs if r[3] in ("fell", "toppled", "down")]
        other = len(rs) - len(rec) - len(fell)
        steps = [r[5] for r in rs if r[3] not in ("down", "none", "crash")]
        along = [r[6] for r in rs if r[5] > 0]
        print(f"{tn + '/' + cn:12s} {st.mean([r[2] for r in rs]):5.2f}  {len(rec):3d} {len(fell):4d}{'+' + str(other) if other else '  '} "
              f"{st.mean(steps) if steps else 0:5.1f} {st.mean(along) if along else 0:7.2f} {st.mean([r[7] for r in rs]):7.0f} "
              f"{max(r[8] for r in rs):6.1f} {st.mean([r[4] for r in rec]) if rec else 0:6.2f}")
