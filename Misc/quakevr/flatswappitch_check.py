"""flatswappitch_check.py <dir> -- flatswappitch_test.sh's checks. vr_flat_swap_debug's line as the hands meet,
    flatswap: in view P F: off x y z turn p y r
    flatswap: in view P F: main x y z turn p y r
gives each hand's place (fwd, right, up off the head) and turn in the swap's frame: the view's, its pitch held to
vr_flat_pose_pitch as the hands' own poses hold it (F = P within it). A weapon held in both hands: the free hand, at the
empty hand's guard as the weapon is passed to it, must be at the same place and turn in that frame at every pitch the
swap was played at (-60, 0, +60: the view's own frame; +-85, the look's limit: the hands' held frame). The swaps alternate
(there, then back: the other hand holds), so each is compared with the level view's swap of the same way. A weapon in
one hand: the run ends and the hands meet at every pitch. Prints "flatswappitch: N passed, M failed"; exits 1 on a
failure."""
import os
import re
import sys

POS_TOL = 0.1  # units
ROT_TOL = 0.25  # degrees
CASES = [("rl2h", True), ("ads2h", True), ("saw2h", True), ("sg1h", False)]
NPITCH = 5
LINE = re.compile(r"flatswap: in view (\S+) (\S+): off (\S+) (\S+) (\S+) turn (\S+) (\S+) (\S+)\s*\n"
                  r"flatswap: in view \S+ \S+: main (\S+) (\S+) (\S+) turn (\S+) (\S+) (\S+)")

passed = failed = 0


def check(ok, what):
    global passed, failed
    print(f"flatswappitch: {'PASS' if ok else 'FAIL'} {what}")
    if ok:
        passed += 1
    else:
        failed += 1


def wrap(d):
    return (d + 180.0) % 360.0 - 180.0


def main():
    d = sys.argv[1]
    for case, two in CASES:
        path = os.path.join(d, case + ".log")
        text = open(path, encoding="utf-8", errors="replace").read() if os.path.exists(path) else ""
        check(f"flatswappitch_test: {case} done" in text, f"{case}: the run reached its end")
        rows = [[float(x) for x in m.groups()] for m in LINE.finditer(text)]
        check(len(rows) == 2 * NPITCH, f"{case}: {len(rows)} meetings seen (want {2 * NPITCH}: there and back at each pitch)")
        if len(rows) != 2 * NPITCH:
            continue
        pitches = [round(r[0]) for r in rows[0::2]]
        frames = [round(r[1]) for r in rows[0::2]]
        check(pitches[:3] == [-60, 0, 60] and pitches[3] >= 80 and pitches[4] <= -80,
              f"{case}: played at pitch {pitches} (the swap's frame {frames})")
        if not two:
            continue
        for parity, name, free in ((0, "there", 0), (1, "back", 1)):
            group = rows[parity::2]
            col = 2 if free == 0 else 8  # the free hand's place and turn: off's or main's
            ref = group[1][col:col + 6]  # the level view's
            worst_p = worst_r = 0.0
            for r in group:
                v = r[col:col + 6]
                worst_p = max(worst_p, max(abs(v[i] - ref[i]) for i in range(3)))
                worst_r = max(worst_r, max(abs(wrap(v[i] - ref[i])) for i in range(3, 6)))
            hand = "off" if free == 0 else "main"
            check(worst_p <= POS_TOL and worst_r <= ROT_TOL,
                  f"{case} ({name}): the free ({hand}) hand at the same place and turn in the view at every pitch "
                  f"(worst {worst_p:.3f} u, {worst_r:.2f} deg off the level view's {ref[0]:.2f} {ref[1]:.2f} {ref[2]:.2f})")
    print(f"flatswappitch: {passed} passed, {failed} failed")
    return 1 if failed else 0


sys.exit(main())
