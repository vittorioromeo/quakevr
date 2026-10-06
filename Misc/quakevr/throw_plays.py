# Mock-hand throws for the throw-angle tests (vr_mock_play files; docs/vr-port/TESTING.md, "Throwing: the release
# angle"; ROUND21.md, "Climbing: sliding along the wall; throw angle after calibration"):
#   python throw_plays.py [--gunangle 70] [--out <folder>] [--stretch 3.333] [--rate 90]
# writes throws.txt: several throws with the main hand, one after the other, each announced by "echo THROW <name>
# <meant elevation>" and let go (-grabmain) at its release. The controller poses are the real hand's pose plus the
# controller's own turn on it (Gun Angle: the controller is pitched up that much from the hand; the author's
# calibration of 2026-09-28 says 70), so the same file is the same controller motion whatever the game's calibration.
# Tracking space: x right, y up, -z forward; the mock's pitch is up, yaw left, in degrees.
#
# The throws (an arm arcing about the shoulder in a vertical plane, min-jerk; the wrist cocked back and flicked
# forward through the release):
#   overhand   the arm from 150 to 40 degrees (forward = 0, up = 90), released at 95 (5 degrees up), the wrist cocked
#              35 back, flexed to 45 forward: a hard throw (about 7 m/s)
#   lob        an underarm lob: the arm from -120 to -30, released at -60 (30 degrees up), the wrist 20 back to 30
#              forward (the hand's pitch rises through an underarm swing, falls through an overarm one) (about 4 m/s)
#   flat       a push: the hand straight ahead along a line 5 degrees up, no wrist (about 5 m/s)
#   overhand0  the overhand throw with the wrist still (the hand turns only with the arm)
import argparse, math

RATE = 90.0
SHOULDER = (0.20, 1.45, 0.0)


def minjerk(s):
    return s * s * s * (10 - 15 * s + 6 * s * s)


def arc_throw(name, a0, a1, arel, w0, w1, duration, radius=0.6):
    # the arm's angle a (degrees, 0 forward, 90 up) from a0 to a1 over `duration`; the hand's pitch is the tangent's
    # (the arm's angle -90, or +90 underarm) plus the wrist, which goes from w0 (cocked back) to w1 (flexed) around the
    # release. Returns the keys (t, x, y, z, hand pitch) and the release time.
    keys = []
    n = int(duration * RATE)
    sign = 1 if a1 < a0 else -1  # overhand: the angle falls (forward over the top); underarm: it rises
    rel_s = None
    for i in range(n + 1):
        s = i / n
        a = a0 + (a1 - a0) * minjerk(s)
        if rel_s is None and (a - arel) * (a0 - arel) <= 0:
            rel_s = s
        r = math.radians(a)
        x, y, z = SHOULDER[0], SHOULDER[1] + radius * math.sin(r), SHOULDER[2] - radius * math.cos(r)
        # the direction of travel: along the tangent, the way the angle goes
        tangent_pitch = a - 90 if sign > 0 else a + 90
        keys.append([s * duration, x, y, z, tangent_pitch])
    for k in keys:
        s = k[0] / duration
        # the wrist: cocked back until 0.2 before the release, flexed by 0.15 after it (smooth)
        u = min(1.0, max(0.0, (s - (rel_s - 0.2)) / 0.35))
        k[4] += w0 + (w1 - w0) * minjerk(u) if w0 != w1 else w0
    return keys, rel_s * duration


def line_throw(name, elev, speed_peak, duration):
    keys = []
    n = int(duration * RATE)
    length = speed_peak * duration / 1.875
    e = math.radians(elev)
    p0 = (0.20, 1.30, -0.05)
    for i in range(n + 1):
        s = i / n
        d = length * minjerk(s)
        keys.append([s * duration, p0[0], p0[1] + d * math.sin(e), p0[2] - d * math.cos(e), elev])
    return keys, 0.5 * duration


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gunangle", type=float, default=70.0, help="the controller's pitch on the real hand (degrees)")
    ap.add_argument("--out", default="throws.txt")
    ap.add_argument("--stretch", type=float, default=1.0,
                    help="every time times this: the same motions made that many times slower (bullet time's 1/0.3)")
    ap.add_argument("--rate", type=float, default=90.0,
                    help="keys a second of the motion (90: a headset's; more: a smoother motion between the game's frames,"
                         " whose throws then depend less on where the frames fall)")
    ap.add_argument("--grip", type=int, default=2,
                    help="2: the analog grip opens as a hand does (grip main keys: 1 until 15 ms before the release, 0"
                         " 35 ms after; it crosses the release's line, 0.7 of its peak, at the release, between the"
                         " frames); 1: the grip button let go at the release (a step: the release up to a frame late); both"
                         " besides the +grabmain/-grabmain commands, which wait for a server frame (up to 25 ms late at"
                         " 240 fps), while the grip's release is timed on the tracking clock; 0: the commands alone (as"
                         " before 2026-10-06)")
    ap.add_argument("--digits", type=int, default=6,
                    help="decimals of the positions (the angles two fewer): 4 as before 2026-10-06, whose 0.1 mm steps"
                         " made 1000 keys a second +-0.1 m/s of noise in the played velocity")
    args = ap.parse_args()
    global RATE
    RATE = args.rate
    throws = [
        ("overhand", 5, arc_throw("overhand", 150, 40, 95, 35, -45, 0.30)),
        ("lob", 30, arc_throw("lob", -120, -30, -60, -20, 30, 0.45)),
        ("flat", 5, line_throw("flat", 5, 5.0, 0.30)),
        ("overhand0", 5, arc_throw("overhand0", 150, 40, 95, 0, 0, 0.30)),
    ]
    L = ["0.000 main 0.25 1.1 -0.2 70 0 0", "0.000 off -0.25 1.1 -0.2 70 0 0"]
    t0 = 1.0
    for name, elev, (keys, rel) in throws:
        L.append(f"{t0 - 0.5:.3f} main {keys[0][1]:.4f} {keys[0][2]:.4f} {keys[0][3]:.4f} {keys[0][4] + args.gunangle:.2f} 0 0")
        L.append(f"{t0 - 0.45:.3f} cmd echo THROW {name} {elev}")
        L.append(f"{t0 - 0.4:.3f} cmd +grabmain")
        if args.grip == 1:
            L.append(f"{t0 - 0.4:.3f} button main grip 1")
        elif args.grip == 2:
            L.append(f"{t0 - 0.4:.3f} grip main 1")
        d, a = args.digits, max(2, args.digits - 2)
        for k in keys:
            L.append(f"{t0 + k[0]:.6f} main {k[1]:.{d}f} {k[2]:.{d}f} {k[3]:.{d}f} {k[4] + args.gunangle:.{a}f} 0 0")
        L.append(f"{t0 + rel:.6f} cmd -grabmain")
        if args.grip == 1:
            L.append(f"{t0 + rel:.6f} button main grip 0")
        elif args.grip == 2:
            L.append(f"{t0 + rel - 0.015:.6f} grip main 1")
            L.append(f"{t0 + rel + 0.035:.6f} grip main 0")
        t0 += keys[-1][0] + 1.0
        L.append(f"{t0 - 0.6:.3f} main 0.25 1.1 -0.2 70 0 0")
    L.append(f"{t0:.3f} off -0.25 1.1 -0.2 70 0 0")
    if args.stretch != 1.0:
        L = [f"{float(l.split(' ', 1)[0]) * args.stretch:.6f} {l.split(' ', 1)[1]}" for l in L]
        t0 *= args.stretch
    with open(args.out, "w", newline="\n") as f:
        f.write("\n".join(L) + "\n")
    print(f"{args.out}: {t0:.2f} s")


if __name__ == "__main__":
    main()
