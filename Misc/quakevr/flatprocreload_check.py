#!/usr/bin/env python3
# flatprocreload_check.py <dir> [base|matrix|all] -- flatprocreload_test.sh's checks over its runs' logs (<dir>/<run>.log). Prints a PASS or
# FAIL line a check and "flatprocreload: N passed, M failed"; exits 1 on a failure.
import os
import re
import sys

D = sys.argv[1]
MODE = sys.argv[2] if len(sys.argv) > 2 else "all"
passed = failed = 0


def check(ok, what):
    global passed, failed
    print(f"flatprocreload: {'PASS' if ok else 'FAIL'} {what}")
    if ok:
        passed += 1
    else:
        failed += 1


def lines(run):
    # The console wraps long lines: a line that doesn't start one of ours goes back onto the one before.
    p = os.path.join(D, run + ".log")
    out = []
    for l in (open(p, encoding="utf-8", errors="replace").read().splitlines() if os.path.exists(p) else []):
        if out and not re.match(r"^(flat|fprl_|exit=)", l):
            out[-1] += l
        else:
            out.append(l)
    return out


def timed_out(run):
    return any(l.startswith("fprl_test: %s timed out" % run) for l in lines(run))


START = re.compile(r"^flat proc reload: start (\w+) hand \((.*)\): weapon (\d+), clip (\d+) of (\d+), storage (\d+)")
IN = re.compile(r"^flat proc reload: (\w+) hand: (\d+) in \(([\d.]+) s\): clip (\d+) of (\d+), storage (\d+)")
END = re.compile(r"^flat proc reload: (\w+) hand (done|cancelled) \((.*); ([\d.]+) s, (\d+) taken\): clip (\d+) of (\d+), storage (\d+)")
TAKE = re.compile(r"^flat proc reload: take: (\d+) from the pouch")

def base_checks():
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

    # melee: the shove's impulse cancels; the pair back.
    L = lines("melee")
    st = [m for m in map(START.match, L) if m]
    end = [m for m in map(END.match, L) if m]
    check(not timed_out("melee") and bool(end) and end[-1].group(2) == "cancelled" and "quick melee" in end[-1].group(3),
          "melee: a quick melee cancelled it (%s)" % (end[-1].group(3) if end else "no end"))
    if st and end:
        b = int(st[0].group(4)) + int(st[0].group(6))
        a = int(end[-1].group(6)) + int(end[-1].group(8))
        check(a == b, "melee: no shell lost (clip + storage %d before, %d after)" % (b, a))

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

    # dual: the timed trip, no procedural one.
    for run in ("dual",):
        L = lines(run)
        basic = [l for l in L if l.startswith("flat reload: start main hand")]
        proc = [l for l in L if l.startswith("flat proc reload: start")]
        check(not timed_out(run) and bool(basic) and not proc, "%s: the timed trip to the hip (%d), no procedural reload (%d)" %
              (run, len(basic), len(proc)))
    L = lines("dual")
    check(any(l.startswith("flat reload: start off hand") for l in L), "dual: the off hand's shotgun reloads at the hip too")

    # prlfix: the gun's path over a whole reload (its model's origin from the camera, each frame): no step, no back-and-forth
    # spike. Before the fix the stance went off and on with the pair in the hand and the reloading arm pushed the gun: 4 units in
    # a frame (forward and back) every trip, 0.5-0.8 unit dips mid-carry; after it, 0.09 a frame at most with the gun held tilted
    # and 0.17 the most its per-frame move changes (72 fps).
    TRACE = re.compile(r"^fprl_trace: ([\d.]+) (\w+) ([\d.-]+) ([\d.]+) h (\S+) (\S+) (\S+) r (\S+) (\S+) (\S+) g (\S+) (\S+) (\S+)")
    for run, rate in (("trips", 72), ("cont144", 144)):
        rows = [m for m in map(TRACE.match, lines(run)) if m]
        gun = [tuple(float(m.group(i)) for i in (11, 12, 13)) for m in rows]
        tilt = [float(m.group(4)) for m in rows]
        dist = lambda a, b: sum((x - y) ** 2 for x, y in zip(a, b)) ** 0.5
        steps = [dist(gun[i], gun[i - 1]) for i in range(1, len(gun))]
        held = [steps[i - 1] for i in range(1, len(gun)) if tilt[i] >= 1.0 and tilt[i - 1] >= 1.0]
        acc = [dist([a - 2 * b + c for a, b, c in zip(gun[i + 1], gun[i], gun[i - 1])], (0, 0, 0)) for i in range(1, len(gun) - 1)]
        k = 72.0 / rate
        check(not timed_out(run) and len(rows) > 60, "%s: the gun traced over the reload (%d frames)" % (run, len(rows)))
        check(bool(held) and max(held) <= 0.25 * k, "%s: the gun held tilted moves at most %.2f a frame (%.2f)" %
              (run, 0.25 * k, max(held) if held else -1))
        check(bool(acc) and max(acc) <= 0.4 * k, "%s: no spike: its per-frame move changes at most %.2f (%.2f)" %
              (run, 0.4 * k, max(acc) if acc else -1))
        check(bool(steps) and max(steps) <= 1.0 * k, "%s: no step anywhere (the tilt in and out included): at most %.2f a frame (%.2f)" %
              (run, 1.0 * k, max(steps) if steps else -1))

    # lock: each of the shotgun's keys held (1 reach: the pouch, 2 grip: a pair, 3 carry: the pair at the port's mouth, 4 insert:
    # in the port, 5 reach again, 6 return: rest, the gun untilted), as its pose; an edit live; next / previous; the scrub; let go
    # (and never a server reload).
    LOCK = re.compile(r"^flatprocreload: lock (on|off) key (\d+) \((.*?)\) ([\d.]+); free hand \w+ from the gun hand "
                      r"(\S+) (\S+) (\S+) turned (\S+) (\S+) (\S+); gun (\S+) (\S+) (\S+) pair (\d) busy (\d) round gap (\S+)")
    L = lines("lock")
    reps = []
    label = None
    for l in L:
        if l.startswith("fprl_test: "):
            label = l[len("fprl_test: "):].strip()
        m = LOCK.match(l)
        if m:
            reps.append((label, m))
            label = None
    bykey = {}
    for lab, m in reps:
        if lab is None and m.group(1) == "on":
            bykey.setdefault(int(m.group(2)), []).append(m)
    rel = lambda m: tuple(float(m.group(i)) for i in (5, 6, 7))
    gun = lambda m: tuple(float(m.group(i)) for i in (11, 12, 13))
    KEYS = range(1, 7)
    allk = all(len(bykey.get(k, [])) >= 2 for k in KEYS)
    check(not timed_out("lock") and allk, "lock: every key 1-6 locked and reported (%s)" % sorted((k, len(v)) for k, v in bykey.items()))
    if allk:
        still = max(dist(rel(bykey[k][0]), rel(bykey[k][1])) for k in KEYS)
        check(still < 0.05, "lock: each key holds its pose (the most a pose moved between two reports %.3f)" % still)
        guns = [gun(bykey[k][1]) for k in KEYS]
        check(all(dist(g, (-20, 60, 69)) < 0.1 for g in guns[:5]) and dist(guns[5], (0, 0, 0)) < 0.1,
              "lock: the gun's tilt by key (tilted 1-5, untilted at the return: %s)" % guns)
        pairs = [int(bykey[k][1].group(14)) for k in KEYS]
        check(pairs == [0, 1, 1, 1, 0, 0], "lock: a pair in the hand from the grip to the insert (%s)" % pairs)
        check(all(int(bykey[k][1].group(15)) == 1 for k in KEYS), "lock: the free hand is busy (off the grip, no pushes) while locked")
        away = dist(rel(bykey[1][1]), rel(bykey[6][1]))
        check(away > 4.0 and dist(rel(bykey[1][1]), rel(bykey[2][1])) < 0.05,
              "lock: the hand at the pouch is away from its rest (%.1f) and the grip is the same place (reach and grip)" % away)
        gap3, gap4 = float(bykey[3][1].group(16)), float(bykey[4][1].group(16))
        check(0.2 < gap3 < 1.0 and gap4 < 0.1, "lock: the pair at the port's mouth %.2f out (approach 0.5) and in the port %.2f "
              "(the hand solved backwards from the pair's place)" % (gap3, gap4))
        tuned = [m for lab, m in reps if lab == "tuned"]
        before = [m for lab, m in reps if lab == "before tune"]
        moved = dist(rel(tuned[0]), rel(before[0])) if tuned and before else -1
        check(bool(tuned) and int(tuned[0].group(2)) == 1 and 3.5 < moved < 4.5,
              "lock: key 1's hand offset up 1 -> 5 moved the held pose live (%.2f units)" % moved)
    nx = [m for lab, m in reps if lab == "next"]
    pv = [m for lab, m in reps if lab == "prev"]
    sc = [m for lab, m in reps if lab == "scrub"]
    check(bool(nx) and int(nx[0].group(2)) == 1 and bool(pv) and int(pv[0].group(2)) == 6,
          "lock: next from the last key is the first, previous from the first is the last (%s, %s)" %
          (nx[0].group(2) if nx else "-", pv[0].group(2) if pv else "-"))
    check(bool(sc) and int(sc[0].group(2)) == 3 and 0.3 < float(sc[0].group(4)) < 0.9 and int(sc[0].group(14)) == 1,
          "lock: scrub 0.4 is mid-carry with the pair (%s)" % (sc[0].group(0)[16:80] if sc else "none"))
    # prlmove: the free hand's fingers grip the pair (key 2) as a headset's hand does, open at the pouch without one (key 1); the
    # hand-at-port offsets (forward 1, up 2 on key 4) move the locked hand at the port by their length in the gun's frame.
    GRIP = re.compile(r"grip ([\d.]+) fingers (\S+)")
    gm = lambda m: GRIP.search(m.string)
    if all(len(bykey.get(k, [])) >= 2 for k in (1, 2)) and gm(bykey[1][1]) and gm(bykey[2][1]):
        g1, f1 = float(gm(bykey[1][1]).group(1)), float(gm(bykey[1][1]).group(2))
        g2, f2 = float(gm(bykey[2][1]).group(1)), float(gm(bykey[2][1]).group(2))
        check(g1 == 0.0 and g2 == 1.0 and f2 > f1 + 0.2, "lock: the fingers grip the pair (key 2: grip %.2f, fingers %.2f) and not at the "
              "pouch without it (key 1: grip %.2f, fingers %.2f)" % (g2, f2, g1, f1))
    else:
        check(False, "lock: the fingers' grip reported at keys 1 and 2")
    pb = [m for lab, m in reps if lab == "port before"]
    pn = [m for lab, m in reps if lab == "port nudged"]
    moved = dist(rel(pn[0]), rel(pb[0])) if pb and pn else -1
    check(bool(pb) and bool(pn) and int(pn[0].group(2)) == 4 and 2.0 < moved < 2.5,
          "lock: key 4's offset forward 1, up 2 moved the hand at the port %.2f units (2.24)" % moved)
    check(any(l.startswith("flatprocreload: none -> return (the lock let go") for l in L) and
          any(l.startswith("flatprocreload: the off hand back in its pose") for l in L), "lock: let go, the hand back in its pose")
    check(not any(l.startswith("flat proc reload: start") for l in L) and not any(l.startswith("flatprocreload: impulse 99") for l in L),
          "lock: no reload ran, nothing taken from the pouch")

    # prlmove: full reloads on the move. Before: the pair, swept from where it was a frame ago, met the body and the gun's hull
    # and the hand let go of it (a pair lost a trip), and the hand aimed it from the drawn (swaying) gun and the interpolated
    # pair, 2-4 units beside the port the server tests: every run cancelled by the failsafe, nothing loaded.
    WENT = re.compile(r"^flatprocreload: the round went in .*fingers (\S+), grip ([\d.]+)")
    for run in ("mrun", "mjump", "mslide", "mcrouch", "mrun60", "mrun144"):
        L = lines(run)
        st = [m for m in map(START.match, L) if m]
        ins = [int(m.group(2)) for m in map(IN.match, L) if m]
        end = [m for m in map(END.match, L) if m]
        lost = [l for l in L if l.startswith("flatprocreload: the round left the hand")]
        fs = [l for l in L if l.startswith("flatprocreload: cancelled (failsafe") or
              (l.startswith("flat proc reload: main hand cancelled") and "failsafe" in l)]
        full = bool(end) and end[-1].group(2) == "done" and int(end[-1].group(6)) == int(end[-1].group(7))
        check(not timed_out(run) and bool(st) and full and not lost and not fs and ins and all(n == 2 for n in ins),
              "%s: a full reload on the move, every pair seated (%s), none dropped (%d), no failsafe (%d), %s" %
              (run, ins, len(lost), len(fs), end[-1].group(0)[18:70] if end else "no end"))
        fingers = [float(m.group(1)) for m in map(WENT.match, L) if m]
        check(bool(fingers) and min(fingers) >= 0.6, "%s: the fingers closed on each pair as it went in (%s)" %
              (run, ["%.2f" % f for f in fingers]))


    # reloadsys: the super shotgun. ssg: fired empty (broken open by itself), a pair into its chambers, closed by the end's flick;
    # ssgshut: R before it broke open by itself: broken open by the reload's flick (impulse 107), the pair in, closed. Nothing lost.
    OPEN = re.compile(r"^flat proc reload: main hand: the super shotgun broken open: clip (\d+) of (\d+)")
    CLOSED = re.compile(r"^flat proc reload: main hand: the super shotgun closed \((.*)\): clip (\d+)")
    for run, n in (("ssg", 2), ("ssgshut", 2)):
        L = lines(run)
        st = [m for m in map(START.match, L) if m]
        ins = [int(m.group(2)) for m in map(IN.match, L) if m]
        end = [m for m in map(END.match, L) if m]
        op = [m for m in map(OPEN.match, L) if m]
        cl = [m for m in map(CLOSED.match, L) if m]
        full = bool(end) and end[-1].group(2) == "done" and int(end[-1].group(6)) == 2
        check(not timed_out(run) and bool(st) and int(st[0].group(3)) == 5 and full and sum(ins) == n,
              "%s: the super shotgun's procedural reload: %d in (%s), full (%s)" % (run, n, ins, end[-1].group(0)[18:70] if end else "no end"))
        check(bool(cl) and "flick" in cl[0].group(1) and int(cl[0].group(2)) == 2, "%s: closed by the end's flick, 2 in (%s)" %
              (run, cl[0].group(0)[35:] if cl else "not closed"))
        if run == "ssgshut":
            check(bool(op) and int(op[0].group(1)) == 0, "ssgshut: broken open by the reload's flick (%s)" %
                  (op[0].group(0)[35:] if op else "not opened"))
        if st and end:
            b = int(st[0].group(4)) + int(st[0].group(6))
            a = int(end[-1].group(6)) + int(end[-1].group(8))
            check(a == b + (0 if run == "ssg" else 0), "%s: no shell lost (clip + storage %d before, %d after)" % (run, b, a))

    # reloadmags: the nailgun and the super nailgun fired empty (dropped by themselves): no eject, a magazine seated, full.
    for run in ("magempty_ng", "magempty_sng"):
        L = lines(run)
        end = [m for m in map(END.match, L) if m]
        check(not timed_out(run) and bool(end) and end[-1].group(2) == "done" and int(end[-1].group(6)) == int(end[-1].group(7)),
              "%s: done, the gun full (%s)" % (run, end[-1].group(0)[18:80] if end else "no end"))
        mag_checks(run, False)
    # magfire_ng: a shot while the magazine is carried: cancelled, the magazine back in the pouch; the partly used one's rounds
    # back in the reserve as it was let out: nothing lost (the gun left empty, no magazine in).
    L = lines("magfire_ng")
    st = [m for m in map(START.match, L) if m]
    end = [m for m in map(END.match, L) if m]
    check(bool(end) and end[-1].group(2) == "cancelled" and "fired" in end[-1].group(3), "magfire_ng: cancelled by the shot (%s)" %
          (end[-1].group(0)[18:80] if end else "no end"))
    if st and end:
        b = int(st[0].group(4)) + int(st[0].group(6))
        a = int(end[-1].group(6)) + int(end[-1].group(8))
        check(a == b, "magfire_ng: no round lost (clip + storage %d before, %d after)" % (b, a))


# reloadsys's matrix ("almost always succeed"): both guns, standing, running, jumping, crouch-strafing, sliding, at 60 and 144
# fps: started, every round taken went in (none dropped), no failsafe, the gun full (the super shotgun closed too).
MAGOUT = re.compile(r"^flat proc reload: main hand: the magazine out \((.*)\): (\d+) of its (\d+) back in the reserve")
MAGS = ("ng", "sng", "lg", "np")


# reloadmags: a magazine gun's run: the magazine let out by the eject key with the rounds it had back in the reserve (when
# `ejected`; none otherwise: dropped by itself already), one magazine seated holding the clip's size, nothing lost.
def mag_checks(run, ejected):
    L = lines(run)
    st = [m for m in map(START.match, L) if m]
    end = [m for m in map(END.match, L) if m]
    out = [m for m in map(MAGOUT.match, L) if m]
    takes = [int(m.group(1)) for m in map(TAKE.match, L) if m]
    if ejected:
        check(len(out) == 1 and "eject" in out[0].group(1) and bool(st) and int(out[0].group(3)) == int(st[0].group(4)) ==
              int(out[0].group(2)), "%s: the eject key let the magazine out, its rounds back in the reserve (%s)" %
              (run, out[0].group(0)[18:] if out else "no eject"))
    else:
        check(not out, "%s: no eject key (the empty magazine dropped by itself already)" % run)
    if st and end:
        check(len(takes) == 1 and takes[0] == int(end[-1].group(6)) == int(end[-1].group(7)),
              "%s: one magazine of the clip's size seated (%s taken, clip %s of %s)" % (run, takes, end[-1].group(6), end[-1].group(7)))
        b = int(st[0].group(4)) + int(st[0].group(6))
        a = int(end[-1].group(6)) + int(end[-1].group(8))
        check(a == b, "%s: no round lost (clip + storage %d before, %d after)" % (run, b, a))


def matrix_checks():
    guns = (MODE[7:],) if MODE.startswith("matrix-") else ("sg", "ssg") + MAGS
    for gun in guns:
        for fps in (60, 144):
            for stance in ("stand", "run", "jump", "crouch", "slide"):
                run = "x_%s_%s_%d" % (gun, stance, fps)
                L = lines(run)
                st = [m for m in map(START.match, L) if m]
                ins = [int(m.group(2)) for m in map(IN.match, L) if m]
                takes = [int(m.group(1)) for m in map(TAKE.match, L) if m]
                end = [m for m in map(END.match, L) if m]
                lost = [l for l in L if l.startswith("flatprocreload: the round left the hand")]
                fs = [l for l in L if l.startswith("flatprocreload: cancelled (failsafe") or
                      (l.startswith("flat proc reload: main hand cancelled") and "failsafe" in l)]
                full = bool(end) and end[-1].group(2) == "done" and int(end[-1].group(6)) == int(end[-1].group(7))
                shut = gun != "ssg" or any(l.startswith("flat proc reload: main hand: the super shotgun closed") for l in L)
                check(not timed_out(run) and bool(st) and full and shut and not lost and not fs and sum(ins) == sum(takes) > 0,
                      "%s: full, every round seated (%d taken, %s in), none dropped (%d), no failsafe (%d)%s" %
                      (run, sum(takes), ins, len(lost), len(fs), "" if shut else ", NOT CLOSED"))
                if gun in MAGS:
                    mag_checks(run, True)

if not MODE.startswith("matrix"):
    base_checks()
if MODE != "base":
    matrix_checks()
print(f"flatprocreload: {passed} passed, {failed} failed")
sys.exit(1 if failed else 0)
