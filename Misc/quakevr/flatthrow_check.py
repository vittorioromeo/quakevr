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


THROWN = r"flat throw: (\S+) thrown by the (\w+) hand(?: \((?:one-handed: the other hand let go|two-handed)\))? after ([\d.]+) s: the hand at ([\d.]+) m/s"
DROPPED = r"flat drop: (\S+) dropped by the (\w+) hand after ([\d.]+) s: the hand at ([\d.]+) m/s, it leaves at (\d+) u/s"
LEAVES = r"it leaves at (\d+) u/s"

ls = lines("axe")
done(ls, "axe")
t = find(ls, THROWN)
check(bool(t) and t[0].group(1) == "thrown_weapon", "axe: J held, let go: the axe is thrown (%s)" % (t[0].group(0) if t else "none"))
check(bool(find(ls, r'button: .*\(target "r12b_open"\) pressed by (player: )?thrown_weapon')), "axe: it presses lesson 12's high target (r12b_open)")
landed = find(ls, r"flat throw: landed: thrown_weapon (\d+) units level from where it left, (-?\d+) down, (.*)")
check(bool(landed) and landed[0].group(3) == "stuck", "axe: it sticks (%s)" % (landed[0].group(0) if landed else "no landing"))
LEFT = r"flat throw:   left (-?[\d.]+) forward, (-?[\d.]+) right, (-?[\d.]+) up of the eye"
AMMO = r"flat throw:   ammo (\d+) at the wind-up, (\d+) now; fire refused (\d+) frames"
lf = find(ls, LEFT)
check(bool(lf) and float(lf[0].group(2)) > 5, "axe: it leaves from the right (main) hand, not the middle of the view (%s)" % (lf[0].group(0) if lf else "none"))

ls = lines("rock")
done(ls, "rock")
d = find(ls, DROPPED)
t = find(ls, THROWN)
v = find(ls, LEAVES)
ROCKS = ("vr_rock", "vr_brick", "vr_debris_piece")
check(len(d) >= 1 and d[0].group(1) in ROCKS and len(t) >= 1 and t[0].group(1) in ROCKS,
      "rock: quick tap drops rock, held charge throws rock (dropped=%s, thrown=%s)" % ([m.group(1) for m in d], [m.group(1) for m in t]))
landed = find(ls, r"flat throw: landed: vr_(?:rock|brick|debris_piece) (\d+) units level from where it left, (-?\d+) down, (.*)")
check(bool(landed) and int(landed[0].group(1)) < 90, "rock: the quick tap drops gently near the player (%s)" % (landed[0].group(0) if landed else "none"))
if len(d) >= 1 and len(t) >= 1 and len(v) >= 2:
    tap, full = float(d[0].group(3)), float(t[0].group(3))
    vt, vf = int(d[0].group(5)), int(v[-1].group(1))
    check(tap < 0.18 and full > 0.9 and float(d[0].group(4)) < float(t[0].group(4)),
          "rock: a tap drop (%.2f s, hand %s m/s) and a charged throw (%.2f s, hand %s m/s)" % (tap, d[0].group(4), full, t[0].group(4)))
    check(vf > vt * 3.0, "rock: the charged throw leaves much faster than the tap drop (%d vs %d u/s)" % (vf, vt))
else:
    check(False, "rock: a tap drop and a charged throw (missing drop/throw)")
    check(False, "rock: the charged throw leaves faster (missing drop/throw)")
check(bool(find(ls, r'button: .*\(target "r10_open"\) pressed by (player: )?vr_(rock|brick|debris_piece)')), "rock: it presses lesson 10's button behind the bars (r10_open: the gate)")
lf = find(ls, LEFT)
side = 1 if len(t) >= 1 and t[0].group(2) == "main" else -1
check(len(lf) >= 1 and float(lf[0].group(2)) * side > 5,
      "rock: the charged rock leaves from its hand's side (the %s hand: %s)" % (t[0].group(2) if len(t) > 0 else "?", lf[0].group(0) if len(lf) > 0 else "none"))

ls = lines("nofire")
done(ls, "nofire")
w = find(ls, r"flat throw: [JB]: the main hand winds up weapon \d+.* \(ammo (\d+)\)")
lf = find(ls, AMMO)
if len(w) >= 2 and len(lf) >= 2:
    check(int(w[1].group(1)) < int(w[0].group(1)), "nofire: the fire button held before B shoots (ammo %s, then %s)" % (w[0].group(1), w[1].group(1)))
    check(lf[1].group(1) == lf[1].group(2), "nofire: held through the wind-up it shoots nothing (ammo %s at the wind-up, %s at the throw)" % (lf[1].group(1), lf[1].group(2)))
else:
    check(False, "nofire: two nail gun throws (%d wind-ups, %d throws)" % (len(w), len(lf)))

ls = lines("gren")
done(ls, "gren")
check(bool(find(ls, r"flat throw: [JB]: the main hand winds up its grenade")), "gren: B winds up the quick grenade")
check(bool(find(ls, r"flat grenade: thrown by the main hand after 0\.[5-9]\d s wound up")), "gren: B's release throws it (as the fire button's)")
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

# (flatfb1124) move: a health box tapped standing, running forward, strafing.
ls = lines("move")
done(ls, "move")
PATH = r"flat throw: path ([-\d.e]+) ([-\d.e]+) ([-\d.e]+) at ([-\d.e]+) ([-\d.e]+) ([-\d.e]+),"
seg = {}
cur = None
for l in ls:
    m = re.search(r"flatthrow_test: move (\w+)", l)
    if m:
        cur = m.group(1)
        seg[cur] = []
    elif cur:
        seg[cur].append(l)
dist = {}
for k in ("stand", "run", "side"):
    s = seg.get(k, [])
    g = find(s, r"box3d: \d+ item_health thrown by 1 at ([\d.]+) m/s .*: (.*)")
    check(bool(g) and g[0].group(2) == "passes through the hands",
          "move %s: the box passes through him as it leaves (%s)" % (k, g[0].group(0) if g else "no box3d throw line"))
    # Clean until it comes down: its level velocity unchanged (within 20 u/s a frame) while it is over the floor (z > 8).
    knock = None
    prev = None
    for m in find(s, PATH):
        z = float(m.group(3))
        v = (float(m.group(4)), float(m.group(5)))
        if prev is not None and z > 8 and ((v[0] - prev[0]) ** 2 + (v[1] - prev[1]) ** 2) ** 0.5 > 20:
            knock = "at z %.1f: %.0f %.0f from %.0f %.0f" % (z, v[0], v[1], prev[0], prev[1])
            break
        prev = v
    check(prev is not None and knock is None, "move %s: it flies clean until it comes down (%s)" % (k, knock or ("clean" if prev else "no path")))
    ld = find(s, r"flat throw: landed: item_health (\d+) units level")
    dist[k] = int(ld[0].group(1)) if ld else -1
    if k != "stand":
        # His motion kept (vr_throw_inherit 0.9): its velocity less the throw's at least 0.8 of his speed.
        w = find(s, r"carry: with your motion \((\d+) u/s\): ([-\d.e]+) ([-\d.e]+) ([-\d.e]+) ")
        t = find(s, r"carry: thrown at '\s*([-\d.]+)\s+([-\d.]+)\s+([-\d.]+)'")
        got = -1.0
        if w and t:
            got = sum((float(w[0].group(i + 2)) - float(t[0].group(i + 1))) ** 2 for i in range(3)) ** 0.5
        ok = bool(w) and got >= 0.8 * float(w[0].group(1))
        check(ok, "move %s: it keeps his motion (%.0f u/s of his %s added)" % (k, got, w[0].group(1) if w else "?"))
check(dist.get("run", -1) > dist.get("stand", -1) and dist.get("stand", -1) > 100,
      "move: thrown running it lands further (%d vs %d units standing)" % (dist.get("run", -1), dist.get("stand", -1)))

print("flatthrow: %d passed, %d failed" % (passed, failed))
sys.exit(1 if failed else 0)
