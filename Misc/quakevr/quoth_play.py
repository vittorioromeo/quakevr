#!/usr/bin/env python3
"""quoth_play.py <log>: quoth_play.sh's checks. Each step: start's exit to e1m1quoth taken; each flow's quothplay
verdict (its PASS/FAIL lines); the exits from e1m1quoth (to e1m2quoth) and kellbase1 (to start) taken; no engine error;
the run reached its end. Prints the FAIL lines and "quoth_play: N passed, M failed"."""
import re
import sys

text = open(sys.argv[1], encoding="utf-8", errors="replace").read()
passed, failed = 0, []


def check(ok, what):
    global passed
    if ok:
        passed += 1
    else:
        failed.append(what)


for dest in ("e1m1quoth", "e1m2quoth", "start"):
    check(re.search(r"vr_goto_exit: .* in the exit to %s\b" % dest, text) is not None, "the exit to " + dest + " taken")
flows = re.findall(r"quoth_play: flow (\w+)", text)
verdicts = re.findall(r"quothplay: (\d+) passed, (\d+) failed", text)
check(len(flows) == 5 and len(verdicts) == 5, "5 flows, 5 verdicts (%d, %d)" % (len(flows), len(verdicts)))
for m, (p, f) in zip(flows, verdicts):
    check(f == "0", "%s's flow: %s passed, %s failed" % (m, p, f))
for line in text.splitlines():
    if "quothplay" in line and ("FAIL" in line or "PASS" in line):
        print("  " + line.strip())
check(not re.search(r"Host_Error|SV_Error|OBJECT ERROR|Couldn't spawn", text), "no engine error")
check("quoth_play: done" in text, "the run reached its end")
for f in failed:
    print("FAIL " + f)
print("quoth_play: %d passed, %d failed" % (passed, len(failed)))
