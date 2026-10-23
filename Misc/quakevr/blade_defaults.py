#!/usr/bin/env python3
"""blade_defaults.py -- the author's blade shapes and fist line as shipped defaults: the edits to paste.

Input: a vr_propose_defaults file (quakevr/proposed_defaults/<stamp>.cfg; run it with his config loaded). Output (stdout)
the blade-shape part of it as the source edits that ship it (ROUND22.md, "His blade shapes and fist line as defaults"):

  - the fist line (vr_melee_fist_*): the vr_cvars.inc defaults, and the vr_cvars.cpp defaultChanges rows (old default:
    a config still holding it takes the new one);
  - each weapon slot's blade keys (vr_wofs_bl_*_NN) and, for a slot whose blade keys changed, its tip (the Muzzle
    offsets, vr_wofs_muzzle_x/y/z_NN: Edit Blade Shape's tip handle moves them; a melee weapon's only: a gun's Muzzle is
    where its shots start, so its tip change is reported, not shipped): the vr_weapons.inc QVR_WEAPON_DEFAULT
    lines and the vr_weapons.cpp OldValue rows of a wofs-version step (a config still holding the old value takes the
    new).

A value equal as a number to its default (within 1e-4: a saved "-0.7" for "-0.700006") is not a change. Nothing is
written: review the lines, then paste them.

Check each slot's lines against what he edited: a config can keep an old default that a later default replaced
without a migration (2026-10-24: the axe's head box corners all 0, the shipped seed since); leave those out with --skip.

Usage: python Misc/quakevr/blade_defaults.py <proposed.cfg> [--skip 01 ...] [--inc Quake/vr/vr_weapons.inc]
"""

import argparse
import os
import re
import sys

LINE = re.compile(r'^(\S+)\s+"([^"]*)"\s*//\s*default\s+"([^"]*)"')
KEY = re.compile(r'^QVR_WEAPON_KEY\((\w+),\s*"([^"]+)"\)')
FIST = re.compile(r'^vr_melee_fist_\w+$')
WOFS = re.compile(r'^vr_wofs_(\w+?)_(\d\d)$')
ID = re.compile(r'^QVR_WEAPON_DEFAULT\((\d+),\s*ID,\s*"([^"]+)"\)')
# Quake VR's own melee weapons (vr_weapons.inc's TorchClip list: the axe, Mjolnir, the two swords, the chainsaw, the
# crowbar); the AD and monster melee weapons are the IDs ending "#melee".
MELEE = (0, 8, 18, 19, 20, 23)


def number(s):
    try:
        return float(s)
    except ValueError:
        return None


def same(a, b):
    x, y = number(a), number(b)
    if x is not None and y is not None:
        return abs(x - y) < 1e-4
    return a == b


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("proposed")
    ap.add_argument("--inc", default=os.path.join(here, "..", "..", "Quake", "vr", "vr_weapons.inc"))
    ap.add_argument("--skip", nargs="*", default=[], help="slots (the cvars' NN, e.g. 01) to leave out: a stale value, "
                    "not his edit (the config kept an old default)")
    args = ap.parse_args()

    keys = {}  # "bl_near" -> "BladeNear"
    melee = set(MELEE)  # the slots (indices) whose tip is only a blade's: the rest are guns, their Muzzle where shots start
    with open(args.inc, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = KEY.match(line)
            if m:
                keys[m.group(2)] = m.group(1)
            m = ID.match(line)
            if m and m.group(2).endswith("#melee"):
                melee.add(int(m.group(1)))

    fist = []
    slots = {}  # cvar suffix NN -> {suffix key: (value, old)}
    with open(args.proposed, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = LINE.match(line)
            if not m:
                continue
            name, value, old = m.groups()
            if same(value, old):
                continue
            if FIST.match(name):
                fist.append((name, value, old))
                continue
            w = WOFS.match(name)
            if w and (w.group(1).startswith("bl_") or w.group(1) in ("muzzle_x", "muzzle_y", "muzzle_z")):
                slots.setdefault(w.group(2), {})[w.group(1)] = (value, old)

    print("// == vr_cvars.inc: the fist line's new defaults ==")
    for name, value, old in fist:
        print(f'{name} "{old}" -> "{value}"')
    print("\n// == vr_cvars.cpp defaultChanges rows (version: the next config version) ==")
    for name, value, old in fist:
        print(f'    {{VERSION, &{name}, "{old}"}}, // {value}')

    lines, rows = [], []
    for nn in sorted(slots):
        keyvals = slots[nn]
        if not any(k.startswith("bl_") for k in keyvals) or nn in args.skip:
            continue  # a tip moved without a blade edit: not a blade shape (or a slot left out by hand)
        slot = int(nn) - 1  # the cvars' NN is the menu's slot number, one past the index
        for k in sorted(keyvals):
            if k not in keys:
                print(f"unknown key {k} (slot {nn})", file=sys.stderr)
                continue
            value, old = keyvals[k]
            if k.startswith("muzzle_") and slot not in melee:
                print(f"slot {nn}: {k} {old} -> {value} not shipped: a gun's tip is its firing muzzle", file=sys.stderr)
                continue
            lines.append(f'QVR_WEAPON_DEFAULT({slot}, {keys[k]}, "{value}") // was {old}')
            literal = old if "." in old else old + "."
            rows.append(f"{{{slot}, Key::{keys[k]}, {literal}f}}")
    print("\n// == vr_weapons.inc QVR_WEAPON_DEFAULT lines (a Muzzle offset: edit its line in place) ==")
    print("\n".join(lines))
    print("\n// == vr_weapons.cpp OldValue rows of the wofs-version step ==")
    print(",\n".join(rows))


if __name__ == "__main__":
    main()
