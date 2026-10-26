"""cup_aim_check.py <log> -- the verdict of cup_aim_test.sh: the barrel's pitch at each step (vr_aim_check after an
"cupaim step on<0|1> <mid|up|down>" line). On: raising and lowering the main hand 4 cm with the off hand still tilts the
barrel (more than 2 degrees each way, opposite ways); off: it stays within 0.5 degrees. Every step's shots within 0.5
degrees of the barrel. The off hand must have taken the grip."""
import re
import sys

lines = open(sys.argv[1], encoding="utf-8", errors="replace").read().splitlines()
pitch, shot = {}, {}
step = None
took = any("hand took it" in l for l in lines)
for l in lines:
    m = re.search(r"cupaim step (on[01]) (mid|up|down)", l)
    if m:
        step = (m.group(1), m.group(2))
        continue
    m = re.search(r"aim_check: main hand slot \d+: shot-barrel ([-\d.]+) deg.*pitch shot ([-\d.]+) barrel ([-\d.]+)", l)
    if m and step:
        shot[step] = float(m.group(1))
        pitch[step] = float(m.group(3))
        step = None
passed = failed = 0


def check(what, ok, got):
    global passed, failed
    print("cupaim: %s: %s (%s)" % (what, "PASS" if ok else "FAIL", got))
    if ok:
        passed += 1
    else:
        failed += 1


check("the off hand took the cup", took, "a '2h grip: ... took it' line" if took else "none")
for on in ("on1", "on0"):
    if not all((on, k) in pitch for k in ("mid", "up", "down")):
        check(on + ": all steps measured", False, str(sorted(pitch)))
        continue
    up = pitch[(on, "up")] - pitch[(on, "mid")]
    down = pitch[(on, "down")] - pitch[(on, "mid")]
    got = "barrel pitch mid %.2f, up %+.2f, down %+.2f" % (pitch[(on, "mid")], up, down)
    if on == "on1":
        check("Two-Handed Aim on: the barrel follows the line between the hands", abs(up) > 2 and abs(down) > 2 and up * down < 0, got)
    else:
        check("Two-Handed Aim off: only the main hand aims", abs(up) < 0.5 and abs(down) < 0.5, got)
if ("on1", "mid") in pitch and ("on0", "mid") in pitch:
    jump = pitch[("on1", "mid")] - pitch[("on0", "mid")]
    check("taking the cup doesn't turn the weapon", abs(jump) < 1, "on %+.2f deg against off" % jump)
worst = max([abs(v) for v in shot.values()] or [99])
check("the shots follow the barrel", worst < 0.5, "worst shot-barrel %.2f deg" % worst)
print("cupaim: %d passed, %d failed" % (passed, failed))
