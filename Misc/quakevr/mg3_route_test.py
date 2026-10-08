"""Dawn of the Machine (MG3) full-campaign route sweep (M3-29), headless, in three legs:

  main   the normal game: start (skill brush) -> map1 -> secret1 (rune 1) -> hub -> map1 -> map2 -> map2b -> map3 ->
         secret5 (rune 2) -> hub -> map3 -> map4 -> secret6 -> map5 -> secret3 (rune 3) -> hub -> map5 -> map6 ->
         secret4 -> map7 -> map8 (rune 4) -> hub -> secret2 -> boss (Chthon's fight and death) -> the finale -> credits.
         Equipment and upgrade masks seeded in map1 are checked at every later hop; each first visit of a map saves on
         arrival, loads that save, dies for real (its respawn autoloads it) and only then leaves.
  bn     Bloody Nightmare: the hub's skill buttons (skill 4) -> the four runes given (shortcut) -> secret2 -> boss (his
         death in a Bloody Nightmare game, mg3ctest 4: its new game, map1, runes cleared) -> secret1 (rune 1) -> hub -> the exit
         to secret2 leads to boss2 -> Shub's death (mg3shubtest 4 presses on itself) -> the final text -> credits; the strip checked after a changelevel.
  exits  every trigger_changelevel of the 20 playable BSPs (both copies of the hub's chapter exits): `map <src>`,
         walk into the exit, arrive in its map.

  python mg3_route_test.py write <leg>          writes the private step scripts quakevr/mg3route<N>.cfg (git-ignored)
  python mg3_route_test.py check <leg> <log>    checks a run's console log (PASS/FAIL lines and a total)
  python mg3_route_test.py lang                 the language gate covers every identifier MG3's VR QC uses

mg3_route_test.sh runs them. The game side: QC vr_mg_hub_test.qc's route driver (vr_mg_route_stage: each new map's
or loaded save's first frame runs mg3route<stage>.cfg once; its presser leaves intermissions and respawns the dead
with real jump presses) and vr_mg3_test.qc's route requests (walk into an exit, report, seed, skill brush, death).
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "quakevr")


def W(n):
    return ["wait"] * n


REPORT = ["vr_mg3_test 31"] + W(3)
SEED = ["+grabmain", "+graboff"] + W(5) + ["vr_mg3_test 32"] + W(10) + ["echo mg3route: seeded"] + REPORT
RUNE = ["vr_mg3_test 10"] + W(5)
HUBCHECK = ["vr_mg3_test 11"] + W(700)  # (its doors' check prints 6 s later)
DEATH = ["vr_mg3_test 35"]


# vr_mg3_test.qc MG3_T_RouteMap's numbering (the engine has no string user cvars: the request carries it).
MAPS = ["", "start", "map1", "map2", "map2b", "map3", "map4", "map5", "map6", "map7", "map8", "hub", "secret1",
        "secret2", "secret3", "secret4", "secret5", "secret6", "boss", "boss2"]


def walk(dest, nth=0):
    return ["vr_mg3_test %d" % (100 + 2 * MAPS.index(dest) + nth)]


def visit(label, mapname, dest, arrive=(), after=(), full=True, nth=0):
    """A map's steps: (label, map, commands, kind). A full visit saves on arrival and loads it, then dies (the
    respawn autoloads that save), then does its actions and leaves; a light one only leaves."""
    if not full:
        return [(label, mapname, list(arrive) + list(after) + walk(dest, nth), "light")]
    save = "mg3r_" + re.sub(r"\W", "", label)
    return [(label + " arrive", mapname, list(arrive) + ["save " + save] + W(200) + ["load " + save], "arrive"),
            (label + " loaded", mapname, DEATH, "loaded"),
            (label + " respawned", mapname, list(after) + (walk(dest, nth) if dest else []), "respawned")]


SKILL = 1
MAIN = (
    visit("start", "start", "map1", after=["vr_mg3_test %d" % (40 + SKILL)] + W(5)) +
    visit("map1", "map1", "secret1", arrive=SEED) +
    visit("secret1", "secret1", "hub", after=RUNE) +
    visit("hub rune1", "hub", "map1", after=HUBCHECK) +
    visit("map1 again", "map1", "map2", full=False) +
    visit("map2", "map2", "map2b") +
    visit("map2b", "map2b", "map3") +
    visit("map3", "map3", "secret5") +
    visit("secret5", "secret5", "hub", after=RUNE) +
    visit("hub rune2", "hub", "map3", after=HUBCHECK, full=False) +
    visit("map3 again", "map3", "map4", full=False) +
    visit("map4", "map4", "secret6") +
    visit("secret6", "secret6", "map5") +
    visit("map5", "map5", "secret3") +
    visit("secret3", "secret3", "hub", after=RUNE) +
    visit("hub rune3", "hub", "map5", after=HUBCHECK, full=False) +
    visit("map5 again", "map5", "map6", full=False) +
    visit("map6", "map6", "secret4") +
    visit("secret4", "secret4", "map7") +
    visit("map7", "map7", "map8") +
    visit("map8", "map8", "hub", after=RUNE) +
    visit("hub rune4", "hub", "secret2", after=HUBCHECK, full=False) +
    visit("secret2", "secret2", "boss") +
    visit("boss", "boss", None, after=["vr_mg3_ctest 3", "vr_mg3_test 37"])
)
BN = (
    visit("hub bn", "hub", "secret2", arrive=["vr_mg3_test 15"] + W(5) + ["vr_mg3_test 36"] + W(3) +
          ["vr_mg3_test 16"] + W(5) + REPORT, full=False) +
    [("secret2 strip", "secret2", ["vr_mg3_test 17"] + W(3) + walk("boss"), "light")] +
    visit("boss bn", "boss", None, after=["vr_mg3_ctest 4", "vr_mg3_test 37"]) +
    visit("map1 newgame", "map1", "secret1") +
    visit("secret1 newgame", "secret1", "hub", after=RUNE, full=False) +
    visit("hub newgame", "hub", "secret2", after=["vr_mg3_test 36"] + W(3) + HUBCHECK) +
    visit("boss2", "boss2", None, after=["vr_mg3_shubtest 4", "vr_mg3_test 38"])
)
EXIT_PAIRS = [
    ("start", "map1", 0), ("map1", "secret1", 0), ("map1", "map2", 0), ("map2", "map2b", 0), ("map2b", "map3", 0),
    ("map3", "map4", 0), ("map3", "secret5", 0), ("map4", "map5", 0), ("map4", "secret6", 0), ("map4", "hub", 0),
    ("map5", "map6", 0), ("map5", "secret3", 0), ("map6", "secret4", 0), ("map6", "map7", 0), ("map7", "map8", 0),
    ("map7", "hub", 0), ("map8", "hub", 0),
    ("hub", "map1", 0), ("hub", "map1", 1), ("hub", "map3", 0), ("hub", "map3", 1), ("hub", "map5", 0),
    ("hub", "map5", 1), ("hub", "map7", 0), ("hub", "map7", 1), ("hub", "secret2", 0),
    ("secret1", "hub", 0), ("secret1", "secret1", 0), ("secret2", "boss", 0), ("secret3", "hub", 0),
    ("secret4", "map7", 0), ("secret5", "hub", 0), ("secret6", "map5", 0), ("boss", "hub", 0),
]


def exits_steps():
    steps = []
    for i, (src, dest, nth) in enumerate(EXIT_PAIRS):
        steps.append(("walk %s -> %s#%d" % (src, dest, nth), src, walk(dest, nth), "exit"))
        nxt = EXIT_PAIRS[i + 1][0] if i + 1 < len(EXIT_PAIRS) else None
        steps.append(("arrived %s from %s" % (dest, src), dest, ["map " + nxt] if nxt else
                      ["echo mg3route: exits done", "vr_mg_route_stage 0"] + W(10) + ["toggleconsole", "quit"], "arrived"))
    return steps


LEGS = {"main": MAIN, "bn": BN}
FIRST = {"main": "start", "bn": "hub", "exits": "start"}


def steps_of(leg):
    return exits_steps() if leg == "exits" else LEGS[leg]


def write(leg):
    steps = steps_of(leg)
    for i, (label, _, cmds, _) in enumerate(steps, start=1):
        lines = ["vr_mg_route_stage %d" % (i + 1), "god 1", "notarget 1"] + W(30) + \
                ["echo mg3route: stage %d %s" % (i, label)] + REPORT + cmds
        with open(os.path.join(OUT, "mg3route%d.cfg" % i), "w", newline="\n") as f:
            f.write("\n".join(lines) + "\n")
    end = ["vr_mg_route_stage 0"] + W(300) + ["echo mg3route: end after the finale", "toggleconsole", "quit"]
    with open(os.path.join(OUT, "mg3route_end.cfg"), "w", newline="\n") as f:
        f.write("\n".join(end) + "\n")
    print("mg3route: wrote %d %s steps into %s (first map %s)" % (len(steps), leg, OUT, FIRST[leg]))


def reports(lines):
    """[(stage, report dict)] in log order."""
    out, stage, cur = [], 0, None
    for line in lines:
        m = re.search(r"mg3route: stage (\d+)", line)
        if m:
            stage = int(m.group(1))
        m = re.search(r"mg3route: report map (\S+) skill (\S+) serverflags (\S+) runes (\S+)", line)
        if m:
            cur = dict(stage=stage, map=m.group(1), skill=float(m.group(2)), flags=int(float(m.group(3))),
                       runes=int(float(m.group(4))))
            continue
        if cur is None:
            continue
        m = re.search(r"mg3route: player health (\S+) max (\S+) armor (\S+) shells (\S+) nails (\S+) rockets (\S+) cells (\S+)", line)
        if m:
            cur.update(health=float(m.group(1)), max=float(m.group(2)), armor=float(m.group(3)),
                       ammo=tuple(float(m.group(k)) for k in range(4, 8)))
        m = re.search(r"mg3route: masks (\S+) (\S+) (\S+) (\S+) (\S+) bloody (\S+) caps (\S+) (\S+) (\S+) (\S+) (\S+)", line)
        if m:
            cur.update(masks=tuple(int(float(m.group(k))) for k in range(1, 6)), bloody=int(float(m.group(6))),
                       caps=tuple(float(m.group(k)) for k in range(7, 12)))
        m = re.search(r"mg3route: hands (\S+)/(\S+) clips (\S+)/(\S+) ids (\S+)/(\S+)", line)
        if m:
            cur.update(hands=(m.group(1), m.group(2)), clips=(m.group(3), m.group(4)), ids=[m.group(5), m.group(6)])
        m = re.search(r"mg3route: holsters (.*) clips (.*) ids (.*)", line)
        if m:
            cur.update(hol=m.group(1).split(), hclips=m.group(2).split(), hids=m.group(3).split())
            out.append((cur["stage"], cur))
            cur = None
    return out


def check(leg, path):
    lines = open(path, encoding="latin1", errors="replace").read().splitlines()
    steps = steps_of(leg)
    fails, passes = [0], [0]

    def ok(cond, what):
        print("mg3route: %s %s" % ("PASS" if cond else "FAIL", what))
        (passes if cond else fails)[0] += 1

    stage_lines = {}
    for i, l in enumerate(lines):
        m = re.search(r"mg3route: stage (\d+)", l)
        if m:
            stage_lines.setdefault(int(m.group(1)), i)
    ok(set(stage_lines) == set(range(1, len(steps) + 1)), "every step ran (%d of %d)" % (len(stage_lines), len(steps)))
    reps = reports(lines)
    first = {}
    for s, r in reps:
        first.setdefault(s, r)
    for i, (label, mapname, _, _) in enumerate(steps, start=1):
        r = first.get(i)
        if i in stage_lines:
            ok(r is not None and r["map"] == mapname, "step %d (%s) in %s" % (i, label, r["map"] if r else "?"))
    bad = [l for l in lines if re.search(r"Host_Error|ENGINE (ERROR|CRASH)|load failed|no exit to|TIMEOUT|"
                                         r"No spawn function|is not a field|PR_ExecuteProgram|SV_Error|runaway", l)
           and '"property 1" is not a field' not in l]  # (secret1's BSP: an upstream key with a space, ignored)
    ok(not bad, "no engine error, QC error, stall or missing exit" + ("" if not bad else ": " + bad[0][:110]))
    fails_qc = [l for l in lines if re.search(r"mg3(test|ctest|shubtest): FAIL|mg3(ctest|shubtest): total \d+/[1-9]", l)]
    ok(not fails_qc, "every in-game check passed" + ("" if not fails_qc else ": " + fails_qc[0][:110]))
    walks = [l for l in lines if "mg3route: walk " in l]
    nofit = [l for l in walks if l.rstrip().endswith("fits 0")]
    print("mg3route: %d exits walked, %d reached in noclip (behind a door or bars): %s" %
          (len(walks), len(nofit), ", ".join(re.sub(r".*walk (\S+) -> (\S+).*", r"\1->\2", l) for l in nofit)))

    if leg == "exits":
        return total(passes, fails)
    ended = any("Credits: native campaign end presentation opened" in l for l in lines)
    ok(ended and any("mg3route: end after the finale" in l for l in lines), "the finale, then the credits")
    deaths = sum("mg3route: died" in l for l in lines)
    nloaded = sum(1 for s in steps if s[3] == "loaded")
    ok(deaths == nloaded, "a real death in each fully visited map (%d of %d)" % (deaths, nloaded))
    # Death and load restore the arrival state: each full visit's three reports agree.
    for i, (label, _, _, kind) in enumerate(steps, start=1):
        if kind != "arrive":
            continue
        a = [r for s, r in reps if s == i]
        a = a[-1] if a else None
        b, c = first.get(i + 1), first.get(i + 2)
        if not (a and b and c):
            ok(False, "%s: arrival, load and respawn reports" % label)
            continue
        keys = ("flags", "health", "max", "ammo", "masks", "bloody", "hands", "clips", "hol", "hclips")
        same = all(a[k] == b[k] == c[k] for k in keys)
        ok(same, "%s: the load and the death's autoload restore the save (health %g/%g, flags %d)" %
           (label, c["health"], c["max"], c["flags"]))

    if leg == "main":
        seed_i = next(i for i, s in enumerate(steps, start=1) if s[0] == "map1 arrive")
        seeded = [r for s, r in reps if s == seed_i]
        seeded = seeded[1] if len(seeded) > 1 else None
        ok(seeded is not None and seeded["clips"] == ("3", "7") and seeded["hclips"] == list("123456") and
           seeded["masks"] == (5, 2, 8192, 16384, 1), "map1: hands' magazines 3/7, holsters' 1..6, masks seeded")
        if seeded:
            ids0 = seeded["ids"] + seeded["hids"]
            carried = 0
            for i in range(seed_i + 1, len(steps) + 1):
                r = first.get(i)
                if not r:
                    continue
                ids = r["ids"] + r["hids"]
                good = (r["hands"] == seeded["hands"] and r["clips"] == seeded["clips"] and r["hol"] == seeded["hol"] and
                        r["hclips"] == seeded["hclips"] and len(set(ids)) == 8 and "0" not in ids and
                        r["masks"] == seeded["masks"] and r["caps"] == seeded["caps"] and r["max"] == seeded["max"] and
                        r["ammo"][0] >= seeded["ammo"][0] and r["health"] >= 1)
                carried += good
                if not good:
                    ok(False, "step %d (%s): equipment, masks and capacities carried: %s" % (i, r["map"], r))
            ok(carried == sum(1 for i in range(seed_i + 1, len(steps) + 1) if first.get(i)),
               "equipment (hands, holsters, magazines, 8 distinct ids), masks %s and capacities %s carried through %d steps"
               % ("/".join(map(str, seeded["masks"])), "/".join("%g" % c for c in seeded["caps"]), carried))
        # Runes: each secret's (and map8's) rune taken, its bit carried; the hub's doors match.
        for label, bits in (("hub rune1", 1), ("hub rune2", 3), ("hub rune3", 7), ("hub rune4", 15)):
            i = next(k for k, s in enumerate(steps, start=1) if s[0].startswith(label))
            r = first.get(i)
            ok(r is not None and (r["flags"] & 15) == bits, "%s: runes %d" % (label, r["flags"] & 15 if r else -1))
        hub = [l for l in lines if re.search(r"mg3test: hub \d+ passed, 0 failed", l)]
        ok(len(hub) == 4, "the hub's rune doors and exit match the runes at each return (%d of 4)" % len(hub))
        runes = [l for l in lines if re.search(r"mg3test: runes [1-9]\d* passed, 0 failed", l)]
        ok(len(runes) == 4, "a rune taken in secret1, secret5, secret3 and map8 (%d of 4)" % len(runes))
        ok(any(re.search(r"mg3route: skill brush %d found 1: skill cvar %d" % (SKILL, SKILL), l) for l in lines),
           "start's skill brush %d sets skill %d" % (SKILL, SKILL))
        ok(any(re.search(r"mg3ctest: total \d+/0", l) for l in lines), "Chthon's fight and death (mg3ctest total n/0)")
    if leg == "bn":
        ok(any(re.search(r"mg3test: skill \d+ passed, 0 failed", l) for l in lines), "the hub's skill buttons: Bloody Nightmare on")
        ok(any(re.search(r"mg3test: strip \d+ passed, 0 failed", l) for l in lines), "the Bloody Nightmare strip after a changelevel")
        i = next(k for k, s in enumerate(steps, start=1) if s[0] == "map1 newgame arrive")
        r = first.get(i)
        ok(r is not None and r["flags"] == 448 and r["masks"] == (0, 0, 0, 0, 0) and r["skill"] == 3,
           "Chthon's death: the Bloody Nightmare new game (map1, serverflags %s, masks cleared, skill 3)" % (r["flags"] if r else "?"))
        i = next(k for k, s in enumerate(steps, start=1) if s[0] == "boss2 arrive")
        ok(first.get(i) is not None, "the hub's exit to secret2 leads to boss2 in the new game")
        ok(any(re.search(r"mg3shubtest: total \d+/0", l) for l in lines), "Shub's death (mg3shubtest total n/0)")
    return total(passes, fails)


def total(passes, fails):
    print("mg3route: %d passed, %d failed" % (passes[0], fails[0]))
    return 1 if fails[0] else 0


def lang():
    """Every $identifier MG3's VR QC prints is in the language gate (Quake/vr/vr_loc_mg3.inc)."""
    import glob
    used = set()
    for p in glob.glob(os.path.join(ROOT, "QC", "vr_mg3*.qc")):
        used |= set(re.findall(r'"(\$[A-Za-z0-9_]+)', open(p, encoding="latin1").read()))
    gate = set(re.findall(r'"(\$[A-Za-z0-9_]+)"', open(os.path.join(ROOT, "Quake", "vr", "vr_loc_mg3.inc")).read()))
    missing = sorted(used - gate)
    print("mg3route: %s the language gate covers the %d identifiers of MG3's VR QC (%d in the gate)%s" %
          ("PASS" if not missing else "FAIL", len(used), len(gate), "" if not missing else ": missing " + " ".join(missing)))
    return 1 if missing else 0


def main():
    a = sys.argv[1:]
    if len(a) == 2 and a[0] == "write" and a[1] in FIRST:
        write(a[1])
        return 0
    if len(a) == 3 and a[0] == "check" and a[1] in FIRST:
        return check(a[1], a[2])
    if a == ["lang"]:
        return lang()
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
