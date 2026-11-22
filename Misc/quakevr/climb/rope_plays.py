# Mock-hand plays for vr_rope_climb_test (docs/vr-port/ROPES.md, "Tests"): python rope_plays.py <out dir>
# writes rope_climb.txt and rope_jump.txt there. The test hangs a rope half a metre ahead of the player (tracking space:
# +x right, +y up, -z forward; the hands at x +-0.02, z -0.5 are at it); the plays' "cmd vr_rope_climb_check" lines are
# the checks, at the plays' own times (the game clock: the same at any frame rate).
#   rope_climb: the off hand takes the rope at 1.45 m, the main hand higher (1.85 m) and pulls down to 1.25 while the off
#     hand goes up and takes it higher, and pulls down in turn: two pulls, the body up at least 30 units; then down: the
#     main hand under the off one, the off hand lets go, the main hand pushes up 0.6 m (down at least 15); both hands
#     on it, a sideways nudge of its swing (150 units/s), the body swung at least 15 units, let go: the fall keeps the
#     rope's speed at the hands (at least 60 units/s level)
#   rope_jump: up one pull, both hands on it, the nudge, a jump: up at least 0.6 of vr_rope_jump over the rope's own
import os
import sys

REST = {"off": (-0.12, 1.2, -0.3), "main": (0.12, 1.2, -0.3)}
AT = {"off": -0.02, "main": 0.02}
FWD = -0.5


class Play:
    def __init__(self):
        self.lines = []
        for h, p in REST.items():
            self.key(0.0, h, p)

    def key(self, t, hand, p):
        self.lines.append(f"{t:.3f} {hand} {p[0]:.3f} {p[1]:.3f} {p[2]:.3f} 0 0 0")

    def rope(self, t, hand, y):
        self.key(t, hand, (AT[hand], y, FWD))

    def cmd(self, t, c):
        self.lines.append(f"{t:.3f} cmd {c}")

    def check(self, t, c):
        self.cmd(t, "vr_rope_climb_check " + c)

    def write(self, path):
        with open(path, "w", newline="\n") as f:
            f.write("\n".join(self.lines) + "\n")


def up_two(p):
    # the off hand takes it, then hand over hand: two pulls
    p.rope(0.5, "off", 1.45)
    p.cmd(0.7, "+graboff")
    p.check(0.9, "held 1")
    p.check(0.9, "mark")
    p.key(0.9, "main", REST["main"])
    p.rope(1.4, "main", 1.85)
    p.cmd(1.5, "+grabmain")
    p.check(1.6, "held 2")
    p.rope(1.6, "main", 1.85)
    p.cmd(1.65, "-graboff")
    p.rope(1.65, "off", 1.45)
    p.rope(2.4, "main", 1.25)
    p.key(1.9, "off", (-0.12, 1.3, -0.3))
    p.rope(2.5, "off", 1.85)
    p.cmd(2.6, "+graboff")
    p.rope(2.6, "off", 1.85)
    p.cmd(2.7, "-grabmain")
    p.rope(2.7, "main", 1.25)
    p.rope(3.4, "off", 1.25)
    p.key(2.9, "main", (0.12, 1.3, -0.3))
    p.check(3.5, "rose 30")


def climb():
    p = Play()
    up_two(p)
    # down: the main hand under the off one, the off lets go, the main pushes up
    p.rope(3.7, "main", 1.0)
    p.cmd(3.8, "+grabmain")
    p.check(3.85, "mark")
    p.cmd(3.9, "-graboff")
    p.rope(3.9, "main", 1.0)
    p.rope(3.9, "off", 1.25)
    p.rope(4.6, "main", 1.6)
    p.check(4.7, "lowered 15")
    p.check(4.75, "mark")  # (ropes6: climbing down and a mere grab make no pull grunt)
    # both hands, the nudge, the swing, let go
    p.key(4.1, "off", (-0.12, 1.3, -0.3))
    p.rope(5.0, "off", 1.75)
    p.cmd(5.1, "+graboff")
    p.check(5.3, "held 2")
    p.check(5.3, "grip 2")
    p.check(5.3, "grunts 0 0")
    p.check(5.3, "mark")
    p.check(5.3, "nudge 150")
    p.check(5.6, "moved 15")
    p.cmd(5.65, "-graboff")
    p.cmd(5.65, "-grabmain")
    p.check(5.72, "flung 60")
    p.check(5.8, "done")
    return p


def jump():
    p = Play()
    up_two(p)
    p.rope(3.6, "main", 1.75)
    p.cmd(3.7, "+grabmain")
    p.check(3.9, "held 2")
    p.check(3.9, "nudge 150")
    p.cmd(4.2, "+jump")
    p.check(4.25, "jumped")
    p.cmd(4.3, "-jump")
    p.check(4.4, "done")
    return p


def pump():
    # up, both hands, the forward key held 6 s: the swing pumped (pushed only while it swings forward) to the cap
    p = Play()
    up_two(p)
    p.rope(3.6, "main", 1.75)
    p.cmd(3.7, "+grabmain")
    p.check(3.9, "held 2")
    p.check(3.9, "mark")
    p.cmd(3.9, "+forward")
    p.check(9.9, "swing 250")
    p.cmd(9.9, "-forward")
    p.check(9.95, "held 2")
    p.check(10.0, "done")
    return p


def calm():
    # (ropes6) climbing up hand over hand, the body close to the rope: four pulls (the main hand, then the off, ...),
    # each regrab higher; the rope's swing at the hands and its free end barely move (calm: at most these units/s)
    p = Play()
    p.rope(0.5, "off", 1.45)
    p.cmd(0.7, "+graboff")
    p.check(0.9, "held 1")
    p.check(1.0, "mark")
    t = 1.0
    hold, free = "off", "main"
    for _ in range(4):
        grab = {"off": "+graboff", "main": "+grabmain"}
        drop = {"off": "-graboff", "main": "-grabmain"}
        p.key(t, free, (AT[free] * 6, 1.3, -0.3))
        p.rope(t + 0.4, free, 1.85)
        p.cmd(t + 0.5, grab[free])
        p.rope(t + 0.6, free, 1.85)
        p.cmd(t + 0.65, drop[hold])
        p.rope(t + 0.65, hold, 1.45)
        p.rope(t + 1.4, free, 1.25)
        p.key(t + 0.9, hold, (AT[hold] * 6, 1.3, -0.3))
        t += 1.4
        hold, free = free, hold
    p.check(t + 0.1, "rose 50")
    p.check(t + 0.1, "calm 25 30")
    p.check(t + 0.1, "grunts 3 4")
    p.check(t + 0.2, "done")
    return p


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(out, exist_ok=True)
    climb().write(os.path.join(out, "rope_climb.txt"))
    jump().write(os.path.join(out, "rope_jump.txt"))
    pump().write(os.path.join(out, "rope_pump.txt"))
    calm().write(os.path.join(out, "rope_calm.txt"))
    print("rope plays written to", out)
