#!/usr/bin/env python3
"""groups_data.py -- quakevr/data/groups.txt, the monster groups' values as data (design review 3, step 4;
ROUND23.md, "Monster groups as data"). The groups' twin of monsters_data.py.

  python Misc/quakevr/groups_data.py convert
      Once (done 2026-11-01): took each group's defaults out of vr_mgroups.inc (QVR_MGROUP(id, label, kdtrig, resist,
      shove, ...): left QVR_MGROUP(id, label)) and wrote quakevr/data/groups.txt (a block per group, every key of it).
      Run on a tree that still has them.

  python Misc/quakevr/groups_data.py promote <config> [--dry]
      His tweaks become the defaults: each config line naming a group value by its old cvar name
      (`vr_mgrp_ogre_poise "1.4"`, `vr_knockdown_chance_army "0.5"`, `vr_kdtrig_resist_ogre "2"`) written into its
      group's block in groups.txt (the value replaced in place). --dry prints what it would change.

  python Misc/quakevr/groups_data.py check
      Parses groups.txt against vr_mgroups.inc and KEYS: every group's block with every key, no unknown key or group,
      no malformed line.

  python Misc/quakevr/groups_data.py names
      Every value's old cvar name, one a line (the groups' order, then KEYS'): a console script that prints them all.

groups.txt: a `defaults { key "value" ... }` block (a key's value for a group whose block leaves it out), then a
`group <id> { key "value" ... }` block per group. `//` comments anywhere.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DATA = os.path.join(ROOT, "quakevr", "data", "groups.txt")
INC = os.path.join(ROOT, "Quake", "vr", "vr_mgroups.inc")

# The keys, in vr_mgroups.hpp Param's order (vr_mgroups.cpp paramKeys: keep them alike).
KEYS = ["kdtrig", "resist", "ledge", "stairs", "slope", "shotgun", "wounded", "blow", "blast", "thrown", "tackle",
        "shove", "push", "stagger", "sweep", "leggrab", "lift", "necksnap", "manhandle", "throw", "hold", "poise",
        "health", "kdscale", "downtime", "getup", "snapkills", "struggle", "struggle_strength", "struggle_speed",
        "struggle_arms", "struggle_hurt", "skewer"]

# A key's value for a group whose block leaves it out (groups.txt's defaults block, as convert writes it).
DEFAULTS = {k: "1" for k in KEYS}
DEFAULTS.update({"kdtrig": "0", "resist": "0", "shove": "0", "sweep": "0", "leggrab": "0", "lift": "0",
                 "necksnap": "0", "manhandle": "0", "throw": "0", "hold": "-1", "struggle": "0"})

KEY_HELP = {
    "kdtrig": "the ragdoll triggers take it (vr_mgrp_<id>_kdtrig: the humanoids')",
    "resist": "its resistance to them (vr_kdtrig_resist_<id>): every trigger's chance divided by it, its least damage "
              "times it; 0: by its size",
    "ledge": "... blast, thrown, tackle: each trigger's chance times this (vr_mgrp_<id>_<key>; 0: never)",
    "shove": "a shove's base chance to knock it down (vr_knockdown_chance_<id>; only a monster with a get-up of its own)",
    "push": "how far shoves, bashes and unbalancing hits move it, times this (0: bosses stand their ground)",
    "stagger": "hits' chance to unbalance it, times this",
    "sweep": "the leg sweep, the leg pull and lift take it (1 on, 0 never; the skull wizard's robe has no legs)",
    "lift": "... necksnap, manhandle: the lift, the neck snap, grabbing it staggered or weak take it (the humanoids)",
    "throw": "the two-hand throw: 0 never, 1 only when hurt, 2 always",
    "hold": "one hand's hold on it, 0..1 (-1: by its size)",
    "poise": "its Parry Poise (a parry's strength over it: blocked, flinch or stagger; above 1 also the melee's "
             "knockdown chances over it; -1 Off: a parry never staggers it; QC combat.qc VR_Parry_Outcome)",
    "health": "its health times this as it appears (with Monster Health on)",
    "kdscale": "every chance to knock it down times this (a sure one by this chance too)",
    "downtime": "its time down times this; getup: its get-up's speed times this",
    "snapkills": "the neck snap kills it (0: it only hurts, vr_foegrab_necksnap_tough)",
    "struggle": "held while knocked down, its members with a struggle profile struggle (vr_mgroups.cpp "
                "struggleProfiles), their strength, speed and arm drive times struggle_strength, _speed, _arms, "
                "weaker as they are hurt (struggle_hurt 1)",
}

VALUE_RE = re.compile(r'^(\s*)(\w+)\s+"([^"]*)"(\s*//.*)?$')


def old_name(group, key):
    if key == "resist":
        return "vr_kdtrig_resist_" + group
    if key == "shove":
        return "vr_knockdown_chance_" + group
    return "vr_mgrp_%s_%s" % (group, key)


def read_groups():
    return re.findall(r"^QVR_MGROUP\((\w+),", open(INC, encoding="utf-8").read(), re.M)


def convert():
    text = open(INC, encoding="utf-8").read()
    row = re.compile(r'^QVR_MGROUP\((\w+), ("[^"]*"), ([^)]*)\)(.*)$')
    out = ["// quakevr/data/groups.txt -- the monster groups' values: read at start into the group table",
           "// (Quake/vr/vr_mgroups.cpp; the groups, their labels and members: vr_mgroups.inc, vr_mgroups.cpp; the keys:",
           "// vr_mgroups.hpp Param). Each value is reached by its old cvar name (vr_mgrp_<id>_<key>; resist:",
           "// vr_kdtrig_resist_<id>; shove: vr_knockdown_chance_<id>): the config, the console, Combat > Monster Groups,",
           "// the QC. Written once from vr_mgroups.inc's defaults by Misc/quakevr/groups_data.py convert; edited by hand",
           "// since, or by its `promote <config>` (the player's changes, saved in the config by the old names, made the",
           "// defaults).",
           "//",
           "// `defaults { key \"value\" ... }`: a key's value for a group whose block leaves it out. Then `group <id> {",
           "// ... }`: its values (each its effect before the groups: 1 as it was, 0 never)."]
    for k in KEYS:
        if k in KEY_HELP:
            out.append("//   %-18s%s" % (k, KEY_HELP[k]))
    out += ["", "defaults", "{"] + ['    %s "%s"' % (k, DEFAULTS[k]) for k in KEYS] + ["}"]
    kept = []
    pending = []
    started = False
    groups = 0
    for line in text.split("\n"):
        m = row.match(line)
        if not m:
            if started and line.startswith("//"):
                pending.append(line)
            kept.append(line)
            continue
        started = True
        g, label, args, tail = m.groups()
        a = [s.strip() for s in args.split(",")]
        kdtrig, resist, shove, humanoid, throw, push, legs, poise, kdscale, downtime, getup, snapkills, struggle = a
        v = dict(DEFAULTS)
        v.update({"kdtrig": kdtrig, "resist": resist, "shove": shove, "lift": humanoid, "necksnap": humanoid,
                  "manhandle": humanoid, "throw": throw, "push": push, "sweep": legs, "leggrab": legs, "poise": poise,
                  "kdscale": kdscale, "downtime": downtime, "getup": getup, "snapkills": snapkills,
                  "struggle": struggle})
        note = tail.strip()
        out.append("")
        out += pending
        pending = []
        out += ["group %s // %s" % (g, label.strip('"')), "{"]
        for k in KEYS:
            c = " " + note if k == "poise" and "poise" in note else ""
            out.append('    %s "%s"%s' % (k, v[k], c))
        out.append("}")
        kept.append("QVR_MGROUP(%s, %s)" % (g, label))
        groups += 1
    os.makedirs(os.path.dirname(DATA), exist_ok=True)
    open(DATA, "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")
    open(INC, "w", encoding="utf-8", newline="\n").write("\n".join(kept))
    print(f"convert: {groups} groups, {groups * len(KEYS)} values -> {DATA}; vr_mgroups.inc's defaults taken out")


def parse_data():
    """groups.txt as blocks ({"g": id or None for defaults, "keys": {key: line index}}); fails on a malformed file."""
    lines = open(DATA, encoding="utf-8").read().split("\n")
    blocks = []
    cur = None
    for i, line in enumerate(lines):
        s = line.split("//", 1)[0].strip()
        if not s:
            continue
        if cur is None:
            m = re.match(r"^(defaults|group (\w+))$", s)
            if not m:
                sys.exit(f"groups.txt:{i + 1}: expected `defaults` or `group <id>`: {line}")
            cur = {"g": m.group(2), "open": None, "keys": {}}
            continue
        if cur["open"] is None:
            if s != "{":
                sys.exit(f"groups.txt:{i + 1}: expected {{")
            cur["open"] = i
            continue
        if s == "}":
            blocks.append(cur)
            cur = None
            continue
        m = VALUE_RE.match(line)
        if not m or m.group(2) not in KEYS:
            sys.exit(f"groups.txt:{i + 1}: not a known key's line: {line}")
        cur["keys"][m.group(2)] = i
    if cur is not None:
        sys.exit("groups.txt: a block not closed")
    return lines, blocks


def check():
    groups = read_groups()
    lines, blocks = parse_data()
    by_g = {b["g"]: b for b in blocks if b["g"]}
    bad = 0
    for g in groups:
        have = set(by_g[g]["keys"]) if g in by_g else set()
        for key in [k for k in KEYS if k not in have]:
            print(f"check: group {g}: no {key}")
            bad += 1
    for g in by_g:
        if g not in groups:
            print(f"check: group {g}: not in vr_mgroups.inc")
            bad += 1
    if bad:
        sys.exit(f"check: {bad} problems")
    print(f"check: {len(groups)} groups, {len(groups) * len(KEYS)} values: ok")


def same_number(a, b):
    try:
        x, y = float(a), float(b)
    except ValueError:
        return False
    return abs(x - y) <= 1e-5 * max(1.0, abs(x))


def promote(cfg, dry):
    where = {old_name(g, k): (g, k) for g in read_groups() for k in KEYS}
    lines, blocks = parse_data()
    by_g = {b["g"]: b for b in blocks if b["g"]}
    sets = 0
    changed = 0
    for line in open(cfg, encoding="utf-8", errors="replace"):
        m = re.match(r'^(\w+) "([^"]*)"\s*$', line.strip())
        if not m or m.group(1).lower() not in where:
            continue
        sets += 1
        g, key = where[m.group(1).lower()]
        i = by_g[g]["keys"][key]
        v = VALUE_RE.match(lines[i])
        if v.group(3) == m.group(2) or same_number(v.group(3), m.group(2)):
            continue
        print(f"{g} {key}: \"{v.group(3)}\" -> \"{m.group(2)}\"")
        lines[i] = f'{v.group(1)}{key} "{m.group(2)}"' + (v.group(4) or "")
        changed += 1
    if dry:
        print(f"promote: {sets} lines in {cfg}, {changed} would change (--dry: groups.txt not written)")
        return
    open(DATA, "w", encoding="utf-8", newline="\n").write("\n".join(lines))
    print(f"promote: {sets} lines in {cfg}, {changed} changed in {DATA}")


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    if sys.argv[1] == "convert":
        convert()
    elif sys.argv[1] == "promote" and len(sys.argv) >= 3:
        promote(sys.argv[2], "--dry" in sys.argv)
    elif sys.argv[1] == "check":
        check()
    elif sys.argv[1] == "names":
        for g in read_groups():
            for k in KEYS:
                print(old_name(g, k))
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
