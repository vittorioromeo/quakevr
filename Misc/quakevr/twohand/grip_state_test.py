#!/usr/bin/env python3
"""grip_state_test.py <agent> [--keep]

Two-handed grip state across weapon changes (ROUND21.md, "Grips reset with the weapon"; NOTES.md
vrfiringrange_2026-10-01_22-40-50): a sword held by its blade, both hands on it, let go of; then a shotgun, a crowbar
and the shotgun again, each gripped by its foregrip, its blade or anywhere, one swapped while the other hand still grips
it. No gun may be held "by its blade" (vr_dumpview), and the helping hand on the shotgun's pump is drawn as it is
without a sword before (a control run). Both ways round: the main hand holding, then the off hand.

Taking a carried sword back (NOTES.md vrfiringrange_2026-10-02_00-32-29, _00-34-04): the other hand on its blade, the
holding hand lets go (it stays in the other), takes the blade by that hand, both swing it (vr_mock_swing_both) and let
go; the hand closes on the handle and holds the sword by it again: with the grip alone, then with the trigger pulled
with it (a fist: the force grab's branch took that grip, and nothing happened).

Flick reload (NOTES.md vrfiringrange_2026-10-02_01-08-19): the double-barrelled shotgun flick-reloads in one hand and
with the other hand on its cup hotspot, not with it on its barrel's grip or anywhere on it.

Runs e1m1 (id1) in the kit's mock (about 10 s a run, six runs). Exit status 1 on a failure.
"""
import os
import re
import subprocess
import sys

KIT = os.environ.get("KIT", "C:/OHWorkspace/qvr-kit")
BASH = os.environ.get("QVR_BASH", "C:/Program Files/Git/bin/bash.exe")  # (not WSL's)

SETUP = "map e1m1;wait60;developer 1;vr_debug_2h_grip 1;vr_weapon_grip_mode 0;vr_weapon_grab_anywhere 1;"
# Per holding hand: its key and grip button, the other hand's, the mock poses, the drawn helping hand's vr_dumpview row
# (hand_rig: 4 the off hand, 10 the main hand) and the impulse base (150 the main hand, 170 the off hand).
HANDS = {
    "main": dict(grab="right", other="off", ograb="left", pose="vr_mock_hand main 0.15 1.2 -0.45 70 0 0;vr_mock_hand off -0.1 1.0 -0.3;",
                 row=4, base=150),
    "off": dict(grab="left", other="main", ograb="right", pose="vr_mock_hand off -0.15 1.2 -0.45 70 0 0;vr_mock_hand main 0.1 1.0 -0.3;",
                row=10, base=170),
}
SWORD, SHOTGUN, CROWBAR, SUPER_SHOTGUN = 13, 4, 17, 5
PUMP, BLADE, ANYWHERE = 0.75, 0.6, 0.4


def grip(h, frac, name):
    """The helping hand onto the holding hand's weapon at `frac` of the way to its tip, gripping; then a dump."""
    o = HANDS[h]["other"]
    return (f"echo === {name};vr_mock_hand_to {o} held {frac};wait5;vr_mock_hand_to {o} held {frac};wait5;"
            f"+grab{HANDS[h]['ograb']};vr_mock_button {o} grip 1;wait30;vr_dumpview;")


def let_go(hand, key):
    return f"-grab{key};vr_mock_button {hand} grip 0;wait30;"


def take(h, wid):
    d = HANDS[h]
    return f"{d['pose']}wait10;+grab{d['grab']};vr_mock_button {h} grip 1;impulse {d['base'] + wid};wait30;"


def chain(h):
    d = HANDS[h]
    o, og = d["other"], d["ograb"]
    s = SETUP + take(h, SWORD)
    s += grip(h, BLADE, "sword_blade")
    # The holding hand lets go (handed off to the hand on the blade), takes the blade too, then both let go.
    s += f"echo === sword_both;{let_go(h, d['grab'])}vr_mock_hand_to {h} held 0.5;wait5;vr_mock_hand_to {h} held 0.5;wait5;"
    s += f"+grab{d['grab']};vr_mock_button {h} grip 1;wait30;vr_dumpview;"
    s += let_go(h, d["grab"]) + let_go(o, og) + "wait30;"
    s += take(h, SHOTGUN) + grip(h, PUMP, "shotgun_pump") + let_go(o, og)
    # The crowbar into the gripping hand, the other hand on its blade; swapped for the shotgun while it still grips.
    s += f"impulse {d['base'] + CROWBAR};wait30;" + grip(h, BLADE, "crowbar_blade")
    s += f"echo === swap_gripped;impulse {d['base'] + SHOTGUN};wait30;vr_dumpview;" + let_go(o, og) + d["pose"] + "wait10;"
    s += grip(h, PUMP, "shotgun_pump2") + let_go(o, og) + d["pose"] + "wait10;"
    s += grip(h, ANYWHERE, "shotgun_anywhere")
    return s


def retake(h):
    """The sword carried by the other hand off its blade, taken back by its handle: twice (the second time the trigger
    pulled with the grip)."""
    d = HANDS[h]
    o, og = d["other"], d["ograb"]
    trig = "+attack;" if h == "main" else "+offhandattack;"
    s = SETUP + take(h, SWORD) + grip(h, BLADE, "retake_blade")
    for n, pull in (("retake_grip", ""), ("retake_trigger", trig)):
        s += let_go(h, d["grab"])  # (handed off to the other hand, on the blade)
        s += f"vr_mock_hand_to {h} held 0.45;wait5;vr_mock_hand_to {h} held 0.45;wait5;+grab{d['grab']};vr_mock_button {h} grip 1;wait30;"
        s += "vr_mock_swing_both 1;vr_mock_swing 0.3;wait120;vr_mock_swing 0;vr_mock_swing_both 0;wait10;" + let_go(h, d["grab"])
        s += f"echo === {n};vr_mock_hand_to {h} carried;wait5;vr_mock_hand_to {h} carried;wait5;{pull}"
        s += f"+grab{d['grab']};vr_mock_button {h} grip 1;wait30;{trig.replace('+', '-')}vr_dumpview;"
    return s


def flick():
    """The double-barrelled shotgun in the main hand, a shot fired, flicked (+flickreloadright): alone, the off hand on
    its barrel's grip (hotspot 0), on its cup (1), anywhere on it."""
    # (The classic flick reload: Break Open off. With it on, the flick breaks the gun open instead: reload_test.sh section 7.)
    s = SETUP + "god;give s 50;vr_reload_ssg_break 0;" + take("main", SUPER_SHOTGUN)
    fire, flk = "+attack;wait3;-attack;wait60;", "+flickreloadright;wait3;-flickreloadright;wait60;"
    off_on = lambda where: (f"vr_mock_hand_to off {where};wait5;vr_mock_hand_to off {where};wait5;+grableft;"
                            "vr_mock_button off grip 1;wait30;")
    s += f"echo === flick_one;{fire}vr_dumpview;{flk}vr_dumpview;"
    s += f"echo === flick_barrel;{off_on('heldspot 0')}{fire}vr_dumpview;{flk}vr_dumpview;" + let_go("off", "left")
    s += f"echo === flick_cup;{off_on('heldspot 1')}vr_dumpview;{flk}vr_dumpview;" + let_go("off", "left")
    s += f"echo === flick_anywhere;{fire}{off_on('held ' + str(ANYWHERE))}vr_dumpview;{flk}vr_dumpview;"
    return s


def clips(lines, hand):
    """The (weapon, clip, flick reload allowed) of each vr_dumpview of `hand`."""
    out = []
    for line in lines:
        m = re.match(rf"{hand} hand: two-handed .*weapon (\d+) clip (\d+), flick reload (allowed|not allowed)", line)
        if m:
            out.append((int(m.group(1)), int(m.group(2)), m.group(3) == "allowed"))
    return out


def control():
    s = SETUP
    for h in ("main", "off"):
        d = HANDS[h]
        s += take(h, SHOTGUN) + grip(h, PUMP, f"control_{h}") + let_go(d["other"], d["ograb"]) + let_go(h, d["grab"])
        s += f"impulse {d['base']};wait30;"  # (the fist: empty)
    return s


def run(agent, script):
    out = subprocess.run([BASH, f"{KIT}/run.sh", agent, "-Script", script + "toggleconsole;quit", "-Filter",
                          r"^=== |grips reset|by its blade|2h grip|^ ?(4|10) progs/hand_rig|hand: two-handed|rror"],
                         capture_output=True, text=True).stdout
    sections, name = {}, None
    for line in out.splitlines():
        m = re.match(r"=== (\w+)", line)
        if m:
            name = m.group(1)
            sections[name] = []
        elif name:
            sections[name].append(line)
    return out, sections


def drawn(lines, row):
    for line in lines:
        m = re.match(rf"\s*{row} progs/hand_rig\.mdl.*org \(([-\d. ]+)\) ang \(([-\d ]+)\)", line)
        if m:
            return [float(v) for v in m.group(1).split()], [float(v) for v in m.group(2).split()]
    return None


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    agent, keep = sys.argv[1], "--keep" in sys.argv
    fails = []

    def check(ok, what):
        print(f"  {'ok' if ok else 'FAIL'}: {what}")
        if not ok:
            fails.append(what)

    out, ctl = run(agent, control())
    for h in ("main", "off"):
        print(f"== {h} hand holding")
        text, sec = run(agent, chain(h))
        if keep:
            open(f"grip_state_{h}.txt", "w").write(text)
        blade = lambda n: any("by its blade" in l for l in sec.get(n, []))
        row = HANDS[h]["row"]
        ref = drawn(ctl.get(f"control_{h}", []), row)
        check(blade("sword_blade"), "sword: held two-handed by its blade")
        check(any("both hands hold it" in l for l in sec.get("sword_both", [])), "sword: both hands on the carried blade")
        for n in ("shotgun_pump", "swap_gripped", "shotgun_pump2", "shotgun_anywhere"):
            check(n in sec and not blade(n), f"{n}: not held by a blade")
        check(any("grips reset" in l for l in sec.get("swap_gripped", [])), "swap while gripped: the grips reset")
        check(any("took the weapon anywhere" in l for l in sec.get("shotgun_anywhere", [])), "shotgun: taken anywhere")
        for n in ("shotgun_pump", "shotgun_pump2"):
            got = drawn(sec.get(n, []), row)
            same = ref is not None and got is not None and all(abs(a - b) < 0.5 for a, b in zip(ref[0] + ref[1], got[0] + got[1]))
            check(same, f"{n}: the helping hand drawn as without the sword before ({got} vs {ref})")
        print(f"  (crowbar by its blade: {'yes' if blade('crowbar_blade') else 'no'})")
        text, sec = run(agent, retake(h))
        if keep:
            open(f"grip_retake_{h}.txt", "w").write(text)
        check(any("both hands hold it" in l for l in text.splitlines()), "retake: both hands held the carried blade")
        for n in ("retake_grip", "retake_trigger"):
            got = clips(sec.get(n, []), h)
            check(bool(got) and got[-1][0] == SWORD,
                  f"{n}: the {h} hand holds the sword by its handle again ({got})")
    print("== flick reload (main hand)")
    text, sec = run(agent, flick())
    if keep:
        open("grip_flick.txt", "w").write(text)
    for n, allowed in (("flick_one", True), ("flick_barrel", False), ("flick_cup", True), ("flick_anywhere", False)):
        got = clips(sec.get(n, []), "main")
        ok = len(got) == 2 and got[0][0] == SUPER_SHOTGUN and got[0][1] < 2 and got[0][2] == allowed and (
            got[1][1] == 2) == allowed
        check(ok, f"{n}: flick reload {'reloads' if allowed else 'does nothing'} ({got})")
    print("PASS" if not fails else f"FAIL ({len(fails)})")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
