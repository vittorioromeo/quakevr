"""Checks a Machine Horde wave-monitor log (vr_mg_horde_test 13) against the official MG1 horde.qc tables.

  python check_horde_waves.py <console log> [--min-waves N]

Each "mghordewave" line must carry the squad budget SpawnWavePrep computes for its wave, skill and living players
(army waves, boss/elite/fodder counts, the player scalar). Each "mghordesquad" group must be one of the squads
SpawnSquad2 can spawn for its category, army flag and skill, and a wave that finished spawning must have spent
exactly its budget. Exit 0 when everything matches.
"""
import math
import re
import sys
from collections import Counter, defaultdict

# Official SpawnSquad2 compositions (monster classnames), by skill > 0 and skill 0.
def squads(cat, army, skill):
    hi = skill > 0
    def c(**kw):
        return Counter({("monster_" + k): v for k, v in kw.items() if v})
    if army and cat == "fodder":
        return [c(army=3 if hi else 2), c(dog=1, army=2), c(dog=2 if hi else 1), c(enforcer=1)]
    if army and cat == "elite":
        return [c(enforcer=2 if hi else 1), c(ogre=1)]
    if cat == "fodder":
        return [c(knight=2 if hi else 1), c(zombie=2), c(wizard=1)]
    if cat == "elite":
        return [c(hell_knight=2 if hi else 1), c(hell_knight=1, knight=2), c(ogre=1), c(wizard=3 if hi else 2)]
    if cat == "boss":
        demons = [c(demon1=2 if skill >= 1 else 1)] + ([c(shambler=2)] if skill >= 3 else [])
        return [c(shambler=1), c(shalrath=1)] + demons
    return []


def budget(wave, skill, players, prev_bosses=0):
    temp = wave + (6 if skill >= 3 else 3 if skill >= 2 else 0)
    army = (temp + 2) % 3 == 0
    scalar = 2 if players >= 4 else 1.5 if players >= 3 else 1.25 if players >= 2 else 1
    bosses = prev_bosses
    if temp % 3 == 0:
        bosses = math.floor((temp + 1) / 4)
    elif skill > 1 and not army and temp > 9:
        bosses = math.floor((temp + 1) / 8)
    elites = math.floor((math.ceil((temp - 1) / 3) - math.floor(bosses / 2)) * scalar)
    fodder = math.floor(((temp + 2) - (bosses * 2 + elites)) * scalar)
    return army, fodder, elites, bosses


def main():
    path = sys.argv[1]
    min_waves = int(sys.argv[sys.argv.index("--min-waves") + 1]) if "--min-waves" in sys.argv else 1
    text = open(path, encoding="latin1").read()
    waves = {}
    groups = defaultdict(Counter)
    meta = {}
    for m in re.finditer(r"mghordewave: wave (\d+) skill (\d+) players (\d+) army (\d+) squads (\d+)/(\d+)/(\d+)", text):
        w, sk, pl, ar, f, e, b = map(int, m.groups())
        waves[w] = (sk, pl, ar, f, e, b)
    for m in re.finditer(r"mghordesquad: wave (\d+) squad (\d+) cat (\w+) army (\d+) class (\w+)", text):
        w, sq, cat, ar, cls = int(m[1]), int(m[2]), m[3], int(m[4]), m[5]
        groups[sq][cls] += 1
        meta[sq] = (w, cat, ar)
    fails = 0
    def bad(msg):
        nonlocal fails
        fails += 1
        print("FAIL", msg)
    for w, (sk, pl, ar, f, e, b) in sorted(waves.items()):
        if w == 0:
            continue
        exp = budget(w, sk, pl)
        got = (bool(ar), f, e, b)
        # A carried boss count (the source never resets it on non-boss waves) is 0 after a fully spawned wave.
        if got != exp:
            bad(f"wave {w} skill {sk} players {pl}: budget army/fodder/elites/bosses {got}, source {exp}")
        else:
            print(f"ok   wave {w} skill {sk} players {pl}: army {int(exp[0])} squads {f}/{e}/{b}")
    spent = defaultdict(Counter)
    for sq, cls in sorted(groups.items()):
        w, cat, ar = meta[sq]
        if cat == "other":
            continue  # authored or deferred arena monster, not a squad
        sk = waves.get(w, (None,))[0]
        if sk is None:
            continue
        spent[w][cat] += 1
        if cls not in squads(cat, ar, sk):
            bad(f"wave {w} squad {sq} ({cat}, army {ar}, skill {sk}): {dict(cls)} is not a source squad")
    complete = 0
    for w, (sk, pl, ar, f, e, b) in sorted(waves.items()):
        if w == 0 or w + 1 not in waves:
            continue  # the last logged wave may still be spawning
        got = (spent[w]["fodder"], spent[w]["elite"], spent[w]["boss"])
        if got != (f, e, b):
            bad(f"wave {w}: spent squads {got}, budget {(f, e, b)}")
        else:
            complete += 1
            n = sum(sum(groups[sq].values()) for sq in groups if meta[sq][0] == w and meta[sq][1] != "other")
            print(f"ok   wave {w}: spent its budget, {n} monsters")
    if complete < min_waves:
        bad(f"only {complete} completed waves logged (wanted {min_waves})")
    print(f"check_horde_waves: {len(waves)} waves, {len(groups)} squads, {complete} completed, {fails} failures")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
