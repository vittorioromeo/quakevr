#!/usr/bin/env python3
"""flash_trigger_test.py <agent> -- the trigger at the belt lamp (vr_flashlight.cpp onButton's triggerAtLamp; his note
vrtesthall_2026-11-08_17-29-05: the main hand holding the shotgun over the chest lamp switched it instead of firing).

The lamp on the body (e1m1 start, the head pitched 50): the main hand, holding the shotgun, put at the lamp, its trigger
pressed: the lamp stays as it was (the gun fires); the off hand, empty, at the lamp, its trigger pressed: the lamp
switches. The spot is the off hand's grab spot (flash_grab_test.py REF), swept a little until the hand is at the lamp
(the probe's "at 1"). Prints "flash_trigger: PASS/FAIL ..." and "flash_trigger: N passed, M failed".
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import flash_grab_test as fg  # noqa: E402

ROT = fg.HAND_ROT
OFFSETS = [(0, 0, 0), (0.03, 0, 0), (-0.03, 0, 0), (0, 0.03, 0), (0, -0.03, 0), (0, 0, 0.03), (0, 0, -0.03)]


def main():
    agent = sys.argv[1] if len(sys.argv) > 1 else "flash"
    rx, ry, rz = fg.REF
    cmds = fg.setup() + ["vr_flashlight 1", "vr_mock_look 50 0", "vr_weapon_grip_mode 1", "vr_mock_button main grip 1", "impulse 154"] + fg.waits(30)  # (the shotgun in the main hand: impulse 150 + WID_SHOTGUN)
    for i, (dx, dy, dz) in enumerate(OFFSETS):
        p = f"{rx + dx:.4f} {ry + dy:.4f} {rz + dz:.4f} {ROT}"
        # The main hand (the shotgun) at the lamp, the off hand aside; its trigger.
        cmds += ["vr_mock_hand off -0.35 1.0 -0.2 0 0 0", f"vr_mock_hand main {p}"] + fg.waits(30) + [
            f"vr_flashlight_probe g{i}", "vr_mock_button main trigger 1"] + fg.waits(4) + [
            "vr_mock_button main trigger 0"] + fg.waits(4) + [f"vr_flashlight_probe g{i}p"]
        # The off hand (empty) at the lamp, the main hand aside; its trigger.
        cmds += ["vr_mock_hand main 0.35 1.0 -0.2 0 0 0", f"vr_mock_hand off {p}"] + fg.waits(30) + [
            f"vr_flashlight_probe e{i}", "vr_mock_button off trigger 1"] + fg.waits(4) + [
            "vr_mock_button off trigger 0"] + fg.waits(4) + [f"vr_flashlight_probe e{i}p"]
    lines = fg.run(agent, "flash_trigger", cmds)
    got = fg.parse(lines)
    for line in lines:  # (the mode line: "torchprobe TAG mode M holder H placed P on O ...")
        w = line.split()
        if len(w) > 2 and w[2] == "mode" and "on" in w:
            got.setdefault(w[1], {})["on"] = int(w[w.index("on") + 1])
    passed = failed = 0

    def check(ok, what):
        nonlocal passed, failed
        print("flash_trigger: %s %s" % ("PASS" if ok else "FAIL", what))
        passed += ok
        failed += not ok

    gun = empty = None
    for i in range(len(OFFSETS)):
        g, e = got.get(f"g{i}"), got.get(f"e{i}")
        if gun is None and g and g.get("main", {}).get("at") == 1:
            gun = i
        if empty is None and e and e.get("off", {}).get("at") == 1:
            empty = i
    check(gun is not None, "the main hand reaches the lamp at a spot (%s)" % gun)
    check(empty is not None, "the off hand reaches the lamp at a spot (%s)" % empty)
    if gun is not None:
        a, b = got[f"g{gun}"], got[f"g{gun}p"]
        check(a["main"].get("empty") == 0, "the main hand holds a weapon (empty %s)" % a["main"].get("empty"))
        check(a["on"] == b["on"], "the main hand's trigger at the lamp, holding the shotgun: the lamp unchanged (on %d -> %d)"
              % (a["on"], b["on"]))
    if empty is not None:
        a, b = got[f"e{empty}"], got[f"e{empty}p"]
        check(a["off"].get("empty") == 1, "the off hand is empty (empty %s)" % a["off"].get("empty"))
        check(a["on"] != b["on"], "the empty off hand's trigger at the lamp: it switches (on %d -> %d)" % (a["on"], b["on"]))
    print("flash_trigger: %d passed, %d failed" % (passed, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
