#!/usr/bin/env python3
# flatreload_check.py <dir> -- flatreload_test.sh's checks over its runs' logs (<dir>/<run>.log). Prints a PASS or FAIL line
# a check and "flatreload: N passed, M failed"; exits 1 on a failure.
import os
import re
import sys

D = sys.argv[1]
passed = failed = 0
DOWN, HOLD, UP = 0.3, 0.2, 0.3  # vr_flat_reload_down, _hold, _up (the tunables' defaults)
TOL = 0.05  # seconds: the client sees the server's bit a frame or two late (1/72 s each)


def check(ok, what):
    global passed, failed
    print(f"flatreload: {'PASS' if ok else 'FAIL'} {what}")
    if ok:
        passed += 1
    else:
        failed += 1


def lines(run):
    p = os.path.join(D, run + ".log")
    return open(p, encoding="utf-8", errors="replace").read().splitlines() if os.path.exists(p) else []


def ended(run):
    return any(l.startswith(f"flatreload_test: {run} done") for l in lines(run))


START = re.compile(r"^flat reload: start (main|off) hand \((.*)\): weapon (\d+), clip (\d+) of (\d+), storage (\d+)")
HIP = re.compile(r"^flat reload: (main|off) hand at the hip \(([\d.]+) s\): clip (\d+) of (\d+), storage (\d+)")
DONE = re.compile(r"^flat reload: (main|off) hand done \(([\d.]+) s\): clip (\d+) of (\d+), storage (\d+)")
CHIP = re.compile(r"^flatreload: (main|off) hand at the hip \(([\d.]+) s\): ([\d.]+) units from the (left|right) hip holster, pitch ([-\d.]+)")
CBACK = re.compile(r"^flatreload: (main|off) hand back in its pose \(([\d.]+) s\)")
HUD = re.compile(r"^flathud: right hand: weapon (\d+) .*\(raw: ammo type \d+ counter (\d+) clip (\d+)/(\d+)\)")
REP = re.compile(r"^flatreload: main hand: server (\d), share ([\d.]+), .*hotspot (\d+)")
CANCEL = re.compile(r"^flat reload: (main|off) hand cancelled")
TOOK = re.compile(r"^flat reload: main hand took weapon (\d+): clip (\d+) of (\d+), storage (\d+)")
AWAY = re.compile(r"^flat reload: main hand put weapon (\d+) away: (\d+) rounds")


def all_of(run, rx):
    return [m for m in (rx.match(l) for l in lines(run)) if m]


for run in ("rkey", "auto", "autooff", "twohand", "switch", "contact", "vrhip"):
    check(ended(run), f"{run}: the run reached its last line")

# rkey: R reloads with VR's hip-holster sums; the trip's times and pose; nothing to reload; no fire during the trip.
st, hip, chip, cback = all_of("rkey", START), all_of("rkey", HIP), all_of("rkey", CHIP), all_of("rkey", CBACK)
if st and hip:
    a, size, b = int(st[0].group(4)), int(st[0].group(5)), int(st[0].group(6))
    c, s = int(hip[0].group(3)), int(hip[0].group(5))
    check(st[0].group(2) == "R" and a < size and c == size and s == b - (size - a),
          f"rkey: R reloads the shotgun: clip {a} of {size}, storage {b} -> clip {c}, storage {s} (VR's sums: {size}, {b - (size - a)})")
    check(abs(float(hip[0].group(2)) - DOWN) <= TOL, f"rkey: the rounds go in at the hip, {hip[0].group(2)} s after R (vr_flat_reload_down {DOWN})")
else:
    check(False, "rkey: R started a reload and it reached the hip")
    check(False, "rkey: the rounds go in at the hip")
if chip and cback:
    t1, gap, pitch, t2 = float(chip[0].group(2)), float(chip[0].group(3)), float(chip[0].group(5)), float(cback[0].group(2))
    check(abs(t1 - DOWN) <= TOL and gap <= 2.0 and chip[0].group(4) == "right",
          f"rkey: the drawn hand reaches the right hip holster at {t1:.2f} s ({gap:.1f} units from it; down {DOWN} s)")
    check(85.0 <= pitch <= 95.0, f"rkey: the gun turned muzzle-down there (pitch {pitch:.1f}, vr_flat_reload_angle 90)")
    check(abs(t2 - (DOWN + HOLD + UP)) <= TOL, f"rkey: the hand back in its pose at {t2:.2f} s (the trip {DOWN + HOLD + UP:.2f} s)")
else:
    for w in ("the drawn hand reaches the hip", "the gun turned muzzle-down", "the hand back in its pose"):
        check(False, f"rkey: {w} (no trip printed)")
nothing = [l for l in lines("rkey") if l.startswith("flat reload: R: nothing to reload")]
check(any("clip 8 of 8" in l for l in nothing), "rkey: R with the clip full does nothing")
check(any(re.search(r"storage 0;", l) for l in nothing), "rkey: R with no shells left does nothing")
# The second trip: the trigger held from 4 frames after R to past the hip; the next report still full, the one after the
# shot taken once the trip is over one round down.
done = all_of("rkey", DONE)
huds = all_of("rkey", HUD)
rk = lines("rkey")
if len(done) >= 2:
    i = rk.index(done[1].string)
    after = [HUD.match(l) for l in rk[i:] if HUD.match(l)]
    ok = len(after) >= 2 and int(after[0].group(3)) == int(after[0].group(4)) and int(after[1].group(3)) < int(after[1].group(4))
    check(ok, "rkey: the trigger held through the trip fires nothing (clip full at its end: %s), and fires after it (%s)" %
          (after[0].group(3) if after else "?", after[1].group(3) if len(after) > 1 else "?"))
else:
    check(False, "rkey: the second trip (the trigger held) ran to its end")

# auto, autooff
st = all_of("auto", START)
hip = all_of("auto", HIP)
check(bool(st) and st[0].group(2).startswith("auto") and st[0].group(4) == "0" and bool(hip) and hip[0].group(3) == hip[0].group(4),
      "auto: the clip fired empty reloads by itself (%s)" % (st[0].group(0) if st else "no start"))
h = all_of("autooff", HUD)
check(not all_of("autooff", START) and bool(h) and h[-1].group(3) == "0", "autooff: with vr_flat_autoreload 0 the empty clip stays empty (%s/%s)" %
      ((h[-1].group(3), h[-1].group(4)) if h else ("?", "?")))

# twohand
stance = [l for l in lines("twohand") if l.startswith("  stance:")]
check(len(stance) >= 3 and "going on it, share 1.00" in stance[0] and "hanging" in stance[1] and "going on it, share 1.00" in stance[2],
      "twohand: the free hand on the nail gun's hotspot, hanging relaxed during the trip, back on it after (%s)" %
      " | ".join(s.split(",")[2].strip() if len(s.split(",")) > 2 else s for s in stance[:3]))

# switch: a switch before the hip moves nothing; after the hip the rounds are in; clip + storage kept; then R as ever.
sw = lines("switch")
st, cancel, took, away = all_of("switch", START), all_of("switch", CANCEL), all_of("switch", TOOK), all_of("switch", AWAY)
check(len(st) >= 3 and len(cancel) >= 2, f"switch: two trips cancelled by a weapon switch ({len(cancel)} cancelled of {len(st)} started)")
if len(st) >= 2 and len(took) >= 2 and len(cancel) >= 2:
    i0, ic0 = sw.index(st[0].string), sw.index(cancel[0].string)
    nohip = not any(HIP.match(l) for l in sw[i0:ic0])
    sum0 = int(st[0].group(4)) + int(st[0].group(6))
    sum1 = int(took[0].group(2)) + int(took[0].group(4))
    check(nohip and sum0 == sum1, f"switch: before the hip nothing moved: clip + storage {sum0} before, {sum1} with the gun back")
    i1, ic1 = sw.index(st[1].string), sw.index(cancel[1].string)
    hip1 = [HIP.match(l) for l in sw[i1:ic1] if HIP.match(l)]
    sum2 = int(st[1].group(4)) + int(st[1].group(6))
    sum3 = int(took[1].group(2)) + int(took[1].group(4))
    check(bool(hip1) and sum2 == sum3, f"switch: after the hip the rounds were in: clip + storage {sum2} before, {sum3} with the gun back")
else:
    check(False, "switch: the sums before and after a switch before the hip")
    check(False, "switch: the sums before and after a switch after the hip")
d = all_of("switch", DONE)
reps = all_of("switch", REP)
check(bool(d) and d[-1].group(3) == d[-1].group(4) and bool(reps) and reps[-1].group(1) == "0" and reps[-1].group(2) == "0.00",
      "switch: the gun back in the hand reloads again as ever, and nothing is left on its trip (server 0, share 0.00)")

# Q mid-trip: the third cancel, the shotgun in the left hand with the clip it had, nothing on its trip.
left = [l for l in sw if l.startswith("flathud: left hand:")]
check(len(cancel) >= 3 and bool(left) and re.search(r"weapon 4 .*clip [0-9]+/8", left[-1]) is not None and
      bool(reps) and reps[-1].group(1) == "0",
      "switch: Q mid-trip cancels it; the shotgun in the left hand with its clip (%s)" % (left[-1][-40:] if left else "no report"))

# contact: nothing reloads at a holster, the hand at one (hotspot 9: the right upper holster) and at the hip.
cs = all_of("contact", HUD)
reps = all_of("contact", REP)
clips = sorted(set(m.group(3) for m in cs))
check(not all_of("contact", START) and not all_of("contact", HIP) and len(clips) == 1,
      f"contact: no reload with the view turning, looking up and down and walking, the gun at rest, at the hip and half way (clips seen: {clips})")
check(any(m.group(3) == "9" for m in reps) and any(m.group(2) == "1.00" for m in reps),
      "contact: the hand was at a holster (hotspot 9 seen) and held at the hip (share 1.00): the test saw what a reload would need")

# vrhip: the headset's holster reloads (the control), with the same sums.
h = all_of("vrhip", HUD)
if len(h) >= 2:
    a, size, b = int(h[0].group(3)), int(h[0].group(4)), int(h[0].group(2))
    c, s = int(h[-1].group(3)), int(h[-1].group(2))
    check(a < size and c == size and s == b - (size - a),
          f"vrhip: in the headset the hand at a holster reloads (clip {a} -> {c}, storage {b} -> {s}): the sums flat R gives")
else:
    check(False, "vrhip: the headset's holster reload reported")

# The guns' own parts (his words 2026-11-10; vr_flatreload.qc).
for run in ("ssg", "mag", "revolver", "revempty", "kick"):
    check(ended(run), f"{run}: the run reached its last line")
BREAK_DELAY = 0.15  # vr_flat_ssg_break_delay
L = lines("ssg")
# (The console wraps the long line: "after its last shot" may be on the next one.)
brk = [re.search(r"broken open by flat-screen play: fired empty: (\d+) spent and (\d+) live thrown out(?:, ([\d.]+) s)?", l) for l in L]
brk = [m for m in brk if m]
check(bool(brk) and brk[0].group(1) == "2" and brk[0].group(2) == "0" and brk[0].group(3) is not None and
      abs(float(brk[0].group(3)) - BREAK_DELAY) <= TOL,
      f"ssg: fired empty, it breaks open by itself {brk[0].group(3) if brk else '?'} s after the shot (vr_flat_ssg_break_delay "
      f"{BREAK_DELAY}), its 2 spent shells thrown out")
check(any("a dry click: the super shotgun" in l for l in L), "ssg: open, the trigger only clicks")
shut = [l for l in L if "closed by flat-screen play" in l]
check(bool(shut) and shut[0].rstrip().endswith("2 loaded") and len(brk) == 1,
      f"ssg: R shuts it with 2 loaded ({shut[0].strip() if shut else 'not shut'}), broken open once")
L = lines("mag")
out = [l for l in L if re.search(r"out of the gun \(hand \d+, flat-screen play: fired empty\)", l)]
seat = [l for l in L if "a new magazine seated" in l]
check(len(out) == 1, f"mag: fired empty, the nailgun drops its magazine ({len(out)} dropped)")
check(bool(seat) and not seat[0].rstrip().endswith("(clip 0)"), "mag: R seats a new one with its rounds")
L = lines("revolver")
sw = [l for l in L if "swung out by flat-screen play: the reload" in l]
ej = [re.search(r"its shells out by flat-screen play: the reload: (\d+) spent, (\d+) live", l) for l in L]
ej = [m for m in ej if m]
home = [re.search(r"swung home by flat-screen play: the reload.*, (\d+) live", l) for l in L]
home = [m for m in home if m]
check(len(sw) == 1, "revolver: R swings the cylinder out")
rst = all_of("revolver", START)
fired = int(rst[0].group(5)) - int(rst[0].group(4)) if rst else -1
check(bool(ej) and fired > 0 and int(ej[0].group(1)) == fired and ej[0].group(2) == "0",
      f"revolver: its spent shells out ({fired} fired), the live ones kept in ({ej[0].group(0) if ej else 'none'})")
check(bool(home) and home[0].group(1) == "6", f"revolver: home again with 6 live ({home[0].group(0) if home else 'not home'})")
# revempty: fired empty, its cylinder out by itself with its 6 spent shells, a dry click out, R: home with 6 live.
L = lines("revempty")
iout = [i for i, l in enumerate(L) if "swung out by flat-screen play: fired empty" in l]
ej = [m for m in (re.search(r"its shells out by flat-screen play: fired empty: (\d+) spent, (\d+) live", l) for l in L) if m]
click = [i for i, l in enumerate(L) if "a dry click: the revolver" in l]
rst = all_of("revempty", START)
home = [m for m in (re.search(r"swung home by flat-screen play: the reload.*, (\d+) live", l) for l in L) if m]
check(len(iout) == 1 and not any("swung out by flat-screen play: the reload" in l for l in L),
      "revempty: fired empty, its cylinder swings out by itself (once; not again for the reload)")
check(bool(ej) and ej[0].group(1) == "6" and ej[0].group(2) == "0", f"revempty: its 6 spent shells out ({ej[0].group(0) if ej else 'none'})")
check(bool(click) and bool(iout) and click[0] > iout[0], "revempty: out, the trigger only clicks")
check(bool(rst) and rst[0].group(4) == "0" and bool(home) and home[0].group(1) == "6",
      f"revempty: R from empty, home again with 6 live ({home[0].group(0) if home else 'not home'})")
kicks = [float(m.group(1)) for m in (re.match(r"^flatxhair: main kick [\d.]+ spread ([\d.]+) px", l) for l in lines("kick")) if m]
check(len(kicks) == 3 and kicks[0] == 0 and kicks[1] > 2 and kicks[2] < 0.5,
      f"kick: the crosshair spreads with the shot's recoil and settles back (spread {kicks} px)")

print(f"flatreload: {passed} passed, {failed} failed")
sys.exit(1 if failed else 0)
