#!/usr/bin/env python3
"""props_data.py -- quakevr/data/props.txt, the held objects' settings as data (design review 3, step 2; ROUND23.md,
"Props as data"). The props' twin of weapons_data.py (whose parser and promote it uses).

  python Misc/quakevr/props_data.py convert
      Once (done 2026-11-01): wrote quakevr/data/props.txt from vr_props.inc's QVR_PROP_KEY defaults and
      QVR_PROP_DEFAULT rows (their comments carried along), and took the defaults and the rows out of vr_props.inc.
      Run on a tree that still has them.

  python Misc/quakevr/props_data.py promote <config> [--dry]
      His tweaks become the defaults: each `vr_prop_<key>_NN "value"` line of a config (the player's Held Object Offsets
      and Weights, saved as overrides of props.txt) written into props.txt's block of slot NN - 1 (the value replaced
      in place, or the key added). --dry prints what it would change.

  python Misc/quakevr/props_data.py check
      Parses props.txt against vr_props.inc's keys: an unknown key, a bad slot or a malformed line fails.

props.txt: a `defaults { key "value" ... }` block (every slot's value of a key; a key not listed: "0"), then a
`prop <slot> { key "value" ... }` block per slot with values of its own (slots from 0, as vr_prop_dump numbers them;
the config's and console's names vr_prop_<key>_NN number them from 1). `//` comments anywhere.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import weapons_data  # noqa: E402

ROOT = weapons_data.ROOT
INC = os.path.join(ROOT, "Quake", "vr", "vr_props.inc")
DATA = os.path.join(ROOT, "quakevr", "data", "props.txt")
NUM_SLOTS = 80  # vr_props.hpp numSlots

KEY_RE = re.compile(r'^QVR_PROP_KEY\((\w+), "([^"]+)"')
OLD_KEY_RE = re.compile(r'^(QVR_PROP_KEY\(\w+, "[^"]+"), "([^"]*)"\)(.*)$')
ROW_RE = re.compile(r'^QVR_PROP_DEFAULT\((\d+), (\w+), "([^"]*)"\)\s*(//.*)?$')

PROPS = weapons_data.Table(INC, KEY_RE, DATA, "prop", NUM_SLOTS, "vr_prop_")

HEADER = """\
// quakevr/data/props.txt -- every held object's settings (Held Object Offsets, Held Object Weights): read at start into
// the prop table (Quake/vr/vr_props.cpp; the keys and what each does: vr_props.inc). Written once from the C++ defaults
// by Misc/quakevr/props_data.py convert; edited by hand since, or by its `promote <config>` (the player's changes, saved
// in the config as vr_prop_<key>_NN "value" lines over these, made the defaults).
//
// `defaults { key "value" ... }`: every slot's value of a key (a key not listed: "0"). Then `prop <slot> { ... }`: a
// slot's own values, its `id` (the model it is for: "progs/gib1.mdl", "maps/b_explob.bsp") first. Slots from 0 (as
// vr_prop_dump numbers them); the config's and the console's names number them from 1 (vr_prop_<key>_NN, NN = slot +
// 1). A model with no slot has every key at its default: its mass estimated from its size and what it is made of.
"""


def convert():
    lines = open(INC, encoding="utf-8").read().split("\n")
    keys = []
    defaults = {}
    for i, line in enumerate(lines):
        m = OLD_KEY_RE.match(line)
        if m:
            enum, name = KEY_RE.match(line).groups()
            keys.append((enum, name))
            defaults[name] = m.group(2)
            # (its comment kept where it was, in line with the comment lines under it)
            pad = " " * (len(m.group(2)) + 4) if m.group(3) else ""
            lines[i] = f"{m.group(1)}){pad}{m.group(3)}"
    if not keys:
        sys.exit("convert: no QVR_PROP_KEY with a default in vr_props.inc (converted already?)")
    enum_to_name = dict(keys)
    start = lines.index("#ifdef QVR_PROP_DEFAULT")
    end = lines.index("#endif", start)
    values = [dict() for _ in range(NUM_SLOTS)]    # slot -> key name -> value
    comments = [dict() for _ in range(NUM_SLOTS)]  # slot -> key name -> [comment lines above, inline comment]
    pending = []
    nrows = 0
    for line in lines[start + 1:end]:
        s = line.strip()
        if s.startswith("//"):
            pending.append(s)
            continue
        if not s:
            continue
        m = ROW_RE.match(s)
        if not m:
            sys.exit(f"convert: not a row: {s}")
        slot, enum, value, inline = int(m.group(1)), m.group(2), m.group(3), m.group(4)
        if enum not in enum_to_name or not 0 <= slot < NUM_SLOTS:
            sys.exit(f"convert: bad row: {s}")
        name = enum_to_name[enum]
        values[slot][name] = value
        c = comments[slot].setdefault(name, [[], None])
        c[0].extend(pending)
        if inline:
            c[1] = inline
        pending = []
        nrows += 1
    if pending:
        sys.exit("convert: comments after the last row")
    order = [name for _, name in keys]
    out = [HEADER, "defaults", "{"]
    for name in order:
        out.append(f'    {name} "{defaults[name]}"')
    out.append("}")
    for slot in range(NUM_SLOTS):
        if not values[slot]:
            continue
        out.append("")
        out.append(f"// slot {slot}: {values[slot].get('id', defaults['id'])} (vr_prop_*_{slot + 1:02d})")
        out.append(f"prop {slot}")
        out.append("{")
        for name in values[slot]:  # (in vr_props.inc's rows' order: their comments read as they did)
            above, inline = comments[slot][name]
            out.extend("    " + c for c in above)
            out.append(f'    {name} "{values[slot][name]}"' + (f" {inline}" if inline else ""))
        out.append("}")
    os.makedirs(os.path.dirname(DATA), exist_ok=True)
    open(DATA, "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")
    # vr_props.inc keeps its keys (without their defaults): the defaults and the rows are data now.
    kept = lines[:start]
    while kept and kept[-1] == "":
        kept.pop()
    kept.append("")
    open(INC, "w", encoding="utf-8", newline="\n").write("\n".join(kept))
    nvals = sum(len(v) for v in values)
    print(f"convert: {len(keys)} keys, {nrows} rows -> {nvals} slot values in {DATA}; defaults and rows taken out of {INC}")


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    if sys.argv[1] == "convert":
        convert()
    elif sys.argv[1] == "promote" and len(sys.argv) >= 3:
        weapons_data.promote(sys.argv[2], "--dry" in sys.argv, PROPS)
    elif sys.argv[1] == "check":
        lines, blocks = weapons_data.parse_data(PROPS)
        print(f"check: {len(blocks)} blocks, {sum(len(b['keys']) for b in blocks)} values: ok")
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
