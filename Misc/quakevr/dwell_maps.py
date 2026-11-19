#!/usr/bin/env python3
"""dwell_maps.py <run.log> <save folder>: Dwell P1b's acceptance verdict (dwell_maps.sh). Per map: missing spawn
functions (only Dwell's own classnames may miss until P2), errors, monsters live + waiting (Copper's triggered spawn),
coop-only (Copper 4096) placements present in single player (must be 0); the exit chain and the loaded saves. Ends with
`dwell_maps: N passed, M failed`."""

import collections
import os
import re
import sys

DWELL_OWN = {"monster_kamikaze", "monster_brute", "monster_excavator", "monster_d1boss", "monster_d2boss",
             "monster_boss_egypt", "monster_morph", "weapon_quad_shotgun", "weapon_railgun", "weapon_rifle",
             "item_artifact_haste", "item_artifact_berserk", "item_artifact_wetsuit", "misc_crystal", "func_fade",
             "info_storycamera", "trigger_killboss", "target_bullethell", "misc_textbook"}
# A map's own mistake, which Copper itself reports the same way (its objerror removes the entity; play goes on): Ancient's
# target_state at 600 -504 8 has no targetname ("target_state not targeted by anything").
MAP_OWN = {("ancient", "OBJECT ERROR in cu_target_state")}


def save_census(path):
    live = waiting = coop_only = ents = 0
    with open(path, encoding="latin1") as f:
        text = f.read()
    for block in re.findall(r"\{([^{}]*)\}", text):
        kv = dict(re.findall(r'"([^"]*)"\s+"([^"]*)"', block))
        cls = kv.get("classname", "")
        if not cls:
            continue
        ents += 1
        sf = int(float(kv.get("spawnflags", "0")))
        if sf & 4096:
            coop_only += 1
        if cls.startswith("monster_"):
            if float(kv.get("MG_deferred", "0")):
                waiting += 1
            elif float(kv.get("health", "0")) > 0:
                live += 1
    return live, waiting, coop_only, ents


def main():
    log, saves = sys.argv[1], sys.argv[2]
    lines = open(log, encoding="latin1").read().split("\n")
    missing = collections.Counter()
    per_map = {}
    errors = []
    pending = collections.Counter()
    ran, chain, loaded, ruleset = set(), [], [], {}
    current = None
    expect_cls = False
    for l in lines:
        if "No spawn function for" in l:
            expect_cls = True
            continue
        m = re.match(r"\s*classname\s+(\w+)", l)
        if expect_cls and m:
            pending[m.group(1)] += 1
            expect_cls = False
            continue
        # (`dwell: map X` is echoed before `map X`: what its spawn prints, missing spawn functions and errors, is X's)
        m = re.search(r"dwell: map (\w+)", l)
        if m:
            current = m.group(1)
            pending = per_map[current] = collections.Counter()
            continue
        m = re.search(r"Ruleset: (\w+)", l)
        if m and current:
            ruleset.setdefault(current, m.group(1))
        m = re.search(r"dwell: ran (\w+)", l)
        if m:
            ran.add(m.group(1))
        if "dwell: chain" in l or "vr_goto_exit:" in l:
            chain.append(l.strip())
        m = re.search(r"dwell: loaded (\w+)", l)
        if m:
            loaded.append(m.group(1))
        if re.search(r"Host_Error|SV_Error|OBJECT ERROR|PR_RunError|runaway", l) and \
                not any(current == mp and text in l for mp, text in MAP_OWN):
            errors.append((current, l.strip()[:150]))
    passed = failed = 0
    print("%-9s %-7s %8s %8s %6s %s" % ("map", "rules", "monsters", "waiting", "coop", "missing (not Dwell's own / Dwell's own)"))
    for mp in sorted(per_map):
        miss = per_map[mp]
        bad = {k: v for k, v in miss.items() if k not in DWELL_OWN}
        own = sum(v for k, v in miss.items() if k in DWELL_OWN)
        sav = os.path.join(saves, "dwc_%s.sav" % mp)
        live, waiting, coop, _ = save_census(sav) if os.path.isfile(sav) else (-1, -1, -1, -1)
        errs = [e for e in errors if e[0] == mp]
        ok = not bad and not own and coop == 0 and mp in ran and ruleset.get(mp) == "Copper" and not errs  # (P2a: Dwell's own spawn too)
        passed += ok
        failed += not ok
        print("%-9s %-7s %8d %8d %6d %s / %d%s%s" % (mp, ruleset.get(mp, "?"), live, waiting, coop, bad or "0", own,
                                                 "" if mp in ran else " NOT RUN", " ERRORS %d" % len(errs) if errs else ""))
    for c in chain:
        print("chain:", c)
    chain_ok = sum("in the exit to" in c for c in chain) == 2
    loads_ok = len(loaded) == 3
    passed += chain_ok + loads_ok
    failed += (not chain_ok) + (not loads_ok)
    print("loaded:", ", ".join(loaded))
    for e in errors[:10]:
        print("error:", e)
    print("dwell_maps: %d passed, %d failed" % (passed, failed))


if __name__ == "__main__":
    main()
