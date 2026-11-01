#!/usr/bin/env python3
"""monsters_data.py -- quakevr/data/monsters.txt, the monsters' per-class values as data (design review 3, step 3;
ROUND23.md, "Monsters as data"). The monsters' twin of weapons_data.py and props_data.py.

  python Misc/quakevr/monsters_data.py convert
      Once (done 2026-11-01): took the per-monster cvar families out of vr_cvars.inc (vr_mhull_<m>,
      vr_smallgibs_mult_<m>, vr_smallgibs_brains_mult_<m>, vr_ragdoll_<m>_<key>), wrote quakevr/data/monsters.txt (a
      block per classname, their defaults and comments carried along) and Quake/vr/vr_monsterdata.inc (each value's
      old cvar name, its classname and its key). Run on a tree that still has them.

  python Misc/quakevr/monsters_data.py promote <config> [--dry]
      His tweaks become the defaults: each config line naming a monster value by its old cvar name
      (`vr_ragdoll_army_mass "75"`; the player's changes, saved as overrides of monsters.txt) written into its
      classname's block in monsters.txt (the value replaced in place). --dry prints what it would change.

  python Misc/quakevr/monsters_data.py check
      Parses monsters.txt against vr_monsterdata.inc: every value the .inc names in its block, no unknown key or
      classname, no malformed line.

  python Misc/quakevr/monsters_data.py names
      Every value's old cvar name, one a line (vr_monsterdata.inc's order): a console script that prints them all.

monsters.txt: a `defaults { key "value" ... }` block (a key's value for a classname whose block leaves it out), then a
`monster <classname> { key "value" ... }` block per classname. `//` comments anywhere.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CVARS = os.path.join(ROOT, "Quake", "vr", "vr_cvars.inc")
INC = os.path.join(ROOT, "Quake", "vr", "vr_monsterdata.inc")
DATA = os.path.join(ROOT, "quakevr", "data", "monsters.txt")

# A family's token -> the classname its values are for (the block they go in).
CLASSNAMES = {
    "army": "monster_army", "grunt": "monster_army",
    "enforcer": "monster_enforcer",
    "dog": "monster_dog",
    "demon": "monster_demon1", "fiend": "monster_demon1",
    "ogre": "monster_ogre",
    "knight": "monster_knight",
    "hknight": "monster_hell_knight", "hellknight": "monster_hell_knight",
    "shalrath": "monster_shalrath", "vore": "monster_shalrath",
    "shambler": "monster_shambler",
    "wizard": "monster_wizard", "scrag": "monster_wizard",
    "fish": "monster_fish",
    "zombie": "monster_zombie",
    "tarbaby": "monster_tarbaby",
    "gremlin": "monster_gremlin",
    "scourge": "monster_scourge", "centroid": "monster_scourge",
    "eel": "monster_eel",
    "mummy": "monster_mummy",
}
CLASS_ORDER = ["monster_army", "monster_enforcer", "monster_dog", "monster_demon1", "monster_ogre", "monster_knight",
               "monster_hell_knight", "monster_shalrath", "monster_shambler", "monster_wizard", "monster_fish",
               "monster_zombie", "monster_tarbaby", "monster_gremlin", "monster_scourge", "monster_eel",
               "monster_mummy"]
RAGDOLL_KEYS = ["start", "mass", "friction", "joint_friction", "joint_stiffness", "limits", "damping", "blast",
                "inherit"]
KEY_ORDER = ["hull", "smallgibs", "smallgibs_brains"] + ["ragdoll_" + k for k in RAGDOLL_KEYS]
FAMILIES = [  # (old name's pattern: group 1 the token, group 2 the ragdoll key; the data file's key)
    (re.compile(r"^vr_mhull_(\w+)()$"), "hull"),
    (re.compile(r"^vr_smallgibs_brains_mult_(\w+)()$"), "smallgibs_brains"),
    (re.compile(r"^vr_smallgibs_mult_(\w+)()$"), "smallgibs"),
    (re.compile(r"^vr_ragdoll_(\w+?)_(" + "|".join(RAGDOLL_KEYS) + r")$"), "ragdoll_"),
]
CVAR_RE = re.compile(r'^QVR_CVAR\((\w+), "([^"]*)", (\w+)\)\s*(//.*)?$')
ENTRY_RE = re.compile(r"^QVR_MONSTER_VALUE\((\w+), (\w+), (\w+)\)")
VALUE_RE = re.compile(r'^(\s*)(\w+) "([^"]*)"(\s*//.*)?$')

INC_HEADER = """\
// vr_monsterdata.inc -- the monsters' per-class values (design review 3, step 3; ROUND23.md, "Monsters as data"):
// QVR_MONSTER_VALUE(name, classname, key), each a value of quakevr/data/monsters.txt's `monster <classname>` block (its
// default there; the player's change in the config by `name`). `name` is its old cvar's: the C++ reads it as a cvar_t
// by that name (a stand-in, vr_monsterdata.cpp: not in the cvar list), and the menus, the config, the console and the
// QC's cvar handles reach it by that name. A new value: its line here and its key in its classname's block.
// Written by Misc/quakevr/monsters_data.py convert from vr_cvars.inc's rows.
"""

INC_SECTIONS = {
    "hull": "// Monster Hitbox (Movement > Monster Hitbox; vr_hull.cpp monsterClasses, HULLS.md \"Monsters\"): a class's width\n"
            "// against walls: -1 its own box's (QC setsize; the default), 0 Quake's hull, or 16 up.",
    "smallgibs": "// Small gibs per enemy (Gore > Small Gibs > Per Enemy; QC vr_smallgibs.qc VR_SmallGib_Mult): a monster's small gibs\n"
                 "// (smallgibs) and brain chunks (smallgibs_brains), times these. 1 as now, 0 none, 2 twice as many (a hit's, a\n"
                 "// gibbing's, and a head's bursts).",
    "ragdoll_": "// Ragdolls per monster class (Gibs and Corpses > Ragdolls > Grunt, ...; vr_box3d_internal.hpp ragdollClasses): -1\n"
                "// (Global) is the vr_ragdoll_ setting, any other value replaces it for that class. Each class's mass is its own\n"
                "// (kg; the author, 2026-10-03: ROUND21.md, \"Ragdoll masses per monster\").",
}

DATA_HEADER = """\
// quakevr/data/monsters.txt -- the monsters' per-class values: read at start into the monster table
// (Quake/vr/vr_monsterdata.cpp; which values each classname has, and their old cvar names: vr_monsterdata.inc).
// Written once from vr_cvars.inc's defaults by Misc/quakevr/monsters_data.py convert; edited by hand since, or by its
// `promote <config>` (the player's changes, saved in the config by the old cvar names, made the defaults).
//
// `defaults { key "value" ... }`: a key's value for a classname whose block leaves it out. Then `monster <classname> {
// ... }`: its values.
//   hull              its width against walls (vr_mhull_<m>): -1 its own box's, 0 Quake's hull, or 16 up
//   smallgibs         its small gibs, times this (vr_smallgibs_mult_<m>)
//   smallgibs_brains  its brain chunks, times this (vr_smallgibs_brains_mult_<m>)
//   ragdoll_<key>     its ragdoll's (vr_ragdoll_<m>_<key>): -1 the global vr_ragdoll_<key>; mass in kg
"""


def classify(name):
    for pat, key in FAMILIES:
        m = pat.match(name)
        if m and m.group(1) in CLASSNAMES:
            return CLASSNAMES[m.group(1)], key + m.group(2) if key == "ragdoll_" else key, key
    return None


def convert():
    lines = open(CVARS, encoding="utf-8").read().split("\n")
    entries = []  # (name, classname, key, section, default, comment)
    kept = []
    for line in lines:
        m = CVAR_RE.match(line)
        c = classify(m.group(1)) if m else None
        if not c:
            kept.append(line)
            continue
        if m.group(3) != "CVAR_ARCHIVE":
            sys.exit(f"convert: {m.group(1)} is not CVAR_ARCHIVE")
        entries.append((m.group(1), c[0], c[1], c[2], m.group(2), m.group(4)))
    if not entries:
        sys.exit("convert: no per-monster QVR_CVAR in vr_cvars.inc (converted already?)")
    inc = [INC_HEADER]
    for section in ("hull", "smallgibs", "ragdoll_"):
        inc.append(INC_SECTIONS[section])
        for name, cls, key, sec, _, _ in entries:
            if sec == section or (section == "smallgibs" and sec == "smallgibs_brains"):
                inc.append(f"QVR_MONSTER_VALUE({name}, {cls}, {key})")
        inc.append("")
    open(INC, "w", encoding="utf-8", newline="\n").write("\n".join(inc))
    values = {}
    for name, cls, key, _, default, comment in entries:
        values.setdefault(cls, {})[key] = (default, comment)
    defaults = {}
    for key in KEY_ORDER:
        seen = [v[key][0] for v in values.values() if key in v]
        defaults[key] = "-1" if key.startswith("ragdoll_") else max(set(seen), key=seen.count)
    out = [DATA_HEADER, "defaults", "{"]
    out += [f'    {k} "{defaults[k]}"' for k in KEY_ORDER]
    out.append("}")
    for cls in CLASS_ORDER:
        if cls not in values:
            continue
        out += ["", f"monster {cls}", "{"]
        for key in KEY_ORDER:
            if key in values[cls]:
                default, comment = values[cls][key]
                out.append(f'    {key} "{default}"' + (f" {comment}" if comment else ""))
        out.append("}")
    missing = set(values) - set(CLASS_ORDER)
    if missing:
        sys.exit(f"convert: no order for {missing}")
    os.makedirs(os.path.dirname(DATA), exist_ok=True)
    open(DATA, "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")
    open(CVARS, "w", encoding="utf-8", newline="\n").write("\n".join(kept))
    print(f"convert: {len(entries)} values of {len(values)} classnames -> {DATA}, {INC}; taken out of {CVARS}")


def read_entries():
    out = []
    for line in open(INC, encoding="utf-8"):
        m = ENTRY_RE.match(line)
        if m:
            out.append(m.groups())
    return out


def parse_data():
    """monsters.txt as blocks ({"cls": classname or None for defaults, "keys": {key: line index}}); fails on a
    malformed file."""
    keys = {e[2] for e in read_entries()}
    lines = open(DATA, encoding="utf-8").read().split("\n")
    blocks = []
    cur = None
    for i, line in enumerate(lines):
        s = line.split("//", 1)[0].strip()
        if not s:
            continue
        if cur is None:
            m = re.match(r"^(defaults|monster (\w+))$", s)
            if not m:
                sys.exit(f"monsters.txt:{i + 1}: expected `defaults` or `monster <classname>`: {line}")
            cur = {"cls": m.group(2), "open": None, "keys": {}}
            continue
        if cur["open"] is None:
            if s != "{":
                sys.exit(f"monsters.txt:{i + 1}: expected {{")
            cur["open"] = i
            continue
        if s == "}":
            blocks.append(cur)
            cur = None
            continue
        m = VALUE_RE.match(line)
        if not m or m.group(2) not in keys:
            sys.exit(f"monsters.txt:{i + 1}: not a known key's line: {line}")
        cur["keys"][m.group(2)] = i
    if cur is not None:
        sys.exit("monsters.txt: a block not closed")
    return lines, blocks


def check():
    entries = read_entries()
    lines, blocks = parse_data()
    by_cls = {b["cls"]: b for b in blocks if b["cls"]}
    wanted = {}
    for _, cls, key in entries:
        wanted.setdefault(cls, set()).add(key)
    bad = 0
    for cls, keys in wanted.items():
        have = set(by_cls[cls]["keys"]) if cls in by_cls else set()
        for key in sorted(keys - have):
            print(f"check: monster {cls}: no {key}")
            bad += 1
        for key in sorted(have - keys):
            print(f"check: monster {cls}: {key} is not one of its values (vr_monsterdata.inc)")
            bad += 1
    for cls in by_cls:
        if cls not in wanted:
            print(f"check: monster {cls}: no values in vr_monsterdata.inc")
            bad += 1
    if bad:
        sys.exit(f"check: {bad} problems")
    print(f"check: {len(by_cls)} classnames, {len(entries)} values: ok")


def promote(cfg, dry):
    where = {name: (cls, key) for name, cls, key in read_entries()}
    lines, blocks = parse_data()
    by_cls = {b["cls"]: b for b in blocks if b["cls"]}
    sets = 0
    changed = 0
    for line in open(cfg, encoding="utf-8", errors="replace"):
        m = re.match(r'^(\w+) "([^"]*)"\s*$', line.strip())
        if not m or m.group(1).lower() not in where:
            continue
        sets += 1
        cls, key = where[m.group(1).lower()]
        i = by_cls[cls]["keys"][key]
        v = VALUE_RE.match(lines[i])
        if v.group(3) == m.group(2) or same_number(v.group(3), m.group(2)):
            continue
        print(f"{cls} {key}: \"{v.group(3)}\" -> \"{m.group(2)}\"")
        lines[i] = f'{v.group(1)}{key} "{m.group(2)}"' + (v.group(4) or "")
        changed += 1
    if dry:
        print(f"promote: {sets} lines in {cfg}, {changed} would change (--dry: monsters.txt not written)")
        return
    open(DATA, "w", encoding="utf-8", newline="\n").write("\n".join(lines))
    print(f"promote: {sets} lines in {cfg}, {changed} changed in {DATA}")


def same_number(a, b):
    try:
        x, y = float(a), float(b)
    except ValueError:
        return False
    return abs(x - y) <= 1e-5 * max(1.0, abs(x))


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
        for name, _, _ in read_entries():
            print(name)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
