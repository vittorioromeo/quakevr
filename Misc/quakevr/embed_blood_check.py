"""embed_blood_test.sh's verdict: after each `embedblood: <case> stuck` / `in hand` mark, the vr_gore_hands_info gear
lines that follow it. For cases 7-10 (an axe swung into the training dummy, a sword's stab into it, the same into a
grunt): the blow's blood went onto the part that went in (vr_wounds_debug's "on the part that went in" line before the
stuck mark), and that weapon's gear shows blood (texels > 0) stuck (in the world) and pulled out (in the main hand)."""
import re
import sys

CASES = {"7": ("axe", "an axe swung into the dummy"), "8": ("sword", "a sword's stab into the dummy"),
         "9": ("axe", "an axe swung into a grunt"), "10": ("sword", "a sword's stab into a grunt")}
marks = {}
part = {}
seen_part = False
current = None
for line in sys.stdin:
    if "on the part that went in" in line:
        seen_part = True
    m = re.search(r"embedblood: (\d+) (stuck|in hand)", line)
    if m:
        current = (m.group(1), m.group(2))
        marks[current] = 0
        if m.group(2) == "stuck":
            part[m.group(1)] = seen_part
        seen_part = False
        continue
    g = re.search(r"gear #\d+ (\S+) (in the world|in the main hand)\S*.*blood on (\d+) texels", line)
    if g and current and current[0] in CASES:
        want = "in the world" if current[1] == "stuck" else "in the main hand"
        if CASES[current[0]][0] in g.group(1) and g.group(2) == want:
            marks[current] = max(marks[current], int(g.group(3)))
passed = failed = 0
def check(ok, text):
    global passed, failed
    passed += ok
    failed += not ok
    print(f"embedblood: {'PASS' if ok else 'FAIL'} {text}")
for case, (_, name) in CASES.items():
    check(part.get(case, False), f"{name}: the blow's blood on the part that went in")
    for when in ("stuck", "in hand"):
        n = marks.get((case, when))
        check(bool(n), f"{name}, {when}: its blood {'not seen' if n is None else f'{n} texels'}")
    # Pulled out, it keeps what it had (its own mask in the hand, not a new one): at least 0.8 of it.
    stuck, held = marks.get((case, "stuck")) or 0, marks.get((case, "in hand")) or 0
    check(stuck > 0 and held >= 0.8 * stuck, f"{name}: in the hand it keeps its blood ({held} of {stuck} texels)")
print(f"embedblood: {passed} passed, {failed} failed")
