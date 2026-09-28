# Mock-hand plays for the climbing tests (vr_mock_play files; docs/vr-port/TESTING.md, "Climbing"):
# python climb_plays.py ladder|ladderlean|ledge|mantle|e1m1|ledgehang|runghang|ledgeodd|rungodd|push|overtop|pressL<d>|pressR<d>
# writes <name>.txt here and prints how long it plays (the hangs: both hands on the ledge or rung 56, pulled up a little
# and held, for screenshots, the odd ones with the controllers turned oddly; ladderlean: the ladder with the head leant
# in, as a player's is (the holds within a real arm's reach); push and overtop: staying within reach and
# the mantle's motion; the presses: the main hand gripping d units in front of the ledge or rung 56 and pulling, for the
# grab leniency).
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

def hang(y, fwd, pull=0.25, pitch=40, yaw=0, roll=0):
    # both hands onto the hold (setpos 78 176 24: the ledge, y 1.638; setpos 71 0 24: rung 56, y 1.943), then pulled
    # down `pull` metres (not enough for a mantle) and held still; the controllers turned by pitch, yaw, roll (the off
    # hand's yaw and roll mirrored)
    L = []
    for h, x, m in (("off", -0.12, -1), ("main", 0.12, 1)):
        a = f"{pitch} {yaw * m} {roll * m}"
        L += [f"0.000 {h} {x} 1.2 -0.3 {a}", f"0.500 {h} {x} {y} {fwd} {a}", f"1.000 {h} {x} {y} {fwd} {a}",
              f"2.000 {h} {x} {y - pull:.3f} {fwd} {a}", f"6.000 {h} {x} {y - pull:.3f} {fwd} {a}"]
    return L + ["0.700 cmd +graboff", "0.750 cmd +grabmain"], 6.0

# ROUND21.md, "Climbing: hand orientation, staying attached, small ledges". These lean the head in towards the wall
# (0.28 m: the body's box stays 16 units from the face, a real body doesn't), as a player at a wall does, so that the
# holds are within a real arm's reach of the shoulders.
LEAN = ["0.000 head 0 1.646 0", "0.600 head 0 1.62 -0.28"]

def push(hy=1.638, fwd=-0.69):
    # both hands on the ledge (setpos 78 176 24), pulled up 0.25 m; then three times: the hands drawn in to the chest
    # (the body against the face: it can't follow) and pushed out past where they took hold; then out further (past
    # an arm's reach); then the main hand lets go and the off hand alone does it twice more; then both let go
    L = list(LEAN); t = [0.0]
    pos = {"off": [-0.25, 1.1, -0.2], "main": [0.25, 1.1, -0.2]}
    def key(h, x, y, z):
        pos[h] = [x, y, z]; L.append(f"{t[0]:.3f} {h} {x:.3f} {y:.3f} {z:.3f} 70 0 0")
    def keys(hs, y, z, dt):
        for h in hs: key(h, pos[h][0], pos[h][1], pos[h][2])
        t[0] += dt
        for h in hs: key(h, pos[h][0], y, z)
    def cmd(c): L.append(f"{t[0]:.3f} cmd {c}")
    key("off", *pos["off"]); key("main", *pos["main"]); t[0] = 0.6
    key("off", -0.12, hy, fwd); key("main", 0.12, hy, fwd); t[0] = 0.7; cmd("+graboff"); t[0] = 0.75; cmd("+grabmain")
    t[0] = 1.0; keys(("off", "main"), hy - 0.25, fwd, 0.6)
    both = ("off", "main")
    for _ in range(3):
        t[0] += 0.2; keys(both, hy - 0.25, -0.35, 0.6)
        t[0] += 0.2; keys(both, hy - 0.25, fwd - 0.16, 0.6)
    t[0] += 0.2; keys(both, hy - 0.25, -1.0, 0.6)
    t[0] += 0.3; keys(both, hy - 0.25, fwd, 0.6)
    t[0] += 0.2; cmd("-grabmain"); keys(("main",), 1.1, -0.2, 0.5)
    for _ in range(2):
        t[0] += 0.2; keys(("off",), hy - 0.25, -0.35, 0.6)
        t[0] += 0.2; keys(("off",), hy - 0.25, fwd - 0.16, 0.6)
    t[0] += 0.5; cmd("-graboff"); t[0] += 1.0
    for h in both: key(h, *pos[h])
    return L, t[0]

def overtop(hy=1.638, fwd=-0.69):
    # the mantle's motion at a ledge (setpos 78 176 24), the narrow wall (setpos -218 -280 24) or the ledge with no room
    # on top (setpos -138 -280 24): both hands on it, pulled down to the hips and in, then pushed down and out over the
    # top, held, and let go
    L = list(LEAN)
    for h, x in (("off", -0.12), ("main", 0.12)):
        L += [f"0.000 {h} {x * 2} 1.1 -0.2 70 0 0", f"0.600 {h} {x} {hy} {fwd} 70 0 0", f"1.000 {h} {x} {hy} {fwd} 70 0 0",
              f"2.000 {h} {x} 1.0 -0.35 70 0 0", f"2.300 {h} {x} 1.0 -0.35 70 0 0", f"2.900 {h} {x} 0.95 -0.75 70 0 0",
              f"4.000 {h} {x} 0.95 -0.75 70 0 0", f"5.000 {h} {x * 2} 1.1 -0.2 70 0 0"]
    return L + ["0.700 cmd +graboff", "0.750 cmd +grabmain", "4.000 cmd -graboff", "4.000 cmd -grabmain"], 5.0

def press(where, d, pull=0.25):
    # the main hand d units in front of the ledge's face (setpos 78 176 24: x 96 - d, 1 under the lip) or rung 56's
    # (setpos 71 0 24: x 88 - d, 2 under its top), grips, pulls down `pull` metres, lets go; the off hand stays down
    x, y = (96 - d - 77.3, 1.676) if where == "L" else (88 - d - 70.3, 2.076)
    fwd = -x / 26.25
    L = ["0.000 off -0.35 1.2 -0.3 0 0 0", "4.000 off -0.35 1.2 -0.3 0 0 0", "0.000 main 0.35 1.2 -0.3 0 0 0",
         f"0.500 main 0.12 {y} {fwd:.4f} 0 0 0", f"1.200 main 0.12 {y} {fwd:.4f} 0 0 0",
         f"2.000 main 0.12 {y - pull:.3f} {fwd:.4f} 0 0 0", f"3.000 main 0.12 {y - pull:.3f} {fwd:.4f} 0 0 0",
         "0.700 cmd +grabmain", "3.000 cmd -grabmain"]
    return L, 4.0

def main():
    import sys
    if sys.argv[1].startswith("press"):
        L, t = press(sys.argv[1][5], float(sys.argv[1][6:]))
        write(sys.argv[1] + ".txt", L)
        print(f"{sys.argv[1]}: {t:.2f} s")
        return
    L, t = {"ladder": lambda: ladder(11, True), "ledge": lambda: ledge(7, 1), "mantle": mantle, "e1m1": lambda: ledge(0, 2, 1.638, -0.76, "mantle"),
            "ledgehang": lambda: hang(1.638, -0.69), "runghang": lambda: hang(1.943, -0.72),
            "ledgeodd": lambda: hang(1.638, -0.69, 0.25, 10, 50, 70), "rungodd": lambda: hang(1.943, -0.72, 0.25, 110, -40, -60),
            "push": push, "overtop": overtop, "ladderlean": lambda: (lambda L, t: (LEAN + L, t))(*ladder(11, True))}[sys.argv[1]]()
    write(sys.argv[1] + ".txt", L)
    print(f"{sys.argv[1]}: {t:.2f} s")

if __name__ == "__main__":
    main()
