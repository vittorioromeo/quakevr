"""crawl_test.py <agent> -- crawling with the arms lying down (vr_prone_crawl; vr_physics_water.cpp VR_AfterCrawlMove,
vr_hull.cpp crouchSpeedMax's vr_prone_stick_scale; ROUND24.md, "Crawling with the arms") with the mock headset in
vrteleporters' crouching room, facing east (+x). Each case plays a scripted hand path (vr_mock_play, written to
<repo>/scratch/crawl_*.mock) and measures how far the player went along x (vr_prone_status):
  stroke    lying down, three reach-and-pull strokes (the hands in turn: ahead through the air, down to the floor, pulled
            back along it, lifted), both hands closed (+grabmain, +graboff): forward, at least 60 units;
  air       lying down, the hands reaching ahead and coming back through the air (never on the floor): under 2 units;
  pose      the empty off hand alone (vr_prone_crawl_hand, his note 2026-11-08): Closed: open still, gripping or its
            trigger held forward 20+; Open: open forward, gripping still; Either: both forward;
  push/side the off hand pushed ahead along the floor: back 20+; pulled left: right 15+, along x under 10;
  hover     the strokes 8 cm over the floor (the fist's lowest point over Arm Crawl Floor Height 1 and its 1.5): still;
  (The floor strokes at y 0.015: the fist's lowest point 0.7 units over the floor. The commands, not vr_mock_button:
  a mock press is a key the open console takes, the game never sees the grip.)
  stand     standing, the strokes' path: under 2 units;  crouch: crouching (the head at 0.9 m): under 2 units;
  synth     lying down, the strokes with vr_handsource_force 1 (the hands' motion made up by the game): under 2 units;
  off       lying down, vr_prone_crawl 0: under 2 units;
  stick     the stick full ahead lying down: vr_prone_stick_scale 0.25 of the crawl's 96 (22-26 u/s); standing, the
            stick's speed untouched (over 250).
Prints "crawl_test: PASS/FAIL <what>" and "crawl_test: N passed, M failed"; the run ends on its own "crawl done".
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qvrtest  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SCRATCH = os.path.join(ROOT, "scratch").replace("\\", "/")

STATUS = re.compile(r"prone: on (\d) lying (\d) box (\d+) eye (-?[\d.]+) limit (-?[\d.]+) height (\d+) speed (-?[\d.]+) "
                    r"velz (-?[\d.]+) ground (\d) crouchfits (\d) z (-?[\d.]+) x (-?[\d.]+)")
STEP = re.compile(r"crawl step (\w+)")
YLINE = re.compile(r"prone: y (-?[\d.]+)")

T = qvrtest.Tally("crawl_test")

# Tracking space: metres, +x right, +y up, -z ahead. The hands' floor (y 0.015) and their raised height (y 0.30).
FLOOR, RAISED, HIGH = 0.015, 0.30, 0.55  # (FLOOR: the fist's lowest point 0.7 units over the floor; 0.04 until 2026-11-08)
HOVER_Y = FLOOR + 0.08  # (8 cm over the stroke's floor: the knuckles clear of Arm Crawl Floor Height, 1 unit)
NEAR, FAR = -0.15, -0.65
PERIOD = 1.3


def stroke_keys(hand, x, t0, low, kind="pull"):
    """One stroke from t0: up at its start, through the air to where it goes down (to `low`), along the floor, up
    again. pull: down ahead, pulled back; push: down near, pushed ahead; side: down on the right, pulled left."""
    a, b = {"pull": ((x, FAR), (x, NEAR)), "push": ((x, NEAR), (x, FAR)), "side": ((x + 0.2, -0.4), (x - 0.3, -0.4))}[kind]
    return [(t0, hand, b[0], RAISED, b[1]), (t0 + 0.4, hand, a[0], RAISED, a[1]), (t0 + 0.6, hand, a[0], low, a[1]),
            (t0 + 1.1, hand, b[0], low, b[1]), (t0 + 1.3, hand, b[0], RAISED, b[1])]


def write_mock(name, low, hands=("main", "off"), kind="pull"):
    keys = []
    for i in range(3):
        hand = hands[i % len(hands)]
        keys += stroke_keys(hand, 0.2 if hand == "main" else -0.2, i * PERIOD, low, kind)
    # (the other hand rests raised, near, until its turn)
    keys = [(0.0, "off", -0.2, RAISED, NEAR), (0.0, "main", 0.2, RAISED, NEAR)] + keys
    path = "%s/crawl_%s.mock" % (SCRATCH, name)
    os.makedirs(SCRATCH, exist_ok=True)
    with open(path, "w") as f:
        for t, hand, x, y, z in sorted(keys, key=lambda k: k[0]):
            f.write("%.3f %s %.3f %.3f %.3f 0 0 0\n" % (t, hand, x, y, z))
    return path


STROKES = write_mock("stroke", FLOOR)
AIR = write_mock("air", HIGH)
# The off hand alone (empty: the pose rule, vr_prone_crawl_hand), and the ways a stroke goes.
OFF = write_mock("off", FLOOR, ("off",))
PUSH = write_mock("push", FLOOR, ("off",), "push")
SIDE = write_mock("side", FLOOR, ("off",), "side")
HOVER = write_mock("hover", HOVER_Y, ("off",))
PLAY_FRAMES = int((3 * PERIOD + 0.6) * 72)  # the play's length and the glide after it

ST = "vr_prone_status"
HANDS_REST = "vr_mock_hand main 0.2 %.2f %.2f;vr_mock_hand off -0.2 %.2f %.2f" % (RAISED, NEAR, RAISED, NEAR)
PLACE = ["setpos 480 -320 24 0 0 0", "wait5", "noclip 0", "wait5"]


def case(name, head, before=(), after=(), mock=STROKES):
    """Placed, the head at `head` (None: standing), settled; the status; the play; the status."""
    head_cmd = "vr_mock_hand head 0 %.2f 0" % head if head is not None else "vr_mock_hand head"
    return (PLACE + [head_cmd, HANDS_REST] + list(before) + ["wait30", "echo crawl step %s_a" % name, ST,
            "vr_mock_play " + mock, "wait%d" % PLAY_FRAMES, "echo crawl step %s_b" % name, ST] + list(after))


GRIP, UNGRIP = "+graboff", "-graboff"  # (the bound commands: a mock button press is a key the open console takes)
TRIG, UNTRIG = "+offhandattack", "-offhandattack"
SCRIPT = ";".join(["map vrteleporters", "wait60", "god", "notarget", "vr_crouch_height 36", "vr_prone 1",
                   "vr_prone_crawl 1", "vr_prone_stick_scale 0.25", "vr_prone_crawl_debug 1", "vr_prone_crawl_hand 1",
                   "vr_prone_crawl_plant_height 1", "vr_prone_crawl_gain 2.5"] +
                  case("stroke", 0.4, [GRIP, "+grabmain"], [UNGRIP, "-grabmain"]) + case("air", 0.4, [GRIP], [UNGRIP], mock=AIR) +
                  case("offopen", 0.4, mock=OFF) + case("offclosed", 0.4, [GRIP], [UNGRIP], mock=OFF) +
                  case("offfist", 0.4, [TRIG], [UNTRIG], mock=OFF) +
                  case("openmode", 0.4, ["vr_prone_crawl_hand 2"], mock=OFF) +
                  case("openmodeclosed", 0.4, [GRIP], [UNGRIP, "vr_prone_crawl_hand 0"], mock=OFF) +
                  case("either", 0.4, mock=OFF) + case("eitherclosed", 0.4, [GRIP], [UNGRIP, "vr_prone_crawl_hand 1"], mock=OFF) +
                  case("push", 0.4, [GRIP], [UNGRIP], mock=PUSH) + case("side", 0.4, [GRIP], [UNGRIP], mock=SIDE) +
                  case("hover", 0.4, [GRIP], [UNGRIP], mock=HOVER) +
                  case("stand", None, [GRIP], [UNGRIP]) + case("crouch", 0.9, [GRIP], [UNGRIP]) +
                  case("synth", 0.4, ["vr_handsource_force 1", GRIP], ["vr_handsource_force 0", UNGRIP]) +
                  case("off", 0.4, ["vr_prone_crawl 0", GRIP], ["vr_prone_crawl 1", UNGRIP]) +
                  PLACE + ["vr_mock_hand head 0 0.4 0", HANDS_REST, "wait30", "vr_mock_stick off 0 1", "wait60",
                           "echo crawl step stickprone", ST, "vr_mock_stick off 0 0", "wait30"] +
                  # (standing: the tunnel's mouth at x 552 stops a standing box: 12 frames at most speed, about 45 units)
                  PLACE + ["vr_mock_hand head", "wait30", "vr_mock_stick off 0 1", "wait12", "echo crawl step stickstand", ST,
                           "vr_mock_stick off 0 0",
                           "echo crawl done", "toggleconsole", "quit"])


def steps(text):
    out = {}
    step = last = None
    for line in text.splitlines():
        m = STEP.search(line)
        if m:
            step = m.group(1)
            continue
        m = STATUS.search(line)
        if m and step:
            g = [float(x) for x in m.groups()]
            out[step] = dict(lying=g[1], box=g[2], eye=g[3], speed=g[6], x=g[11], y=0.0)
            last = step
            step = None
            continue
        m = YLINE.search(line)
        if m and last:
            out[last]["y"] = float(m.group(1))
            last = None
    return out


def moved(s, name, axis="x"):
    a, b = s.get(name + "_a"), s.get(name + "_b")
    if not a or not b:
        T.check(False, "%s: no status lines" % name)
        return None, None
    return b[axis] - a[axis], a


def main():
    agent = sys.argv[1] if len(sys.argv) > 1 else "crawl"
    text = qvrtest.run_sh(agent, ["-Script", SCRIPT, "-Filter", "^(prone|crawl)|rror", "-QuitOn", "crawl done",
                                  "-Timeout", "240"])
    for line in text.splitlines():
        if line.startswith("crawl:") or "rror" in line:
            print(line)
    s = steps(text)
    for name, lying, lo, hi, what in (("stroke", 1, 60.0, 400.0, "three reach-and-pull strokes: forward 60+"),
                                      ("air", 1, -2.0, 2.0, "reaching ahead and back through the air: still"),
                                      ("offopen", 1, -2.0, 2.0, "the empty off hand open, Closed: still"),
                                      ("offclosed", 1, 20.0, 400.0, "the empty off hand gripping, Closed: forward"),
                                      ("offfist", 1, 20.0, 400.0, "the empty off hand, its trigger held, Closed: forward"),
                                      ("openmode", 1, 20.0, 400.0, "the empty off hand open, Open: forward"),
                                      ("openmodeclosed", 1, -2.0, 2.0, "the empty off hand gripping, Open: still"),
                                      ("either", 1, 20.0, 400.0, "the empty off hand open, Either: forward"),
                                      ("eitherclosed", 1, 20.0, 400.0, "the empty off hand gripping, Either: forward"),
                                      ("push", 1, -400.0, -20.0, "the hand pushed ahead along the floor: back"),
                                      ("hover", 1, -2.0, 2.0, "the strokes 8 cm over the floor: still"),
                                      ("stand", 0, -2.0, 2.0, "standing, the strokes: still"),
                                      ("crouch", 0, -2.0, 2.0, "crouching, the strokes: still"),
                                      ("synth", 1, -2.0, 2.0, "synthesized hand motion: still"),
                                      ("off", 1, -2.0, 2.0, "vr_prone_crawl 0: still")):
        dx, a = moved(s, name)
        if dx is None:
            continue
        T.check(a["lying"] == lying and lo <= dx <= hi,
                "%s: lying %d (want %d), moved %.1f units (%.0f..%.0f): %s" % (name, a["lying"], lying, dx, lo, hi, what))
    dy, a = moved(s, "side", "y")
    dxs, _ = moved(s, "side")
    if dy is not None:
        T.check(a["lying"] == 1 and dy <= -15.0 and abs(dxs) < 10.0,
                "side: the hand pulled left along the floor: right %.1f units (15+), along x %.1f (under 10)" % (-dy, dxs))
    p = s.get("stickprone")
    T.check(bool(p) and p["lying"] == 1 and 22.0 <= p["speed"] <= 26.0,
            "stick lying down: speed %.1f (0.25 of 96: 22-26)" % (p["speed"] if p else -1))
    p = s.get("stickstand")
    T.check(bool(p) and p["lying"] == 0 and p["speed"] > 250.0,
            "stick standing: speed %.1f (untouched: over 250)" % (p["speed"] if p else -1))
    return T.verdict()


if __name__ == "__main__":
    sys.exit(main())
