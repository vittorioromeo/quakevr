#!/usr/bin/env python3
"""The two-handed topple (the author's note vrfiringrange_2026-10-01_00-33; ROUND21.md, "Phasing through a box toppled
in both hands"): in vrfiringrange, the long explosive box (the map's 207, standing) is taken in both hands at its sides
near the top, toppled forward about its far bottom edge so it lands on its long side (the hands carry it down, a little
into the floor at the end), and let go of with the hands' motion; then the player walks into it from 8 directions.
`vr_physics_inside` counts the frames his origin is inside its drawn shape grown by his column (where no move of his
may go): he went through it.

    python Misc/quakevr/boxtopple_repro.py [--time 0.6] [--press 3] [--flick 1.5] [--set "..."] [--out bt_x.cfg]
    bash <kit>/run.sh <name> -Script "exec bt_x.cfg" -Filter "BT|vr_physics_inside:|vr_physics_inlevel|^owner|carry:" > out.txt
    python Misc/quakevr/boxtopple_repro.py --check out.txt   (nonzero exit: he went into it over 2 units)

Needs quakevr/bt_w.cfg (the kit's wait aliases) and quakevr/bt_author.cfg (the author's ironwail.cfg), not committed.
"""
import argparse
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.normpath(os.path.join(HERE, "..", "..", "quakevr"))

BOX = 207
# The player at the box's -y side facing +y (yaw 90); world to tracking (metres) measured with vr_mock_hand_to there.
PLAYER = "-224 -836 42 0 90 0"
SCALE = 31.5


def tracking(wx, wy, wz):
    return ((wx + 224.0) / SCALE, (wz - 18.83) / SCALE, -(wy + 835.1) / SCALE)


def keyframes(t0, duration, press, flick):
    """The hands' keys: at the box's sides (x -206 and -242) at y -792, z 72, swung forward 90 degrees about the far
    bottom edge (y -776, z 17.1), the last `press` units into the floor; then `flick` m/s on down and forward over the
    last 0.1 s (the release's motion)."""
    out = []
    steps = 12
    for i in range(steps + 1):
        th = math.radians(90.0 * i / steps)
        dy = -16.0 * math.cos(th) + 55.0 * math.sin(th)
        dz = 16.0 * math.sin(th) + 55.0 * math.cos(th) - press * i / steps
        t = t0 + duration * i / steps
        for hand, x in (("main", -206.0), ("off", -242.0)):
            p = tracking(x, -776.0 + dy, 17.1 + dz)
            out.append(f"{t:.3f} {hand} {p[0]:.4f} {p[1]:.4f} {p[2]:.4f} {-90.0 * i / steps:.2f} 0 0")
    t_end = t0 + duration
    if flick > 0:
        d = flick * 0.1 * SCALE  # units moved in 0.1 s
        for hand, x in (("main", -206.0), ("off", -242.0)):
            p = tracking(x, -776.0 + 55.0 + d * 0.7, 17.1 + 16.0 - press - d * 0.7)
            out.append(f"{t_end + 0.1:.3f} {hand} {p[0]:.4f} {p[1]:.4f} {p[2]:.4f} -90 0 0")
    return out, t_end + (0.1 if flick > 0 else 0.0)


def build(args):
    play = os.path.join(GAME, args.out.replace(".cfg", ".play"))
    keys, t_rel = keyframes(0.3, args.time, args.press, args.flick)
    lines = [
        "0.000 cmd +grabright", "0.000 button main grip 1",
        "0.150 cmd +graboff", "0.150 button off grip 1",
    ]
    # Start: both hands on the box's sides (angles 0) before the grips.
    for hand, x in (("main", -206.0), ("off", -242.0)):
        p = tracking(x, -792.0, 72.0)
        lines.append(f"0.000 {hand} {p[0]:.4f} {p[1]:.4f} {p[2]:.4f} 0 0 0")
    lines += keys
    lines += [f"{t_rel:.3f} cmd -grabright", f"{t_rel:.3f} button main grip 0",
              f"{t_rel:.3f} cmd -graboff", f"{t_rel:.3f} button off grip 0"]
    with open(play, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")

    p0 = tracking(-206.0, -792.0, 72.0)
    q0 = tracking(-242.0, -792.0, 72.0)
    out = ["exec bt_w.cfg", "exec bt_author.cfg", args.set, "vr_explobox_impact 0", "map vrfiringrange", "w300",
           "developer 1",
           f"setpos {PLAYER}; noclip; w30",
           f"vr_mock_hand main {p0[0]:.4f} {p0[1]:.4f} {p0[2]:.4f} 0 0 0; vr_mock_hand off {q0[0]:.4f} {q0[1]:.4f} {q0[2]:.4f} 0 0 0; w30",
           "echo BT topple",
           f"vr_mock_play {play.replace(os.sep, '/')}",
           "w60;" * (int((t_rel + 0.5) * 72 / 60) + 1),
           "vr_mock_play; vr_mock_hand main; vr_mock_hand off",
           "w60; w60; w60",
           f"echo BT rest; vr_physics_list {BOX}; vr_physics_inlevel {BOX}; edict {BOX}",
           "vr_physics_inside 1"]
    for k in range(8):
        out.append(f"echo BT walk {k * 45}; vr_physics_player near {BOX} {k * 45} 64; w10; +forward; w60; w30; -forward; w20; vr_physics_list {BOX}")
    out += ["echo BT end; vr_physics_inside; vr_physics_list", "vr_physics_inside 0", "toggleconsole; quit"]
    with open(os.path.join(GAME, args.out), "w", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    print(f"wrote {os.path.join(GAME, args.out)} and {play}")


def check(path):
    deepest, frames, owner = 0.0, 0, False
    for l in open(path, encoding="utf-8", errors="replace"):
        m = re.search(r"(\d+) times in a prop, (\d+) of \d+ frames .*deepest ([0-9.]+)", l)
        if m:
            frames, deepest = int(m.group(2)), float(m.group(3))
        if l.startswith("owner"):
            owner = True
    # (Its owner may still be the player at rest, the throw never ended by a hit: his traces meet it all the same.)
    print(f"inside {frames} frames, deepest {deepest:.2f}; still his at rest: {owner}")
    return 1 if deepest > 2.0 else 0


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--time", type=float, default=0.6)
    ap.add_argument("--press", type=float, default=3.0)
    ap.add_argument("--flick", type=float, default=1.5)
    ap.add_argument("--set", default="")
    ap.add_argument("--out", default="bt_run.cfg")
    ap.add_argument("--check")
    a = ap.parse_args()
    if a.check:
        sys.exit(check(a.check))
    build(a)
