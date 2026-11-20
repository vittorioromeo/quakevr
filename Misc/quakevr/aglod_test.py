# aglod_test.py -- aglod_test.sh's verdict: every run's checks passed (3 runs, "test 228 over: N passed, 0 failed") and
# the wake traces of the three runs agree, trial by trial. Prints `aglod_test: N passed, M failed`.
import re
import sys

text = open(sys.argv[1], encoding="utf-8", errors="replace").read()
passed = failed = 0


def check(ok, what):
    global passed, failed
    print(("aglod_test: PASS " if ok else "aglod_test: FAIL ") + what)
    if ok:
        passed += 1
    else:
        failed += 1


overs = re.findall(r"test 228 over: (\d+) passed, (\d+) failed", text)
check(len(overs) == 3, "three runs ended (%d)" % len(overs))
for i, (p, f) in enumerate(overs):
    check(int(f) == 0 and int(p) > 0, "run %d: %s passed, %s failed" % (i + 1, p, f))
traces = re.findall(r"aglod wake (\d) trace (-?[0-9.]+)", text)
by = {}
for trial, h in traces:
    by.setdefault(trial, []).append(h)
# Seen: posed every frame from the wake in all three runs. Shot: behind him, alert, posed every other frame as before
# 1.1.1 until it aims (vr_agrunt_pose_lod 0's every frame differs by design): runs 1 and 2 must agree.
hs = by.get("0", [])
check(len(hs) == 3 and len(set(hs)) == 1, "trial 0 (seen): the same trace in all three runs: %s" % " ".join(hs))
hs = by.get("1", [])
check(len(hs) == 3 and hs[0] == hs[1], "trial 1 (shot): the same trace with 8 frames and 2 (as before): %s" % " ".join(hs))
print("aglod_test: %d passed, %d failed" % (passed, failed))
