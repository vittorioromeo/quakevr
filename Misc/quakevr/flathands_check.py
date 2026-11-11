#!/usr/bin/env python3
"""flathands_check.py <dir> -- the checks of flathands_test.sh's logs (flat-screen drop, quick grenade, hand swap).

Prints one line per check and "flathands: N passed, M failed"; exits 1 on a failure."""
import os
import re
import sys

D = sys.argv[1]
passed = failed = 0


def lines(tag):
    p = os.path.join(D, tag + ".log")
    if not os.path.exists(p):
        return []
    return open(p, encoding="utf-8", errors="replace").read().splitlines()


def check(ok, what):
    global passed, failed
    if ok:
        passed += 1
    else:
        failed += 1
    print(("PASS " if ok else "FAIL ") + what)


def hud(ls):
    """[(left weapon, right weapon)] of each vr_flathud_report."""
    out, left = [], None
    for l in ls:
        m = re.match(r"flathud: (left|right) hand: weapon (-?\d+)", l)
        if not m:
            continue
        if m.group(1) == "left":
            left = int(m.group(2))
        elif left is not None:
            out.append((left, int(m.group(2))))
            left = None
    return out


def has(ls, pat):
    return any(re.search(pat, l) for l in ls)


def done(tag, ls):
    check(has(ls, r"^flathands_test: %s done" % tag), "%s: the run reached its end" % tag)


# drop: G with no rockets drops nothing; H drops the 2H nail gun, the off hand stays empty.
ls = lines("drop")
done("drop", ls)
h = hud(ls)
stance = [l for l in ls if l.startswith("  stance:")]
check(len(stance) >= 1 and ("going on it" in stance[0] or "holding its grip" in stance[0]), "drop: the free hand on the rocket launcher's grip first (%s)" %
      (stance[0].split(",")[2].strip() if stance and len(stance[0].split(",")) > 2 else "?"))
check(has(ls, r"No rockets for a grenade"), "drop: G with no rockets: no grenade, and says so")
check(len(h) >= 3 and h[0][1] == h[1][1] != 0, "drop: G is no longer the drop: the gun still in the main hand after it (%s)" % (h[:2],))
check(has(ls, r"flat drop: weapon \d+ out of hand 1") and not has(ls, r"handed off"),
      "drop: H drops the gun out of the main hand, never handed off to the grip hand")
check(len(h) >= 3 and h[2] == (0, 0), "drop: both hands empty after H (%s)" % ((h[2] if len(h) >= 3 else "?"),))

# gren2h
ls = lines("gren2h")
done("gren2h", ls)
check(has(ls, r"flat grenade: G: the off hand goes for the pouch \(off the weapon.s grip\)"),
      "gren2h: G takes the off hand off the rocket launcher's grip to the pouch")
m = [re.search(r"has it \(([\d.]+) s\), (\d+) rockets left", l) for l in ls]
m = [x for x in m if x]
check(bool(m) and m[0].group(2) == "9" and abs(float(m[0].group(1)) - 0.3) < 0.03,
      "gren2h: the grenade in the hand as it reaches the pouch (0.3 s), a rocket gone (%s)" %
      (str((m[0].group(1), m[0].group(2))) if m else "none"))
places = {}
for l in ls:
    p = re.match(r"flatgren: off hand at (the pouch|held|overhead|the follow-through): ([-\d.]+) forward, ([-\d.]+) right, ([-\d.]+) up", l)
    if p:
        places.setdefault(p.group(1), tuple(float(x) for x in p.group(2, 3, 4)))
pouch, held, over = places.get("the pouch"), places.get("held"), places.get("overhead")
check(pouch is not None and pouch[0] < 0 and pouch[2] < -15,
      "gren2h: the drawn hand at the pouch: behind the eye and well below it, out of view (%s)" % (pouch,))
check(held is not None and held[0] > 8 and held[1] < 0,
      "gren2h: back in view with the grenade: ahead of the eye, at the left (%s)" % (held,))
check(over is not None and over[2] > 0 and (held is None or over[2] > held[2] + 5),
      "gren2h: wound up: the hand raised above the eye, above where it held it (%s)" % (over,))
check(has(ls, r"grenade: hand grenade armed \(flat: the fire button\)"), "gren2h: RMB arms it (the pin, its fuse)")
check(has(ls, r"flat grenade: winding up"), "gren2h: RMB again winds up (two presses)")
t = [re.search(r"thrown by the off hand after ([\d.]+) s wound up: (\d+) u/s", l) for l in ls]
t = [x for x in t if x]
if t:
    held_s, speed = float(t[0].group(1)), float(t[0].group(2))
    want = 350 + 550 * min(1.0, held_s / 1.0)
check(bool(t) and abs(held_s - 0.69) < 0.05 and abs(speed - want) < 40,
      "gren2h: let go, thrown as fast as it was wound up (%s)" % (str((t[0].group(1), t[0].group(2))) if t else "none"))
stance = [l for l in ls if l.startswith("  stance:")]
check(len(stance) >= 2 and all(("going on it" in s or "holding its grip" in s) for s in (stance[0], stance[-1])),
      "gren2h: the off hand back on the gun's grip after the throw (%s)" % " | ".join(
          s.split(",")[2].strip() for s in stance if len(s.split(",")) > 2))
check(has(ls, r"flatgren: phase 5 -> 0"), "gren2h: the follow-through ends")

# grenempty
ls = lines("grenempty")
done("grenempty", ls)
check(has(ls, r"flat grenade: G: the main hand goes for the pouch$"), "grenempty: both hands empty: the main hand goes")
check(has(ls, r"flat grenade: .*over \(put back in the pouch\)"), "grenempty: G again puts the unarmed grenade back")
t = [re.search(r"thrown by the main hand after ([\d.]+) s wound up: (\d+) u/s", l) for l in ls]
t = [x for x in t if x]
check(bool(t) and abs(float(t[0].group(1)) - 0.42) < 0.05,
      "grenempty: one press: held, it arms and winds up; let go, it is thrown (%s)" % (str((t[0].group(1), t[0].group(2))) if t else "none"))

# grendual
ls = lines("grendual")
done("grendual", ls)
check(has(ls, r"flat grenade: G: no free hand") and not has(ls, r"goes for the pouch"), "grendual: dual wielding: G does nothing")

# grenoff, grenmain: one hand empty (either side): G takes it, two presses arm and wind up, let go throws.
for tag, hand, other in (("grenoff", "off", "main"), ("grenmain", "main", "off")):
    ls = lines(tag)
    done(tag, ls)
    check(has(ls, r"flat grenade: G: the %s hand goes for the pouch$" % hand), "%s: the %s hand empty: it goes" % (tag, hand))
    check(has(ls, r"thrown by the %s hand after" % hand), "%s: armed, wound up and thrown by the %s hand" % (tag, hand))

# The hand slot (every grenade run, vr_debug_carry 2): the grenade a round's kind (4) unarmed, a grenade's (5) once armed,
# the hand changing with it at once; no mismatch, clash or stale line anywhere; the hand empty once it is thrown.
for tag, hand in (("gren2h", "off"), ("grenempty", "main"), ("grenoff", "off"), ("grenmain", "main")):
    ls = lines(tag)
    check(has(ls, r"^handslot: %s hand takes vr_ammo_front \(kind 4\)" % hand), "%s: the unarmed grenade in the %s hand: a round (kind 4)" % (tag, hand))
    check(has(ls, r"^handslot: %s hand's grenade now kind 5 \(was 4\)" % hand), "%s: armed: the %s hand's kind a grenade's (5) at once" % (tag, hand))
    check(has(ls, r"^handslot: %s hand lets go of grenade \(throw\)" % hand), "%s: thrown: the %s hand lets go of it" % (tag, hand))
    end = [l for l in ls if l.startswith("flat grenade: done;")]
    check(bool(end) and end[-1] == "flat grenade: done; the %s hand holds nothing (kind 0)" % hand,
          "%s: after the throw the %s hand is empty, kind 0 (%s)" % (tag, hand, end[-1] if end else "none"))
check(has(lines("grenempty"), r"^handslot: main hand lets go of vr_ammo_front \(stow\)"), "grenempty: put back: the main hand lets go of it")
bad = [(tag, l) for tag in ("drop", "gren2h", "grenempty", "grendual", "grenoff", "grenmain", "swap1", "swap2h", "swapdual", "swapoff")
       for l in lines(tag) if re.match(r"handslot: (mismatch|clash|stale)", l)]
check(not bad, "no handslot mismatch/clash/stale line in any run (%s)" % (bad[:2],))

# swaps
for tag, two in (("swap1", False), ("swap2h", True), ("swapdual", False)):
    ls = lines(tag)
    done(tag, ls)
    h = hud(ls)
    check(len(h) >= 2 and h[1] == (h[0][1], h[0][0]) and h[0] != (0, 0), "%s: swapped (%s -> %s)" % (tag, h[0] if h else "?",
                                                                                                    h[1] if len(h) > 1 else "?"))
    kind = "a weapon in both hands: release, pass, attach" if two else "the hands to each other"
    check(has(ls, r"flatswap: start \(%s" % re.escape(kind)), "%s: the swap drawn: %s" % (tag, kind))
    if two:
        # The release, then the pass, then the attach (his words 2026-11-10): the free hand at the empty hand's guard
        # before the gun comes across, the swap there, the other hand on the grip at the end.
        idx = lambda pat: next((i for i, l in enumerate(ls) if re.search(pat, l)), -1)
        rel = [re.search(r"2h: released \(([\d.]+) s\): the off hand ([\d.]+) units from the empty hand's guard, ([\d.]+) from the main hand \(moved ([\d.]+)", l) for l in ls]
        rel = [x for x in rel if x]
        check(bool(rel) and abs(float(rel[0].group(1)) - 0.2) < 0.03 and float(rel[0].group(2)) < 1 and float(rel[0].group(3)) > 6,
              "swap2h: first the off hand off the grip to the empty hand's guard (%s)" % (rel[0].group(0) if rel else "none",))
        i_rel, i_met, i_sent, i_seen, i_done = (idx(p) for p in (r"2h: released", r"flatswap: met: .*\(the pass\)", r"swap sent",
                                                                 r"flatswap: swapped", r"flatswap: done"))
        check(-1 < i_rel < i_met and -1 < i_rel < i_sent < i_seen < i_done,
              "swap2h: the phases in order: released, passed (met), sent, swapped, done (%s)" % ((i_rel, i_met, i_sent, i_seen, i_done),))
        met = [re.search(r"met: the hands ([\d.]+) units apart \(the pass\), the off hand ([\d.]+) from the guard", l) for l in ls]
        met = [x for x in met if x]
        check(bool(met) and float(met[0].group(1)) < 6 and float(met[0].group(2)) < 1,
              "swap2h: the gun passed to the hand at the guard (%s)" % (met[0].group(0) if met else "none",))
        sent = [l for l in ls if "swap sent" in l]
        want = 0.45
        stance = [l for l in ls if l.startswith("  stance:")]
        check(len(stance) >= 2 and "holding its grip" in stance[-1] and "torso 15.0" in stance[-1],
              "swap2h: ends with the main hand holding the gun's grip, the gun in the off hand (%s)" % (stance[-1] if stance else "none",))
    else:
        met = [re.search(r"met: the hands ([\d.]+) units apart \(were ([\d.]+)\)", l) for l in ls]
        met = [x for x in met if x]
        check(bool(met) and float(met[0].group(1)) < 6 and float(met[0].group(2)) > float(met[0].group(1)) + 2,
              "%s: the hands meet (%s units apart, were %s)" % (tag, met[0].group(1) if met else "?", met[0].group(2) if met else "?"))
        sent = [l for l in ls if "swap sent" in l]
        want = 0.15
    check(bool(sent) and abs(float(re.search(r"\(([\d.]+) s\)", sent[0]).group(1)) - want) < 0.03,
          "%s: the swap sent as they meet (%s)" % (tag, sent[0] if sent else "none"))
    check(has(ls, r"flatswap: done"), "%s: the hands back, the animation over" % tag)
ls = lines("swapoff")
done("swapoff", ls)
h = hud(ls)
check(len(h) >= 2 and h[1] == (h[0][1], h[0][0]) and not has(ls, r"flatswap: start"), "swapoff: no animation, the swap at once (%s)" % (h[:2],))

print("flathands: %d passed, %d failed" % (passed, failed))
sys.exit(1 if failed else 0)
