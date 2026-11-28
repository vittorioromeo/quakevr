# aglod_test.py -- aglod_test.sh's verdict: every run's checks passed (4 runs: the 4th vr_agrunt_think_lod 0, "test 228 over: N passed, 0 failed") and
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
check(len(overs) == 4, "four runs ended (%d)" % len(overs))
for i, (p, f) in enumerate(overs):
    check(int(f) == 0 and int(p) > 0, "run %d: %s passed, %s failed" % (i + 1, p, f))
pk = re.findall(r"test 229 over: (\d+) passed, (\d+) failed", text)
check(len(pk) == 1 and int(pk[0][1]) == 0 and int(pk[0][0]) > 0, "229 (a worn pack walked into view): %s" % pk)
traces = re.findall(r"aglod wake (\d) trace (-?[0-9.]+)", text)
by = {}
for trial, h in traces:
    by.setdefault(trial, []).append(h)
# Seen: posed every frame from the wake in all three runs. Shot: behind him, alert, posed every other frame as before
# 1.1.1 until it aims (vr_agrunt_pose_lod 0's every frame differs by design): runs 1 and 2 must agree.
hs = by.get("0", [])
check(len(hs) == 4 and len(set(hs)) == 1, "trial 0 (seen): the same trace in all four runs: %s" % " ".join(hs))
hs = by.get("1", [])
check(len(hs) == 4 and hs[0] == hs[1], "trial 1 (shot): the same trace with 8 frames and 2 (as before): %s" % " ".join(hs))
# The game alone (its place, frame, turn, state, shots, his health; not its weapon's place): the same in all four runs,
# trial by trial: the think LOD (vr_agrunt_think_lod 0 in run 4) and the pose LOD change nothing a player could notice.
games = re.findall(r"aglod wake (\d) game (-?[0-9.]+)", text)
gb = {}
for trial, h in games:
    gb.setdefault(trial, []).append(h)
for t in ("0", "1", "2", "3"):
    hs = gb.get(t, [])
    check(len(hs) == 4 and len(set(hs)) == 1, "trial %s: the same game in all four runs: %s" % (t, " ".join(hs)))
# The unarmed one (its fists: no weapon's place in the trace, the poser's LOD only): the same in all three runs.
for t, what in (("2", "unarmed, heard"), ("3", "unarmed, shot")):
    hs = by.get(t, [])
    check(len(hs) == 4 and len(set(hs)) == 1, "trial %s (%s): the same trace in all four runs: %s" % (t, what, " ".join(hs)))
# Its reaction (the frame it took him for its enemy, the frames till it was posed after): the same in all three runs.
reacts = re.findall(r"aglod react (\d) (enemy -?\d+ pose -?\d+)", text)
rb = {}
for trial, r in reacts:
    rb.setdefault(trial, []).append(r)
for t in ("0", "1", "2", "3"):
    rs = rb.get(t, [])
    check(len(rs) == 4 and len(set(rs)) == 1 and "pose -1" not in rs[0],
          "trial %s: the same reaction in all four runs: %s" % (t, " | ".join(rs)))
print("aglod_test: %d passed, %d failed" % (passed, failed))
