# Mock-hand plays for the climbing tests (vr_mock_play files; docs/vr-port/TESTING.md, "Climbing"):
# python climb_plays.py ladder|ledge|mantle|e1m1 writes <name>.txt here and prints how long it plays.
import sys
HI, LO, FWD = 1.943, 1.18, -0.72

def write(name, lines):
    with open(name, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")

def ladder(n_half, top_out=False):
    # hand over hand up the rungs, starting with the off hand
    L = []
    pos = {"off": [-0.12, 1.2, -0.3], "main": [0.12, 1.2, -0.3]}
    def key(t, h, x, y, z):
        pos[h] = [x, y, z]
        L.append(f"{t:.3f} {h} {x:.3f} {y:.3f} {z:.3f} 0 0 0")
    for h in pos:
        key(0.0, h, *pos[h])
    grab = {"off": "+graboff", "main": "+grabmain"}
    free = {"off": "-graboff", "main": "-grabmain"}
    t = 0.2
    hand, other = "off", "main"
    for k in range(n_half):
        x = pos[hand][0]
        key(t, hand, *pos[hand])
        t += 0.25
        key(t, hand, x, 0.5 * (pos[hand][1] + HI), -0.45)  # back off the rung, then up
        t += 0.35
        key(t, hand, x, HI, FWD)          # onto the next rung
        t += 0.1
        L.append(f"{t:.3f} cmd {grab[hand]}")
        t += 0.1
        key(t, hand, x, HI, FWD)
        key(t, other, *pos[other])
        t += 1.0
        key(t, hand, x, LO, FWD)          # pull down (the other hand still holding, still)
        key(t, other, *pos[other])
        t += 0.1
        if k > 0 or True:
            L.append(f"{t:.3f} cmd {free[other]}")  # the lower hand lets go
        t += 0.1
        hand, other = other, hand
    if top_out:
        # over the top of the wall (the hand hovering just short of its edge), a long pull, and the mantle
        x = pos[hand][0]
        key(t, hand, *pos[hand]); t += 0.25
        key(t, hand, x, 2.0, -0.45); t += 0.35
        key(t, hand, x, 2.10, -0.80); t += 0.1
        L.append(f"{t:.3f} cmd {grab[hand]}"); t += 0.1
        key(t, hand, x, 2.10, -0.80); key(t, other, *pos[other]); t += 1.2
        key(t, hand, x, 1.10, -0.60); key(t, other, *pos[other]); t += 0.3
        L.append(f"{t:.3f} cmd {free[hand]}"); L.append(f"{t:.3f} cmd {free[other]}")
        key(t, hand, x, 1.2, -0.3); key(t, other, pos[other][0], 1.2, -0.3); t += 1.0
    return L, t

def ledge(left, right, hy=1.638, fwd=-0.69, finish="fall"):
    # both hands on the long ledge: a two-hand hang (the hands pulling unevenly, one letting go mid-pull), a shimmy
    # `left` hand-overs to the left then `right` to the right, and both let go
    L = []; t = [0.0]
    pos = {"off": [-0.35, 1.2, -0.3], "main": [0.35, 1.2, -0.3]}
    def key(h, x, y, z, at=None):
        pos[h] = [x, y, z]
        L.append(f"{(t[0] if at is None else at):.3f} {h} {x:.3f} {y:.3f} {z:.3f} 0 0 0")
    def hold(dt):
        for h in pos: key(h, *pos[h])
        t[0] += dt
        for h in pos: key(h, *pos[h])
    def cmd(c): L.append(f"{t[0]:.3f} cmd {c}")
    hold(0.3)
    key("off", -0.12, hy, fwd); key("main", 0.12, hy, fwd); t[0] += 0.5; hold(0.1)
    cmd("+graboff"); t[0] += 0.3; cmd("+grabmain"); hold(0.3)
    # both pull down, the off hand twice as far as the main hand
    t[0] += 0.8; key("off", -0.12, hy - 0.20, fwd); key("main", 0.12, hy - 0.10, fwd); hold(0.3)
    # both pull on; the main hand lets go halfway, while both still move
    t0 = t[0]
    t[0] = t0 + 0.8; key("off", -0.12, hy - 0.30, fwd); key("main", 0.12, hy - 0.20, fwd)
    L.append(f"{t0 + 0.4:.3f} cmd -grabmain")
    hold(0.2)
    # the main hand takes hold again while the off hand pushes up (the body lowers)
    key("main", 0.12, hy - 0.05, fwd); t[0] += 0.4; hold(0.05)
    t0 = t[0]; t[0] = t0 + 0.6; key("off", -0.12, hy - 0.20, fwd); key("main", 0.12, hy - 0.05, fwd)
    L.append(f"{t0 + 0.3:.3f} cmd +grabmain")
    hold(0.5)
    base = {"off": pos["off"][1], "main": pos["main"][1]}
    def shimmy(lead, trail, sign, n):
        grab = {"off": "+graboff", "main": "+grabmain"}; free = {"off": "-graboff", "main": "-grabmain"}
        for _ in range(n):
            # the leading hand lets go, reaches on along the ledge (lifted over it), takes hold
            cmd(free[lead]); t[0] += 0.05
            x0 = pos[lead][0]; x1 = x0 + sign * 0.33
            key(lead, 0.5 * (x0 + x1), base[lead] + 0.08, fwd + 0.05); t[0] += 0.25
            key(lead, x1, base[lead], fwd); t[0] += 0.1; cmd(grab[lead]); hold(0.1)
            # both hands push back the other way: the body moves along the ledge
            t[0] += 0.7; key(lead, pos[lead][0] - sign * 0.33, base[lead], fwd); key(trail, pos[trail][0] - sign * 0.33, base[trail], fwd); hold(0.1)
            # the trailing hand lets go, comes back, takes hold
            cmd(free[trail]); t[0] += 0.05
            x0 = pos[trail][0]; x1 = x0 + sign * 0.33
            key(trail, 0.5 * (x0 + x1), base[trail] + 0.08, fwd + 0.05); t[0] += 0.25
            key(trail, x1, base[trail], fwd); t[0] += 0.1; cmd(grab[trail]); hold(0.1)
    shimmy("off", "main", -1, left)   # to the player's left: hands reach left (-x in tracking space)
    hold(0.4)
    shimmy("main", "off", +1, right)
    hold(0.5)
    if finish == "mantle":
        # both hands pull down and in: over the top
        t[0] += 1.0; key("off", pos["off"][0], hy - 0.8, fwd + 0.2); key("main", pos["main"][0], hy - 0.8, fwd + 0.2)
        hold(0.5)
    cmd("-graboff"); cmd("-grabmain"); hold(2.0)
    return L, t[0]

def mantle(hy=1.638, fwd=-0.69):
    L = []
    for h, x in (("off", -0.12), ("main", 0.12)):
        L += [f"0.000 {h} {x} 1.2 -0.3 0 0 0", f"0.500 {h} {x} {hy} {fwd} 0 0 0", f"1.000 {h} {x} {hy} {fwd} 0 0 0",
              f"2.000 {h} {x} {hy - 0.8} {fwd + 0.2} 0 0 0", f"3.000 {h} {x} {hy - 0.8} {fwd + 0.2} 0 0 0"]
    L += ["0.700 cmd +graboff", "0.750 cmd +grabmain", "2.600 cmd -graboff", "2.600 cmd -grabmain"]
    return L, 3.0

def main():
    import sys
    L, t = {"ladder": lambda: ladder(11, True), "ledge": lambda: ledge(7, 1), "mantle": mantle, "e1m1": lambda: ledge(0, 2, 1.638, -0.76, "mantle")}[sys.argv[1]]()
    write(sys.argv[1] + ".txt", L)
    print(f"{sys.argv[1]}: {t:.2f} s")

if __name__ == "__main__":
    main()
