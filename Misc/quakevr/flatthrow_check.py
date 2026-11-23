"""flatthrow_check.py <dir> -- the checks of flatthrow_test.sh's logs (one per run: axe, rock, gren)."""
import re
import sys

D = sys.argv[1]
passed = failed = 0


def check(ok, what):
    global passed, failed
    print(("PASS " if ok else "FAIL ") + what)
    if ok:
        passed += 1
    else:
        failed += 1


def lines(tag):
    try:
        with open("%s/%s.log" % (D, tag), encoding="utf-8", errors="replace") as f:
            return f.read().splitlines()
    except OSError:
        return []


def find(ls, pat):
    return [m for m in (re.search(pat, l) for l in ls) if m]


def done(ls, tag):
    check(any(("flatthrow_test: %s done" % tag) in l for l in ls), "%s: the run reached its end (no timeout)" % tag)


THROWN = r"flat throw: (\S+) thrown by the (\w+) hand(?: \(both hands\))? after ([\d.]+) s: the hand at ([\d.]+) m/s"
LEAVES = r"it leaves at (\d+) u/s"

ls = lines("axe")
done(ls, "axe")
t = find(ls, THROWN)
check(bool(t) and t[0].group(1) == "thrown_weapon", "axe: J held, let go: the axe is thrown (%s)" % (t[0].group(0) if t else "none"))
check(bool(find(ls, r'button: .*\(target "r12b_open"\) pressed by (player: )?thrown_weapon')), "axe: it presses lesson 12's high target (r12b_open)")
landed = find(ls, r"flat throw: landed: thrown_weapon (\d+) units level from where it left, (-?\d+) down, (.*)")
check(bool(landed) and landed[0].group(3) == "stuck", "axe: it sticks (%s)" % (landed[0].group(0) if landed else "no landing"))

ls = lines("rock")
done(ls, "rock")
t = find(ls, THROWN)
v = find(ls, LEAVES)
ROCKS = ("vr_rock", "vr_brick", "vr_debris_piece")
check(len(t) >= 2 and t[0].group(1) in ROCKS and t[1].group(1) in ROCKS, "rock: two rocks thrown (%s)" % [m.group(1) for m in t])
landed = find(ls, r"flat throw: landed: vr_(?:rock|brick|debris_piece) (\d+) units level from where it left, (-?\d+) down, (.*)")
check(bool(landed) and int(landed[0].group(1)) > 100, "rock: the tap's rock flies and lands (%s)" % (landed[0].group(0) if landed else "none"))
if len(t) >= 2 and len(v) >= 2:
    tap, full = float(t[0].group(3)), float(t[1].group(3))
    vt, vf = int(v[0].group(1)), int(v[1].group(1))
    check(tap < 0.1 and full > 1.0 and float(t[0].group(4)) < float(t[1].group(4)),
          "rock: a tap (%.2f s, hand %s m/s) and a charge (%.2f s, hand %s m/s)" % (tap, t[0].group(4), full, t[1].group(4)))
    check(vf > vt * 1.4, "rock: the charged throw leaves much faster than the tap (%d vs %d u/s)" % (vf, vt))
else:
    check(False, "rock: a tap and a charge (missing throws)")
    check(False, "rock: the charged throw leaves faster (missing throws)")
check(bool(find(ls, r'button: .*\(target "r10_open"\) pressed by (player: )?vr_(rock|brick|debris_piece)')), "rock: it presses lesson 10's button behind the bars (r10_open: the gate)")

ls = lines("gren")
done(ls, "gren")
check(bool(find(ls, r"flat throw: J: the main hand winds up its grenade")), "gren: J winds up the quick grenade")
check(bool(find(ls, r"flat grenade: thrown by the main hand after 0\.[5-9]\d s wound up")), "gren: J's release throws it (as the fire button's)")
check(bool(find(ls, r"flatgren: main hand at overhead")), "gren: the drawn hand reaches the shared wind-up")

print("flatthrow: %d passed, %d failed" % (passed, failed))
sys.exit(1 if failed else 0)
