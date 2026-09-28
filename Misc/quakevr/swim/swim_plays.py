# Mock-hand plays for the swimming tests (vr_mock_play files; docs/vr-port/TESTING.md, "Swimming"):
# python swim_plays.py writes, here:
#   strokes_main.txt, strokes_off.txt  one stroke per case from rest, 2.6 s apart (past the stroke memory), the hand
#                                      jumped (> 8 m/s: ignored as tracking) to each start: a pull with the palm leading,
#                                      a backhand (back of the hand leading), a sweep in with the back leading (square
#                                      and at 45 degrees), a slice edge first, a sweep out with the palm leading
#   cycle_edge.txt, cycle_backhand.txt the hands in turn (1.2 s a cycle): a pull back with the palm leading (1.8 m/s),
#                                      then the hand brought forward edge-on at 1 m/s, or (backhand) with the back of the
#                                      hand leading, as brisk; viewpos at 1 s and 7 s: the distance swum in 6 s
#   intent.txt                         a backhand, then 0.4 s later a slower pull the other way with the palm leading
#   air.txt                            put under water at Air Supply 1, 1.5, 2 and 3 in turn (setpos: gravity off)
# Mock poses: x right, y up, -z forward; pitch up, yaw left, roll. With Gun Angle 70, pitch 70 is level and -20 points
# the hand straight down. The main hand's palm faces the controller's left, the off hand's its right: pointing down,
# yaw 90 (main) or -90 (off) faces the palm back, towards the body; yaw 180 faces the main palm right, the off palm left.
Y = 1.05


def write(name, lines):
    with open(name, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def strokes(hand):
    s = 1 if hand == "main" else -1
    x0, back = 0.25 * s, 90 * s
    cases = [
        ("pull_palm_leading", (x0, Y, -0.45), (x0, Y, 0.15), back),
        ("backhand_back_leading", (x0, Y, 0.15), (x0, Y, -0.45), back),
        ("sweep_in_back_leading", (x0 + 0.3 * s, Y, -0.2), (x0 - 0.3 * s, Y, -0.2), 180),
        ("sweep_in_oblique45", (x0 + 0.3 * s, Y, -0.2), (x0 - 0.3 * s, Y, -0.2), 135 * s),
        ("slice_edge_leading", (x0 + 0.3 * s, Y, -0.2), (x0 - 0.3 * s, Y, -0.2), back),
        ("sweep_out_palm_leading", (x0 - 0.3 * s, Y, -0.2), (x0 + 0.3 * s, Y, -0.2), 180),
    ]
    other, ox = ("off", -0.25) if hand == "main" else ("main", 0.25)
    L = [f"0 {other} {ox} 1.0 0.0 70 0 0"]
    t = 0.5
    for name, a, b, yaw in cases:
        ang = f"-20 {yaw} 0"
        L.append(f"{t:.3f} {hand} {a[0]} {a[1]} {a[2]} {ang}")
        L.append(f"{t + 0.8:.3f} {hand} {a[0]} {a[1]} {a[2]} {ang}")
        L.append(f"{t + 0.8:.3f} cmd echo STROKE {hand} {name}")
        L.append(f"{t + 1.1:.3f} {hand} {b[0]} {b[1]} {b[2]} {ang}")  # 0.6 m in 0.3 s: 2 m/s
        L.append(f"{t + 2.6:.3f} {hand} {b[0]} {b[1]} {b[2]} {ang}")
        t += 2.602
    write(f"strokes_{hand}.txt", L)


def cycle(recovery):
    L = []
    for hand, s, phase in (("main", 1, 0.0), ("off", -1, 0.6)):
        x, back = 0.25 * s, 90 * s
        L.append(f"0 {hand} {x} {Y} -0.45 -20 {back} 0")
        t = 0.2 + phase
        while t < 8.5:
            L.append(f"{t:.3f} {hand} {x} {Y} -0.45 -20 {back} 0")
            L.append(f"{t + 0.33:.3f} {hand} {x} {Y} 0.15 -20 {back} 0")
            if recovery == "edge":
                L.append(f"{t + 0.40:.3f} {hand} {x} {Y} 0.15 -20 0 0")
                L.append(f"{t + 1.00:.3f} {hand} {x} {Y} -0.45 -20 0 0")
            else:
                L.append(f"{t + 0.40:.3f} {hand} {x} {Y} 0.15 -20 {back} 0")
                L.append(f"{t + 0.73:.3f} {hand} {x} {Y} -0.45 -20 {back} 0")
            L.append(f"{t + 1.10:.3f} {hand} {x} {Y} -0.45 -20 {back} 0")
            t += 1.2
    L += ["1.0 cmd viewpos", "7.0 cmd viewpos"]
    L.sort(key=lambda l: float(l.split()[0]))
    write(f"cycle_{recovery}.txt", L)


def intent():
    write("intent.txt", [
        "0 off -0.25 1.0 0.0 70 0 0",
        f"0.5 main 0.25 {Y} 0.15 -20 90 0",
        f"1.3 main 0.25 {Y} 0.15 -20 90 0",
        "1.3 cmd echo STROKE backhand 2.0 m/s",
        f"1.6 main 0.25 {Y} -0.45 -20 90 0",
        f"2.0 main 0.25 {Y} -0.45 -20 90 0",
        "2.0 cmd echo STROKE pull 1.6 m/s, 0.4 s later",
        f"2.375 main 0.25 {Y} 0.15 -20 90 0",
        f"4.0 main 0.25 {Y} 0.15 -20 90 0",
    ])


def air():
    L, t = [], 0.5
    for m in (1, 1.5, 2, 3):
        L += [f"{t:.2f} cmd setpos 612 474 60 0 0 0", f"{t + 0.02:.2f} cmd noclip", f"{t + 0.05:.2f} cmd vr_air_supply {m}",
              f"{t + 1.0:.2f} cmd echo AIR supply {m}: under water", f"{t + 1.0:.2f} cmd setpos 612 474 -180 0 0 0",
              f"{t + 1.02:.2f} cmd noclip"]
        t += 1.0 + 12 * m + 2.5
    L.append(f"{t:.2f} cmd echo AIRDONE")
    write("air.txt", L)
    return t


if __name__ == "__main__":
    strokes("main")
    strokes("off")
    cycle("edge")
    cycle("backhand")
    intent()
    print(f"written; air.txt plays {air():.0f} s")
