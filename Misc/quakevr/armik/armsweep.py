# Arm IK pose tests (ROUND21.md, "Elbow tucked by the face"): mock hand poses with the author's arm calibration, the
# arms printed by vr_debug_arm, the elbows summarised (body axes, cm from the shoulder: out past it, back behind it,
# down below it; the elbow's angle, its swing to ease the wrist, the wrist's strain, how tucked, its swing out of the
# torso). Run from a scratch folder (logs land in the current directory); QVR_AGENT names the kit agent (worktree).
# usage: python armsweep.py <tag> poses|orient|play [extra console cmds]   (e.g. "vr_body_elbow_tuck 0": as before)
#        python armsweep.py <tag> parseorient                             (again from the logs)
#        python contin.py <tag> [<tag> ...]      the play sweeps' continuity (elbow jump per frame and per hand step)
#        python orientsum.py <tag> [<tag> ...]   the orientation grid: the elbow's swing from straight down and back
import subprocess, sys, re, json, math, os

KIT = "C:/OHWorkspace/qvr-kit"
NAME = os.environ.get("QVR_AGENT", "elbowik")
OUT = os.getcwd().replace("\\", "/")

AUTHOR = ('vr_body_arm_length 1;vr_body_arm_stretch 1.2;vr_body_elbow_back 0.35;vr_body_elbow_hand 0.5;'
          'vr_body_elbow_lift 8;vr_body_elbow_out 0.35;vr_body_elbow_spread 1;vr_body_forearm_twist 0.5;'
          'vr_body_shoulder_reach 0.08;vr_body_wrist_limits 1;vr_bodycal_forearm 22.7;vr_bodycal_shoulder_rise 13.5;'
          'vr_bodycal_shoulder_swing 0;vr_bodycal_shoulders_back -0.023;vr_bodycal_shoulders_out -0.057;'
          'vr_bodycal_shoulders_up -0.004;vr_bodycal_upper_arm 29.4;vr_gunangle 70;vr_gunyaw 0;vr_handcal_off_mirror 1;'
          'vr_handcal_off_x -0.88;vr_handcal_off_y -1.15;vr_handcal_off_z 3.20;vr_handcal_x -4;vr_handcal_y 0.58;'
          'vr_handcal_z -2.5;vr_height_calibration 1.552;vr_body_collide_elbows 1')

# Head at 0 1.7 0 (tracking metres: x right, y up, z back). Pitch 70 ~ the gun level (vr_gunangle 70).
OFF_REST = (-0.25, 1.0, -0.1, 0, 0, 0)
POSES = {
    # name: (main, off)
    "ads1":      ((0.03, 1.62, -0.24, 70, 0, 0), OFF_REST),
    "ads2":      ((0.04, 1.60, -0.25, 70, 0, 0), (0.0, 1.55, -0.45, 70, 0, -30)),
    "axeface":   ((0.10, 1.62, -0.18, 140, 0, 0), OFF_REST),
    "axeface2":  ((0.05, 1.70, -0.16, 150, 10, 20), OFF_REST),
    "chest":     ((0.06, 1.35, -0.14, 70, -40, 0), (-0.06, 1.35, -0.14, 70, 40, 0)),
    "overhead":  ((0.15, 2.15, -0.10, 140, 0, 0), (-0.15, 2.15, -0.10, 140, 0, 0)),
    "behindhead":((0.10, 1.85, 0.20, 150, 0, 0), (-0.10, 1.85, 0.20, 150, 0, 0)),
    "forehead":  ((0.06, 1.82, -0.12, 160, 0, 0), (-0.06, 1.82, -0.12, 160, 0, 0)),
    "low":       ((0.25, 0.95, -0.10, 30, 0, 0), (-0.25, 0.95, -0.10, 30, 0, 0)),
    "far":       ((0.25, 1.45, -0.70, 70, 0, 0), (-0.25, 1.45, -0.70, 70, 0, 0)),
    "cross":     ((-0.25, 1.35, -0.30, 70, 40, 0), (0.25, 1.35, -0.30, 70, -40, 0)),
    "rest":      ((0.20, 1.30, -0.40, 70, 0, 0), (-0.20, 1.30, -0.40, 70, 0, 0)),
    "pistolface":((0.12, 1.55, -0.18, 70, 0, 0), (-0.12, 1.55, -0.18, 70, 0, 0)),
}

def hand_cmd(h, p):
    return "vr_mock_hand %s %.3f %.3f %.3f %.1f %.1f %.1f" % ((h,) + tuple(p))

def run(script, tag):
    cmd = ["C:/Program Files/Git/bin/bash.exe", KIT + "/run.sh", NAME, "-Script", script, "-Filter", "^arm |^armT |^armcost |ENGINE|SWEEP", "-Timeout", "240"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if "ENGINE" in r.stdout: print(r.stdout[-600:])
    txt = open("C:/OHWorkspace/qvr-kit/bases/%s/qbase/qconsole.log" % NAME, errors="replace").read()
    open("%s/sweep_%s.log" % (OUT, tag), "w").write(txt)
    return txt

NUM = r"(-?[\d.]+)"
ARM = re.compile(r"^arm ([LR]) S %s %s %s E %s %s %s E0 %s %s %s W %s %s %s elbow %s swivel %s .*strain %s pole" % ((NUM,) * 15))

def parse(txt):
    out = []
    marker = None
    for line in txt.splitlines():
        m = re.search(r"SWEEP (\S+)", line)
        if m:
            marker = m.group(1)
            continue
        m = ARM.search(line.strip())
        if m:
            v = [float(x) for x in m.groups()[1:]]
            t = re.search(r"tuck (-?[\d.]+) torso (-?[\d.]+)", line)
            v += [float(t.group(1)), float(t.group(2))] if t else [0.0, 0.0]
            out.append((marker, m.group(1), v))
    return out

def metrics(side, v):
    S, E, W = v[0:3], v[3:6], v[9:12]
    angle, swivel, strain = v[12], v[13], v[14]
    sgn = 1.0 if side == "L" else -1.0  # left is +y
    out = (E[1] - S[1]) * sgn            # elbow wider than the shoulder (cm)
    back = S[0] - E[0]                   # elbow behind the shoulder (cm)
    down = S[2] - E[2]
    return dict(tuck=v[15], torso=v[16], fwd=E[0], out=out, back=back, down=down, angle=angle, swivel=swivel, strain=strain, E=E, W=W)

def poses_script(extra):
    s = ["map e1m1", "wait60", AUTHOR] + ([extra] if extra else []) + ["impulse 1", "wait10"]
    for name, (m, o) in POSES.items():
        s += [hand_cmd("main", m), hand_cmd("off", o), "wait40", "echo SWEEP " + name, "vr_debug_arm 1", "wait3"]
    s += ["toggleconsole", "quit"]
    return ";".join(s)

def lerp(a, b, t):
    return tuple(x + (y - x) * t for x, y in zip(a, b))

# Sweeps: the main hand moved in small steps (1 cm or so), one frame each, the arm traced each step.
SWEEPS = {
    "far_to_face": ((0.20, 1.45, -0.65, 70, 0, 0), (0.03, 1.62, -0.20, 70, 0, 0), 50),
    "rest_to_axe": ((0.20, 1.30, -0.40, 70, 0, 0), (0.08, 1.66, -0.16, 150, 10, 20), 50),
    "chest_cross": ((0.25, 1.35, -0.25, 70, 0, 0), (-0.20, 1.35, -0.20, 70, 40, 0), 50),
    "low_to_over": ((0.25, 0.95, -0.15, 30, 0, 0), (0.15, 2.10, -0.10, 140, 0, 0), 60),
    "face_behind": ((0.05, 1.62, -0.22, 70, 0, 0), (0.12, 1.85, 0.20, 150, 0, 0), 40),
    "face_roll": ((0.05, 1.60, -0.22, 70, 0, -80), (0.05, 1.60, -0.22, 70, 0, 80), 40),
    "face_pitch": ((0.06, 1.60, -0.20, 30, 0, 0), (0.06, 1.60, -0.20, 170, 0, 0), 40),
    "face_yaw": ((0.06, 1.60, -0.20, 90, -60, 0), (0.06, 1.60, -0.20, 90, 60, 0), 40),
}

def sweep_script(extra):
    s = ["map e1m1", "wait60", AUTHOR] + ([extra] if extra else []) + ["impulse 1", "wait10"]
    for name, (a, b, n) in SWEEPS.items():
        s += [hand_cmd("main", a), hand_cmd("off", lerp(a, b, 0)[:3] and (-0.25, 1.0, -0.1, 0, 0, 0)), "wait40"]
        s += ["echo SWEEP " + name]
        for i in range(n + 1):
            s += [hand_cmd("main", lerp(a, b, i / n)), "wait2", "vr_debug_arm 1", "wait1"]
    s += ["toggleconsole", "quit"]
    return ";".join(s)

def report_poses(rows):
    print("%-11s %s %6s %6s %6s %6s %6s %7s %6s %5s %6s" % ("pose", "s", "fwd", "out", "back", "down", "angle", "swivel", "strain", "tuck", "torso"))
    for name, side, v in rows:
        m = metrics(side, v)
        print("%-11s %s %6.1f %6.1f %6.1f %6.1f %6.1f %7.1f %6.2f %5.2f %6.1f" % (name, side, m["fwd"], m["out"], m["back"], m["down"], m["angle"], m["swivel"], m["strain"], m["tuck"], m["torso"]))

def report_sweeps(rows):
    by = {}
    for name, side, v in rows:
        if side == "R":
            by.setdefault(name, []).append(metrics(side, v))
    for name, ms in by.items():
        jumps = [math.dist(a["E"], b["E"]) for a, b in zip(ms, ms[1:])]
        hand = [math.dist(a["W"], b["W"]) for a, b in zip(ms, ms[1:])]
        ratio = [j / max(h, 0.3) for j, h in zip(jumps, hand)]
        worst = max(range(len(ratio)), key=lambda i: ratio[i]) if ratio else 0
        print("%-12s steps %3d  max elbow jump %5.1f cm (hand %4.1f) at %d  max ratio %4.1f  mean jump %4.2f  min angle %5.1f  max out %5.1f  max back %5.1f" % (
            name, len(ms), max(jumps), hand[worst] if hand else 0, worst, max(ratio), sum(jumps) / len(jumps),
            min(m["angle"] for m in ms), max(m["out"] for m in ms), max(m["back"] for m in ms)))

ORIENT_POS = {"face": (0.04, 1.62, -0.22), "faceR": (0.12, 1.60, -0.20), "chin": (0.05, 1.50, -0.18), "chest": (0.08, 1.35, -0.16)}
ORIENTS = [(p, y, r) for p in (40, 70, 100, 130, 160) for y in (-45, 0, 45) for r in (-60, 0, 60)]

def orient_script(extra, only):
    s = ["map e1m1", "wait60", AUTHOR] + ([extra] if extra else []) + ["impulse 1", "wait10"]
    for pn, pos in ORIENT_POS.items():
        if pn != only: continue
        for o in ORIENTS:
            mp = pos + o
            op = (-pos[0], pos[1], pos[2], o[0], -o[1], -o[2])
            s += [hand_cmd("main", mp), hand_cmd("off", op), "wait25", "echo SWEEP %s_%d_%d_%d" % ((pn,) + o), "vr_debug_arm 1", "wait2"]
    s += ["toggleconsole", "quit"]
    return ";".join(s)

def report_orient(rows):
    import collections
    bad = collections.Counter(); n = collections.Counter(); worst = {}
    for name, side, v in rows:
        m = metrics(side, v)
        pn = name.split("_")[0]
        key = (pn, side)
        n[key] += 1
        # broken-looking: the elbow far in or out past the shoulder, or up near shoulder height with the hand low
        score = max(abs(m["out"]) - 10, 0) + max(8 - m["down"], 0) + max(-m["back"] - 25, 0)
        if score > 0: bad[key] += 1
        if score > worst.get(key, (0, ""))[0]: worst[key] = (score, name, m)
    for key in sorted(n):
        w = worst.get(key)
        ws = "" if not w else "worst %s out %.1f back %.1f down %.1f angle %.0f swivel %.0f" % (w[1], w[2]["out"], w[2]["back"], w[2]["down"], w[2]["angle"], w[2]["swivel"])
        outs = [metrics(s_, v)["out"] for nm, s_, v in rows if nm.startswith(key[0] + "_") and s_ == key[1]]
        backs = [metrics(s_, v)["back"] for nm, s_, v in rows if nm.startswith(key[0] + "_") and s_ == key[1]]
        ms = [metrics(s_, v) for nm, s_, v in rows if nm.startswith(key[0] + "_") and s_ == key[1]]
        print("%-6s %s bad %2d/%2d  out %5.1f..%5.1f  back %5.1f..%5.1f  minDown %5.1f  maxSwivel %4.0f  meanStrain %5.2f  minAngle %3.0f  %s" % (key + (bad[key], n[key], min(outs), max(outs), min(backs), max(backs),
            min(m["down"] for m in ms), max(abs(m["swivel"]) for m in ms), sum(m["strain"] for m in ms) / len(ms), min(m["angle"] for m in ms), ws)))

import os
TRACE = "C:/OHWorkspace/qvr-kit/bases/%s/qbase/quakevr/arm_trace.txt" % NAME

def play_sweep(name, a, b, extra, tag):
    mock = "%s/sweep_%s.mock" % (OUT, name)
    off = (-0.25, 1.0, -0.1, 0, 0, 0)
    with open(mock, "w") as f:
        for t, pose in ((0.0, a), (1.0, a), (4.0, b), (4.5, b)):
            f.write("%.3f main %.3f %.3f %.3f %.1f %.1f %.1f" % ((t,) + tuple(pose)) + chr(10))
            f.write("%.3f off %.3f %.3f %.3f %.1f %.1f %.1f" % ((t,) + off) + chr(10))
    s = ["map e1m1", "wait60", AUTHOR] + ([extra] if extra else []) + ["impulse 1", hand_cmd("main", a), hand_cmd("off", off), "wait30",
         "vr_debug_arm 2", "vr_mock_play " + mock, "wait600", "vr_debug_arm 0", "wait3", "toggleconsole", "quit"]
    run(";".join(s), tag + "_" + name)
    rows = []
    for line in open(TRACE, errors="replace"):
        m = ARM.search(line.split(" ", 2)[2].strip())
        if m and m.group(1) == "R":
            v = [float(x) for x in m.groups()[1:]]
            t = re.search(r"tuck (-?[0-9.]+) torso (-?[0-9.]+)", line)
            v += [float(t.group(1)), float(t.group(2))] if t else [0.0, 0.0]
            rows.append((name, "R", v))
    open("%s/trace_%s_%s.txt" % (OUT, tag, name), "w").write(open(TRACE, errors="replace").read())
    return rows

if __name__ == "__main__":
    tag = sys.argv[1]
    mode = sys.argv[2] if len(sys.argv) > 2 else "poses"
    extra = sys.argv[3] if len(sys.argv) > 3 else ""
    if mode == "poses":
        rows = parse(run(poses_script(extra), tag + "_poses"))
        report_poses(rows)
    elif mode == "sweep":
        rows = parse(run(sweep_script(extra), tag + "_sweep"))
        report_sweeps(rows)
    elif mode == "orient":
        rows = []
        for pn in ORIENT_POS:
            rows += parse(run(orient_script(extra, pn), tag + "_orient_" + pn))
        report_orient(rows)
    elif mode == "play":
        rows = []
        for name, (a, b, n) in SWEEPS.items():
            rows += play_sweep(name, a, b, extra, tag)
        report_sweeps(rows)
    elif mode == "parseorient":
        rows = []
        for pn in ORIENT_POS:
            rows += parse(open("%s/sweep_%s_orient_%s.log" % (OUT, tag, pn)).read())
        report_orient(rows)
    elif mode == "parse":
        rows = parse(open("%s/sweep_%s.log" % (OUT, tag)).read())
        report_poses(rows) if "poses" in tag else report_orient(rows) if "orient" in tag else report_sweeps(rows)
