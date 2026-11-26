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
            raw = f.read().splitlines()
    except OSError:
        return []
    # Rejoin 120-column console-wrapped continuation lines (e.g. a 2H melee weapon's "(one-handed: ...)" throw line).
    out = []
    for l in raw:
        if out and l and not re.match(r"^(exit=|  |flat ?(throw|grenade|gren)|throw:|button:|force grab:|flatthrow_test|axestick|carry:|bladespinto|box3d:)", l):
            out[-1] = out[-1] + l
        else:
            out.append(l)
    return out


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
landed = find(ls, r"flat throw: landed: thrown_weapon (\d+) units level from where it left, (-?\d+) down, (.*?)(?: at .*)?$")
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
landed = find(ls, r"flat throw: landed: vr_(?:rock|brick|debris_piece) (\d+) units level from where it left, (-?\d+) down, (.*?)(?: at .*)?$")
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
landed = find(ls, r"flat throw: landed: thrown_weapon (\d+) units level from where it left, (-?\d+) down, (.*?)(?: at .*)?$")
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

# (flatthrow2) cancel: the throwing hand's attack button with B held.
def between(ls, start, end=None):
    i = next((k for k, l in enumerate(ls) if start in l), len(ls))
    j = next((k for k, l in enumerate(ls) if end and k > i and end in l), len(ls))
    return ls[i:j]


ls = lines("cancel")
done(ls, "cancel")
WIND6 = r"flat throw: [JB]: the main hand winds up weapon 6.* \(ammo (\d+)\)"
g1 = between(ls, "flatthrow_test: cancel gun", "flatthrow_test: cancel gun again")
g2 = between(ls, "flatthrow_test: cancel gun again", "flatthrow_test: cancel gren")
w1, w2 = find(g1, WIND6), find(g2, WIND6)
c1 = find(g1, r"flat throw: cancelled: the main hand keeps weapon 6 \(ammo (\d+)\)")
check(bool(w1) and bool(c1) and w1[0].group(1) == c1[0].group(1) and not find(g1, THROWN),
      "cancel: B held, LMB: the nail gun's throw cancelled, nothing thrown, no nail (ammo %s at the wind-up, %s cancelled)" %
      (w1[0].group(1) if w1 else "?", c1[0].group(1) if c1 else "?"))
check(bool(find(g1, r"flatthrow: the main hand's attack button \(left\) with \S+ held: throw cancelled")),
      "cancel: the client sent the cancel for the main hand's left button")
t2 = find(g2, THROWN)
check(bool(w1) and bool(w2) and w2[0].group(1) == w1[0].group(1) and bool(t2) and t2[0].group(1) == "thrown_weapon",
      "cancel: LMB held on past B's release shot nothing, the gun still held: B again winds it up (ammo %s) and throws it (%s)" %
      (w2[0].group(1) if w2 else "?", t2[0].group(0) if t2 else "none"))
g3 = between(ls, "flatthrow_test: cancel gren")
check(bool(find(g3, r"flat throw: [JB]: the main hand winds up its grenade")) and
      bool(find(g3, r"flat throw: cancelled: the main hand keeps its grenade \(unarmed\)")),
      "cancel: the quick grenade wound up by B, LMB: cancelled, kept unarmed")
check(not find(g3, r"flat grenade: thrown by") and not find(g3, r"flat grenade: winding up") and bool(find(g3, r"flatgren: phase 2, hand main")),
      "cancel: the grenade neither thrown nor armed by that press, held again (phase 2)")

ls = lines("cancoff")
done(ls, "cancoff")
g = between(ls, "flatthrow_test: cancoff")
check(bool(find(g, r"flat throw: [JB]: the off hand winds up item_health")) and
      bool(find(g, r"flat throw: cancelled: the off hand keeps item_health")),
      "cancoff: the box in the off hand wound up by B, RMB (its button): cancelled")
check(not find(g, r"flat (throw|drop): item_health (thrown|dropped)") and bool(find(g, r"flatthrow: phase 0, .*cancels 1")),
      "cancoff: the box neither thrown nor dropped, one cancel")

ls = lines("guide")
v = find(ls, r"flatitems: (\d+) passed, (\d+) failed")
check(bool(v) and int(v[0].group(2)) == 0 and int(v[0].group(1)) >= 13,
      "guide: vr_flatitems_test 12: the guiding off hand catches nothing while B winds up; no catch without H, caught with a timed H (%s)" %
      (v[0].group(0) if v else "no verdict"))

# (flatthrow3) release: carried on into the follow-through, then let go of at the hand.
ls = lines("release")
done(ls, "release")
LEFT = r"flat throw:   left the hand ([\d.]+) s after the key's release, ([\d.]+) units from it"
FWD = r"flat throw:   left (-?[\d.]+) forward"
r0 = between(ls, "flatthrow_test: release 0", "flatthrow_test: release default")
r1 = between(ls, "flatthrow_test: release default", "flatthrow_test: release gren")
l0, l1, f0, f1 = find(r0, LEFT), find(r1, LEFT), find(r0, FWD), find(r1, FWD)
check(bool(l0) and float(l0[0].group(1)) < 0.005 and bool(find(r0, THROWN)),
      "release: Release Delay 0: the nail gun leaves the hand at B's release (%s)" % (l0[0].group(0) if l0 else "no throw"))
check(bool(l1) and 0.055 <= float(l1[0].group(1)) <= 0.09 and bool(find(r1, r"carried on 0\.07 s into the follow-through")),
      "release: the default 0.07 s: carried on into the follow-through, it leaves that long after B's release (%s)" %
      (l1[0].group(0) if l1 else "no throw"))
check(bool(f0) and bool(f1) and float(f1[0].group(1)) > float(f0[0].group(1)) + 2,
      "release: it leaves from the hand further forward than at once (%s vs %s units ahead of the eye)" %
      (f1[0].group(1) if f1 else "?", f0[0].group(1) if f0 else "?"))
check(bool(l1) and float(l1[0].group(2)) <= 16,
      "release: it starts at the hand (no pop-in: %s units from it)" % (l1[0].group(2) if l1 else "?"))
rg = between(ls, "flatthrow_test: release gren")
lg = find(rg, r"flat grenade: left the hand ([\d.]+) s after the release")
check(bool(lg) and 0.055 <= float(lg[0].group(1)) <= 0.09,
      "release: the quick grenade's fire-button throw: the same delay (%s)" % (lg[0].group(0) if lg else "no throw"))
for l in ls:
    if "flatitems: FAIL" in l:
        print("  " + l)

print("flatthrow: %d passed, %d failed" % (passed, failed))
sys.exit(1 if failed else 0)
