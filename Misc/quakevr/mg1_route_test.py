"""Dimension of the Machine (MG1) campaign route sweep: start -> hub -> every episode's maps in order through their real
exits -> the final gate -> mgend -> the credits, with equipment carry, saves/loads and a death on the way.

  python mg1_route_test.py write            writes the private step scripts quakevr/mg1route<N>.cfg (git-ignored)
  python mg1_route_test.py check <log> [--skill N]   checks a run's console log (prints PASS/FAIL lines and a total)

mg1_route_test.sh runs both. The game side is QC vr_mg_hub_test.qc: with vr_mg_route_stage set, each new map's (or
loaded save's) first frame runs mg1route<stage>.cfg once; a step ends by walking into the map's exit
(vr_mg_hub_test 30/32: the player put inside the trigger, the engine's own touch runs the real changelevel; an exit
behind a shut door is reached in noclip) and the presser's real jump presses leave the intermission. Steps:
seeded hand/holster equipment (vr_mg_hub_test 20, both grips held) carried through mge1m1 -> mge1m3 (secret) ->
mge1m2 and mge2m1 -> mge2m2; a save/load in mge1m1 and in the hub with all five runes; a real death in mge1m2
(vr_mg_hub_test 36) whose respawn autoloads the save made there; each rune taken (vr_mg_hub_test 4; mge2m2 through
the electrode puzzle, vr_mg_hub_test 2), the hub's next gate, the final gate, mgend's exit and the credits.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "..", "quakevr"))


def W(n):
    return ["wait"] * n


REPORT = ["vr_mg_hub_test 3"] + W(3)
WALK = ["vr_mg_hub_test 30"]
WALK_ALT = ["vr_mg_hub_test 32"]
RUNE = ["vr_mg_hub_test 4"] + W(5)
SEED = ["+grabmain", "+graboff"] + W(5) + ["vr_mg_hub_test 20"] + W(10) + ["echo mg1route: seeded"] + REPORT


def save_load(name):
    return ["save " + name] + W(200) + ["load " + name]


# (label, map the step runs in, commands)
STEPS = [
    ("start", "start", WALK),
    ("hub fresh", "hub", WALK),
    ("mge1m1 seed+save", "mge1m1", SEED + save_load("mg1r_a")),
    ("mge1m1 loaded", "mge1m1", WALK_ALT),
    ("mge1m3 secret", "mge1m3", WALK),
    ("mge1m2 save+death", "mge1m2", ["save mg1r_b"] + W(200) + ["vr_mg_hub_test 36"]),
    ("mge1m2 after death", "mge1m2", RUNE + REPORT + WALK),
    ("hub rune1", "hub", WALK),
    ("mge2m1 seed", "mge2m1", SEED + WALK),
    ("mge2m2 puzzle", "mge2m2", ["vr_mg_hub_test 2"] + W(2600) + REPORT + WALK),
    ("hub rune2", "hub", WALK),
    ("mge3m1", "mge3m1", WALK),
    ("mge3m2", "mge3m2", RUNE + REPORT + WALK),
    ("hub rune3", "hub", WALK),
    ("mge4m1", "mge4m1", WALK),
    ("mge4m2", "mge4m2", RUNE + REPORT + WALK),
    ("hub rune4", "hub", WALK),
    ("mge5m1", "mge5m1", WALK),
    ("mge5m2", "mge5m2", RUNE + REPORT + WALK),
    ("hub all runes save", "hub", save_load("mg1r_c")),
    ("hub loaded", "hub", WALK),
    ("mgend", "mgend", WALK),
]


def write():
    for i, (label, _, cmds) in enumerate(STEPS, start=1):
        lines = ["vr_mg_route_stage %d" % (i + 1), "god 1", "notarget 1"] + W(30) + \
                ["echo mg1route: stage %d %s" % (i, label)] + REPORT + cmds
        with open(os.path.join(OUT, "mg1route%d.cfg" % i), "w", newline="\n") as f:
            f.write("\n".join(lines) + "\n")
    end = ["vr_mg_route_stage 0"] + W(100) + ["echo mg1route: end after credits", "toggleconsole", "quit"]
    with open(os.path.join(OUT, "mg1route_end.cfg"), "w", newline="\n") as f:
        f.write("\n".join(end) + "\n")
    print("mg1route: wrote %d steps into %s" % (len(STEPS), OUT))


def reports(lines):
    """[(stage, report dict)] in log order."""
    out, stage, cur = [], 0, None
    for line in lines:
        m = re.search(r"mg1route: stage (\d+)", line)
        if m:
            stage = int(m.group(1))
        m = re.search(r"mghubtest: report map (\S+) flags (\S+) indicators \S+ active (\S+) ghosts (\S+) finalgate (\S+)", line)
        if m:
            cur = dict(stage=stage, map=m.group(1), flags=int(float(m.group(2))), active=int(m.group(3)),
                       gate=int(m.group(5)), hol=[])
            continue
        if cur is None:
            continue
        m = re.search(r"mghubtest: player health (\S+) max (\S+)", line)
        if m:
            cur.update(health=float(m.group(1)), max=float(m.group(2)))
        m = re.search(r"mghubtest: hands shells (\S+) weapons (\S+)/(\S+) clips (\S+)/(\S+) ids (\S+)/(\S+)", line)
        if m:
            cur.update(shells=float(m.group(1)), hands=(m.group(2), m.group(3)), clips=(m.group(4), m.group(5)),
                       ids=(m.group(6), m.group(7)))
        m = re.search(r"mghubtest: holster(\d) weapon (\S+) clip (\S+) id (\S+)", line)
        if m:
            cur["hol"].append((m.group(2), m.group(3), m.group(4)))
            if m.group(1) == "5":
                out.append((cur["stage"], cur))
                cur = None
    return out


def check(path, skill):
    lines = open(path, encoding="latin1", errors="replace").read().splitlines()
    fails = [0]
    passes = [0]

    def ok(cond, what):
        print("mg1route: %s %s" % ("PASS" if cond else "FAIL", what))
        (passes if cond else fails)[0] += 1

    stages = {int(m.group(1)) for m in (re.search(r"mg1route: stage (\d+)", l) for l in lines) if m}
    ok(stages == set(range(1, len(STEPS) + 1)), "every step ran (%d of %d)" % (len(stages), len(STEPS)))
    for i, (label, mapname, _) in enumerate(STEPS, start=1):
        ran = [l for l in lines if re.search(r"mg1route: stage %d " % i, l)]
        rep = [r for s, r in reports(lines) if s == i]
        if ran and rep:
            ok(rep[0]["map"] == mapname, "step %d (%s) in %s" % (i, label, rep[0]["map"]))
    ok(any("mg1route: end after credits" in l for l in lines), "the credits after mgend's exit")
    bad = [l for l in lines if re.search(r"Host_Error|ENGINE (ERROR|CRASH)|load failed|no open exit|TIMEOUT", l)]
    ok(not bad, "no engine error, stall or missing exit" + ("" if not bad else ": " + bad[0][:100]))

    reps = reports(lines)
    first = {}
    for s, r in reps:
        first.setdefault(s, r)
    cap = 50 if skill >= 3 else 100
    carried = min(73, cap)
    # The hub: fresh equipment each entry, the runes so far, the final gate only with all five.
    want = {2: 0, 8: 1, 11: 3, 14: 7, 17: 15, 20: 31, 21: 31}
    for s, flags in sorted(want.items()):
        r = first.get(s)
        if not r:
            ok(False, "hub report at step %d" % s)
            continue
        ok((r["flags"] & 31) == flags and r["active"] == bin(flags).count("1") + 1 and r["gate"] == (flags == 31),
           "hub step %d: runes %d, %d indicators active, final gate %d" % (s, r["flags"] & 31, r["active"], r["gate"]))
        ok(r["shells"] == 25 and r["hands"] == ("0", "0") and r["health"] == cap and r["max"] == cap,
           "hub step %d: fresh equipment (shells 25, empty hands, health %g/%g)" % (s, r["health"], r["max"]))
    # The seeded equipment (step 3, 9) carried through the episode's maps, a load and a death's autoload.
    for seed_step, follow in ((3, (4, 5, 6, 7)), (9, (10,))):
        seeded = [r for s, r in reps if s == seed_step]
        seeded = seeded[1] if len(seeded) > 1 else None
        if not seeded:
            ok(False, "seeded report at step %d" % seed_step)
            continue
        ok(seeded["clips"] == ("3", "7") and [h[1] for h in seeded["hol"]] == ["1", "2", "3", "4", "5", "6"],
           "step %d: hands' magazines 3/7 and holsters' 1..6 seeded" % seed_step)
        for s in follow:
            r = first.get(s)
            if not r:
                ok(False, "report at step %d" % s)
                continue
            # Step 4 is the same map's load (the seeded health as it was); a level change carries at most the cap.
            health = seeded["health"] if s == 4 else carried
            # Ids: a carried weapon keeps its own when the new map's records left it free (vr_weaponinst.qc
            # WeaponInst_FreeUid), else gets a new one; each stays distinct.
            ids = list(r["ids"]) + [h[2] for h in r["hol"]]
            kept = sum(a == b for a, b in zip(ids, list(seeded["ids"]) + [h[2] for h in seeded["hol"]]))
            ok(r["shells"] == 42 and r["hands"] == seeded["hands"] and r["clips"] == seeded["clips"] and
               [h[:2] for h in r["hol"]] == [h[:2] for h in seeded["hol"]] and len(set(ids)) == 8 and
               "0" not in ids and r["health"] == health and r["max"] == cap,
               "step %d (%s): equipment carried (health %g/%g, clips %s, holsters' clips kept, 8 distinct ids, %d kept)"
               % (s, r["map"], r["health"], r["max"], "/".join(r["clips"]), kept))
    print("mg1route: %d passed, %d failed" % (passes[0], fails[0]))
    return 1 if fails[0] else 0


def main():
    if len(sys.argv) >= 2 and sys.argv[1] == "write":
        write()
        return 0
    if len(sys.argv) >= 3 and sys.argv[1] == "check":
        skill = int(sys.argv[4]) if len(sys.argv) >= 5 and sys.argv[3] == "--skill" else 1
        return check(sys.argv[2], skill)
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
