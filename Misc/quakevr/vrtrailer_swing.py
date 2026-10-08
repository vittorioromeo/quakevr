#!/usr/bin/env python3
# vrtrailer_swing.py -- the trailer scene's killing blow as a vr_mock_play script (Misc/quakevr/vrtrailer_swing.mock):
# the Super Axe held in both hands (the main hand on its handle, the off hand further up it), swung right to left
# across a grunt's neck (motion_synth.py's decapitation_horizontal_rtl path, its head on the blade's line, both hands
# gripping), for vrtrailer_test.sh (MAPPING.md, "vrtrailer").
#
#   python Misc/quakevr/vrtrailer_swing.py [--out Misc/quakevr/vrtrailer_swing.mock] [--neck 0.4] [--duration 0.16]
#       [--off 0.6] [--reach 0.62]   (a grunt 30 units ahead: --reach 0.6-0.65 with --neck 0.34-0.42 behead him;
#       lower hits his shoulders, a shorter reach misses, a longer one strikes with the handle)
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import motion_synth as ms


def build(eye, neck, duration, off_share, distance, reach):
    z = eye - neck
    path = ((0.05, -0.55, z), (reach, -0.05, z), (0.2, 0.4, z))           # the main hand (forward, left, up)
    axes = ((0.3, -0.95, 0.05), (1.0, 0.0, 0.02), (0.3, 0.95, 0.05))      # its head's way from the hand
    far = sum(c * c for c in ms.WEAPON_FAR["superaxe"]) ** 0.5  # the head's distance from the hand
    take = ms.Take("vrtrailer_decap_2h", rate=90, world_scale=1.25, eye_height=eye, main_weapon="superaxe",
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
    ap.add_argument("--neck", type=float, default=0.4, help="metres from the eye down to the hands (the neck's height)")
    ap.add_argument("--duration", type=float, default=0.16, help="seconds the swing takes")
    ap.add_argument("--off", type=float, default=0.6, help="the off hand's place up the handle (0 the main hand, 1 the head)")
    ap.add_argument("--reach", type=float, default=0.62, help="metres the hands reach forward half way through the swing")
    ap.add_argument("--distance", type=float, default=0.9, help="metres from the head to the grunt's middle (the take's note)")
    args = ap.parse_args()
    take = build(args.eye_height, args.neck, args.duration, args.off, args.distance, args.reach)
    ms.write_mock(take, args.out)
    print("wrote %s" % args.out)


if __name__ == "__main__":
    main()
