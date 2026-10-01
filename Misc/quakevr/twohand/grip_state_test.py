#!/usr/bin/env python3
"""grip_state_test.py <agent> [--keep]

Two-handed grip state across weapon changes (ROUND21.md, "Grips reset with the weapon"; NOTES.md
vrfiringrange_2026-10-01_22-40-50): a sword held by its blade, both hands on it, let go of; then a shotgun, a crowbar
and the shotgun again, each gripped by its foregrip, its blade or anywhere, one swapped while the other hand still grips
it. No gun may be held "by its blade" (vr_dumpview), and the helping hand on the shotgun's pump is drawn as it is
without a sword before (a control run). Both ways round: the main hand holding, then the off hand.

Runs e1m1 (id1) in the kit's mock (about 10 s a run, three runs). Exit status 1 on a failure.
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
SWORD, SHOTGUN, CROWBAR = 13, 4, 17
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


def control():
    s = SETUP
    for h in ("main", "off"):
        d = HANDS[h]
        s += take(h, SHOTGUN) + grip(h, PUMP, f"control_{h}") + let_go(d["other"], d["ograb"]) + let_go(h, d["grab"])
        s += f"impulse {d['base']};wait30;"  # (the fist: empty)
    return s


def run(agent, script):
    out = subprocess.run([BASH, f"{KIT}/run.sh", agent, "-Script", script + "toggleconsole;quit", "-Filter",
                          r"^=== |grips reset|by its blade|2h grip|^ ?(4|10) progs/hand_rig|rror"],
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
    print("PASS" if not fails else f"FAIL ({len(fails)})")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
