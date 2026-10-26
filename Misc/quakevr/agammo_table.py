#!/usr/bin/env python3
"""Tables from `vr_agrunt_test 47`'s console log (QC vr_armedgrunt_ammo_test.qc): each case's spawn ammo, damage a hit
on the player, loot, and the sounds its armed monster and its shots started (vr_debug_sound_log 2's `sndlog:` lines
between `agammo: case <i> <name>` and `agammo: case end`).

Usage: python agammo_table.py <log> [--sounds]
"""
import re
import sys

CASE = re.compile(r"agammo: case (\d+) (.+)$")
SPAWN = re.compile(r"agammo: (.+?) spawn: (.+): (PASS|FAIL)")
DAMAGE = re.compile(r"agammo: (.+?) damage: (\d+) hits, ([\d.]+) a hit before the armed scale, ([\d.]+) after, "
                    r"(\d+) shots, (\d+) blows, (\d+) reloads(.*): (PASS|FAIL)")
LOOT = re.compile(r"agammo: (.+?) loot: (.+), expected (.+): (PASS|FAIL)")
SND = re.compile(r"sndlog: t ([\d.]+) ent (\d+) (\S*) chan (\d+) (\S+) vol ([\d.]+)(.*)$")


def main():
    path = sys.argv[1]
    sounds = "--sounds" in sys.argv
    cases = []
    cur = None
    # The console wraps long lines: a line not starting with a known prefix continues the one before.
    lines = []
    for raw in open(path, encoding="utf-8", errors="replace"):
        raw = raw.rstrip("\n")
        if lines and raw and not re.match(r"(agammo|sndlog|agtest|agrunt|SV_|exit=|TIMEOUT|ENGINE|gadget)", raw):
            lines[-1] += raw
        else:
            lines.append(raw)
    for line in lines:
        m = CASE.search(line)
        if m and "skipped" not in line:
            cur = {"i": int(m.group(1)), "name": m.group(2), "snd": {}}
            cases.append(cur)
            continue
        if cur is None:
            continue
        if "agammo: case end" in line:
            cur = None
            continue
        m = SPAWN.search(line)
        if m:
            cur["spawn"] = (m.group(2), m.group(3))
        m = DAMAGE.search(line)
        if m:
            cur["dmg"] = m.groups()[1:]
        m = LOOT.search(line)
        if m:
            cur["loot"] = (m.group(2), m.group(3), m.group(4))
        m = SND.search(line)
        if m and m.group(3) != "player":
            key = (m.group(3), m.group(4), m.group(5), m.group(7).strip())
            cur["snd"][key] = cur["snd"].get(key, 0) + 1
    print("| case | spawn | | hits | raw/hit | to you/hit | shots | blows | reloads | | loot | expected | |")
    print("|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    for c in cases:
        s = c.get("spawn", ("?", "?"))
        d = c.get("dmg", ("?",) * 8)
        lo = c.get("loot", ("?", "?", "?"))
        print(f"| {c['name']} | {s[0]} | {s[1]} | {d[0]} | {d[1]} | {d[2]} | {d[3]} | {d[4]} | {d[5]} | {d[7]} | "
              f"{lo[0]} | {lo[1]} | {lo[2]} |")
    if sounds:
        print()
        for c in cases:
            parts = [f"{cls}:{ch}:{smp}x{n}{(' ' + why) if why else ''}" for (cls, ch, smp, why), n in
                     sorted(c["snd"].items(), key=lambda kv: -kv[1])]
            print(f"- {c['name']}: " + ", ".join(parts))


if __name__ == "__main__":
    main()
