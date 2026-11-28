#!/usr/bin/env python3
# flatprocreload_check.py <dir> -- flatprocreload_test.sh's checks over its runs' logs (<dir>/<run>.log). Prints a PASS or
# FAIL line a check and "flatprocreload: N passed, M failed"; exits 1 on a failure.
import os
import re
import sys

D = sys.argv[1]
passed = failed = 0


def check(ok, what):
    global passed, failed
    print(f"flatprocreload: {'PASS' if ok else 'FAIL'} {what}")
    if ok:
        passed += 1
    else:
        failed += 1


def lines(run):
    p = os.path.join(D, run + ".log")
    return open(p, encoding="utf-8", errors="replace").read().splitlines() if os.path.exists(p) else []


def timed_out(run):
    return any("fprl_test: %s timed out" % run in l for l in lines(run))


START = re.compile(r"^flat proc reload: start (\w+) hand \((.*)\): weapon (\d+), clip (\d+) of (\d+), storage (\d+)")
IN = re.compile(r"^flat proc reload: (\w+) hand: (\d+) in \(([\d.]+) s\): clip (\d+) of (\d+), storage (\d+)")
END = re.compile(r"^flat proc reload: (\w+) hand (done|cancelled) \((.*); ([\d.]+) s, (\d+) taken\): clip (\d+) of (\d+), storage (\d+)")
TAKE = re.compile(r"^flat proc reload: take: (\d+) from the pouch")

# trips: 2 a trip to full, nothing lost.
L = lines("trips")
st = [m for m in map(START.match, L) if m]
ins = [m for m in map(IN.match, L) if m]
end = [m for m in map(END.match, L) if m]
takes = [int(m.group(1)) for m in map(TAKE.match, L) if m]
check(not timed_out("trips") and bool(st) and bool(end), "trips: R started a procedural reload and it ended (%d starts, %d ends)" %
      (len(st), len(end)))
if st and end:
    clip0, size, stor0 = int(st[0].group(4)), int(st[0].group(5)), int(st[0].group(6))
    clip1, stor1 = int(end[-1].group(6)), int(end[-1].group(8))
    check(end[-1].group(2) == "done" and clip1 == size, "trips: done with the gun full (%s; clip %d of %d)" % (end[-1].group(3), clip1, size))
    steps = [int(m.group(2)) for m in ins]
    check(len(steps) >= 2 and all(n == 2 for n in steps[:-1]) and steps[-1] in (1, 2), "trips: the clip rose by 2 a trip (%s)" % steps)
    check(clip0 + stor0 == clip1 + stor1, "trips: no shell lost (clip + storage %d before, %d after)" % (clip0 + stor0, clip1 + stor1))
    check(sum(takes) == clip1 - clip0, "trips: taken from the pouch %d, loaded %d" % (sum(takes), clip1 - clip0))
    times = [float(m.group(3)) for m in ins]
    gaps = [b - a for a, b in zip(times, times[1:])]
    check(bool(times) and times[0] < 0.9 and all(g < 0.9 for g in gaps), "trips: a trip under 0.9 s (first %s s, then %s)" %
          (times[:1], ["%.2f" % g for g in gaps]))
    print("flatprocreload: timing: first pair in at %.2f s, every next one %.2f s apart, done at %s s" %
          (times[0] if times else -1, (sum(gaps) / len(gaps)) if gaps else -1, end[-1].group(4)))

# fire: a shot mid-carry cancels; the pair back in the pouch; nothing lost.
L = lines("fire")
st = [m for m in map(START.match, L) if m]
end = [m for m in map(END.match, L) if m]
back = [l for l in L if l.startswith("flat proc reload: the round in the off hand back in the pouch")]
check(not timed_out("fire") and bool(st) and bool(end) and end[-1].group(2) == "cancelled" and end[-1].group(3) == "fired",
      "fire: the shot cancelled it (%s)" % (end[-1].group(3) if end else "no end"))
check(bool(back), "fire: the pair in the hand went back in the pouch")
if st and end:
    b = int(st[0].group(4)) + int(st[0].group(6))
    a = int(end[-1].group(6)) + int(end[-1].group(8))
    check(a == b, "fire: no shell lost (clip + storage %d before, %d after)" % (b, a))

# blocked: the client's failsafe, the pair back.
L = lines("blocked")
end = [m for m in map(END.match, L) if m]
fs = [l for l in L if l.startswith("flatprocreload: cancelled (failsafe: the round wasn't seated")]
st = [m for m in map(START.match, L) if m]
check(not timed_out("blocked") and bool(fs), "blocked: the client's failsafe cancelled it (%d)" % len(fs))
check(bool(end) and end[-1].group(2) == "cancelled" and "client's cancel" in end[-1].group(3), "blocked: the server cancelled it (%s)" %
      (end[-1].group(3) if end else "no end"))
if st and end:
    b = int(st[0].group(4)) + int(st[0].group(6))
    a = int(end[-1].group(6)) + int(end[-1].group(8))
    check(a == b and int(end[-1].group(6)) == int(st[0].group(4)), "blocked: nothing loaded, nothing lost (%d before, %d after)" % (b, a))

# dual, ssg: the timed trip, no procedural one.
for run in ("dual", "ssg"):
    L = lines(run)
    basic = [l for l in L if l.startswith("flat reload: start main hand")]
    proc = [l for l in L if l.startswith("flat proc reload: start")]
    check(not timed_out(run) and bool(basic) and not proc, "%s: the timed trip to the hip (%d), no procedural reload (%d)" %
          (run, len(basic), len(proc)))
L = lines("dual")
check(any(l.startswith("flat reload: start off hand") for l in L), "dual: the off hand's shotgun reloads at the hip too")

print(f"flatprocreload: {passed} passed, {failed} failed")
sys.exit(1 if failed else 0)
