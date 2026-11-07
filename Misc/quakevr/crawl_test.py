"""crawl_test.py <agent> -- crawling with the arms lying down (vr_prone_crawl; vr_physics_water.cpp VR_AfterCrawlMove,
vr_hull.cpp crouchSpeedMax's vr_prone_stick_scale; ROUND24.md, "Crawling with the arms") with the mock headset in
vrteleporters' crouching room, facing east (+x). Each case plays a scripted hand path (vr_mock_play, written to
<repo>/scratch/crawl_*.mock) and measures how far the player went along x (vr_prone_status):
  stroke    lying down, three reach-and-pull strokes (the hands in turn: ahead through the air, down to the floor, pulled
            back along it, lifted): forward, at least 60 units;
  air       lying down, the hands reaching ahead and coming back through the air (never on the floor): under 2 units;
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

T = qvrtest.Tally("crawl_test")

# Tracking space: metres, +x right, +y up, -z ahead. The hands' floor (y 0.04) and their raised height (y 0.30).
FLOOR, RAISED, HIGH = 0.04, 0.30, 0.55
NEAR, FAR = -0.15, -0.65
PERIOD = 1.3


def stroke_keys(hand, x, t0, low):
    """One stroke from t0: up near, ahead through the air, down (to `low`), pulled back, up again."""
    return [(t0, hand, x, RAISED, NEAR), (t0 + 0.4, hand, x, RAISED, FAR), (t0 + 0.6, hand, x, low, FAR),
            (t0 + 1.1, hand, x, low, NEAR), (t0 + 1.3, hand, x, RAISED, NEAR)]


def write_mock(name, low):
    keys = []
    for i in range(3):
        keys += stroke_keys("main" if i % 2 == 0 else "off", 0.2 if i % 2 == 0 else -0.2, i * PERIOD, low)
    # (the other hand rests raised, near, until its turn)
    keys = [(0.0, "off", -0.2, RAISED, NEAR)] + keys
    path = "%s/crawl_%s.mock" % (SCRATCH, name)
    os.makedirs(SCRATCH, exist_ok=True)
    with open(path, "w") as f:
        for t, hand, x, y, z in sorted(keys, key=lambda k: k[0]):
            f.write("%.3f %s %.3f %.3f %.3f 0 0 0\n" % (t, hand, x, y, z))
    return path


STROKES = write_mock("stroke", FLOOR)
AIR = write_mock("air", HIGH)
PLAY_FRAMES = int((3 * PERIOD + 0.6) * 72)  # the play's length and the glide after it

ST = "vr_prone_status"
HANDS_REST = "vr_mock_hand main 0.2 %.2f %.2f;vr_mock_hand off -0.2 %.2f %.2f" % (RAISED, NEAR, RAISED, NEAR)
PLACE = ["setpos 480 -320 24 0 0 0", "wait5", "noclip 0", "wait5"]


def case(name, head, before=(), after=(), mock=STROKES):
    """Placed, the head at `head` (None: standing), settled; the status; the play; the status."""
    head_cmd = "vr_mock_hand head 0 %.2f 0" % head if head is not None else "vr_mock_hand head"
    return (PLACE + [head_cmd, HANDS_REST] + list(before) + ["wait30", "echo crawl step %s_a" % name, ST,
            "vr_mock_play " + mock, "wait%d" % PLAY_FRAMES, "echo crawl step %s_b" % name, ST] + list(after))


SCRIPT = ";".join(["map vrteleporters", "wait60", "god", "notarget", "vr_crouch_height 36", "vr_prone 1",
                   "vr_prone_crawl 1", "vr_prone_stick_scale 0.25", "vr_prone_crawl_debug 1"] +
                  case("stroke", 0.4) + case("air", 0.4, mock=AIR) + case("stand", None) + case("crouch", 0.9) +
                  case("synth", 0.4, ["vr_handsource_force 1"], ["vr_handsource_force 0"]) +
                  case("off", 0.4, ["vr_prone_crawl 0"], ["vr_prone_crawl 1"]) +
                  PLACE + ["vr_mock_hand head 0 0.4 0", HANDS_REST, "wait30", "vr_mock_stick off 0 1", "wait60",
                           "echo crawl step stickprone", ST, "vr_mock_stick off 0 0", "wait30"] +
                  # (standing: the tunnel's mouth at x 552 stops a standing box: 12 frames at most speed, about 45 units)
                  PLACE + ["vr_mock_hand head", "wait30", "vr_mock_stick off 0 1", "wait12", "echo crawl step stickstand", ST,
                           "vr_mock_stick off 0 0",
                           "echo crawl done", "toggleconsole", "quit"])


def steps(text):
    out = {}
    step = None
    for line in text.splitlines():
        m = STEP.search(line)
        if m:
            step = m.group(1)
            continue
        m = STATUS.search(line)
        if m and step:
            g = [float(x) for x in m.groups()]
            out[step] = dict(lying=g[1], box=g[2], eye=g[3], speed=g[6], x=g[11])
            step = None
    return out


def moved(s, name):
    a, b = s.get(name + "_a"), s.get(name + "_b")
    if not a or not b:
        T.check(False, "%s: no status lines" % name)
        return None, None
    return b["x"] - a["x"], a


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
                                      ("stand", 0, -2.0, 2.0, "standing, the strokes: still"),
                                      ("crouch", 0, -2.0, 2.0, "crouching, the strokes: still"),
                                      ("synth", 1, -2.0, 2.0, "synthesized hand motion: still"),
                                      ("off", 1, -2.0, 2.0, "vr_prone_crawl 0: still")):
        dx, a = moved(s, name)
        if dx is None:
            continue
        T.check(a["lying"] == lying and lo <= dx <= hi,
                "%s: lying %d (want %d), moved %.1f units (%.0f..%.0f): %s" % (name, a["lying"], lying, dx, lo, hi, what))
    p = s.get("stickprone")
    T.check(bool(p) and p["lying"] == 1 and 22.0 <= p["speed"] <= 26.0,
            "stick lying down: speed %.1f (0.25 of 96: 22-26)" % (p["speed"] if p else -1))
    p = s.get("stickstand")
    T.check(bool(p) and p["lying"] == 0 and p["speed"] > 250.0,
            "stick standing: speed %.1f (untouched: over 250)" % (p["speed"] if p else -1))
    return T.verdict()


if __name__ == "__main__":
    sys.exit(main())
