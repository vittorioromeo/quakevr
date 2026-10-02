#!/usr/bin/env python3
"""flash_grab_test.py <agent> [mounted|returning|timing|all]

"Highlighted implies grabbable" for the flashlight (ROUND21.md, "Flashlight: lit but not taken"): in the mock, with
the torso turned several ways (the head's yaw, the main hand held out or across: the torso faces between the head and
the hands), the off hand is swept round the lamp and at each spot `vr_flashlight_probe pre` records whether the lamp is
lit for it; the grip is then pressed from an open, still hand and `vr_flashlight_probe post` records whether it took
the lamp. A spot lit and not taken is a failure (its pre-press probe says why: the game's grip winning, the hand not
at the lamp as the game reads it, moving).

  mounted    the lamp on the body (the belt/chest mount)
  timing     a reach to the lamp at several speeds, the grip pressed at several moments round the arrival, at a
             time scale of 1, 0.3 (vr_timescale) and in bullet time; and presses that must not take it (a punch)
  returning  the lamp let go of and springing home: the main hand, still, at spots along its way, grips at several
             moments of the flight (a catch)

Runs e1m1 (id1) in the kit's mock (vr_fixed_frames 1, vr_mock_fast 2: about 30 s a pass). Exit status 1 on a failure.
"""
import os
import re
import subprocess
import sys

KIT = os.environ.get("KIT", "C:/OHWorkspace/qvr-kit")
BASH = os.environ.get("QVR_BASH", "C:/Program Files/Git/bin/bash.exe")  # (not WSL's)
UNITS_PER_METRE = 32.67  # the mock's tracking space to the world (e1m1 start: +x world x, +y world z, +z world -y)

# The torso's turns: (the head's pitch and yaw, the main hand's pose or None for the standing pose's).
CONFIGS = [
    (50, 0, None), (50, 25, None), (50, -25, None), (50, 45, None), (50, -45, None),
    (30, 0, "0.45 1.35 -0.55 0 0 0"),   # the main arm straight out to the right front
    (30, 0, "-0.25 1.2 -0.35 0 0 0"),   # across the body
    (60, 20, "0.3 1.0 0.1 0 0 0"),      # low and back on the right
    (50, -30, "0.5 1.45 -0.2 0 0 0"),   # out to the side, head turned away
    # A gun in the main hand (vr_weapon_grip_mode 1, its grip held, impulse 154: the shotgun) near the lamp: the off hand in the two-handed grip's hotspot.
    (50, 0, "0.05 1.3 -0.3 0 0 0", ["vr_weapon_grip_mode 1", "vr_mock_button main grip 1", "impulse 154"]),
    (50, -20, "0.12 1.2 -0.22 0 0 0", ["vr_mock_button main grip 1", "impulse 155"]),
]
REF = (-0.066, 1.38, -0.072)  # the off hand at the belt lamp (e1m1 start, head 50 0)
HAND_ROT = "-80 0 0"


def waits(n):
    return ["wait"] * n


def setup():
    return ["map e1m1"] + waits(60) + ["god", "notarget", "developer 1", "vr_fixed_frames 1", "vr_mock_fast 2",
                                       "r_norefresh 1", "vr_mock_fingers off 0 0", "vr_mock_fingers main 0 0"] + [
        c for c in os.environ.get("FG_EXTRA", "").split(";") if c]  # e.g. FG_EXTRA="vr_body_collide 0"


def config_cmds(c):
    pitch, yaw, main = c[:3]
    return [f"vr_mock_look {pitch} {yaw}", f"vr_mock_hand main {main}" if main else "vr_mock_hand main"] + (
        c[3] if len(c) > 3 else [])


def run(agent, name, cmds):
    base = f"{KIT}/bases/{agent}/qbase"
    with open(f"{base}/id1/{name}.cfg", "w", newline="\n") as f:
        f.write("\n".join(cmds + ["toggleconsole", "quit"]) + "\n")
    subprocess.run([BASH, f"{KIT}/run.sh", agent, "-Script", f"exec {name}.cfg", "-Filter", "^$", "-Timeout", "600"],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    with open(f"{base}/qconsole.log", encoding="latin-1") as f:
        return [l.rstrip() for l in f if l.startswith("torchprobe ")]


def parse(lines):
    """{tag: {"mode":..., "holder":..., "off": {...}, "main": {...}, "torso": ..., "lamp": (...), ...}}"""
    out = {}
    for l in lines:
        w = l.split()
        tag, rest = w[1], w[2:]
        d = out.setdefault(tag, {})
        if rest[0] == "mode":
            d["mode"], d["holder"] = rest[1], int(rest[3])
        elif rest[0] in ("off", "main"):
            d[rest[0]] = {rest[i]: int(rest[i + 1]) for i in range(1, len(rest) - 1, 2)}
        elif rest[0] == "torso":
            v = [float(x) for x in rest[1::1] if re.match(r"^-?[0-9.]+$", x)]
            d["torso"], d["lamp"], d["offpos"], d["mainpos"] = v[0], v[1:4], v[4:7], v[7:10]
    return out


def centres(agent):
    """Where the lamp is for each torso turn, in the mock's tracking space (the off hand's grid centre)."""
    cmds = setup()
    for i, c in enumerate(CONFIGS):
        cmds += config_cmds(c) + [f"vr_mock_hand off {REF[0]} {REF[1]} {REF[2]} {HAND_ROT}"] + waits(30) + [
            f"vr_flashlight_probe cal{i}"]
    got = parse(run(agent, "fg_cal", cmds))
    result = []
    for i, c in enumerate(CONFIGS):
        g = got[f"cal{i}"]
        d = [g["lamp"][k] - g["offpos"][k] for k in range(3)]
        result.append((REF[0] + d[0] / UNITS_PER_METRE, REF[1] + d[2] / UNITS_PER_METRE, REF[2] - d[1] / UNITS_PER_METRE,
                       g["torso"]))
    return result


def grid(step=0.035, n=4):
    r = [k * step for k in range(-n, n + 1)]
    return [(x, y, z) for x in r for y in r for z in r]


def mounted(agent):
    cen = centres(agent)
    cmds = setup()
    samples = []
    for i, c in enumerate(CONFIGS):
        cx, cy, cz, _ = cen[i]
        cmds += config_cmds(c)
        for j, (dx, dy, dz) in enumerate(grid()):
            tag = f"m{i}_{j}"
            samples.append((tag, i, (cx + dx, cy + dy, cz + dz)))
            cmds += [f"vr_mock_hand off {cx + dx:.4f} {cy + dy:.4f} {cz + dz:.4f} {HAND_ROT}"] + waits(24) + [
                f"vr_flashlight_probe {tag}", "vr_mock_button off grip 1"] + waits(2) + [
                f"vr_flashlight_probe {tag}p", "vr_mock_button off grip 0", "vr_flashlight 0"] + waits(2) + [
                "vr_flashlight 1"] + waits(2)
    return judge(run(agent, "fg_mounted", cmds), samples, "off", 0, cen)


def returning(agent):
    """The off hand takes the lamp, holds it out and lets go; the main hand, open and still at a spot near the lamp's
    way home, grips k frames later."""
    cen = centres(agent)
    cmds = setup()
    samples = []
    for i, c in enumerate(CONFIGS[:5]):  # the head's turns (the main hand is the catcher)
        cx, cy, cz, _ = cen[i]
        cmds += config_cmds(c)
        out = (cx + 0.05, cy + 0.12, cz - 0.30)  # held out in front
        for j, f in enumerate([0.55, 0.75, 0.9]):
            for dx in (-0.04, 0.0, 0.04):
                for k in (4, 8, 12, 18, 26):
                    q = [out[a] + (b - out[a]) * f for a, b in enumerate((cx, cy, cz))]
                    q[0] += dx + 0.03  # beside the off hand's way, not in it
                    tag = f"r{i}_{j}_{dx:+.2f}_{k}"
                    samples.append((tag, i, tuple(q)))
                    cmds += [f"vr_mock_hand main {q[0]:.4f} {q[1]:.4f} {q[2]:.4f} -80 0 0",
                             f"vr_mock_hand off {cx:.4f} {cy:.4f} {cz:.4f} {HAND_ROT}"] + waits(24) + [
                        "vr_mock_button off grip 1"] + waits(3) + [
                        f"vr_mock_hand off {out[0]:.4f} {out[1]:.4f} {out[2]:.4f} {HAND_ROT}"] + waits(24) + [
                        "vr_mock_button off grip 0"] + waits(k) + [
                        f"vr_flashlight_probe {tag}", "vr_mock_button main grip 1"] + waits(2) + [
                        f"vr_flashlight_probe {tag}p", "vr_mock_button main grip 0", "vr_flashlight 0"] + waits(2) + [
                        "vr_flashlight 1"] + waits(2)
                cmds += config_cmds(c)
    return judge(run(agent, "fg_returning", cmds), samples, "main", 1, cen)


FPS = 72  # the mock's fixed frames (vr_fixed_frames 1: 1/72 s each)
SCALES = {  # the time scales the timing test runs at
    "1": [],
    "timescale0.3": ["vr_timescale 0.3"],
    "bullettime": ["vr_bullettime_duration 100000", "vr_bullettime"],
}


def path(a, b, speed):
    """The hand's places, one per frame, from a to b at `speed` m/s (b the last)."""
    d = sum((b[k] - a[k]) ** 2 for k in range(3)) ** 0.5
    n = max(1, round(d / (speed / FPS)))
    return [tuple(a[k] + (b[k] - a[k]) * (i / n) for k in range(3)) for i in range(1, n + 1)]


def hand_cmd(p):
    return f"vr_mock_hand off {p[0]:.4f} {p[1]:.4f} {p[2]:.4f} {HAND_ROT}"


def trial(tag, start, frames, press, hold=50):
    """The off hand open and still at `start`, then one place a frame (`frames`); its grip pressed at frame `press`
    (counted from the first move; past the end: that many frames after it stopped) and held `hold` frames after the
    last move, then probed."""
    cmds = [hand_cmd(start), "vr_mock_button off grip 0"] + waits(30)
    total = max(len(frames), press + 1)
    for i in range(total):
        if i == press:
            cmds.append("vr_mock_button off grip 1")
        if i < len(frames):
            cmds.append(hand_cmd(frames[i]))
        cmds.append("wait")
    return cmds + waits(hold) + [f"vr_flashlight_probe {tag}", "vr_mock_button off grip 0", "vr_flashlight 0"] + waits(
        2) + ["vr_flashlight 1"] + waits(2)


def timing(agent):
    """"Highlighted implies grabbing works" at any timing and speed, also in slow motion: the off hand reaches the lamp
    from 33 cm out in front at several speeds, its grip pressed at several moments round its arrival (on its way, from
    20 cm out, to after it stopped), and stays there with the grip held: it must hold the lamp. Controls that must not
    take it: a punch out of the guard and back with the grip pressed on the way out; a grip pressed at the lamp, then the
    hand taken 15 cm away; a grip pressed 33 cm out (a hand that reaches the lamp already closed)."""
    cen = centres(agent)
    ok = True
    for scale, extra in SCALES.items():
        cmds = setup() + extra + waits(90) + ["vr_slowmo_probe"]
        samples = []  # (tag, should take)
        for i in (0, 3, 4):  # the head's yaw 0, 45, -45
            cx, cy, cz, _ = cen[i]
            c = (cx, cy, cz)
            cmds += config_cmds(CONFIGS[i])
            start = (cx + 0.05, cy + 0.12, cz - 0.30)
            for speed in (0.4, 0.8, 1.5, 2.2):
                frames = path(start, c, speed)
                step = speed / FPS
                for k in (-14, -6, -2, 0, 2, 6, 12, 30):
                    if -k * step > 0.2:
                        continue  # pressed more than 20 cm out (a control below)
                    press = len(frames) - 1 + k
                    tag = f"t{scale}_{i}_{speed}_{k}"
                    samples.append((tag, True))
                    cmds += trial(tag, start, frames, press)
            # Controls. A punch from the guard at the lamp, out 35 cm at 3 m/s and back, the grip pressed 2 frames in.
            out = (cx + 0.02, cy + 0.08, cz - 0.35)
            punch = path(c, out, 3.0) + path(out, c, 3.0)
            tag = f"c{scale}_{i}_punch"
            samples.append((tag, False))
            cmds += trial(tag, c, punch, 2)
            # Pressed at the lamp while moving through it, then 15 cm past it.
            away = (cx - 0.15, cy, cz)
            tag = f"c{scale}_{i}_away"
            samples.append((tag, False))
            cmds += trial(tag, start, frames_through(start, c, away), len(path(start, c, 1.5)) - 1)
            # Pressed 33 cm out, before the reach (the hand comes to the lamp closed).
            tag = f"c{scale}_{i}_closed"
            samples.append((tag, False))
            cmds += trial(tag, start, path(start, c, 0.4), 0)
        lines = run(agent, "fg_timing", cmds)
        got = parse(lines)
        with open(f"{KIT}/bases/{agent}/qbase/qconsole.log", encoding="latin-1") as f:
            probe = [l.split()[2] for l in f if l.startswith("probe scale")]
        bad = []
        taken = 0
        for tag, want in samples:
            g = got.get(tag)
            took = bool(g) and g["mode"] == "held" and g["holder"] == 0
            taken += took
            if took != want:
                bad.append(tag + (" (no probe)" if not g else ""))
        pos = sum(1 for _, w in samples if w)
        print(f"  scale {scale} (in effect: {probe[:1]}): {len(samples)} trials ({pos} reaches, {len(samples) - pos} controls), taken {taken}, "
              f"wrong {len(bad)} {bad[:8]}")
        ok = ok and not bad
    return ok


def frames_through(a, b, c):
    return path(a, b, 1.5) + path(b, c, 1.5)


def judge(lines, samples, hand, handIndex, cen):
    got = parse(lines)
    lit = taken = bad = litNotTaken = takenNotLit = 0
    drawnOff = [0, 0]  # [drawn at the lamp, unlit; lit, drawn away from it]
    why = {}
    perConfig = {}
    for tag, i, pos in samples:
        pre, post = got.get(tag), got.get(tag + "p")
        if not pre or not post or hand not in pre:
            bad += 1
            continue
        h = pre[hand]
        if not h["empty"]:
            bad += 1
            continue
        took = post["mode"] == "held" and post["holder"] == handIndex
        lit += h["lit"]
        taken += took
        pc = perConfig.setdefault(i, [0, 0, 0])
        pc[0] += h["lit"]
        pc[1] += took
        if h["lit"] and not took:
            litNotTaken += 1
            pc[2] += 1
            reason = ("game hotspot %d" % h["hotspot"]) if h["game"] else "not at (game's hands)" if not h["at"] else \
                "moving" if not h["still"] else "other"
            why[reason] = why.get(reason, 0) + 1
            if litNotTaken <= 6:
                print(f"  FAIL {tag} hand {pos[0]:.3f} {pos[1]:.3f} {pos[2]:.3f} torso {pre.get('torso', 0):.1f}: {reason}")
        if took and not h["lit"]:
            takenNotLit += 1
        if h.get("drawn", 0) != h["lit"] and h["at"] == h["lit"]:  # (the lamp judged right, the hand drawn elsewhere)
            drawnOff[h["lit"]] += 1
    print(f"{len(samples)} spots ({bad} unusable), lit {lit}, taken {taken}; lit and not taken {litNotTaken} {why}; "
          f"taken unlit {takenNotLit}; hand drawn at the unlit lamp {drawnOff[0]}, drawn off the lit one {drawnOff[1]}")
    for i in sorted(perConfig):
        c = CONFIGS[i]
        print(f"  torso {cen[i][3]:6.1f} (head {c[0]} {c[1]}, main {c[2] or 'standing'}): lit {perConfig[i][0]}, "
              f"taken {perConfig[i][1]}, lit not taken {perConfig[i][2]}")
    return litNotTaken == 0 and lit > 0


def main():
    agent = sys.argv[1]
    what = sys.argv[2] if len(sys.argv) > 2 else "all"
    ok = True
    if what in ("mounted", "all"):
        print("== mounted (on the body)")
        ok = mounted(agent) and ok
    if what in ("returning", "all"):
        print("== returning (let go, springing home)")
        ok = returning(agent) and ok
    if what in ("timing", "all"):
        print("== timing (a reach, the grip pressed round the arrival; slow motion)")
        ok = timing(agent) and ok
    print("PASS" if ok else "FAIL")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
