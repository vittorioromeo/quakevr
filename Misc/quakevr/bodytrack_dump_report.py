#!/usr/bin/env python3
"""Summarise a vr_bodytrack_dump CSV (ROUND22.md, "Elbow fit on real data").

Per arm, in cm (the world's units divided by m2u): the elbow's miss, the tracked wrist from ours, the tracked shoulder
from ours, our wrist from the controller, the tracked arm's bone lengths against the ones our fit took, and how often the
fit met a limit. Also the tracked head from the headset's eyes, to check the spaces line up.

    python bodytrack_dump_report.py <dump.csv>
"""
import csv
import math
import sys


def vec(row, name):
    return (float(row[name + "_x"]), float(row[name + "_y"]), float(row[name + "_z"]))


def dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def stats(values):
    if not values:
        return "n/a"
    values = sorted(values)
    mean = sum(values) / len(values)
    return "mean %6.1f  median %6.1f  max %6.1f" % (mean, values[len(values) // 2], values[-1])


def forward(pitch, yaw):
    # Quake angles (degrees): pitch down positive, yaw left positive.
    p, y = math.radians(pitch), math.radians(yaw)
    return (math.cos(p) * math.cos(y), math.cos(p) * math.sin(y), -math.sin(p))


def main(path):
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        print("no rows")
        return
    print("%s: %d frames, %.1f s of game time" % (path, len(rows), float(rows[-1]["time"]) - float(rows[0]["time"])))
    active = [r for r in rows if r["active"] == "1" and r["fresh"] == "1"]
    print("active and fresh: %d frames; confidence %s" % (len(active), stats([float(r["confidence"]) for r in active])))
    print("weights: torso %s" % stats([float(r["w_torso"]) for r in rows]))
    for side, ctrl in (("l", "controller_off"), ("r", "controller_main")):
        print("\narm %s (weight %s)" % (side.upper(), stats([float(r["w_arm_" + side]) for r in rows])))
        gates = {}
        for r in rows:
            g = r.get("gate_" + side, "?")
            gates[g] = gates.get(g, 0) + 1
        print("  tracking on, or why not: " + ", ".join("%s %.0f%%" % (g, 100.0 * c / len(rows)) for g, c in gates.items()))
        cm = lambda r, v: v * 100.0 / max(float(r["m2u"]), 1e-3)
        out = {k: [] for k in ("miss", "wrist", "shoulder", "ours_ctrl", "theirs_ctrl", "t_upper", "t_fore", "share_u",
                               "share_f", "along")}
        limits = {k: 0 for k in ("turn", "clav", "upper", "fore")}
        fitted = 0
        wrist_from = {}
        wrist_joint, twist_joint = (19, 12) if side == "l" else (45, 17)
        for r in rows:
            aw, ae, ash = vec(r, "av_wrist_" + side), vec(r, "av_elbow_" + side), vec(r, "av_shoulder_" + side)
            te, tsh = vec(r, "tw_elbow_" + side), vec(r, "tw_shoulder_" + side)
            # The tracked wrist: the hand's wrist joint when valid, else the forearm's twist joint by it (Virtual
            # Desktop sends no hand joints with controllers), else none.
            if int(r["raw%d_flags" % wrist_joint]) & 1:
                tw, source = vec(r, "tw_wrist_" + side), "wrist joint"
            elif ("tw_twist_%s_x" % side) in r and int(r["raw%d_flags" % twist_joint]) & 1:
                tw, source = vec(r, "tw_twist_" + side), "twist joint"
            else:
                tw, source = None, "none valid"
            wrist_from[source] = wrist_from.get(source, 0) + 1
            c = vec(r, ctrl)
            out["miss"].append(cm(r, dist(ae, te)))
            out["shoulder"].append(cm(r, dist(ash, tsh)))
            out["ours_ctrl"].append(cm(r, dist(aw, c)))
            out["t_upper"].append(cm(r, dist(tsh, te)))
            if tw is not None:
                out["wrist"].append(cm(r, dist(aw, tw)))
                out["theirs_ctrl"].append(cm(r, dist(tw, c)))
                out["t_fore"].append(cm(r, dist(te, tw)))
                # The tracked wrist from ours along the controller's forward (+ ahead of ours).
                fwd = forward(float(r[ctrl + "_pitch"]), float(r[ctrl + "_yaw"]))
                out["along"].append(cm(r, sum(x * y for x, y in zip(sub(tw, aw), fwd))))
            rise_t = (te[2] - tsh[2]) * 100.0 / max(float(r["m2u"]), 1e-3)
            out.setdefault("rise_t", []).append(rise_t)
            out.setdefault("rise_o", []).append((ae[2] - tsh[2]) * 100.0 / max(float(r["m2u"]), 1e-3))
            if rise_t > 0.0:
                out.setdefault("miss_raised", []).append(cm(r, dist(ae, te)))
            if float(r["fit_%s_w" % side]) > 0.0:
                fitted += 1
                out["share_u"].append(float(r["fit_%s_upper_share" % side]))
                out["share_f"].append(float(r["fit_%s_fore_share" % side]))
                for k in limits:
                    limits[k] += int(r["fit_%s_%s" % (side, k)])
        print("  elbow miss (ours to tracked)      %s" % stats(out["miss"]))
        print("    with the elbow over the shoulder %s (%d frames)" % (stats(out.get("miss_raised", [])),
                                                                   len(out.get("miss_raised", []))))
        print("  elbow over the tracked shoulder: tracked %s; ours %s" % (stats(out["rise_t"]), stats(out["rise_o"])))
        print("  tracked wrist from: " + ", ".join("%s %d" % kv for kv in wrist_from.items()))
        print("  wrist, tracked from ours          %s" % stats(out["wrist"]))
        print("    along the controller (+ ahead)  %s" % stats(out["along"]))
        print("  shoulder, tracked from ours       %s" % stats(out["shoulder"]))
        print("  our wrist from the controller     %s" % stats(out["ours_ctrl"]))
        print("  tracked wrist from the controller %s" % stats(out["theirs_ctrl"]))
        print("  tracked upper arm %s / forearm %s" % (stats(out["t_upper"]), stats(out["t_fore"])))
        print("  fit on %d frames; bone shares taken: upper %s, fore %s" % (fitted, stats(out["share_u"]), stats(out["share_f"])))
        if fitted:
            print("  at a limit: " + ", ".join("%s %.0f%%" % (k, 100.0 * v / fitted) for k, v in limits.items()))
    heads = [dist(vec(r, "tw_head"), vec(r, "head")) * 100.0 / max(float(r["m2u"]), 1e-3) for r in rows]
    print("\ntracked head joint from the eyes: %s" % stats(heads))


if __name__ == "__main__":
    main(sys.argv[1])
