"""offhand_recoil_check.py <log> -- offhand_recoil_test.sh's verdict: reads its "recoil2h step <label>" lines, each
followed by vr_recoil_2h_print's main-hand line, and checks each step (see the .sh). Prints PASS/FAIL lines and
"offhandrecoil: N passed, M failed"; exits 1 on a failure."""
import re
import sys

STEP = re.compile(r"^recoil2h step (\S+)")
MAIN = re.compile(r"^recoil2h: main slot (-?\d+) support ([\d.]+) hotspot ([\d.]+) help (\d) shares ([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+)")


def main():
    steps = {}
    label = None
    shell_taken = False
    with open(sys.argv[1], encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            shell_taken = shell_taken or "a shell taken from the pouch by hand 0" in line
            m = STEP.match(line)
            if m:
                label = m.group(1)
                continue
            m = MAIN.match(line)
            if m and label:
                steps[label] = {"slot": int(m.group(1)), "support": float(m.group(2)), "hotspot": float(m.group(3)),
                                "help": int(m.group(4)), "shares": [float(g) for g in m.groups()[4:8]]}
                label = None
    passed = failed = 0

    def check(ok, what, got):
        nonlocal passed, failed
        if ok:
            passed += 1
        else:
            failed += 1
        print(f"offhandrecoil: {'PASS' if ok else 'FAIL'} {what} ({got})")

    def get(name):
        s = steps.get(name)
        if s is None:
            check(False, f"step {name} printed", "missing")
        return s

    ones = [1.0, 1.0, 1.0, 1.0]
    one = get("one")
    if one:
        check(one["slot"] >= 0 and one["support"] == 0 and one["shares"] == ones, "one-handed: support 0, shares 1", one)
    hot = get("hot")
    two = hot["shares"] if hot else None
    if hot:
        check(hot["support"] == 1 and hot["help"] == 1 and all(v < 1 for v in two),
              "the hotspot gripped: support 1, help 1, the two-handed shares (< 1)", hot)
    anys = [k for k in steps if k.startswith("any") and not k.endswith(("rel", "off"))]
    check(len(anys) >= 4, "four grips anywhere tried", len(anys))
    grips = [("hot", 1)] + [(k, 2) for k in anys]
    for name, help_kind in grips:
        s = get(name) if name != "hot" else hot
        if s and name != "hot" and two:
            check(s["support"] == 1 and s["help"] == 2 and s["hotspot"] == 0 and s["shares"] == two,
                  f"{name}: gripped anywhere (not a hotspot): support 1, the hotspot's shares", s)
        rel, off = get(name + "rel"), get(name + "off")
        if rel and two:
            check(0 < rel["support"] < 1 and all(t <= v < 1 for v, t in zip(rel["shares"], two)),
                  f"{name}: two frames after letting go, easing out (0 < support < 1)", rel)
        if off:
            check(off["support"] == 0 and off["shares"] == ones, f"{name}: let go of: one-handed again", off)
    check(shell_taken, "the off hand took a shell from the ammo pouch", shell_taken)
    for name, what in (("touch", "the off hand on its body, not gripping"),
                       ("shellbody", "the off hand holding a shell, gripping, on its body"),
                       ("shellhot", "the off hand holding a shell, gripping, on its hotspot")):
        s = get(name)
        if s:
            check(s["support"] == 0 and s["shares"] == ones, f"{what}: one-handed", s)
    print(f"offhandrecoil: {passed} passed, {failed} failed")
    sys.exit(1 if failed else 0)


main()
