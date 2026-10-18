"""Flinch balance suite (agent/flinch7): 6 cases x N runs (jittered spot and stand phase), one mode, one or more classes.
Usage: python fl7_suite.py <label> <vr_flinch> [runs] [extra console commands]
Env: FL7_CLASS (comma list of vr_flinch_test_class, default 0; "all" = 0,1,2,3; "every" = 0..5), FL7_ONLY (substring of
'terrain/case'), FL7_CASES (comma list of terrain/case to replace the six).
Writes scratch/fl7_<label>.csv, appends a summary to scratch/fl7_table.txt and prints it (per class, plus a one-line
recovered/8 row per class)."""
import os, re, sys, statistics as st

TREE = "C:/OHWorkspace/qvr-agents/fastship"
sys.path.insert(0, TREE + "/Misc/quakevr/testbatch")
import testbatch  # noqa: E402

label, mode = sys.argv[1], sys.argv[2]
runs = int(sys.argv[3]) if len(sys.argv) > 3 else 8
extra = sys.argv[4] if len(sys.argv) > 4 else ""
only = os.environ.get("FL7_ONLY", "")
clsArg = os.environ.get("FL7_CLASS", "0")
classes = ["0", "1", "2", "3"] if clsArg == "all" else ["0", "1", "2", "3", "4", "5"] if clsArg == "every" else clsArg.split(",")
NAMES = {"0": "grunt", "1": "enforcer", "2": "knight", "3": "dknight", "4": "ogre", "5": "zombie"}

TERRAIN = {"flat": (-240, 100, 24), "slope": (-160, 540, 60), "stairs": (200, 592, 140)}
TESTS = {"sg64": (1, 64), "blow": (5, 0), "upper": (10, 0), "sg192": (1, 192), "nail": (3, 0), "snail": (4, 0)}
CASES = [("flat", "sg64"), ("flat", "blow"), ("flat", "upper"), ("slope", "blow"), ("stairs", "sg64"), ("stairs", "blow")]
if os.environ.get("FL7_CASES"):
    CASES = [tuple(c.split("/")) for c in os.environ["FL7_CASES"].split(",")]

b = testbatch.Batch("fastship", "fl7_" + label)
meta = []
for cls in classes:
    for tn, cn in CASES:
        if only and only not in tn + "/" + cn:
            continue
        x, y, z = TERRAIN[tn]
        test, dist = TESTS[cn]
        for k in range(runs):
            jx = (k % 4) * 8 - 12
            jw = 90 + 9 * k
            s = (f"map vrtesthall; wait30; god; notarget; developer 1; vr_flinch_debug 1; vr_flinch {mode}; vr_flinch_test_class {cls}; {extra}; "
                 f"setpos {x} {y - 100} 24 0 90 0; vr_flinch_test_x {x + jx}; vr_flinch_test_y {y}; vr_flinch_test_z {z}; "
                 f"vr_flinch_test 20; wait{jw}; vr_flinch_test_dist {dist}; vr_flinch_test {test}; "
                 f"vr_waitprint 400 result; wait10; toggleconsole; quit")
            n = b.case(["-Script", s, "-Filter", r"flinch: [0-9]+ (result|monster_[a-z_]+ struck)|flinch: monster_[a-z_]+ by|ENGINE",
                        "-Timeout", "120"])
            meta.append((n, cls, tn, cn))
b.run()

rows = []
for n, cls, tn, cn in meta:
    out = b.output(n)
    dv = re.search(r"struck \(\d+ N s, ([0-9.]+) m/s", out)
    r = re.search(r"result (\w+) at ([0-9.]+) s: (\d+) steps, along the hit ([-0-9.]+), stepped (\d+) units, rise ([-0-9.]+)", out)
    down = "down at once" in out or "toppled (no stagger)" in out
    crash = "ENGINE" in out
    if r:
        rows.append((cls, tn, cn, float(dv.group(1)) if dv else -1, r.group(1), float(r.group(2)), int(r.group(3)),
                     float(r.group(4)), int(r.group(5)), float(r.group(6))))
    else:
        rows.append((cls, tn, cn, float(dv.group(1)) if dv else -1, "crash" if crash else "down" if down else "none", 0, 0, 0, 0, 0))

with open(f"{TREE}/scratch/fl7_{label}.csv", "w") as f:
    f.write("class,terrain,case,dv,outcome,t,steps,along,stepped_u,rise_u\n")
    for r in rows:
        f.write(",".join(str(v) for v in r) + "\n")

lines = [f"== {label} (vr_flinch {mode}) {extra}"]
short = []
for cls in classes:
    lines.append(f"-- class {cls} {NAMES.get(cls, '')}: case rec fell steps along stepped rise rec_t")
    cells = []
    for tn, cn in CASES:
        rs = [r for r in rows if r[0] == cls and r[1] == tn and r[2] == cn]
        if not rs:
            continue
        rec = [r for r in rs if r[4] == "recovered"]
        fell = [r for r in rs if r[4] in ("fell", "toppled", "down")]
        other = len(rs) - len(rec) - len(fell)
        steps = [r[6] for r in rs if r[4] not in ("down", "none", "crash")]
        along = [r[7] for r in rs if r[6] > 0]
        lines.append(f"{tn + '/' + cn:13s} {len(rec):3d} {len(fell):4d}{'+' + str(other) if other else '  '} "
                     f"{st.mean(steps) if steps else 0:5.1f} {st.mean(along) if along else 0:6.2f} {st.mean([r[8] for r in rs]):6.0f} "
                     f"{max(r[9] for r in rs):5.1f} {st.mean([r[5] for r in rec]) if rec else 0:6.2f}")
        cells.append(f"{len(rec)}" + (f"(+{other})" if other else "") + f"/{st.mean(steps) if steps else 0:.1f}")
    short.append(f"{NAMES.get(cls, cls):9s} " + "  ".join(f"{c:9s}" for c in cells))
lines.append("short (recovered[(+none)] / mean steps): " + "  ".join(t + "/" + c for t, c in CASES))
lines += short
text = "\n".join(lines) + "\n"
print(text)
with open(f"{TREE}/scratch/fl7_table.txt", "a") as f:
    f.write(text + "\n")
