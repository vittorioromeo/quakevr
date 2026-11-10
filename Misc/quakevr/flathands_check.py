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
check(len(stance) >= 1 and "going on it" in stance[0], "drop: the free hand on the nail gun's grip first (%s)" %
      (stance[0].split(",")[2].strip() if stance and len(stance[0].split(",")) > 2 else "?"))
check(has(ls, r"No rockets for a grenade"), "drop: G with no rockets: no grenade, and says so")
check(len(h) >= 3 and h[0][1] == h[1][1] != 0, "drop: G is no longer the drop: the gun still in the main hand after it (%s)" % h[:2])
check(has(ls, r"flat drop: weapon \d+ out of hand 1") and not has(ls, r"handed off"),
      "drop: H drops the gun out of the main hand, never handed off to the grip hand")
check(len(h) >= 3 and h[2] == (0, 0), "drop: both hands empty after H (%s)" % (h[2] if len(h) >= 3 else "?"))

# gren2h
ls = lines("gren2h")
done("gren2h", ls)
check(has(ls, r"flat grenade: G: the off hand goes for the pouch \(off the weapon's grip\)"),
      "gren2h: G takes the off hand off the nail gun's grip")
m = [re.search(r"has it \(([\d.]+) s\), (\d+) rockets left", l) for l in ls]
m = [x for x in m if x]
check(bool(m) and m[0].group(2) == "9" and abs(float(m[0].group(1)) - 0.3) < 0.03,
      "gren2h: the grenade in the hand as it reaches the pouch (0.3 s), a rocket gone (%s)" %
      ((m[0].group(1), m[0].group(2)) if m else "none"))
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
check(over is not None and over[2] > -4 and (held is None or over[2] > held[2] + 8),
      "gren2h: wound up: the hand up overhead, above where it held it (%s)" % (over,))
check(has(ls, r"grenade: hand grenade armed \(flat: the fire button\)"), "gren2h: RMB arms it (the pin, its fuse)")
check(has(ls, r"flat grenade: winding up"), "gren2h: RMB again winds up (two presses)")
t = [re.search(r"thrown by the off hand after ([\d.]+) s wound up: (\d+) u/s", l) for l in ls]
t = [x for x in t if x]
if t:
    held_s, speed = float(t[0].group(1)), float(t[0].group(2))
    want = 350 + 550 * min(1.0, held_s / 1.0)
check(bool(t) and abs(held_s - 0.69) < 0.05 and abs(speed - want) < 40,
      "gren2h: let go, thrown as fast as it was wound up (%s)" % ((t[0].group(1), t[0].group(2)) if t else "none"))
stance = [l for l in ls if l.startswith("  stance:")]
check(len(stance) >= 2 and "going on it" in stance[0] and "going on it" in stance[-1],
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
      "grenempty: one press: held, it arms and winds up; let go, it is thrown (%s)" % ((t[0].group(1), t[0].group(2)) if t else "none"))

# grendual
ls = lines("grendual")
done("grendual", ls)
check(has(ls, r"flat grenade: G: no free hand") and not has(ls, r"goes for the pouch"), "grendual: dual wielding: G does nothing")

# swaps
for tag, two in (("swap1", False), ("swap2h", True), ("swapdual", False)):
    ls = lines(tag)
    done(tag, ls)
    h = hud(ls)
    check(len(h) >= 2 and h[1] == (h[0][1], h[0][0]) and h[0] != (0, 0), "%s: swapped (%s -> %s)" % (tag, h[0] if h else "?",
                                                                                                    h[1] if len(h) > 1 else "?"))
    kind = "a weapon in both hands: the free hand to it" if two else "the hands to each other"
    check(has(ls, r"flatswap: start \(%s" % re.escape(kind)), "%s: the swap drawn: %s" % (tag, kind))
    met = [re.search(r"met: the hands ([\d.]+) units apart \(were ([\d.]+)\)", l) for l in ls]
    met = [x for x in met if x]
    check(bool(met) and float(met[0].group(1)) < 6 and float(met[0].group(2)) > float(met[0].group(1)) + 2,
          "%s: the hands meet (%s units apart, were %s)" % (tag, met[0].group(1) if met else "?", met[0].group(2) if met else "?"))
    sent = [l for l in ls if "swap sent" in l]
    check(bool(sent) and abs(float(re.search(r"\(([\d.]+) s\)", sent[0]).group(1)) - 0.15) < 0.03,
          "%s: the swap sent as they meet (%s)" % (tag, sent[0] if sent else "none"))
    check(has(ls, r"flatswap: done"), "%s: the hands back, the animation over" % tag)
ls = lines("swapoff")
done("swapoff", ls)
h = hud(ls)
check(len(h) >= 2 and h[1] == (h[0][1], h[0][0]) and not has(ls, r"flatswap: start"), "swapoff: no animation, the swap at once (%s)" % h[:2])

print("flathands: %d passed, %d failed" % (passed, failed))
sys.exit(1 if failed else 0)
