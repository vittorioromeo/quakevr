"""prone_test.py <agent> -- lying down and crawling (vr_prone; vr_hull.cpp updateCrouch, vr_avatar_pitch.cpp; ROUND24.md,
"Lying down and crawling" and "A lower box for lying down") in the game, with the mock headset (the head put at 0.4 m:
eyes 14.6 units over the feet) and then flat-screen (-flatscreenmode: vr_prone_toggle, the prone key's toggle), the same
server and body code. In vrteleporters' crouching room (teleporters/crouch_test.sh's places: the 40-high tunnel east of
x 552 to 688):
  stand     standing: not lying, box 0 (standing), the body upright;
  down      lying down: box 16 (vr_prone_height, the lying box's own), the body's pitch eased to lying down's (about 90
            or more);
  jump      +jump lying down: no jump (no upward speed, still on the ground, the same height);
  crawl     the stick (VR) / +forward (flat) east: the crawl's speed 0.3 of 320 (90-100), into the tunnel;
  rise      the head up (VR) / the toggle (flat) in the tunnel: up to the crouched 36 box, not standing (no room);
  ceiling   with vr_crouch_height 44 (no crouched box fits the 40 tunnel) the head up: still lying, box 16 (the ceiling
            keeps him down; flat: his eyes held under its top, 12.5 or lower);
  out       crawled on east out of the tunnel: standing again (box 0);
  off       vr_prone 0, the head low (VR) / the toggle (flat): crouched (box 36), never lying.
The same room's 20-high vent (y -144..-80: the lying box passes, the lowest crouched 24 doesn't; shells in it, and over
it, in the wall 24..56 up, a teleporter to the hub that only a standing box's top would reach):
  ventin    lain down in front of it and crawled in: lying, box 16, inside (x 576-672), not teleported, the shells
            picked up crawling (flat: a headset picks ammo up by hand);
  ventrise  the head up (VR) / the toggle (flat) in it: still lying (no crouched box fits), box 16 (flat: the view held
            under the box's top, 12.5 or lower);
  ventoff   vr_prone 0 in it: still the lying box (getting up needs room);
  ventout   crawled on east out of it: standing (box 0), past x 690.
Every status line: not stuck (the box as it is starts in no solid: getting up never puts him into the ceiling).
Screenshots (side, vr_body_debug 3, and first person looking down) lying down, composed into scratch/prone_vr.png and
scratch/prone_flat.png. Prints "prone_test: PASS/FAIL <what>" and "prone_test: N passed, M failed"; each run ends on its
own "prone done".
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qvrtest  # noqa: E402

STATUS = re.compile(r"prone: on (\d) lying (\d) box (\d+) eye (-?[\d.]+) limit (-?[\d.]+) height (\d+) speed (-?[\d.]+) "
                    r"velz (-?[\d.]+) ground (\d) crouchfits (\d) z (-?[\d.]+) x (-?[\d.]+)")
STATUS2 = re.compile(r"prone: y (-?[\d.]+) stuck (\d) water (\d)")  # (its own line: one line wraps in the log)
PITCH = re.compile(r"body_pitch: angle (-?[\d.]+) target (-?[\d.]+) rate (-?[\d.]+) source (\w+)")
STEP = re.compile(r"prone step (\w+)")
PICKUP = re.compile(r"You got the shells")
BOX = 16  # vr_prone_height's default

T = qvrtest.Tally("prone_test")

ST = "vr_prone_status;vr_body_pitch"
SHOTS = "vr_body_debug 3;wait3;screenshot;vr_body_debug 0;%s;wait3;screenshot;%s"
# (VR: the hands held out ahead and low, as on the elbows; the standing pose's again after)
HANDS = "vr_mock_hand main 0.2 0.15 -0.45;vr_mock_hand off -0.2 0.15 -0.45"
HANDS_BACK = "vr_mock_hand main;vr_mock_hand off"
# (vr_prone_stick_scale 1: the stick's whole crawl, the box's and the speed's checks; crawl_test.py tests the arms and the scale)
START = ["map vrteleporters", "wait60", "god", "notarget", "vr_crouch_height 36", "vr_prone 1", "vr_prone_stick_scale 1",
         "setpos 480 -320 24 0 0 0", "wait5", "noclip 0", "wait5"]
# The vent: in front of it, facing east (the shells' pickup line comes between ventin's crawl and its status line).
VENT = ["vr_crouch_height 36", "vr_prone 1", "setpos 480 -112 24 0 0 0", "wait5", "noclip 0", "wait5"]

VR = ";".join(START + [
    "echo prone step stand", ST,
    "vr_mock_hand head 0 0.4 0", "wait5", "echo prone step down0", ST,
    "wait80", "echo prone step down", ST, HANDS, SHOTS % ("vr_mock_look 50 0", "vr_mock_look 0 0"), HANDS_BACK,
    "+jump", "wait10", "echo prone step jump", ST, "-jump",
    "vr_mock_stick off 0 1", "wait40", "echo prone step crawl", ST, "wait80", "vr_mock_stick off 0 0", "wait5",
    "echo prone step inside", ST,
    "vr_mock_hand head", "wait5", "echo prone step rise", ST,
    "vr_mock_hand head 0 0.4 0", "wait5", "vr_crouch_height 44", "wait5", "vr_mock_hand head", "wait5",
    "echo prone step ceiling", ST,
    "vr_mock_hand head 0 0.4 0", "vr_mock_stick off 0 1", "wait140", "vr_mock_stick off 0 0", "vr_mock_hand head", "wait5",
    "echo prone step out", ST,
    "vr_crouch_height 36", "vr_prone 0", "setpos 480 -320 24 0 0 0", "wait5", "noclip 0", "vr_mock_hand head 0 0.4 0", "wait5",
    "echo prone step off", ST, "vr_prone 1", "vr_mock_hand head", "wait5"] + VENT + [
    # (the hands held low ahead, as on the elbows: a hand reaching up into the wall over the vent touches the teleporter
    # there, as hands touch triggers anywhere)
    "vr_mock_hand head 0 0.4 0", HANDS, "wait20", "echo prone step ventcrawl",
    "vr_mock_stick off 0 1", "wait110", "vr_mock_stick off 0 0", "wait5", "echo prone step ventin", ST,
    "vr_mock_hand head", "wait5", "echo prone step ventrise", ST,
    "vr_prone 0", "wait5", "echo prone step ventoff", ST, "vr_prone 1",
    # (back down to crawl out: up in the vent holds him where he is, vr_lowceil; lowceil_test.py), up again once out
    "vr_mock_hand head 0 0.4 0", "wait5",
    "vr_mock_stick off 0 1", "wait140", "vr_mock_stick off 0 0", "vr_mock_hand head", "wait5", "echo prone step ventout", ST,
    "echo prone done", "toggleconsole", "quit"])

FLAT = ";".join(START + [
    "echo prone step stand", ST,
    "vr_prone_toggle", "wait5", "echo prone step down0", ST,
    "wait80", "echo prone step down", ST, SHOTS % ("centerview;+lookdown;wait20;-lookdown", "centerview"),
    "+jump", "wait10", "echo prone step jump", ST, "-jump",
    "+forward", "wait40", "echo prone step crawl", ST, "wait80", "-forward", "wait5", "echo prone step inside", ST,
    "vr_prone_toggle", "wait60", "echo prone step rise", ST,
    "vr_prone_toggle", "wait60", "vr_crouch_height 44", "wait5", "vr_prone_toggle", "wait60",
    "echo prone step ceiling", ST,
    "+forward", "wait140", "-forward", "wait60", "echo prone step out", ST,
    "vr_crouch_height 36", "vr_prone 0", "setpos 480 -320 24 0 0 0", "wait5", "noclip 0", "wait5", "vr_prone_toggle", "+flatcrouch", "wait40",
    "echo prone step off", ST, "-flatcrouch", "vr_prone 1", "wait60"] + VENT + [
    "vr_prone_toggle", "wait60", "echo prone step ventcrawl",
    "+forward", "wait110", "-forward", "wait5", "echo prone step ventin", ST,
    "vr_prone_toggle", "wait60", "echo prone step ventrise", ST,
    "vr_prone 0", "wait5", "echo prone step ventoff", ST, "vr_prone 1",
    "+forward", "wait140", "-forward", "wait60", "echo prone step ventout", ST,
    "echo prone done", "toggleconsole", "quit"])


def steps(text):
    out = {}
    step = None
    for line in text.splitlines():
        m = STEP.search(line)
        if m:
            step = m.group(1)
            continue
        if PICKUP.search(line) and step:
            out.setdefault(step, {})["pickup"] = True
            continue
        m = STATUS.search(line)
        if m and step:
            g = [float(x) for x in m.groups()]
            out.setdefault(step, {}).update(on=g[0], lying=g[1], box=g[2], eye=g[3], limit=g[4], height=g[5], speed=g[6],
                                            velz=g[7], ground=g[8], fits=g[9], z=g[10], x=g[11])
            continue
        m = STATUS2.search(line)
        if m and step:
            g = [float(x) for x in m.groups()]
            out.setdefault(step, {}).update(y=g[0], stuck=g[1], water=g[2])
            continue
        m = PITCH.search(line)
        if m and step:
            out.setdefault(step, {}).update(angle=float(m.group(1)), target=float(m.group(2)), source=m.group(4))
    return out


def check_mode(mode, s):
    def got(step, key="lying"):
        if step not in s or key not in s[step]:
            T.check(False, "%s %s: no status line" % (mode, step))
            return None
        return s[step]

    p = got("stand")
    if p:
        T.check(p["lying"] == 0 and p["box"] == 0 and p.get("angle", -1) == 0.0,
                "%s stand: lying %d box %d body angle %.2f (0, 0, 0)" % (mode, p["lying"], p["box"], p.get("angle", -1)))
    p = got("down0")
    if p and mode == "vr":
        T.check(p["lying"] == 1 and p["box"] == BOX and p["eye"] < p["limit"],
                "%s down at once: lying %d box %d eye %.1f (under %.1f)" % (mode, p["lying"], p["box"], p["eye"], p["limit"]))
    p = got("down")
    if p:
        T.check(p["lying"] == 1 and p["box"] == BOX and p["height"] == BOX and p["eye"] < p["limit"],
                "%s down: lying %d box %d (%d) eye %.1f (under %.1f)" %
                (mode, p["lying"], p["box"], BOX, p["eye"], p["limit"]))
        T.check(p.get("source") == "prone" and p.get("target", 0) > 45.0 and abs(p.get("angle", 0) - p.get("target", 999)) < 6.0,
                "%s down: body %s angle %.2f target %.2f (lying down's: over 45, on its target)" %
                (mode, p.get("source"), p.get("angle", 0), p.get("target", 0)))
        z0 = p["z"]
        q = got("jump")
        if q:
            T.check(q["lying"] == 1 and q["velz"] <= 0.5 and q["ground"] == 1 and abs(q["z"] - z0) < 1.0,
                    "%s jump: velz %.1f ground %d z %.2f (was %.2f): no jump" % (mode, q["velz"], q["ground"], q["z"], z0))
    p = got("crawl")
    if p:
        T.check(p["lying"] == 1 and 85.0 <= p["speed"] <= 100.0,
                "%s crawl: speed %.1f (0.3 of 320: 85-100)" % (mode, p["speed"]))
    p = got("inside")
    if p:
        T.check(p["lying"] == 1 and 560.0 < p["x"] < 688.0, "%s inside: x %.1f (in the 40-high tunnel, 552-688)" % (mode, p["x"]))
    p = got("rise")
    if p:
        T.check(p["lying"] == 0 and p["box"] == 36, "%s rise in the tunnel: lying %d box %d (36: crouched, no room to stand)" %
                (mode, p["lying"], p["box"]))
    p = got("ceiling")
    if p:
        ok = p["lying"] == 1 and p["box"] == BOX and p["fits"] == 0
        if mode == "flat":
            ok = ok and p["eye"] <= BOX - 3.5
        T.check(ok, "%s ceiling (no crouched box fits): lying %d box %d crouchfits %d eye %.1f (kept down)" %
                (mode, p["lying"], p["box"], p["fits"], p["eye"]))
    p = got("out")
    if p:
        T.check(p["lying"] == 0 and p["box"] == 0 and p["x"] > 688.0,
                "%s out of the tunnel: lying %d box %d x %.1f (standing)" % (mode, p["lying"], p["box"], p["x"]))
    p = got("off")
    if p:
        T.check(p["on"] == 0 and p["lying"] == 0 and p["box"] == 36,
                "%s vr_prone 0, low: on %d lying %d box %d (crouched 36)" % (mode, p["on"], p["lying"], p["box"]))
    # The vent (20 high): the lying box's own height.
    p = got("ventin")
    if p:
        T.check(p["lying"] == 1 and p["box"] == BOX and 576.0 < p["x"] < 672.0 and abs(p["y"] + 112.0) < 4.0,
                "%s vent: crawled in: lying %d box %d x %.1f y %.1f (576-672, -112: in the vent, not teleported)" %
                (mode, p["lying"], p["box"], p["x"], p["y"]))
        if mode == "flat":  # (in a headset, ammo boxes are picked up by hand: carried things)
            T.check(s.get("ventcrawl", {}).get("pickup", False) or p.get("pickup", False),
                    "%s vent: the shells picked up crawling" % mode)
    p = got("ventrise")
    if p:
        ok = p["lying"] == 1 and p["box"] == BOX and p["fits"] == 0
        if mode == "flat":
            ok = ok and p["eye"] <= BOX - 3.5
        T.check(ok, "%s vent: up in it: lying %d box %d crouchfits %d eye %.1f (kept down: no crouch fits)" %
                (mode, p["lying"], p["box"], p["fits"], p["eye"]))
    p = got("ventoff")
    if p:
        T.check(p["on"] == 0 and p["box"] == BOX, "%s vent: vr_prone 0 in it: on %d box %d (still the lying box)" %
                (mode, p["on"], p["box"]))
    p = got("ventout")
    if p:
        T.check(p["lying"] == 0 and p["box"] == 0 and p["x"] > 690.0,
                "%s vent: crawled out: lying %d box %d x %.1f (standing)" % (mode, p["lying"], p["box"], p["x"]))
    stuck = [k for k, v in s.items() if v.get("stuck", 0) != 0]
    T.check(not stuck, "%s never stuck (the box as it is in no solid): %s" % (mode, ", ".join(sorted(stuck)) or "none"))


def main():
    agent = sys.argv[1] if len(sys.argv) > 1 else "prone"
    for mode, extra, script in (("vr", [], VR), ("flat", ["-ExtraArgs", "-flatscreenmode"], FLAT)):
        text = qvrtest.run_sh(agent, extra + ["-Clean", "-Out", "prone_%s.png" % mode, "-Script", script,
                                              "-Filter", "^(prone|body_pitch|composed|You got)|rror", "-QuitOn",
                                              "prone done", "-Timeout", "180"])
        for line in text.splitlines():
            if line.startswith("composed") or "rror" in line:
                print(line)
        check_mode(mode, steps(text))
    return T.verdict()


if __name__ == "__main__":
    sys.exit(main())
