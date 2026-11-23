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


THROWN = r"flat throw: (\S+) thrown by the (\w+) hand(?: \(one-handed: the other hand let go\))? after ([\d.]+) s: the hand at ([\d.]+) m/s"
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

ls = lines("twoh")
done(ls, "twoh")
def after(ls, mark):
    return ls[next((i for i, l in enumerate(ls) if l.startswith(mark)), len(ls)):]


def stance_of(ls):
    m = find(ls, r"^  stance: \w+, torso [-\d.]+ deg, free hand (.+?), share ([\d.]+)")
    return (m[0].group(1), float(m[0].group(2))) if m else ("none", -1.0)


hand, share = stance_of(after(ls, "flatthrow_test: before"))
check(hand == "going on it" and share > 0.95, "twoh: before J the free hand is on the nail gun (%s, share %.2f)" % (hand, share))
hand, share = stance_of(after(ls, "flatthrow_test: winding"))
check(hand == "hanging" and share < 0.05, "twoh: winding up, the free hand has let go, not following the gun (%s, share %.2f)" % (hand, share))
t = find(ls, THROWN)
check(bool(t) and t[0].group(1) == "thrown_weapon" and t[0].group(2) == "main", "twoh: the main hand throws it (%s)" % (t[0].group(0) if t else "none"))
check(not find(ls, r"throw: a weapon held in both hands"), "twoh: one-handed: not the headset's two-handed throw")

ls = lines("wall")
done(ls, "wall")
landed = find(ls, r"flat throw: landed: thrown_weapon (\d+) units level from where it left, (-?\d+) down, (.*)")
check(len(find(ls, r"bladespinto: thrown_weapon meets something")) >= 2, "wall: both throws' spins timed (bladespinto)")
check(len(landed) >= 1 and landed[0].group(3) == "stuck" and int(landed[0].group(1)) > 250,
      "wall: the axe from 280 units sticks in the plain wall (%s)" % (landed[0].group(0) if landed else "no landing"))
check(len(landed) >= 2 and landed[1].group(3) == "stuck",
      "wall: the sword from 150 units sticks, point first (%s)" % (landed[1].group(0) if len(landed) > 1 else "no landing"))

print("flatthrow: %d passed, %d failed" % (passed, failed))
sys.exit(1 if failed else 0)
