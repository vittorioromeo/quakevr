#!/usr/bin/env python3
"""judo_test.py <agent> [cases...] -- the two-hand judo throw with the mock headset's hands (ROUND22.md, "Judo: the
throw's direction, held throws and lifts").

Each case is one game on vrfiringrange: a grunt spawned 60 units ahead (vr_test_spawn 0), both mock hands put on it and
gripped (+grabmain, +graboff), then a vr_mock_play motion pulls them (tracking space: x right, y up, -z forward).

  dir      pulls left, right, towards you and away (vr_foegrab_throw_away 1) with the hands at its shoulders, at its box's
           middle, low on it, and uneven (the main hand higher and faster aside): PASS each when the throw goes the
           pulled way ("foegrab: both hands turn ... to your left") and its ragdoll lies that side of where it stood
           ("throw trace done: ... lies <f> ahead, <l> to your left").
  hold     thrown and held on (vr_foegrab_throw_hold), dragged; off: let go (case_hold).
  lift     lifted, carried, broke free; thrown down; not an ogre, not too far, not off (case_lift).
  neck     the neck snap: a grunt's head twisted both ways and rolled, an enforcer's, a knocked-down, a staggering and a
           lifted grunt's (its ragdoll's head: vr_mock_hand_to ... ragdoll head, then 'rel' keys): snapped, held by 2
           hands, let go of with the grips; push-pulls (one hand forward, the other back) standing, wide, uneven, at its
           neck, knocked down (at its head and at its neck): snapped; not an ogre, the chest, a slow twist or push-pull,
           the option off; a pull or a lift with both hands at its head: nothing (no judo, no lift) (case_neck).

Prints PASS/FAIL per case and the FAIL count; the logs' lines go to scratch/judo_test_<case>.txt (overwritten).
"""
import math
import os
import re
import subprocess
import sys

KIT = "C:/OHWorkspace/qvr-kit"
AGENT = sys.argv[1] if len(sys.argv) > 1 else "foegrab3"
TREE = f"C:/OHWorkspace/qvr-agents/{AGENT}"
CASES = sys.argv[2:] or ["dir"]
MOTIONS = os.path.join(TREE, "quakevr", "motions")  # (git-ignored: quakevr/.gitignore)
SCRATCH = os.path.join(TREE, "scratch")
PRE = ("sv_random_seed 7;map vrfiringrange;wait30;god;notarget;developer 1;vr_foegrab_debug 1;vr_foegrab_leniency 6;"
       "vr_test_spawn 0;vr_test_spawn_dist 36;impulse 241;wait40" +
       (";" + os.environ["JUDO_EXTRA"] if os.environ.get("JUDO_EXTRA") else ""))  # (JUDO_EXTRA: more commands)

# The grunt's box (its origin 1.93 m ahead; the hand's point 0.896 m up in tracking space is 20 units over its origin,
# 31 units a metre): its shoulders, its box's middle (origin + 8), low (origin - 4).
HEIGHTS = {"high": 1.02, "mid": 0.51, "low": 0.35}
NEAR = -1.15   # the hands' depth (on its near side)
GAP = 0.09     # each hand this far from the middle, aside
PULL = 0.45    # metres pulled in PULL_T seconds (3 m/s)
PULL_T = 0.15
WAYS = {"left": (-1, 0), "right": (1, 0), "towards": (0, 1), "away": (0, -1)}  # (x, z) in tracking space
WORDS = {"left": "to your left", "right": "to your right", "towards": "towards you", "away": "away from you"}

FAILS = 0
ONLY = os.environ.get("JUDO_ONLY", "")  # a regex: only the cases whose names (judo_<height>_<way>) match


def verdict(ok, what):
    global FAILS
    print(f"  {'PASS' if ok else 'FAIL'} {what}")
    if not ok:
        FAILS += 1


def run(script, filt, timeout=300, extra=()):
    cmd = ["C:/Program Files/Git/bin/bash.exe", f"{KIT}/run.sh", AGENT, "-Script", script, "-Filter", filt, "-Timeout", str(timeout), *extra]
    out = subprocess.run(cmd, capture_output=True, text=True, errors="replace").stdout
    return out.replace("\r", "")


def motion(name, keys):
    """Writes quakevr/motions/<name>.mock from (t, line) pairs; returns its path for vr_mock_play."""
    os.makedirs(MOTIONS, exist_ok=True)
    with open(os.path.join(MOTIONS, name + ".mock"), "w") as f:
        for t, line in keys:
            f.write(f"{t:.3f} {line}\n")
    return f"quakevr/motions/{name}.mock"


def hands_at(t, main, off):
    return [(t, "main %.3f %.3f %.3f" % main), (t, "off %.3f %.3f %.3f" % off)]


def pull_motion(name, way, height, uneven=False, grab_t=0.3, pull_at=0.8, extra_keys=()):
    """Both hands on the grunt, gripped at grab_t, pulled `way` at pull_at; uneven: the main hand 0.25 m higher and
    moving 1.6 times as far aside (the hands' different speeds at different heights: the old turn's fragile term)."""
    y = HEIGHTS[height]
    dx, dz = WAYS[way]
    main0 = (GAP, y + (0.25 if uneven else 0.0), NEAR - 0.08)  # (its fist sits a little nearer: deeper)
    off0 = (-GAP, y, NEAR)
    k = 1.6 if uneven else 1.0
    main1 = (main0[0] + dx * PULL * k, main0[1], main0[2] + dz * PULL)
    off1 = (off0[0] + dx * PULL, off0[1], off0[2] + dz * PULL)
    keys = hands_at(0.0, main0, off0)
    keys += [(grab_t, "cmd +grabmain"), (grab_t, "cmd +graboff")]
    keys += hands_at(pull_at, main0, off0)
    keys += hands_at(pull_at + PULL_T, main1, off1)
    keys += list(extra_keys)
    return motion(name, keys)


def case_dir():
    for height in ["high", "mid", "low", "uneven"]:
        for way in ["left", "right", "towards", "away"]:
            name = f"judo_{height}_{way}"
            if ONLY and not re.search(ONLY, name):
                continue
            path = pull_motion(name, way, "high" if height == "uneven" else height, uneven=height == "uneven")
            out = run(f"{PRE};vr_foegrab_throw_away 1;vr_mock_play {path};wait300;-grabmain;-graboff;wait5;"
                      "toggleconsole;quit",
                      "foegrab|throw trace|rror|CRASH")
            with open(os.path.join(SCRATCH, f"judo_test_{name}.txt"), "w") as f:
                f.write(out)
            flat = " ".join(out.split())
            m = re.search(r"both hands turn \S+ \(\d+\) (\d+) deg/s at ([\d.]+) m/s, (to your left|to your right|"
                          r"towards you|away from you)", flat)
            lie = re.search(r"lies ([-\d.]+) ahead, ([-\d.]+) to your left", flat)
            tip = re.search(r"throw trace: 0\.[12]\d s tilt \d+ deg.*?head ([-\d.]+) along the throw", flat)
            got = m.group(3) if m else "no throw"
            ahead, left = (float(lie.group(1)), float(lie.group(2))) if lie else (0.0, 0.0)
            early = float(tip.group(1)) if tip else 0.0
            # (Its start too, its head along the throw at 0.1-0.3 s; pulled towards you it passes through you:
            # vr_foegrab_throw_grace.)
            side_ok = {"left": left > 8, "right": left < -8, "towards": ahead < -8, "away": ahead > 8}[way]
            ok = got == WORDS[way] and early > 8 and side_ok and "rror" not in flat and "CRASH" not in flat
            verdict(ok, f"{height} {way}: thrown {got}" + (f" ({m.group(1)} deg/s, {m.group(2)} m/s)" if m else "") +
                    (f", tips {early:.0f} along at 0.2 s, lies {ahead:.0f} ahead, {left:.0f} left" if lie else ", no trace"))


def status(flat, after):
    """The 'holds ... on as a ragdoll' status line printed after `after` in the log: (hands, ahead, left, up) or None."""
    i = flat.find(after)
    m = re.search(r"on as a ragdoll \((?:thrown|lifted|snapped)\) with (\d) hand\(s\), [\d.]+ s: its pelvis ([-\d.]+) ahead, "
                  r"([-\d.]+) to his left, ([-\d.]+) up", flat[i:] if i >= 0 else "")
    return (int(m.group(1)), float(m.group(2)), float(m.group(3)), float(m.group(4))) if m else None


def case_hold():
    """hold: thrown left (hands at its shoulders) and held on (vr_foegrab_throw_hold 1): handed over to both hands, it
    still falls to the left, then the hands, lowered to it, drag it 0.5 m towards you over a second: its pelvis comes 4+
    units nearer.
    Held on off: let go as thrown, no ragdoll hold."""
    y = HEIGHTS["high"]
    for on in [1, 0]:
        name = f"judo_hold{on}"
        if ONLY and not re.search(ONLY, name):
            continue
        main1 = (GAP - PULL, y, NEAR - 0.08)
        off1 = (-GAP - PULL, y, NEAR)
        drag = [(1.55, "cmd echo MARK1"), (1.6, "cmd vr_foegrab_status")]
        # (Down to it first, crouched: from your chest the pull on it is mostly up.)
        low = HEIGHTS["low"]
        drag += hands_at(1.7, main1, off1)
        drag += hands_at(2.2, (main1[0], low, main1[2]), (off1[0], low, off1[2]))
        drag += hands_at(3.2, (main1[0], low, main1[2] + 0.5), (off1[0], low, off1[2] + 0.5))
        drag += [(3.45, "cmd echo MARK2"), (3.5, "cmd vr_foegrab_status")]
        path = pull_motion(name, "left", "high", extra_keys=drag)
        out = run(f"{PRE};vr_debug_ragdoll 1;vr_foegrab_throw_hold {on};vr_mock_play {path};wait500;-grabmain;-graboff;"
                  "wait5;toggleconsole;quit",
                  "foegrab|throw trace|held on by|limb let go|limb lost|getup:|MARK|rror|CRASH")
        with open(os.path.join(SCRATCH, f"judo_test_{name}.txt"), "w") as f:
            f.write(out)
        flat = " ".join(out.split())
        thrown = "both hands turn monster_army" in flat and "to your left" in flat
        handed = re.search(r"thrown, held on as a ragdoll by (\d) hands?", flat)
        lie = re.search(r"lies ([-\d.]+) ahead, ([-\d.]+) to your left", flat)
        left = float(lie.group(2)) if lie else 0.0
        s1, s2 = status(flat, "MARK1"), status(flat, "MARK2")
        bad = "rror" in flat or "CRASH" in flat
        if on:
            pulled = s1 and s2 and s1[1] - s2[1] > 4
            ok = thrown and handed and handed.group(1) == "2" and left > 8 and pulled and not bad
            verdict(ok, f"held on: thrown left {thrown}, held by {handed.group(1) if handed else 0} hands, lies {left:.0f} "
                        f"left; pelvis ahead {s1[1] if s1 else '?'} -> {s2[1] if s2 else '?'} "
                        f"(hands {s1[0] if s1 else 0}, {s2[0] if s2 else 0})")
        else:
            ok = thrown and not handed and s1 is None and left > 8 and not bad
            verdict(ok, f"held on off: thrown left {thrown}, not held ({'held' if handed else 'let go'}), lies {left:.0f} left")


def player_at(flat, after):
    """The player's origin from the first status after `after` (x, y) or None."""
    i = flat.find(after)
    m = re.search(r"foegrab status: player 1 at ([-\d.]+) ([-\d.]+) ([-\d.]+)", flat[i:] if i >= 0 else "")
    return (float(m.group(1)), float(m.group(2))) if m else None


def lift_motion(name, down_at=None):
    """Both hands at its shoulders, gripped, pulled up 0.4 m in 0.25 s (1.6 m/s) at 0.8 s; statuses at 1.6 s (MARK1) and
    3.6 s (MARK3, after it broke free at 2.8 s); down_at: swung down 0.6 m in 0.15 s from then and let go."""
    y = HEIGHTS["high"]
    main0, off0 = (GAP, y, NEAR - 0.08), (-GAP, y, NEAR)
    main1, off1 = (GAP, y + 0.4, NEAR - 0.08), (-GAP, y + 0.4, NEAR)
    keys = hands_at(0.0, main0, off0) + [(0.3, "cmd +grabmain"), (0.3, "cmd +graboff")]
    keys += hands_at(0.8, main0, off0) + hands_at(1.05, main1, off1)
    keys += [(1.55, "cmd echo MARK1"), (1.6, "cmd vr_foegrab_status")]
    if down_at:
        keys += hands_at(down_at, main1, off1)
        keys += hands_at(down_at + 0.15, (main1[0], y - 0.2, main1[2]), (off1[0], y - 0.2, off1[2]))
        keys += [(down_at + 0.15, "cmd -grabmain"), (down_at + 0.15, "cmd -graboff")]
    keys += [(3.55, "cmd echo MARK3"), (3.6, "cmd vr_foegrab_status")]
    return motion(name, keys)


def case_lift():
    """lift: a grunt (and an enforcer), hands at its shoulders, pulled up at 1.6 m/s: lifted (2 hands, flailing), its
    pelvis held up, then it breaks free at 2 s (vr_foegrab_lift_time) and shoves you back 24+ units. Thrown down: swung
    down and let go at 1.8 s: let go of, no break free. An ogre (vr_test_spawn 1): not lifted (not a humanoid). Lift Reach
    24 (it stands 36 off): not lifted. Lift off: no lift."""
    runs = [("grunt", 0, "", None), ("enforcer", 8, "", None), ("down", 0, "", 1.8), ("ogre", 1, "", None),
            ("far", 0, "vr_foegrab_lift_reach 24", None), ("off", 0, "vr_foegrab_lift 0", None)]
    for what, mon, extra, down in runs:
        name = f"judo_lift_{what}"
        if ONLY and not re.search(ONLY, name):
            continue
        path = lift_motion(name, down)
        pre = PRE.replace("vr_test_spawn 0;", f"vr_test_spawn {mon};")
        out = run(f"{pre};vr_knockdown_debug 1;{extra};vr_mock_play {path};wait600;-grabmain;-graboff;wait5;"
                  "toggleconsole;quit", "foegrab|flail:|held on by|MARK|rror|CRASH")
        with open(os.path.join(SCRATCH, f"judo_test_{name}.txt"), "w") as f:
            f.write(out)
        flat = " ".join(out.split())
        bad = "rror" in flat or "CRASH" in flat
        lifted = re.search(r"lifted, held on as a ragdoll by (\d) hands?", flat)
        flails = "it flails in the hands" in flat
        broke = re.search(r"breaks free after ([\d.]+) s lifted", flat)
        s1 = status(flat, "MARK1")
        p1, p3 = player_at(flat, "MARK1"), player_at(flat, "MARK3")
        moved = ((p3[0] - p1[0]) ** 2 + (p3[1] - p1[1]) ** 2) ** 0.5 if p1 and p3 else 0.0
        thrown = "both hands turn" in flat
        if what in ("grunt", "enforcer"):
            # (Held up: its pelvis over the player's origin's height less 5, 19+ units off the floor; lying, about -19.)
            ok = (lifted and lifted.group(1) == "2" and flails and s1 and s1[0] == 2 and s1[3] > -5 and broke and
                  1.9 <= float(broke.group(1)) <= 2.2 and moved >= 24 and not thrown and not bad)
            verdict(ok, f"{what} lifted by {lifted.group(1) if lifted else 0} hands, flails {flails}, pelvis "
                        f"{s1[3] if s1 else '?'} up at 0.5 s, broke free after {broke.group(1) if broke else '-'} s, "
                        f"you shoved {moved:.0f} units")
        elif what == "down":
            let = re.search(r"let go of after ([\d.]+) s held on", flat)
            ok = lifted and let and not broke and not bad
            verdict(ok, f"grunt lifted and thrown down: let go of after {let.group(1) if let else '-'} s, "
                        f"{'broke free (wrong)' if broke else 'no break free'}")
        elif what == "ogre":
            ok = not lifted and "no lift, not a humanoid" in flat and not bad
            verdict(ok, f"ogre: {'lifted (wrong)' if lifted else 'not lifted'} ({'not a humanoid' if 'not a humanoid' in flat else '?'})")
        elif what == "far":
            ok = not lifted and "no lift, too far from you" in flat and not bad
            verdict(ok, f"reach 24, it 36 off: {'lifted (wrong)' if lifted else 'not lifted'} ({'too far' if 'too far' in flat else '?'})")
        else:
            ok = not lifted and "both hands lift" not in flat and not bad
            verdict(ok, f"lift off: {'lifted (wrong)' if lifted else 'not lifted'}")


# The heads (positional damage's head zones, tracking space): a grunt's 23 units over its origin, 3 nearer you; an
# ogre's 33 over, 9 nearer. Each hand NECK_R from its middle, at its sides. Its chest: 5 units over its origin.
GRUNT_HEAD = (0.0, 1.39, -1.16)
OGRE_HEAD = (0.0, 1.71, -1.16)
GRUNT_CHEST = (0.0, 0.80, -1.10)
GRUNT_NECK = (0.0, 1.19, -1.16)      # (its holds' spots 15-17 units over its origin: below its head, over its shoulders)
GRUNT_SHOULDERS = (0.0, 1.09, -1.16)  # (one hold in its head zone, the other not: the lift is tried)
NECK_R = 0.12
RAG_NECK_R = 0.09  # (a ragdoll's head: both hands well within the limb grab's reach of it)


def twist_keys(t0, dur, deg, sign, centre, r, rel=False, axis="y", steps=10):
    """Both hands turned `deg` degrees about `centre` (tracking space) in `dur` s from t0, `sign` the way round: about
    the vertical (axis y: one hand forward, the other back) or about the forward axis (z: one up, the other down). rel:
    'rel' keys (from where the hands are: centre is then an offset)."""
    keys = []
    cx, cy, cz = centre
    for i in range(steps + 1):
        th = math.radians(deg) * i / steps * sign
        t = t0 + dur * i / steps
        c, s = r * math.cos(th), r * math.sin(th)
        main = (cx + c, cy, cz + s) if axis == "y" else (cx + c, cy + s, cz)
        off = (cx - c, cy, cz - s) if axis == "y" else (cx - c, cy - s, cz)
        if rel:
            keys += [(t, "rel main %.4f %.4f %.4f" % main), (t, "rel off %.4f %.4f %.4f" % off)]
        else:
            keys += hands_at(t, main, off)
    return keys


def pp_keys(t0, dur, cm, sign, centre, r, rel=False, off_share=1.0, steps=10):
    """A push-pull: both hands `r` m either side of `centre`, the main hand pushed `cm` forward (sign 1; back: -1) and
    the off hand pulled `cm` * off_share the other way in `dur` s from t0, straight, eased in and out (smoothstep)."""
    keys = []
    cx, cy, cz = centre
    for i in range(steps + 1):
        f = i / steps
        dz = cm / 100.0 * f * f * (3 - 2 * f) * sign
        t = t0 + dur * f
        main, off = (cx + r, cy, cz - dz), (cx - r, cy, cz + dz * off_share)
        if rel:
            keys += [(t, "rel main %.4f %.4f %.4f" % main), (t, "rel off %.4f %.4f %.4f" % off)]
        else:
            keys += hands_at(t, main, off)
    return keys


def turn_keys(t0, dur, deg, sign, centre, r, axis, rel=False):
    """axis y, z: twist_keys (`deg` degrees); pp, ppu (uneven: the off hand 0.4 as far), ppw (the hands 0.18 m out):
    pp_keys (`deg` cm)."""
    if axis.startswith("pp"):
        return pp_keys(t0, dur, deg, sign, centre, 0.18 if axis == "ppw" else r, rel, 0.4 if axis == "ppu" else 1.0)
    return twist_keys(t0, dur, deg, sign, centre, r, rel, axis)


def neck_standing_motion(name, centre, deg, dur, sign, axis="y", pull=None):
    """Both hands at the sides of `centre` (a head, a chest), gripped at 0.3 s, twisted at 0.8 s (turn_keys; or pulled
    `pull` at 3 m/s: a judo throw; "up": lifted 0.4 m at 1.6 m/s, then twisted at 1.3 s); statuses at 1.6 s (MARK1)
    and, the grips let go at 2.2 s, at 2.6 s (MARK2)."""
    cx, cy, cz = centre
    r = 0.18 if axis == "ppw" else NECK_R
    main0, off0 = (cx + r, cy, cz), (cx - r, cy, cz)
    keys = hands_at(0.0, main0, off0) + [(0.3, "cmd +grabmain"), (0.3, "cmd +graboff")]
    keys += hands_at(0.8, main0, off0)
    if pull == "up":
        keys += hands_at(1.05, (main0[0], cy + 0.4, cz), (off0[0], cy + 0.4, cz))
        keys += hands_at(1.3, (main0[0], cy + 0.4, cz), (off0[0], cy + 0.4, cz))
        keys += turn_keys(1.3, dur, deg, sign, (cx, cy + 0.4, cz), NECK_R, axis)
        keys += [(1.85, "cmd echo MARK1"), (1.9, "cmd vr_foegrab_status"), (2.3, "cmd -grabmain"),
                 (2.3, "cmd -graboff"), (2.65, "cmd echo MARK2"), (2.7, "cmd vr_foegrab_status")]
        return motion(name, keys)
    if pull == "uponly":
        keys += hands_at(1.05, (main0[0], cy + 0.4, cz), (off0[0], cy + 0.4, cz))
    elif pull in ("smoothturn", "snapturn", "twistturn"):
        # (The player turned with the stick, the hands still in play space: smooth at 0.6 of vr_turn_speed 3.25, about
        # 195 deg/s, for 0.5 s; snap: two flicks (vr_snap_turn 45 in `extra`); twistturn: smooth while twisting.)
        if pull == "snapturn":
            keys += [(0.8, "cmd vr_mock_stick main 1 0"), (0.9, "cmd vr_mock_stick main 0 0"),
                     (1.0, "cmd vr_mock_stick main 1 0"), (1.1, "cmd vr_mock_stick main 0 0")]
        else:
            keys += [(0.8, "cmd vr_mock_stick main 0.6 0"), (1.3, "cmd vr_mock_stick main 0 0")]
        if pull == "twistturn":
            keys += turn_keys(0.8, dur, deg, sign, centre, NECK_R, axis)
    elif pull:
        dx, dz = WAYS[pull]
        keys += hands_at(0.8 + PULL_T, (main0[0] + dx * PULL, cy, cz + dz * PULL), (off0[0] + dx * PULL, cy, cz + dz * PULL))
    else:
        keys += turn_keys(0.8, dur, deg, sign, centre, NECK_R, axis)
    keys += [(1.55, "cmd echo MARK1"), (1.6, "cmd vr_foegrab_status"), (2.2, "cmd -grabmain"), (2.2, "cmd -graboff"),
             (2.55, "cmd echo MARK2"), (2.6, "cmd vr_foegrab_status")]
    return motion(name, keys)


def neck_ragdoll_motion(name, deg, dur, sign, start=0.0, axis="y", part="head"):
    """From `start`: both hands put at its ragdoll's head (vr_mock_hand_to ... ragdoll head; `part` neck: its head's
    joint), moved RAG_NECK_R apart to its sides (the main hand right), gripped, twisted 0.35 s later (turn_keys);
    statuses (MARK1) 0.6 s after the twist, the grips let go 0.4 s after that, a status (MARK2) 0.35 s later."""
    s = start
    keys = [(s, f"cmd vr_mock_hand_to main ragdoll {part} 0"), (s, f"cmd vr_mock_hand_to off ragdoll {part} 0")]
    keys += [(s + 0.03, "rel main 0 0 0"), (s + 0.03, "rel off 0 0 0")]
    keys += [(s + 0.1, "rel main %.3f 0 0" % RAG_NECK_R), (s + 0.1, "rel off %.3f 0 0" % -RAG_NECK_R)]
    keys += [(s + 0.12, "cmd +grabmain"), (s + 0.12, "cmd +graboff")]
    keys += turn_keys(s + 0.35, dur, deg, sign, (0.0, 0.0, 0.0), RAG_NECK_R, axis, rel=True)
    m1 = s + 0.35 + dur + 0.6
    keys += [(m1, "cmd echo MARK1"), (m1 + 0.05, "cmd vr_foegrab_status"), (m1 + 0.4, "cmd -grabmain"),
             (m1 + 0.4, "cmd -graboff"), (m1 + 0.75, "cmd echo MARK2"), (m1 + 0.8, "cmd vr_foegrab_status")]
    return motion(name, keys)


def case_neck():
    """neck: the neck snap (vr_foegrab_necksnap). Both hands at a grunt's head (its head zone) twisted 75 degrees in
    0.2 s (375 deg/s) about the vertical, both ways, and about the forward axis (one hand up, the other down): its neck
    snapped standing (killed, counted), its body held on by both hands (MARK1: '(snapped) with 2 hand(s)'), let go of
    when the grips let go (MARK2: none). The same on a staggering grunt (a one-hand shove, vr_knockdown_test 21) and a
    knocked-down one (vr_knockdown_test 0), the hands at its ragdoll's head. Not: an ogre's head (not a humanoid), the
    hands at its chest, a twist too slow (75 degrees in 1 s), Neck Snap off. Push-pulls (pp: the main hand 20 cm
    forward, the off hand 20 cm back, or the other way, in 0.3 s, straight, eased): standing at its head, its neck, the
    hands wider (0.36 m apart, 15 cm each), uneven (25 cm and 10 cm), knocked down at its ragdoll's head both ways and
    at its neck: snapped; the same in 1.2 s: nothing. Both hands at its head pulled left, or up: neither thrown nor
    lifted (the neck snap the only move there); lifted by the shoulders (one hand in its head zone), then twisted: snapped.
    The player turning with the stick, the hands still on its head: smooth (about 195 deg/s) or snap (two 45s): nothing;
    a twist while smooth-turning: snapped."""
    runs = [  # (name, monster, centre or 'kd'/'stagger', deg, dur, sign, axis, pull, extra, want)
        ("grunt_cw", 0, GRUNT_HEAD, 75, 0.2, 1, "y", None, "", "snap"),
        ("grunt_ccw", 0, GRUNT_HEAD, 75, 0.2, -1, "y", None, "", "snap"),
        ("grunt_roll", 0, GRUNT_HEAD, 75, 0.2, 1, "z", None, "", "snap"),
        ("enforcer", 8, GRUNT_HEAD, 75, 0.2, 1, "y", None, "", "snap"),
        ("zombie", 2, GRUNT_HEAD, 75, 0.2, 1, "y", None, "", "snap"),
        ("knocked_cw", 0, "kd", 75, 0.2, 1, "y", None, "", "snap"),
        ("knocked_ccw", 0, "kd", 75, 0.2, -1, "y", None, "", "snap"),
        ("stagger", 0, "stagger", 75, 0.2, 1, "y", None,
         "vr_knockdown_chance 0.01;vr_ragdoll_active_stagger_travel 0.03", "snap"),
        ("lifted", 0, GRUNT_SHOULDERS, 75, 0.2, 1, "y", "up", "", "snap"),
        ("pp_fwd", 0, GRUNT_HEAD, 20, 0.3, 1, "pp", None, "", "snap"),
        ("pp_back", 0, GRUNT_HEAD, 20, 0.3, -1, "pp", None, "", "snap"),
        ("pp_wide", 0, GRUNT_HEAD, 15, 0.3, 1, "ppw", None, "", "snap"),
        ("pp_uneven", 0, GRUNT_HEAD, 25, 0.3, -1, "ppu", None, "", "snap"),
        ("pp_neck", 0, GRUNT_NECK, 20, 0.3, 1, "pp", None, "", "snap"),
        ("pp_knocked_fwd", 0, "kd", 20, 0.3, 1, "pp", None, "", "snap"),
        ("pp_knocked_back", 0, "kd", 20, 0.3, -1, "pp", None, "", "snap"),
        ("pp_knocked_neck", 0, "kdneck", 20, 0.3, 1, "pp", None, "", "snap"),
        ("pp_slow", 0, GRUNT_HEAD, 20, 1.2, 1, "pp", None, "", "none"),
        ("pp_knocked_slow", 0, "kd", 20, 1.2, 1, "pp", None, "", "none"),
        ("head_pull", 0, GRUNT_HEAD, 0, 0, 1, "y", "left", "", "none"),
        ("head_lift", 0, GRUNT_HEAD, 0, 0, 1, "y", "uponly", "", "none"),
        ("ogre", 1, OGRE_HEAD, 75, 0.2, 1, "y", None, "", "ogre"),
        ("chest", 0, GRUNT_CHEST, 75, 0.2, 1, "y", None, "", "none"),
        ("slow", 0, GRUNT_HEAD, 75, 1.0, 1, "y", None, "", "none"),
        ("off", 0, GRUNT_HEAD, 75, 0.2, 1, "y", None, "vr_foegrab_necksnap 0", "none"),
        # (The stick's turn is not the wrists': holding the head still while turning smooth or snap: nothing; a twist
        # while turning: snapped.)
        ("smooth_turn", 0, GRUNT_HEAD, 0, 0, 1, "y", "smoothturn", "vr_snap_turn 0", "none"),
        ("snap_turn", 0, GRUNT_HEAD, 0, 0, 1, "y", "snapturn", "vr_snap_turn 45", "none"),
        ("twist_turn", 0, GRUNT_HEAD, 75, 0.2, 1, "y", "twistturn", "vr_snap_turn 0", "snap"),
    ]
    for what, mon, centre, deg, dur, sign, axis, pull, extra, want in runs:
        name = f"judo_neck_{what}"
        if ONLY and not re.search(ONLY, name):
            continue
        pre = PRE.replace("vr_test_spawn 0;", f"vr_test_spawn {mon};")
        if centre in ("kd", "kdneck"):
            # (Knocked down where it stands, down 12 s: you walk up to it, its head in reach.)
            path = neck_ragdoll_motion(name, deg, dur, sign, axis=axis, part="neck" if centre == "kdneck" else "head")
            go = ("vr_knockdown_push 0;vr_knockdown_time_min 12;vr_knockdown_time_max 12;vr_knockdown_test 0;wait60;"
                  f"vr_mock_walk_to nearest monster_army 24;wait200;vr_mock_walk_to off;wait90;vr_mock_play {path}")
        elif centre == "stagger":
            path = neck_ragdoll_motion(name, deg, dur, sign)
            go = f"vr_knockdown_test 21;wait3;vr_mock_play {path}"
        else:
            path = neck_standing_motion(name, centre, deg, dur, sign, axis, pull)
            go = f"vr_mock_play {path}"
        out = run(f"{pre};vr_knockdown_debug 1;{extra};{go};wait500;-grabmain;-graboff;wait5;toggleconsole;quit",
                  "foegrab|necksnap:|held on by|ragdoll: a limb|stagger|MARK|rror|CRASH")
        with open(os.path.join(SCRATCH, f"judo_test_{name}.txt"), "w") as f:
            f.write(out)
        flat = " ".join(out.split())
        bad = "rror" in flat or "CRASH" in flat
        snap = re.search(r"both hands snap (\S+) \(\d+\)'s neck, twisted (\d+) deg at (\d+) deg/s \((standing|a ragdoll)\): "
                         r"(killed|[a-z ,()]+)", flat)
        died = re.search(r"necksnap: (\S+)'s neck snapped \(([a-z ]+), ([a-z ]+)\)", flat)
        killed = snap and snap.group(5).startswith("killed") and died
        s1, s2 = status(flat, "MARK1"), status(flat, "MARK2")
        let = re.search(r"let go of after ([\d.]+) s held on", flat)
        thrown = "both hands turn" in flat
        lifted = "both hands lift" in flat or "lifted, held on as a ragdoll" in flat
        how = (f"{snap.group(1)} snapped {snap.group(4)} ({died.group(2) if died else '?'}), {snap.group(2)} deg at "
               f"{snap.group(3)} deg/s" if snap else "no snap")
        if want == "snap":
            state_ok = (died and died.group(2) == {"kd": "knocked down", "kdneck": "knocked down",
                                                   "stagger": "staggering"}.get(
                centre, "knocked down" if pull == "up" else "standing"))
            ok = (killed and state_ok and s1 and s1[0] == 2 and "(snapped)" in flat and s2 is None and let and
                  not thrown and not bad)
            verdict(ok, f"{what}: {how}; held by {s1[0] if s1 else 0} hands at MARK1, after letting go "
                        f"{'none' if s2 is None else s2[0]}; let go of after {let.group(1) if let else '-'} s"
                        f"{' (thrown: wrong)' if thrown else ''}")
        elif want == "ogre":
            ok = not died and "no neck snap, not a humanoid" in flat and not bad
            verdict(ok, f"ogre: {'snapped (wrong)' if died else 'not snapped'} "
                        f"({'not a humanoid' if 'not a humanoid' in flat else '?'})")
        else:
            ok = not died and not snap and not thrown and not lifted and not bad
            verdict(ok, f"{what}: {how}{', thrown (wrong)' if thrown else ''}{', lifted (wrong)' if lifted else ''}")


def main():
    os.makedirs(SCRATCH, exist_ok=True)
    for c in CASES:
        print(f"== {c}")
        globals()[f"case_{c}"]()
    print(f"FAILS {FAILS}")
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
