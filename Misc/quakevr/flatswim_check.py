"""flatswim_check.py <dir> -- flatswim_test.sh's checks: prints each and "flatswim: N passed, M failed"."""
import os
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


def cases(tag):
    """The run's lines by case: {case: [lines]}."""
    path = os.path.join(D, tag + ".log")
    out, cur = {}, None
    if not os.path.exists(path):
        return out
    with open(path, encoding="utf-8", errors="replace") as f:
        text = f.read()
    # (the console's long lines come wrapped: join a line that does not start a new message)
    lines = []
    for l in text.splitlines():
        if lines and not re.match(r"^(flatswim|  stance)", l):
            lines[-1] += l
        else:
            lines.append(l)
    for l in lines:
        m = re.match(r"^flatswim_test: case (\w+)", l)
        if m:
            cur = m.group(1)
            out[cur] = []
        elif cur:
            out[cur].append(l)
    return out


NUM = r"([-+]?\d+\.\d+)"
HAND = re.compile(r"^flatswim: (off|main) hand (\w+) share " + NUM + " take " + NUM + r" helping (\d) stance " + NUM +
                  r"; probe (\d+) frames: ahead " + NUM + r"\.\." + NUM + " out " + NUM + r"\.\." + NUM + " up " + NUM +
                  r"\.\." + NUM + r" palm \(" + NUM + " " + NUM + " " + NUM + r"\) mean share " + NUM)
STATE = re.compile(r"^flatswim: swimming (\d) in " + NUM + r" phase " + NUM + r" strokes (\d+) speed (\d+) move " + NUM +
                   " kinds fwd " + NUM + " back " + NUM + " side " + NUM + " up " + NUM + " down " + NUM)


def parse(lines):
    st, hands = None, {}
    for l in lines:
        m = STATE.match(l)
        if m:
            g = m.groups()
            st = dict(swimming=int(g[0]), inn=float(g[1]), speed=int(g[4]), move=float(g[5]), fwd=float(g[6]),
                      back=float(g[7]), side=float(g[8]), up=float(g[9]), down=float(g[10]))
        m = HAND.match(l)
        if m:
            g = m.groups()
            v = [float(x) for x in g[2:4]] + [int(g[4]), float(g[5]), int(g[6])] + [float(x) for x in g[7:]]
            hands[g[0]] = dict(holds=g[1] == "holds", share=v[0], take=v[1], helping=v[2], stance=v[3], frames=v[4],
                               ahead=(v[5], v[6]), out=(v[7], v[8]), up=(v[9], v[10]), palm=(v[11], v[12], v[13]),
                               mean=v[14])
    return st, hands


dirs = cases("dirs")
EXPECT = {  # case: (the kind that leads, its sign for side; the palm's axis and sign the stroke shows on average)
    "still": None,
    "forward": ("fwd", 0, -1),
    "back": ("back", 0, +1),
    "right": ("side", 1, None),
    "left": ("side", -1, None),
    "up": ("up", 2, -1),
    "down": ("down", 2, +1),
}
for case, want in EXPECT.items():
    st, hands = parse(dirs.get(case, []))
    if st is None or "off" not in hands or "main" not in hands:
        check(False, f"dirs {case}: the report was printed")
        continue
    off, main = hands["off"], hands["main"]
    span = lambda r: r[1] - r[0]
    swing = max(span(off["ahead"]), span(off["out"]), span(off["up"]))
    check(st["swimming"] == 1 and off["mean"] > 0.95 and main["mean"] > 0.95 and off["frames"] >= 40,
          f"dirs {case}: swimming, both free hands in the stroke (mean share off {off['mean']:.2f} main {main['mean']:.2f}, "
          f"{off['frames']} frames, speed {st['speed']})")
    if want is None:
        kinds = max(st["fwd"], st["back"], abs(st["side"]), st["up"], st["down"])
        check(st["move"] < 0.1 and 0.05 < swing < 0.35 and off["palm"][2] < -0.7,
              f"dirs still: treading (move {st['move']:.2f}, kinds {kinds:.2f}), small sweeps ({swing:.2f} of the reach), palms "
              f"down ({off['palm'][2]:+.2f})")
        continue
    kind, axis, sign = want
    lead = st[kind] if kind != "side" else st["side"] * axis
    others = [abs(st[k]) for k in ("fwd", "back", "side", "up", "down") if k != kind]
    check(lead > 0.6 and lead > 2 * max(others) and st["move"] > 0.3,
          f"dirs {case}: the {kind} stroke leads ({lead:+.2f}, others at most {max(others):.2f}; move {st['move']:.2f})")
    least = 0.12 if kind in ("up", "down") else 0.25  # (up and down: part of a stroke, the pool is 120 units deep)
    check(swing > least, f"dirs {case}: the hand sweeps ({swing:.2f} of the reach: ahead {span(off['ahead']):.2f} out "
          f"{span(off['out']):.2f} up {span(off['up']):.2f})")
    # (the palm: back pulling forward, ahead pushing back, down rising, up sinking; sideways: toward where he does not go,
    # in the body's right that is the out axis times the hand's side: -1 the off hand)
    if kind == "side":
        pr_off = off["palm"][1] * -1  # the off hand's out is the body's left
        pr_main = main["palm"][1]
        check(pr_off * axis < -0.15 and pr_main * axis < -0.15,
              f"dirs {case}: palms toward the side he leaves (body-right: off {pr_off:+.2f} main {pr_main:+.2f})")
    else:
        i = {"fwd": 0, "back": 0, "up": 2, "down": 2}[kind]
        p = (off["palm"][i], main["palm"][i])
        check(p[0] * sign > 0.15 and p[1] * sign > 0.15,
              f"dirs {case}: palms {'back' if kind == 'fwd' else 'ahead' if kind == 'back' else 'down' if kind == 'up' else 'up'} "
              f"on the stroke's average (off {p[0]:+.2f} main {p[1]:+.2f})")
    # mirrored: the two hands' out ranges the same (each out to its own side)
    if kind != "side":
        check(abs(off["out"][0] - main["out"][0]) < 0.03 and abs(off["out"][1] - main["out"][1]) < 0.03,
              f"dirs {case}: the hands mirrored (out off {off['out']} main {main['out']})")

two = cases("twohand")


def stance(lines):
    s = [l for l in lines if l.startswith("  stance:")]
    return s[0] if s else ""


st, h = parse(two.get("land", []))
check(st is not None and st["swimming"] == 0 and "main" in h and h["main"]["holds"] and h["off"]["stance"] > 0.95 and
      "going on it, share 1.00" in stance(two.get("land", [])),
      f"twohand land: the nail gun two-handed (the free hand on its hotspot: {stance(two.get('land', [])).strip()[:60]})")
st, h = parse(two.get("water", []))
check(st is not None and st["swimming"] == 1 and "off" in h and h["off"]["stance"] < 0.05 and h["off"]["helping"] == 0 and
      h["off"]["share"] > 0.95 and "going on it" not in stance(two.get("water", [])),
      "twohand water: one hand, the free hand swims (stance %.2f, helping %d, share %.2f)" %
      ((h["off"]["stance"], h["off"]["helping"], h["off"]["share"]) if h else (0, 0, 0)))
check(st is not None and "main" in h and h["main"]["holds"] and h["main"]["share"] > 0.95,
      "twohand water: the gun hand follows the stroke a little (share %.2f)" % (h["main"]["share"] if h else 0))
_, h1 = parse(two.get("firing", []))
_, h2 = parse(two.get("fired", []))
_, h3 = parse(two.get("hold", []))
_, h4 = parse(two.get("back", []))
ok = all("main" in x for x in (h1, h2, h3, h4))
check(ok and 0.2 < h1["main"]["take"] < 0.9 and h2["main"]["take"] == 0.0 and h2["off"]["take"] == 1.0,
      "twohand firing: the gun hand's stroke gives way smoothly (4 frames in: take %.2f; 24 in: %.2f), the free hand swims on "
      "(%.2f)" % ((h1["main"]["take"], h2["main"]["take"], h2["off"]["take"]) if ok else (0, 0, 0)))
check(ok and h3["main"]["take"] == 0.0 and h4["main"]["take"] == 1.0,
      "twohand firing: held off through vr_flat_swim_fire_hold after the trigger (%.2f), back after (%.2f)" %
      ((h3["main"]["take"], h4["main"]["take"]) if ok else (0, 0)))
st, h = parse(two.get("land2", []))
check(st is not None and st["swimming"] == 0 and "off" in h and h["off"]["stance"] > 0.95 and
      "going on it, share 1.00" in stance(two.get("land2", [])),
      f"twohand land2: out of the water the two-handed grip is back ({stance(two.get('land2', [])).strip()[:60]})")

print(f"flatswim: {passed} passed, {failed} failed")
sys.exit(1 if failed else 0)
