"""lowceil_test.py <agent> -- standing up under a low ceiling (vr_lowceil; vr_hull.cpp updateCrouch's hold, vr_lowceil.cpp;
ROUND24.md, "Standing up under a low ceiling"; his notes vrteleporters_2026-11-10_01-54-06 and _01-54-40), with the mock
headset in vrteleporters' crouching room (scenes prone_vent and prone_tunnel).
The 20-high vent (lying down's 16 box passes, no crouch does; a teleporter in the wall over it, 24..56 over the floor):
  ventin     lain down (the head at 0.4 m) and crawled in: lying, box 16, not held, the view clear;
  venthold   the head up to standing in it: still lying (box 16), held (1: lie back down), the eyes past the ceiling
             (hidden), the view dark (vr_lowceil_dark's 0.9) and blurred, the message shown;
  ventstick  the stick ahead meanwhile: not moved, no speed;
  ventroom   the head stepped 0.6 m ahead in the room: not moved;
  venthands  the hands raised into the wall (the teleporter's place): not teleported;
  ventdown   the head back down: free, the view clear, no message;
  ventmove   the stick ahead: crawls on;
  control    vr_lowceil 0, the head and the hands up again: the hands take the teleporter (the hands' check is real;
             Quake's teleporter pushes him on north from 0 -160).
The 40-high tunnel (the 36 box passes):
  tunin      crouched (the head at 1.0 m: eyes 33.5) and walked in: box 36, not held;
  tunhold    the head up to standing: box 36 kept, held (2: crouch back down), hidden, dark, message;
  tunstick   the stick ahead meanwhile: not moved;
  tunroom    the head stepped 0.6 m ahead in the room: not moved;
  tunhyst    the head down to 1.1 m (eyes over the let-go line, 35): still held;
  tunfree    the head at 1.0 m: free, clear, no message;
  tunmove    the stick ahead: walks on.
Prints "lowceil_test: PASS/FAIL <what>" and "lowceil_test: N passed, M failed"; the run ends on its own "lowceil done".
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qvrtest  # noqa: E402

PRONE = re.compile(r"prone: on (\d) lying (\d) box (\d+) eye (-?[\d.]+) limit (-?[\d.]+) height (\d+) speed (-?[\d.]+) "
                   r"velz (-?[\d.]+) ground (\d) crouchfits (\d) z (-?[\d.]+) x (-?[\d.]+)")
PRONE2 = re.compile(r"prone: y (-?[\d.]+) stuck (\d) water (\d)")
LOW = re.compile(r"lowceil: hold (\d) hidden (\d) dark (-?[\d.]+) blur (-?[\d.]+) message (\d) eye (-?[\d.]+) "
                 r"ceiling (-?[\d.]+) top (-?[\d.]+)")
STEP = re.compile(r"lowceil step (\w+)")

T = qvrtest.Tally("lowceil_test")

ST = "vr_prone_status;vr_lowceil_status"
HANDS_LOW = "vr_mock_hand main 0.2 0.15 -0.45;vr_mock_hand off -0.2 0.15 -0.45"
HANDS_UP = "vr_mock_hand main 0.15 1.5 -0.3;vr_mock_hand off -0.15 1.5 -0.3"
HANDS_BACK = "vr_mock_hand main;vr_mock_hand off"
UP = "vr_mock_hand head"  # (standing: eyes 55.5 over the feet)
STEP_AHEAD = "vr_mock_hand head 0 1.7 -0.6"  # (standing, 0.6 m ahead in the room)


def step(name):
    return ["echo lowceil step " + name, ST]


SCRIPT = ";".join(
    ["map vrteleporters", "wait60", "god", "notarget", "vr_crouch_height 36", "vr_prone 1", "vr_prone_stick_scale 1",
     "vr_lowceil 1", "setpos 480 -112 24 0 0 0", "wait5", "noclip 0", "wait5",
     # The vent: lain down, crawled in, then put in its middle (under the teleporter in the wall over it).
     "vr_mock_hand head 0 0.4 0", HANDS_LOW, "wait20", "vr_mock_stick off 0 1", "wait110", "vr_mock_stick off 0 0",
     "wait40", "setpos 618 -112 24 0 0 0", "wait5", "noclip 0", "wait5"] + step("ventin") +
    [UP, "wait30"] + step("venthold") + ["screenshot"] +
    ["vr_mock_stick off 0 1", "wait60"] + step("ventstick") + ["vr_mock_stick off 0 0", "wait5",
     STEP_AHEAD, "wait30"] + step("ventroom") + [UP, "wait10",
     HANDS_UP, "wait30"] + step("venthands") + [HANDS_LOW, "wait5",
     "vr_mock_hand head 0 0.4 0", "wait30"] + step("ventdown") +
    ["vr_mock_stick off 0 1", "wait30", "vr_mock_stick off 0 0", "wait5"] + step("ventmove") +
    ["setpos 618 -112 24 0 0 0", "wait5", "noclip 0", "wait5", "vr_lowceil 0", UP, "wait10", HANDS_UP, "wait30"] + step("control") +
    [HANDS_BACK, "vr_lowceil 1",
     # The tunnel: crouched, walked in.
     "vr_mock_hand head 0 1.0 0", "setpos 480 -320 24 0 0 0", "wait5", "noclip 0", "wait5", "vr_mock_stick off 0 1", "wait60",
     "vr_mock_stick off 0 0", "setpos 620 -320 24 0 0 0", "wait5", "noclip 0", "wait5"] + step("tunin") +
    [UP, "wait30"] + step("tunhold") +
    ["vr_mock_stick off 0 1", "wait60"] + step("tunstick") + ["vr_mock_stick off 0 0", "wait5",
     STEP_AHEAD, "wait30"] + step("tunroom") + [UP, "wait10",
     "vr_mock_hand head 0 1.1 0", "wait30"] + step("tunhyst") +
    ["vr_mock_hand head 0 1.0 0", "wait30"] + step("tunfree") +
    ["vr_mock_stick off 0 1", "wait30", "vr_mock_stick off 0 0", "wait5"] + step("tunmove") +
    ["echo lowceil done", "toggleconsole", "quit"])


def parse(text):
    out = {}
    name = None
    for line in text.splitlines():
        m = STEP.search(line)
        if m:
            name = m.group(1)
            continue
        if not name:
            continue
        m = PRONE.search(line)
        if m:
            g = [float(x) for x in m.groups()]
            out.setdefault(name, {}).update(lying=g[1], box=g[2], eye=g[3], speed=g[6], x=g[11])
            continue
        m = PRONE2.search(line)
        if m:
            out.setdefault(name, {}).update(y=float(m.group(1)), stuck=int(m.group(2)))
            continue
        m = LOW.search(line)
        if m:
            g = [float(x) for x in m.groups()]
            out.setdefault(name, {}).update(hold=int(g[0]), hidden=int(g[1]), dark=g[2], blur=g[3], message=int(g[4]),
                                            ceiling=g[6], top=g[7])
    return out


def main():
    agent = sys.argv[1] if len(sys.argv) > 1 else "lowceil"
    text = qvrtest.run_sh(agent, ["-Clean", "-Out", "lowceil.png", "-Script", SCRIPT, "-Filter", "^(prone|lowceil)|rror", "-QuitOn", "lowceil done",
                                  "-Timeout", "180"])
    s = parse(text)

    def got(name, key="hold"):
        p = s.get(name)
        if not p or key not in p or "x" not in p:
            T.check(False, "%s: no status line" % name)
            return None
        return p

    def held(p, kind):
        return p["hold"] == kind and p["hidden"] == 1 and p["dark"] >= 0.85 and p["blur"] > 0.0 and p["message"] == 1

    def clear(p):
        return p["hold"] == 0 and p["dark"] == 0.0 and p["blur"] == 0.0 and p["message"] == 0

    def desc(p):
        return "hold %d hidden %d dark %.2f blur %.4f message %d lying %d box %d eye %.1f ceiling %.1f x %.1f y %.1f" % (
            p["hold"], p["hidden"], p["dark"], p["blur"], p["message"], p["lying"], p["box"], p["eye"], p["ceiling"],
            p["x"], p.get("y", 0))

    # The vent.
    p0 = got("ventin")
    if p0:
        T.check(p0["lying"] == 1 and p0["box"] == 16 and clear(p0), "ventin: lying in the vent, free: " + desc(p0))
    p = got("venthold")
    if p and p0:
        T.check(p["lying"] == 1 and p["box"] == 16 and held(p, 1) and abs(p["x"] - p0["x"]) < 1.0,
                "venthold: up in the vent: kept lying, held (lie back down), dark, message: " + desc(p))
    for name, what in (("ventstick", "the stick"), ("ventroom", "a room-scale step"), ("venthands", "the hands in the wall")):
        p = got(name)
        if p and p0:
            T.check(held(p, 1) and abs(p["x"] - p0["x"]) < 1.0 and abs(p["y"] + 112.0) < 1.0 and p["speed"] < 1.0,
                    "%s: %s moved him nowhere, not teleported: %s" % (name, what, desc(p)))
    p = got("ventdown")
    if p:
        T.check(p["lying"] == 1 and clear(p), "ventdown: back down: free, clear: " + desc(p))
    p = got("ventmove")
    if p and p0:
        T.check(p["x"] > p0["x"] + 10.0, "ventmove: crawls on (x %.1f, from %.1f)" % (p["x"], p0["x"]))
    p = got("control")
    if p:
        T.check(abs(p["x"]) < 40.0 and -200.0 < p["y"] < 0.0,
                "control: vr_lowceil 0, the hands up take the teleporter (to 0 -160, pushed on north): " + desc(p))
    # The tunnel.
    p0 = got("tunin")
    if p0:
        T.check(p0["lying"] == 0 and p0["box"] == 36 and clear(p0), "tunin: crouched in the tunnel, free: " + desc(p0))
    p = got("tunhold")
    if p and p0:
        T.check(p["box"] == 36 and held(p, 2) and abs(p["x"] - p0["x"]) < 1.0,
                "tunhold: up in the tunnel: box 36 kept, held (crouch back down), dark, message: " + desc(p))
    for name, what in (("tunstick", "the stick"), ("tunroom", "a room-scale step")):
        p = got(name)
        if p and p0:
            T.check(held(p, 2) and abs(p["x"] - p0["x"]) < 1.0 and p["speed"] < 1.0,
                    "%s: %s moved him nowhere: %s" % (name, what, desc(p)))
    p = got("tunhyst")
    if p:
        T.check(p["hold"] == 2, "tunhyst: a little lower, over the let-go line: still held: " + desc(p))
    p = got("tunfree")
    if p:
        T.check(p["box"] == 36 and clear(p), "tunfree: back down: free, clear: " + desc(p))
    p = got("tunmove")
    if p and p0:
        T.check(p["x"] > p0["x"] + 10.0, "tunmove: walks on (x %.1f, from %.1f)" % (p["x"], p0["x"]))
    stuck = [k for k, v in s.items() if v.get("stuck", 0) != 0]
    T.check(not stuck, "never stuck: %s" % (", ".join(sorted(stuck)) or "none"))
    return T.verdict()


if __name__ == "__main__":
    sys.exit(main())
