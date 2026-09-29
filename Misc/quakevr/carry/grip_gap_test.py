"""grip_gap_test.py -- held props stay in the hand, drawn where they are (ROUND21.md, "Held props: no gap after two
hands").

Two tests in the mock headset (the firing range), each step ending with `vr_carry_check`: for every hand holding
something, how far the prop is drawn in the hand from where the server has it in its hand (the physical body: what
the hands and shots meet), and the gap between the drawn fist and the drawn prop. The hands move in real time (one
step a frame: the server runs at 72 Hz, the frames faster), as in the headset.

- cycle: a health box in the main hand, then in both; one hand pulled away till it comes off; back and grip again;
  the other pulled off; a hand let go of it with the other a few cm off its grip; many times.
- forcegrab: a floating health box force grabbed and caught with the hand still moving and the player walking.

    python grip_gap_test.py <agent> [cycle|forcegrab|both] [cycles]

Fails (exit 1) if a prop is drawn more than TOL_IN_HAND cm / TOL_TURN degrees off its place in the hand, or its fist
gap is more than TOL_GAP cm (negative: pressed in, as a Where Taken grip sits).
"""
import os
import re
import subprocess
import sys

KIT = "C:/OHWorkspace/qvr-kit"
BASH = os.environ.get("QVR_BASH", "C:/Program Files/Git/bin/bash.exe")  # (not WSL's bash)
TOL_IN_HAND = 0.5  # cm
TOL_TURN = 2.0  # degrees
TOL_GAP = 1.0  # cm

OFF_AWAY = "vr_mock_hand off -0.35 1.1 -0.2 70 0 0"
EXTRA = os.environ.get("GRIPGAP_EXTRA", "")  # console commands before each test
NO_SHAKE = ["vr_fatigue_shake 0", "vr_fatigue_shake_angle 0"]  # (tired arms shake the drawn hand and what it holds)


def hand(which, x, y=1.30, z=-0.45, pitch=70, yaw=0, roll=0):
    return "vr_mock_hand %s %.3f %.3f %.3f %d %d %d" % (which, x, y, z, pitch, yaw, roll)


def waits(n):
    return ["wait"] * n


def glide(which, x0, x1, step=0.01, **kw):
    """The hand moved from x0 to x1 one step a frame (1 cm a frame: about 2.5 m/s in the mock)."""
    out = []
    n = max(1, int(round(abs(x1 - x0) / step)))
    for i in range(1, n + 1):
        out += [hand(which, x0 + (x1 - x0) * i / n, **kw), "wait"]
    return out


def check(label):
    return ["echo STEP " + label, "vr_carry_check"]


def grip(which, on):
    side = "left" if which == "off" else "right"
    return ["%sgrab%s" % ("+" if on else "-", side), "vr_mock_button %s grip %d" % (which, 1 if on else 0)]


def cycle_cfg(cycles):
    c = ["developer 1", *NO_SHAKE, OFF_AWAY, hand("main", 0.10), *waits(20), "vr_rigid_place item_health main 0 3 0",
         *grip("main", True), *waits(20), *check("one hand"), hand("off", -0.10), *waits(10)]
    for i in range(1, cycles + 1):
        # A little turned each cycle (both hands alike), so that the grips are not always level.
        kw = {"pitch": 70 - 4 * (i % 3), "yaw": 6 * (i % 2), "roll": 5 * (i % 3)}
        c += [hand("off", -0.10, **kw), hand("main", 0.10, **kw), *waits(10)]
        c += [*grip("off", True), *waits(20), *check("%d both" % i)]
        # The off hand pulled away (the main keeps it), back, grip.
        c += [*glide("off", -0.10, -0.42, **kw), *waits(20), *check("%d off pulled off" % i)]
        c += [*grip("off", False), hand("off", -0.10, **kw), *waits(20), *grip("off", True), *waits(20),
              *check("%d both again" % i)]
        # The main hand pulled away (the off hand keeps it: a hand-over), back, grip.
        c += [*glide("main", 0.10, 0.42, **kw), *waits(20), *check("%d main pulled off" % i)]
        c += [*grip("main", False), hand("main", 0.10, **kw), *waits(20), *grip("main", True), *waits(20),
              *check("%d both, main again" % i)]
        # The off hand 5 cm out (within the drift: each hand off its grip), the main lets go: the off keeps it.
        c += [*glide("off", -0.10, -0.15, **kw), *waits(10), *grip("main", False), *waits(30),
              *check("%d main let go, off 5 cm out" % i)]
        c += [*glide("off", -0.15, -0.10, **kw), *waits(10), *grip("main", True), *waits(20),
              *check("%d both, third" % i)]
        # The main hand 5 cm out, the off lets go: the main keeps it.
        c += [*glide("main", 0.10, 0.15, **kw), *waits(10), *grip("off", False), *waits(30),
              *check("%d off let go, main 5 cm out" % i)]
        c += [*glide("main", 0.15, 0.10, **kw), *waits(20)]
    return c


def forcegrab_play(tries):
    """A vr_mock_play take (it gives the hands their velocities, which the force grab's flick needs; bare vr_mock_hand
    steps have none). Each try: a floating health box ahead of the main hand, pointed at and flicked; the player then
    walks and the hand swings side to side at 2 m/s through the catch (the grip pressed a little later each try)."""
    p = ["0.000 off -0.350 1.100 -0.200 70 0 0"]
    for i in range(tries):
        t0 = 4.0 * i + 0.5
        side = "+moveleft" if i % 2 == 0 else "+moveright"
        p += ["%.3f main 0.100 1.300 -0.450 70 0 0" % t0, "%.3f cmd sv_gravity 0" % t0,
              "%.3f cmd vr_rigid_place item_health main 60 0 0" % (t0 + 0.2), "%.3f cmd +attack" % (t0 + 0.6),
              "%.3f main 0.100 1.300 -0.450 70 0 0" % (t0 + 1.2), "%.3f main 0.100 1.600 -0.450 70 0 0" % (t0 + 1.3),
              "%.3f cmd %s" % (t0 + 1.3, side)]
        t, x = t0 + 1.3, 0.1
        while t < t0 + 2.6:
            t += 0.05
            x = 0.2 if abs(x - 0.1) < 1e-6 else 0.1
            p.append("%.3f main %.3f 1.600 -0.450 70 %d 0" % (t, x, 5 * (i % 3)))
        g = t0 + 1.5 + 0.05 * i
        p += ["%.3f cmd +grabright" % g, "%.3f cmd vr_mock_button main grip 1" % g,
              "%.3f cmd %s" % (t0 + 2.6, side.replace("+", "-")), "%.3f cmd -attack" % (t0 + 2.6),
              "%.3f main 0.100 1.600 -0.450 70 0 0" % (t0 + 2.65), "%.3f cmd echo STEP fg %d" % (t0 + 2.95, i + 1),
              "%.3f cmd vr_carry_check" % (t0 + 3.0), "%.3f cmd -grabright" % (t0 + 3.1),
              "%.3f cmd vr_mock_button main grip 0" % (t0 + 3.1), "%.3f cmd sv_gravity 800" % (t0 + 3.1)]
    p.sort(key=lambda line: float(line.split()[0]))
    return p


def forcegrab_cfg(tries, agent):
    path = os.path.join(KIT, "bases", agent, "qbase", "id1", "gripgap_forcegrab.txt")
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(forcegrab_play(tries)) + "\n")
    frames = int((4.0 * tries + 1.0) * 250)  # (the take runs one server frame a frame: more than enough)
    return ["developer 1", *NO_SHAKE, EXTRA, OFF_AWAY, hand("main", 0.10), *waits(20),
            "vr_mock_play " + path.replace("\\", "/"), *waits(frames)]


def run(agent, name, cfg):
    game = os.path.join(KIT, "bases", agent, "qbase", "id1")
    with open(os.path.join(game, name + ".cfg"), "w", newline="\n") as f:
        f.write("\n".join(cfg) + "\n")
    script = "map vrfiringrange;wait60;god;notarget;exec %s.cfg;toggleconsole;quit" % name
    out = subprocess.run([BASH, KIT + "/run.sh", agent, "-Timeout", "600", "-Script", script, "-Filter",
                          "STEP|carry check|carry: (one|both|taken)|force grab: caught|rror"],
                         capture_output=True, text=True).stdout
    return out


def verdict(out):
    # (Console lines are wrapped: join each check's lines first.)
    text = re.sub(r"\n(?!STEP|carry|force|exit)", "", out)
    fails, steps, label = [], 0, "?"
    for line in text.splitlines():
        if line.startswith("STEP"):
            label = line[5:].strip()
            continue
        m = re.match(r"carry check: (\w+) hand holds \d+ .*?in the hand, drawn ([-\d.]+) cm ([-\d.]+) deg off the "
                     r"physical; fist gap drawn ([-\d.]+) cm", line)
        if m:
            steps += 1
            apart, turn, gap = float(m.group(2)), float(m.group(3)), float(m.group(4))
            ok = apart <= TOL_IN_HAND and turn <= TOL_TURN and gap <= TOL_GAP
            print("%-4s %-34s %-4s hand: in the hand %5.2f cm %4.1f deg, fist gap %6.2f cm" %
                  ("ok" if ok else "FAIL", label, m.group(1), apart, turn, gap))
            if not ok:
                fails.append(label)
    return steps, fails


def main():
    agent = sys.argv[1]
    which = sys.argv[2] if len(sys.argv) > 2 else "both"
    count = int(sys.argv[3]) if len(sys.argv) > 3 else 6
    total_fails = 0
    for name, cfg in (("gripgap_cycle", cycle_cfg(count)), ("gripgap_forcegrab", forcegrab_cfg(count, agent))):
        if which not in ("both", name.split("_")[1]):
            continue
        out = run(agent, name, cfg)
        if "rror" in out and "ENGINE ERROR" in out:
            print(out[-2000:])
        steps, fails = verdict(out)
        print("%s: %d checks, %d failed%s\n" % (name, steps, len(fails), "" if steps else " (no checks: did it run?)"))
        total_fails += len(fails) + (0 if steps else 1)
    sys.exit(1 if total_fails else 0)


if __name__ == "__main__":
    main()
