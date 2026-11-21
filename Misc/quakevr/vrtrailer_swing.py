#!/usr/bin/env python3
# vrtrailer_swing.py -- the trailer scene's killing blow as a vr_mock_play script (Misc/quakevr/vrtrailer_swing.mock):
# the Super Axe held in both hands (the main hand on its handle, the off hand further up it), swung right to left
# across a grunt's neck (motion_synth.py's decapitation_horizontal_rtl path, its head on the blade's line, both hands
# gripping), for vrtrailer_test.sh (MAPPING.md, "vrtrailer").
#
#   python Misc/quakevr/vrtrailer_swing.py [--out Misc/quakevr/vrtrailer_swing.mock] [--neck 0.4] [--duration 0.16]
#       [--off 0.6] [--reach 0.62]   (a grunt 30 units ahead: --reach 0.6-0.65 with --neck 0.34-0.42 behead him;
#       lower hits his shoulders, a shorter reach misses, a longer one strikes with the handle)
#
# The controller poses are worked out for the shipped hand settings (vr_gunangle, vr_gunyaw, vr_handcal_*: their
# compiled defaults, read from Quake/vr/vr_cvars.inc), the shipped world scale and the take's eye height; the mock's
# first line ("# settings: ...", skipped by vr_mock_play) lists them and vrtrailer_test.sh sets them, so a config's
# own hand settings never turn the axe (2026-11-21, ROUND25.md "testenv": the mock of 2026-10-08 was written for
# vr_gunangle 39.5 and an estimated head; the defaults became 70 on 2026-10-16).
import argparse
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import motion_synth as ms

CVARS = os.path.join(HERE, "..", "..", "Quake", "vr", "vr_cvars.inc")
HAND_KEYS = ("vr_gunangle", "vr_gunyaw", "vr_offhandpitch", "vr_offhandyaw", "vr_handcal_x", "vr_handcal_y",
             "vr_handcal_z", "vr_handcal_roll", "vr_handcal_off_mirror", "vr_handcal_off_x", "vr_handcal_off_y",
             "vr_handcal_off_z", "vr_handcal_off_roll", "vr_controller_legacy_pose", "vr_world_scale")


def shipped_settings():
    """The compiled defaults of the settings the swing depends on (QVR_CVAR(name, "value", ...) in vr_cvars.inc)."""
    text = open(CVARS, encoding="utf-8").read()
    out = {}
    for k in HAND_KEYS:
        m = re.search(r'QVR_CVAR\(%s, "([^"]*)"' % k, text)
        if not m:
            sys.exit("vrtrailer_swing.py: %s not found in %s" % (k, CVARS))
        out[k] = m.group(1)
    return out


def build(eye, neck, duration, off_share, distance, reach, scale, lead):
    z = eye - neck
    path = ((0.05, -0.55, z), (reach, -0.05, z), (0.2, 0.4, z))           # the main hand (forward, left, up)
    axes = ((0.3, -0.95, 0.05), (1.0, 0.0, 0.02), (0.3, 0.95, 0.05))      # its head's way from the hand
    # (turned `lead` degrees on, to the left: a heavy head lags the hand in the game, ms.WEAPON_FAR's rigid line doesn't)
    c, s = math.cos(math.radians(lead)), math.sin(math.radians(lead))
    axes = tuple((a[0] * c - a[1] * s, a[0] * s + a[1] * c, a[2]) for a in axes)
    far = sum(c * c for c in ms.WEAPON_FAR["superaxe"]) ** 0.5  # the head's distance from the hand
    take = ms.Take("vrtrailer_decap_2h", rate=90, world_scale=scale, eye_height=eye, main_weapon="superaxe",
                   target=(distance, 0.0), note="synthetic")
    take.grip("main", True)
    take.grip("off", True)
    last = {"right": (0.0, 0.0, -1.0)}

    def pose(s):
        h = ms.bezier(path[0], path[1], path[2], s)
        ax = ms.norm(ms.lerp(axes[0], axes[1], s * 2)) if s < 0.5 else ms.norm(ms.lerp(axes[1], axes[2], s * 2 - 1))
        m = ms.sub(ms.bezier(path[0], path[1], path[2], min(1.0, s + 0.01)),
                   ms.bezier(path[0], path[1], path[2], max(0.0, s - 0.01)))
        r = ms.cross(m, ax)
        if ms.dot(r, r) > 1e-8:
            last["right"] = ms.norm(r)
        q = ms.weapon_pose("main", "superaxe", ax, last["right"])
        # the off hand on the handle, off_share of the way from the main hand to the head
        o = ms.add(h, ms.mul(ax, far * off_share))
        return {"main": (h, q), "off": (o, ms.weapon_pose("off", "superaxe", ax, last["right"]))}
    take.start(pose(0.0))
    take.hold(0.4)
    take.move(duration, pose, phase="rec")
    take.hold(0.4)
    take.hold(0.3, phase="tail")
    return take


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", default=os.path.join(HERE, "vrtrailer_swing.mock"))
    ap.add_argument("--eye-height", type=float, default=1.646)
    ap.add_argument("--neck", type=float, default=0.3, help="metres from the eye down to the hands (the neck's height)")
    ap.add_argument("--duration", type=float, default=0.3, help="seconds the swing takes")
    ap.add_argument("--off", type=float, default=0.6, help="the off hand's place up the handle (0 the main hand, 1 the head)")
    ap.add_argument("--reach", type=float, default=0.8, help="metres the hands reach forward half way through the swing")
    ap.add_argument("--distance", type=float, default=0.9, help="metres from the head to the grunt's middle (the take's note)")
    ap.add_argument("--lead", type=float, default=45.0, help="degrees the head's way is turned on into the swing")
    args = ap.parse_args()
    settings = shipped_settings()
    ms.configure(settings)
    settings["vr_height_calibration"] = "%g" % args.eye_height
    take = build(args.eye_height, args.neck, args.duration, args.off, args.distance, args.reach,
                 float(settings["vr_world_scale"]), args.lead)
    ms.write_mock(take, args.out)
    with open(args.out, encoding="utf-8") as f:
        rows = f.read()
    with open(args.out, "w", newline="\n") as f:
        f.write("# settings: %s\n" % ";".join("%s %s" % kv for kv in settings.items()))
        f.write(rows)
    print("wrote %s" % args.out)


if __name__ == "__main__":
    main()
