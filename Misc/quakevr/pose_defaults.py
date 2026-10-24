#!/usr/bin/env python3
"""pose_defaults.py -- the author's Pose Studio poses, strike keys and grips as shipped defaults.

Input: his config (quakevr/ironwail.cfg: the archived vr_agrunt_poses and vr_agrunt_grips) and/or a Pose Studio Done
file (quakevr/proposed_defaults/agrunt_poses_<time>.txt or agrunt_grips_<time>.txt: the dumps' rows). With both, the
two are compared (a key or grip that differs is reported) and the config is used: it holds his latest edits.

Output: the source rows that ship them (ROUND22.md, "His Pose Studio poses and strikes as defaults"):

  - Quake/vr/vr_agpose.cpp `shipped[]`: the shipped keys with his keys laid in (a key replaces the shipped one of its
    code: agpose::get takes the archived key, else the shipped one; an all-zero key drops the row: QC's pose as it is);
  - Quake/vr/vr_aggrip.cpp `globalDefaults` (his Every Weapon grips, "*") and `defaults` (a weapon's own): the rows
    of the same kind and hand (and weapon) replaced, new ones added (aggrip::offset takes the archived entry, else the
    default: a replacement too).

Without --write the new blocks are printed for review; --write puts them into the two files. Afterwards his archived
cvars equal the defaults (the studio's set() drops an entry equal to its default the next time it stores one; Reset
Pose / Reset Grips fall back to these); clearing them is his choice (vr_agrunt_poses ""; vr_agrunt_grips "").

Usage: python Misc/quakevr/pose_defaults.py [--cfg quakevr/ironwail.cfg] [--done <Done file> ...] [--write]
"""

import argparse
import os
import re
import sys

NUM_PARTS = 7
TURN_PARTS = (1, 3, 6)  # RTurn, LTurn, Torso: degrees (a tenth); the rest units (a hundredth)

CLASSES = ["fists", "a gun", "a one-handed melee weapon", "a two-handed melee weapon", "a chainsaw",
           "two melee weapons", "two guns"]
POSES = {0: "Idle / Carry", 1: "Aim", 2: "Run", 3: "Pickup", 10: "Jab", 11: "Cross", 12: "Hook", 13: "Uppercut",
         14: "Overhead", 15: "Diagonal", 16: "Horizontal", 17: "Backhand", 18: "Saw Thrust", 19: "Dual Strike"}
PHASES = ["wind-up", "blow", "follow-through"]
HANDS = {"Main": 0, "Off": 1, "Second": 2}
HAND_NAMES = {v: k for k, v in HANDS.items()}

CVAR = re.compile(r'^(vr_agrunt_poses|vr_agrunt_grips)\s+"([^"]*)"')
POSE_ROW = re.compile(r'^\s*\{(\d+),\s*\{([^}]*)\}\}')
GLOBAL_ROW = re.compile(r'^\s*\{(-?\d+),\s*aggrip::(\w+),\s*([^}]*)\}')
WEAPON_ROW = re.compile(r'^\s*\{(-?\d+),\s*"([^"]+)",\s*aggrip::(\w+),\s*([^}]*)\}')


def floats(text):
    return [float(x.strip().rstrip("f")) for x in text.split(",") if x.strip()]


def fmt(x):
    s = ("%.4f" % x).rstrip("0")
    if s in ("-0.", "-"):
        s = "0."
    return s + "f"


def pose_name(code):
    kind, cls, pose, phase = code // 10000, (code // 1000) % 10, (code // 10) % 100, code % 10
    if pose == 3 and phase == 1:
        # (vr_aglegs: the pickup's legs, agpose::legCode: pelvis place, pelvis turn, planted foot, kneeling knee's pole,
        # kneeling foot, planted knee's pole; the same 21 numbers as any key)
        return "%s, the pickup's legs (any weapon)" % ("grunt" if kind == 0 else "enforcer")
    name = POSES.get(pose, "?")
    if cls == 0 and pose == 0:
        name = "Idle"
    elif cls == 3 and pose == 0:
        name = "Port Arms"
    elif pose == 1:
        name = "Guard" if cls == 0 else "Aim" if cls in (1, 6) else "Ready"
    text = "%s, %s, %s" % ("grunt" if kind == 0 else "enforcer", CLASSES[cls] if cls < len(CLASSES) else "?", name)
    return text + (", " + PHASES[phase] if pose >= 10 and phase < 3 else "")


def read_cfg(path):
    poses, grips = {}, {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = CVAR.match(line)
            if not m:
                continue
            for entry in m.group(2).split(";"):
                parts = entry.split()
                if not parts:
                    continue
                if m.group(1) == "vr_agrunt_poses" and len(parts) == 1 + NUM_PARTS * 3:
                    poses[int(parts[0])] = [float(x) for x in parts[1:]]
                elif m.group(1) == "vr_agrunt_grips" and len(parts) == 9:
                    grips[(int(parts[0]), parts[1], int(parts[2]))] = [float(x) for x in parts[3:]]
    return poses, grips


def read_done(path):
    poses, grips = {}, {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if line.lstrip().startswith("//"):
                continue
            m = POSE_ROW.match(line)
            if m:
                poses[int(m.group(1))] = floats(m.group(2))
                continue
            m = WEAPON_ROW.match(line)
            if m:
                grips[(int(m.group(1)), m.group(2), HANDS[m.group(3)])] = floats(m.group(4))
                continue
            m = GLOBAL_ROW.match(line)
            if m:
                grips[(int(m.group(1)), "*", HANDS[m.group(2)])] = floats(m.group(3))
    return poses, grips


def close(a, b):
    return len(a) == len(b) and all(abs(x - y) < 0.006 for x, y in zip(a, b))


def compare(what, cfg, done, namer):
    differ = 0
    for k in sorted(set(cfg) | set(done), key=str):
        if k not in cfg:
            print("// %s %s: only in the Done file (not in the cfg: reset or undone since; left out)" % (what, namer(k)), file=sys.stderr)
            differ += 1
        elif k not in done:
            print("// %s %s: only in the cfg (edited after the Done; the cfg's taken)" % (what, namer(k)), file=sys.stderr)
            differ += 1
        elif not close(cfg[k], done[k]):
            print("// %s %s: differs (the cfg's taken): cfg %s / Done %s" % (what, namer(k), cfg[k], done[k]), file=sys.stderr)
            differ += 1
    print("// %s: cfg %d, Done %d, %d differ" % (what, len(cfg), len(done), differ), file=sys.stderr)


def block(text, start_pat):
    """(start, end) of the lines of the constexpr array whose first line matches start_pat: its rows, up to '};'."""
    lines = text.split("\n")
    for i, line in enumerate(lines):
        if re.match(start_pat, line):
            for j in range(i + 1, len(lines)):
                if lines[j].startswith("};"):
                    return lines, i + 1, j
    sys.exit("pose_defaults.py: no block %r" % start_pat)


def pose_block(src_lines, a, b, new):
    shipped = {}
    for line in src_lines[a:b]:
        m = POSE_ROW.match(line)
        if m and int(m.group(1)) >= 0:
            shipped[int(m.group(1))] = floats(m.group(2))
    changed = {c for c, v in new.items() if c not in shipped or not close(shipped[c], v)}
    for c, v in new.items():
        if all(x == 0 for x in v):
            shipped.pop(c, None)
        else:
            shipped[c] = v
    out = []
    for c in sorted(shipped):
        out.append("    // %s" % pose_name(c))
        out.append("    {%d, {%s}}," % (c, ", ".join(fmt(x) for x in shipped[c])))
    out.append("    {-1, {}}, // (the end)")
    return out, shipped, changed


def grip_rows(src_lines, a, b, new, weapon):
    """The block's lines with `new`'s rows put in: a row of the same key replaced (its comment kept), others added."""
    out, seen, changed = [], set(), set()
    pat = WEAPON_ROW if weapon else GLOBAL_ROW
    for line in src_lines[a:b]:
        m = pat.match(line)
        if not m:
            out.append(line)
            continue
        key = (int(m.group(1)), m.group(2), HANDS[m.group(3)]) if weapon else (int(m.group(1)), "*", HANDS[m.group(2)])
        seen.add(key)
        if key in new:
            old = floats(m.group(4) if weapon else m.group(3))
            comment = line[line.index("//"):] if "//" in line[m.end():] else ""
            line = "    " + row(key, new[key], weapon) + (" " + comment if comment else "")
            if not close(old, new[key]):
                changed.add(key)
        out.append(line)
    for key in sorted(k for k in new if k not in seen):
        out.append("    " + row(key, new[key], weapon))
        changed.add(key)
    return out, changed


def row(key, v, weapon):
    kind, ident, hand = key
    nums = ", ".join(("%.2ff" if i < 3 else "%.1ff") % x for i, x in enumerate(v))
    if weapon:
        return '{%d, "%s", aggrip::%s, %s},' % (kind, ident, HAND_NAMES[hand], nums)
    return "{%d, aggrip::%s, %s}," % (kind, HAND_NAMES[hand], nums)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    src = os.path.join(here, "..", "..", "Quake", "vr")
    ap = argparse.ArgumentParser()
    ap.add_argument("--cfg", help="his config (vr_agrunt_poses, vr_agrunt_grips)")
    ap.add_argument("--done", nargs="*", default=[], help="Pose Studio Done files (agrunt_poses_*.txt, agrunt_grips_*.txt)")
    ap.add_argument("--src", default=src, help="the Quake/vr folder")
    ap.add_argument("--write", action="store_true", help="put the new rows into vr_agpose.cpp and vr_aggrip.cpp")
    args = ap.parse_args()
    if not args.cfg and not args.done:
        ap.error("give --cfg and/or --done")

    done_poses, done_grips = {}, {}
    for d in args.done:
        p, g = read_done(d)
        done_poses.update(p)
        done_grips.update(g)
    if args.cfg:
        poses, grips = read_cfg(args.cfg)
        if args.done:
            compare("pose key", poses, done_poses, lambda c: "%d (%s)" % (c, pose_name(c)))
            compare("grip", grips, done_grips, lambda k: "%d %s %s" % (k[0], k[1], HAND_NAMES.get(k[2], k[2])))
    else:
        poses, grips = done_poses, done_grips

    pose_path = os.path.join(args.src, "vr_agpose.cpp")
    grip_path = os.path.join(args.src, "vr_aggrip.cpp")
    with open(pose_path, encoding="utf-8", newline="") as f:
        pose_text = f.read()
    with open(grip_path, encoding="utf-8", newline="") as f:
        grip_text = f.read()

    lines, a, b = block(pose_text, r"^constexpr ShippedRow shipped\[\] = \{")
    rows, shipped, changed = pose_block(lines, a, b, poses)
    pose_text = "\n".join(lines[:a] + rows + lines[b:])

    glob = {k: v for k, v in grips.items() if k[1] == "*"}
    weap = {k: v for k, v in grips.items() if k[1] != "*"}
    lines, a, b = block(grip_text, r"^constexpr GlobalRow globalDefaults\[\] = \{")
    grows, gchanged = grip_rows(lines, a, b, glob, False)
    grip_text = "\n".join(lines[:a] + grows + lines[b:])
    lines, a, b = block(grip_text, r"^constexpr Row defaults\[\] = \{")
    wrows, wchanged = grip_rows(lines, a, b, weap, True)
    grip_text = "\n".join(lines[:a] + wrows + lines[b:])

    counts = {}
    for c in changed:
        k = (c // 10000, (c // 1000) % 10, (c // 10) % 100)
        counts[k] = counts.get(k, 0) + 1
    print("// pose keys: %d shipped, %d new or changed" % (len(shipped), len(changed)), file=sys.stderr)
    for (kind, cls, pose), n in sorted(counts.items()):
        name = pose_name(kind * 10000 + cls * 1000 + pose * 10).replace(", " + PHASES[0], "")
        print("//   %s: %d key%s" % (name, n, "s" if n > 1 else ""), file=sys.stderr)
    print("// grips: %d Every Weapon and %d weapon rows changed" % (len(gchanged), len(wchanged)), file=sys.stderr)
    if weap:
        print("// (a weapon's own default applies with vr_agarms 0 only: vr_aggrip.cpp defaultOf; with vr_agarms 1 the "
              "archived entry is what holds it)", file=sys.stderr)

    if args.write:
        with open(pose_path, "w", encoding="utf-8", newline="") as f:
            f.write(pose_text)
        with open(grip_path, "w", encoding="utf-8", newline="") as f:
            f.write(grip_text)
        print("// written: %s, %s" % (pose_path, grip_path), file=sys.stderr)
    else:
        print("// == vr_agpose.cpp shipped[] ==")
        print("\n".join(rows))
        print("// == vr_aggrip.cpp globalDefaults ==")
        print("\n".join(grows))
        print("// == vr_aggrip.cpp defaults ==")
        print("\n".join(wrows))


if __name__ == "__main__":
    main()
