#!/usr/bin/env python3
"""weapons_data.py -- quakevr/data/weapons.txt, the weapons' settings as data (design review 3; ROUND23.md, "Weapons
as data").

  python Misc/quakevr/weapons_data.py convert
      Once (done 2026-11-01): wrote quakevr/data/weapons.txt from vr_weapons.inc's QVR_WEAPON_DEFAULT and
      QVR_WEAPON_UNINHERITED rows and registerCvars' every-slot defaults (GLOBALS below), the frozen
      Quake/vr/vr_weapons_before48.inc (the AD weapons' defaults before vr_wofs_version 48, for that migration), and
      took the rows out of vr_weapons.inc. Run on a tree that still has the rows.

  python Misc/quakevr/weapons_data.py promote <config> [--dry]
      His tweaks become the defaults: each `vr_wofs_<key>_NN "value"` line of a config (the player's Weapon Offsets and
      Weapon Weights, saved as overrides of weapons.txt) written into weapons.txt's block of slot NN - 1 (the value
      replaced in place, or the key added). --dry prints what it would change. The named values' lines by their old
      cvar names too (vr_reload_port_nail_x "0.5": vr_weaponvalues.inc's slot and key).

  python Misc/quakevr/weapons_data.py check
      Parses weapons.txt against vr_weapons.inc's keys: an unknown key, a bad slot or a malformed line fails.

weapons.txt: a `defaults { key "value" ... }` block (every slot's value of a key; a key not listed: "0"), then a
`weapon <slot> { key "value" ... }` block per slot with values of its own (slots from 0, as vr_wofs_dump numbers them;
the config's and console's names vr_wofs_<key>_NN number them from 1). `//` comments anywhere.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
INC = os.path.join(ROOT, "Quake", "vr", "vr_weapons.inc")
BEFORE48 = os.path.join(ROOT, "Quake", "vr", "vr_weapons_before48.inc")
VALUES = os.path.join(ROOT, "Quake", "vr", "vr_weaponvalues.inc")
DATA = os.path.join(ROOT, "quakevr", "data", "weapons.txt")
NUM_SLOTS = 51
UNINHERITED_SLOTS = (27, 28, 29, 30, 31, 33, 34, 35)  # vr_weapons.cpp uninherited[]

KEY_RE = re.compile(r'^QVR_WEAPON_KEY\((\w+), "([^"]+)"\)')


class Table:
    """A data table (props_data.py has the props'): its keys' .inc, its data file, its blocks' word, its slots and the
    config's and console's name stem of its values (<stem><key>_NN, NN = slot + 1)."""

    def __init__(self, inc, key_re, data, block, num_slots, stem):
        self.inc, self.key_re, self.data, self.block, self.num_slots, self.stem = inc, key_re, data, block, num_slots, stem
        self.file = os.path.basename(data)


WEAPONS = Table(INC, KEY_RE, DATA, "weapon", NUM_SLOTS, "vr_wofs_")
ROW_RE = re.compile(r'^QVR_WEAPON_(DEFAULT|UNINHERITED)\((\d+), (\w+), "([^"]*)"\)\s*(//.*)?$')

# registerCvars' every-slot defaults before the conversion (by enum name; the rest "0").
GLOBALS = {"GripOverlap": "0.3"}
for _n in range(1, 5):
    GLOBALS[f"Hotspot{_n}Overlap"] = "0.3"
    GLOBALS[f"Hotspot{_n}Sticky"] = "1"
for _k in ("SpringStiffness SpringDamping SpringStrength SpringSag SpringSwing SpringTwoHanded SpringSnap SpringRoll "
           "MeleeDamage ThrowDamage SpinAlign ThrowMinSpeed ThrowCurve MeleeMinSpeed MeleeCurve "
           "HapticPunch Recoil2hKick Recoil2hRise Recoil2hTime Recoil2hHaptic FlashSize TracerSpeed TracerLength "
           "TracerWidth TracerChance TracerRed GunfxSparks GunfxSparkSize GunfxSmoke GunfxSmokeSize GunfxWisps GunfxHeat GunfxCool "
           "TorchClip Droop HeavyRecoil BladeWidth").split():
    GLOBALS[_k] = "1"
GLOBALS.update({"GunfxBarrel": "0.5", "RecoilReturn": "0.115","FlashTime": "0.06", "TracerGreen": "0.8", "TracerBlue": "0.45",
                "AnyGripMode": "-1", "Heft": "-1", "BladeNear": "-1", "BladeFrom": "-1"})

HEADER = """\
// quakevr/data/weapons.txt -- every weapon's settings (Weapon Offsets, Weapon Weights): read at start into the weapon
// table (Quake/vr/vr_weapons.cpp; the keys and what each does: vr_weapons.inc). Written once from the C++ defaults by
// Misc/quakevr/weapons_data.py convert; edited by hand since, or by its `promote <config>` (the player's changes, saved in
// the config as vr_wofs_<key>_NN "value" lines over these, made the defaults).
//
// `defaults { key "value" ... }`: every slot's value of a key (a key not listed: "0"). Then `weapon <slot> { ... }`: a
// slot's own values. Slots from 0 (as vr_wofs_dump numbers them); the config's and the console's names number them from
// 1 (vr_wofs_<key>_NN, NN = slot + 1). A slot inherits another's values with `inherit` (that slot's NN).
"""


def read_keys(table=WEAPONS):
    keys = []
    for line in open(table.inc, encoding="utf-8"):
        m = table.key_re.match(line)
        if m:
            keys.append((m.group(1), m.group(2)))
    return keys


def convert():
    keys = read_keys()
    enum_to_name = dict(keys)
    lines = open(INC, encoding="utf-8").read().split("\n")
    start = lines.index("#ifdef QVR_WEAPON_DEFAULT")
    end = lines.index("#endif", start)
    defaults = {name: GLOBALS.get(enum, "0") for enum, name in keys}
    values = [dict() for _ in range(NUM_SLOTS)]       # slot -> key name -> value (DEFAULT, then UNINHERITED)
    comments = [dict() for _ in range(NUM_SLOTS)]     # slot -> key name -> [comment lines above, inline comment]
    before = {}                                       # (slot, enum) -> value before the UNINHERITED pass
    pending = []
    header_skipped = 0
    rows = {"DEFAULT": [], "UNINHERITED": []}
    for line in lines[start + 1:end]:
        s = line.strip()
        if s.startswith("//"):
            if header_skipped < 3:  # (the QVR_WEAPON_UNINHERITED macro's own description)
                header_skipped += 1
                continue
            pending.append(s)
            continue
        if not s:
            continue
        m = ROW_RE.match(s)
        if not m:
            sys.exit(f"convert: not a row: {s}")
        kind, slot, enum, value, inline = m.group(1), int(m.group(2)), m.group(3), m.group(4), m.group(5)
        if enum not in enum_to_name:
            sys.exit(f"convert: unknown key {enum}")
        rows[kind].append((slot, enum, value, inline, pending))
        pending = []
    if pending:
        sys.exit("convert: comments after the last row")
    for kind in ("DEFAULT", "UNINHERITED"):
        for slot, enum, value, inline, above in rows[kind]:
            name = enum_to_name[enum]
            if kind == "UNINHERITED":
                if slot not in UNINHERITED_SLOTS:
                    sys.exit(f"convert: UNINHERITED row for slot {slot}")
                before.setdefault((slot, enum), values[slot].get(name, defaults[name]))
            values[slot][name] = value
            c = comments[slot].setdefault(name, [[], None])
            c[0].extend(above)
            if inline:
                c[1] = inline
    order = [name for _, name in keys]
    out = [HEADER, "defaults", "{"]
    for name in order:
        out.append(f'    {name} "{defaults[name]}"')
    out.append("}")
    for slot in range(NUM_SLOTS):
        if not values[slot]:
            continue
        out.append("")
        out.append(f"// slot {slot}: {values[slot].get('id', defaults['id'])} (vr_wofs_*_{slot + 1:02d})")
        out.append(f"weapon {slot}")
        out.append("{")
        for name in order:
            if name not in values[slot]:
                continue
            above, inline = comments[slot][name]
            out.extend("    " + c for c in above)
            out.append(f'    {name} "{values[slot][name]}"' + (f" {inline}" if inline else ""))
        out.append("}")
    os.makedirs(os.path.dirname(DATA), exist_ok=True)
    open(DATA, "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")

    b = ["// vr_weapons_before48.inc -- frozen migration data, written once by Misc/quakevr/weapons_data.py convert: the AD",
         "// weapons' defaults before vr_wofs_version 48 where they differ from weapons.txt's (vr_weapons.cpp oldDefaults,",
         "// takeUninherited: a config's value still one of these was a placeholder that gave way to the parent's).",
         "//   QVR_WEAPON_BEFORE48(slot, Enum, \"value then\")"]
    n48 = 0
    for (slot, enum), old in sorted(before.items(), key=lambda kv: (kv[0][0], order.index(enum_to_name[kv[0][1]]))):
        if old != values[slot][enum_to_name[enum]]:
            b.append(f'QVR_WEAPON_BEFORE48({slot}, {enum}, "{old}")')
            n48 += 1
    open(BEFORE48, "w", encoding="utf-8", newline="\n").write("\n".join(b) + "\n")

    # vr_weapons.inc keeps its keys (and QVR_WEAPON_OWN): the rows are data now.
    kept = lines[:start]
    for line in lines[end + 1:]:
        if not (line == "" and len(kept) >= 2 and kept[-1] == "" and kept[-2] == ""):
            kept.append(line)
    open(INC, "w", encoding="utf-8", newline="\n").write("\n".join(kept))
    nvals = sum(len(v) for v in values)
    print(f"convert: {len(keys)} keys, {len(rows['DEFAULT'])} + {len(rows['UNINHERITED'])} rows -> {nvals} slot values "
          f"in {DATA}; {n48} before-48 values in {BEFORE48}; rows taken out of {INC}")


VALUE_RE = re.compile(r'^(\s*)(\S+) "([^"]*)"(\s*//.*)?$')


def parse_data(table=WEAPONS):
    """The data file as [(kind, slot)] blocks with their line ranges; fails on a malformed file."""
    names = {name for _, name in read_keys(table)}
    f = table.file
    lines = open(table.data, encoding="utf-8").read().split("\n")
    blocks = []
    cur = None
    for i, line in enumerate(lines):
        s = line.split("//", 1)[0].strip()
        if not s:
            continue
        if cur is None:
            m = re.match(rf"^(defaults|{table.block} (\d+))$", s)
            if not m:
                sys.exit(f"{f}:{i + 1}: expected `defaults` or `{table.block} <slot>`: {line}")
            slot = int(m.group(2)) if m.group(2) else -1
            if m.group(2) and not 0 <= slot < table.num_slots:
                sys.exit(f"{f}:{i + 1}: bad slot {slot}")
            cur = {"slot": slot, "head": i, "open": None, "close": None, "keys": {}}
            continue
        if cur["open"] is None:
            if s != "{":
                sys.exit(f"{f}:{i + 1}: expected {{")
            cur["open"] = i
            continue
        if s == "}":
            cur["close"] = i
            blocks.append(cur)
            cur = None
            continue
        m = VALUE_RE.match(line)
        if not m or m.group(2) not in names:
            sys.exit(f"{f}:{i + 1}: not a known key's line: {line}")
        cur["keys"][m.group(2)] = i
    if cur is not None:
        sys.exit(f"{f}: a block not closed")
    return lines, blocks


def slot_values(table=WEAPONS, by_enum=False, with_defaults=False):
    """The data file's values: {slot: {key: value}}, each slot's own block's (and, `with_defaults`, every key's value
    for every slot: the defaults block's unless its own), keyed by the file's names or, `by_enum`, the .inc's enum
    names (the C++ Key:: names the old QVR_WEAPON_DEFAULT rows used). The other scripts' reader."""
    keys = read_keys(table)
    to_enum = {name: enum for enum, name in keys}
    lines, blocks = parse_data(table)
    defaults = {name: "0" for _, name in keys}
    own = {}
    for b in blocks:
        vals = {k: VALUE_RE.match(lines[i]).group(3) for k, i in b["keys"].items()}
        if b["slot"] < 0:
            defaults.update(vals)
        else:
            own.setdefault(b["slot"], {}).update(vals)
    out = {}
    for slot in range(table.num_slots):
        vals = dict(defaults) if with_defaults else {}
        vals.update(own.get(slot, {}))
        if vals:
            out[slot] = {to_enum[k] if by_enum else k: v for k, v in vals.items()}
    return out


def key_names(table=WEAPONS):
    """{enum name: the file's name} ("GunOffsetX" -> "gunoff_x")."""
    return dict(read_keys(table))


def same_number(a, b):
    """The same number but for float noise (a slider's 32.600002 for 32.6): the file's text kept."""
    try:
        x, y = float(a), float(b)
    except ValueError:
        return False
    return abs(x - y) <= 1e-5 * max(1.0, abs(x))


def named_values():
    """vr_weaponvalues.inc: each named value's old cvar name -> (slot, weapons.txt key)."""
    key_of = dict(read_keys())
    out = {}
    for line in open(VALUES, encoding="utf-8"):
        m = re.match(r"^QVR_WEAPON_VALUE\((\w+), (\d+), (\w+)\)", line)
        if m:
            out[m.group(1)] = (int(m.group(2)), key_of[m.group(3)])
    return out


def promote(cfg, dry, table=WEAPONS):
    order = [name for _, name in read_keys(table)]
    lines, blocks = parse_data(table)
    by_slot = {b["slot"]: b for b in blocks}
    sets = []
    for line in open(cfg, encoding="utf-8", errors="replace"):
        m = re.match(rf'^{table.stem}(\w+)_(\d\d) "([^"]*)"\s*$', line.strip())
        if m and m.group(1) in order and 1 <= int(m.group(2)) <= table.num_slots:
            sets.append((int(m.group(2)) - 1, m.group(1), m.group(3)))
        # the named values by their old cvar names (vr_reload_port_nail_x "0.5": vr_weaponvalues.inc's slot and key)
        m = re.match(r'^(vr_\w+) "([^"]*)"\s*$', line.strip())
        if table is WEAPONS and m and m.group(1) in named_values():
            slot, key = named_values()[m.group(1)]
            sets.append((slot, key, m.group(2)))
    inserts = {}  # line index -> lines to insert before it
    new_blocks = {}  # slot without a block -> {key: value}
    changed = 0
    for slot, key, value in sets:
        b = by_slot.get(slot)
        if b is not None and key in b["keys"]:
            i = b["keys"][key]
            m = VALUE_RE.match(lines[i])
            if m.group(3) == value or same_number(m.group(3), value):
                continue
            print(f"slot {slot} {key}: \"{m.group(3)}\" -> \"{value}\"")
            lines[i] = f'{m.group(1)}{key} "{value}"' + (m.group(4) or "")
        elif b is not None:
            # Before the first key that comes after it in the .inc's order, else at the block's end.
            after = [b["keys"][k] for k in order[order.index(key) + 1:] if k in b["keys"]]
            at = min(after) if after else b["close"]
            print(f"slot {slot} {key}: added \"{value}\"")
            inserts.setdefault(at, []).append(f'    {key} "{value}"')
        else:
            print(f"slot {slot} {key}: added \"{value}\" (a new block)")
            new_blocks.setdefault(slot, {})[key] = value
        changed += 1
    for slot, kv in sorted(new_blocks.items()):
        later = [x["head"] for x in blocks if x["slot"] > slot]
        at = min(later) if later else len(lines)
        while at > 0 and lines[at - 1].startswith("// slot "):  # (before the next block's heading)
            at -= 1
        ins = inserts.setdefault(at, [])
        ins.extend([f"// slot {slot} ({table.stem}*_{slot + 1:02d})", f"{table.block} {slot}", "{"])
        ins.extend(f'    {k} "{kv[k]}"' for k in order if k in kv)
        ins.extend(["}", ""])
    if dry:
        print(f"promote: {len(sets)} lines in {cfg}, {changed} would change (--dry: {table.file} not written)")
        return
    out = []
    for i, line in enumerate(lines):
        out.extend(inserts.get(i, []))
        out.append(line)
    out.extend(inserts.get(len(lines), []))
    open(table.data, "w", encoding="utf-8", newline="\n").write("\n".join(out))
    print(f"promote: {len(sets)} lines in {cfg}, {changed} changed in {table.data}")


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    if sys.argv[1] == "convert":
        convert()
    elif sys.argv[1] == "promote" and len(sys.argv) >= 3:
        promote(sys.argv[2], "--dry" in sys.argv)
    elif sys.argv[1] == "check":
        lines, blocks = parse_data()
        print(f"check: {len(blocks)} blocks, {sum(len(b['keys']) for b in blocks)} values: ok")
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
