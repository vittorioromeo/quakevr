"""Writes quakevr/motions/heavy2h_swings.mock: both hands (rel keys, from where they are) sweep right to left at chest
height through a monster ahead, at several speeds, each back slowly the same way (a reposition: no blow), then a short
jerk and a wiggle. Usage: python gen_swings.py <out.mock> [onehand]"""
import sys

out = sys.argv[1]
hands = ["main"] if len(sys.argv) > 2 and sys.argv[2] == "onehand" else ["main", "off"]
lines = []
t = 0.5
x = 0.6  # where the hands are (rel x)


def key(tt, xx):
    for h in hands:
        lines.append("%.3f rel %s %.4f 0 0" % (tt, h, xx))


def move(to, speed, ease=0.08):
    """From x to `to` at `speed` m/s, eased in and out over `ease` s; keys every 10 ms."""
    global t, x
    dist = abs(to - x)
    cruise = max(dist / speed - ease, 0.01)  # the eased ends cover ease * speed / 2 each
    total = cruise + ease
    n = max(2, int(total / 0.01))
    x0 = x
    for i in range(1, n + 1):
        u = i / n
        tt = u * total
        # trapezoidal speed: the distance covered by time tt
        a = ease / 2
        if tt < a:
            s = 0.5 * tt * tt / a
        elif tt > total - a:
            r = total - tt
            s = (total - a) - 0.5 * r * r / a
        else:
            s = tt - a / 2
        frac = s / (total - a)
        key(t + tt, x0 + (to - x0) * frac)
    t += total
    x = to


def pause(d):
    global t
    key(t + d, x)
    t += d


def step(name):
    lines.append("%.3f cmd echo STEP %s" % (t, name))


key(t, x)
lines.insert(0, "0.000 cmd echo STEP setup")
move(0.6, 1)  # (stay)
pause(0.6)
for v in (0.9, 1.6, 2.2, 3.5):
    lines.append("%.3f cmd impulse 241" % (t - 0.4))
    step("sweep_%.1f" % v)
    pause(0.3)
    move(-0.6, v)
    pause(0.5)
    step("back_slow")
    move(0.6, 0.3)
    pause(0.5)
# A short jerk through it (a reposition) and a wiggle.
move(0.15, 0.3)
pause(0.4)
lines.append("%.3f cmd impulse 241" % (t - 0.3))
step("jerk")
move(-0.12, 1.8)
pause(0.5)
step("wiggle")
for i in range(4):
    move(0.05, 2.0)
    move(-0.15, 2.0)
pause(0.5)
step("end")
lines.sort(key=lambda s: float(s.split()[0]))
open(out, "w").write("\n".join(lines) + "\n")
print("wrote %s: %.1f s" % (out, t))
