#!/usr/bin/env python3
"""The toppled-box phasing sweep (ROUND21.md, "Phasing through a toppled box").

Writes quakevr/pp_sweep.cfg (--out): in vrfiringrange, the long explosive box (the map's 207) is laid on each of its
long faces, dropped onto a long edge, and tipped past its balance to topple by itself, at many yaws; the player's feet
are then put a little into its top (as when it rocks up into them), he is dropped on it, jumps on it, walks off it, and
runs at it from 8 directions, jumping onto it and over it. `vr_physics_inside` counts the frames his origin is inside
the box's drawn shape grown by his column (where no move of his may go).

    python Misc/quakevr/propphase_sweep.py [--yaws 0,15,...] [--quick] [--set "vr_box3d_player_hold 0"] [--out pp_x.cfg]
    bash <kit>/run.sh <name> -Timeout 600 -Script "exec pp_x.cfg"         -Filter "CASE|SINK|vr_physics_inside:|vr_physics_player: [0-9]" > out.txt
    python Misc/quakevr/propphase_sweep.py --check out.txt   (nonzero exit: a sink test not on the box, or over 2 units in)
"""
import argparse
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.normpath(os.path.join(HERE, "..", "..", "quakevr"))

BOX = 207  # vrfiringrange's first misc_explobox (32 x 32 x 64, its origin at a corner)
MINS, MAXS = (0.0, 0.0, 0.0), (32.0, 32.0, 64.0)
FLOOR = 17.1  # the floor there
CENTRE = (-50.0, -700.0)  # an open stretch of level floor near the start


def angle_vectors(p, y, r):
    """Quake's AngleVectors (degrees)."""
    sp, cp = math.sin(math.radians(p)), math.cos(math.radians(p))
    sy, cy = math.sin(math.radians(y)), math.cos(math.radians(y))
    sr, cr = math.sin(math.radians(r)), math.cos(math.radians(r))
    f = (cp * cy, cp * sy, -sp)
    rt = (-sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp)
    u = (cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp)
    return f, rt, u


def origin_for(centre, angles):
    """The origin that puts the box's centre at `centre` turned by `angles` (a brush model's: held::axesFromAngles)."""
    f, rt, u = angle_vectors(*angles)
    axes = (f, tuple(-c for c in rt), u)
    mid = [(a + b) * 0.5 for a, b in zip(MINS, MAXS)]
    return tuple(centre[i] - sum(axes[k][i] * mid[k] for k in range(3)) for i in range(3))


def fmt(v):
    return " ".join(f"{x:.2f}" for x in v)


TRACE = ""


def case(out, name, place, frames=240):
    out.append(f"echo CASE {name}")
    out.append(place)
    out.append("w60;" * (frames // 60))
    out.append("vr_physics_list")
    out.append("vr_physics_inside 1")
    # His feet put a little (half a unit), and more (4 units), into its top, in the air: as when it rocks up into them
    # (it wakes under his weight) or he lands a hair into it. He must end on it, not in it or through it.
    for h in (-1.5, -5):
        out.append(f"echo SINK {h}; vr_physics_player onto {BOX} {h}; w60; vr_physics_player")
    # Dropped on top, two jumps in place, a walk off each way.
    out.append(f"vr_physics_player onto {BOX}; w30")
    out.append("+jump; w5; -jump; w60; +jump; w5; -jump; w60")
    for key in ("forward", "back", "moveleft", "moveright"):
        out.append(f"vr_physics_player onto {BOX}; w30; +{key}; w5; +jump; w5; -jump; w40; -{key}; w20" + TRACE)
    # Run at it from four sides (and the diagonals), jumping onto it, jumping on it, and over it.
    for k in range(8):
        for lead in (5, 12, 18):
            out.append(f"vr_physics_player near {BOX} {k * 45} 70; w10; +forward; w{lead}; +jump; w5; -jump; w40; +jump; w5; -jump; w40; -forward; w20" + TRACE)
    out.append("vr_physics_inside; vr_physics_list")
    out.append("vr_physics_inside 0")


def check(path):
    """Reads a run's filtered output (CASE|SINK|vr_physics_inside:|vr_physics_player: [0-9]): the sink tests' ends and
    each case's count of frames inside the box. Nonzero exit if any fails."""
    import re
    cases, sinks, bad = 0, [0, 0], []
    case, sink, last, prints = "", None, "", 0
    lines = open(path, encoding="utf-8", errors="replace").read().splitlines()

    def end_sink():
        if sink is not None:
            ok = " on 207 " in last and "grounded" in last
            sinks[0 if ok else 1] += 1
            if not ok:
                bad.append(f"{case}: sink {sink}: {last.strip()[:110]}")

    for l in lines:
        if l.startswith("CASE "):
            end_sink()
            sink, case, cases = None, l[5:].strip(), cases + 1
        elif l.startswith("SINK "):
            end_sink()
            sink, last, prints = l[5:].strip(), "", 0
        elif sink is not None and re.match(r"vr_physics_player: [0-9]", l):
            last, prints = l, prints + 1  # (its second: after the minute on it)
            if prints == 2:
                end_sink()
                sink = None
        elif "times in a prop" in l:
            end_sink()
            sink = None
            m = re.search(r"(\d+) times in a prop, (\d+) of \d+ frames \(at most (\d+) in a row\), deepest ([0-9.]+)", l)
            if m and float(m.group(4)) > 2.0:  # (passing in goes deep; a box resting or rocking against him leaves him under a unit in it)
                bad.append(f"{case}: {m.group(1)} times in it, {m.group(3)} frames in a row, {m.group(4)} deep")
    end_sink()
    print(f"{path}: {cases} cases; sinks {sinks[0]} ended on the box, {sinks[1]} not; {len(bad)} failures")
    for b in bad[:12]:
        print("  " + b)
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", help="a run's output to check (see check) instead of writing the cfg")
    ap.add_argument("--yaws", default="0,15,30,45,60,75,90,105,120,135,150,165")
    ap.add_argument("--verbose", action="store_true", help="where the player is after each run")
    ap.add_argument("--set", default="", help="commands after the settings (e.g. \"vr_box3d_player_hold 0\")")
    ap.add_argument("--out", default="pp_sweep.cfg")
    ap.add_argument("--quick", action="store_true", help="the laid-down cases only")
    args = ap.parse_args()
    if args.check:
        raise SystemExit(check(args.check))
    global TRACE
    TRACE = "; vr_physics_player" if args.verbose else ""
    yaws = [float(y) for y in args.yaws.split(",")]
    out = ["exec pp_w.cfg", "exec pp_author.cfg", "exec pp_pre.cfg", args.set, "vr_fixed_frames 1; vr_mock_fast 2; vr_explobox_impact 0", "map vrfiringrange", "w300", "developer 0",
           "vr_rigid_place 208 -50 -1000 20 0 0 0"]  # (no box blows up; the other box out of the way)
    lying = FLOOR + 16.0 + 1.0
    for y in yaws:
        # Laid on each long face (exactly 90 degrees) and dropped a unit.
        for name, ang in (("pitch90", (90.0, y, 0.0)), ("pitch-90", (-90.0, y, 0.0)), ("roll90", (0.0, y, 90.0)),
                          ("roll-90", (0.0, y, -90.0))):
            o = origin_for((CENTRE[0], CENTRE[1], lying), ang)
            case(out, f"{name} yaw {y:g}", f"vr_rigid_place {BOX} {fmt(o)} {fmt(ang)}")
        if args.quick:
            continue
        # Dropped onto a long edge (45 degrees): it falls onto a face as it will.
        for name, ang in (("edge roll45", (0.0, y, 45.0)), ("edge pitch45", (45.0, y, 0.0))):
            o = origin_for((CENTRE[0], CENTRE[1], FLOOR + 32.0), ang)
            case(out, f"{name} yaw {y:g}", f"vr_rigid_place {BOX} {fmt(o)} {fmt(ang)}")
        # Stood up on a bottom edge, tipped past its balance (26.6 degrees): it topples over onto a long side by itself.
        for name, ang in (("topple pitch35", (35.0, y, 0.0)), ("topple roll35", (0.0, y, 35.0))):
            o = origin_for((CENTRE[0], CENTRE[1], FLOOR + 36.0), ang)
            case(out, f"{name} yaw {y:g}", f"vr_rigid_place {BOX} {fmt(o)} {fmt(ang)}", 360)
    out.append("toggleconsole; quit")
    with open(os.path.join(GAME, args.out), "w", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    with open(os.path.join(GAME, "pp_w.cfg"), "w", newline="\n") as f:
        f.write("alias w5 \"wait;wait;wait;wait;wait\"\nalias w3 \"wait;wait;wait\"\nalias w8 \"w5;w3\"\n"
                "alias w20 \"w5;w5;w5;w5\"\nalias w25 \"w20;w5\"\nalias w30 \"w25;w5\"\nalias w40 \"w20;w20\"\n"
                "alias w60 \"w30;w30\"\nalias w300 \"w60;w60;w60;w60;w60\"\n")
    pre = os.path.join(GAME, "pp_pre.cfg")
    if not os.path.exists(pre):
        with open(pre, "w", newline="\n") as f:
            f.write("\n")
    print(f"{args.out}: {sum(1 for l in out if l.startswith('echo CASE'))} cases")


if __name__ == "__main__":
    main()
