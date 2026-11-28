"""aglod_balance.py -- the armed humanoids' think LOD (vr_agrunt_think_lod) on against off in scripted fights: each
scenario and seed run twice, LOD off then on, vr_agrunt_test 235's census read from each (QC/vr_armedgrunt_lod_test.qc:
over 30 s the damage the player took, the frames he was hit in, the armed humanoids' shots, the kills, the skipped
weapon thinks, a hash of the game alone). The same hash with the LOD on and off: the same fight frame for frame.
Appends a row per run to <out> (TSV) and prints `aglod_balance: N same, M differ`.
  python Misc/quakevr/aglod_balance.py <out.tsv> <scenarios, e.g. hip1m1,combat48,arena> <seeds, e.g. 7,11,23>
Runs through the kit's run.sh (fast mode: every frame 1/72 s of game time)."""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qvrtest  # noqa: E402

TREE = os.path.basename(os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
W = "vr_waituntil host_framerate == 99 %d"
SEEDS = "sv_random_seed {s};vr_particle_seed {s};vr_map_seed {s};skill 2;developer 0;vr_agrunt_think_lod {l}"
END = "vr_agrunt_test 235;vr_waitprint 9000 \"aglod census\";toggleconsole;quit"
MIXED = ("monster_army", "monster_ogre", "monster_knight", "monster_wizard")
def grid(x0):
    return ";".join("vr_physics_spawn %s %d %g" % (MIXED[i % 4], x0 + (i // 8) * 48, (i % 8 - 3.5) * 48) for i in range(48))


GRID = grid(180)
SCEN = {
    # questopt2's scene: hip1m1, every monster brought in and woken onto him (40 armed humanoids)
    "hip1m1": "map hip1m1;" + W % 180 + ";god 1;vr_test_monsters 4;" + W % 30 + ";vr_test_monsters 2;" + W % 300,
    # the bench's combat_48 (48 mixed monsters, 12 armed grunts, in front of him on the firing range)
    "combat48": "map vrfiringrange;" + W % 120 + ";god 1;" + GRID + ";" + W % 90,
    # the same 48 farther off (from 480 units) and behind him (the mock head looking back): they hunt him unseen, far
    "behind48": "map vrfiringrange;" + W % 120 + ";god 1;" + grid(480) + ";vr_mock_look 0 180;" + W % 90,
    # the arena's waves from wave 3
    "arena": "map vrarena;" + W % 60 + ";god 1;vr_arena_play default 3;" + W % 300,
}
FIELDS = ("think_lod", "frames", "dmg", "hits", "shots", "kills", "skips", "monsters", "game")


def run(scen, seed, lod):
    script = SEEDS.format(s=seed, l=lod) + ";" + SCEN[scen] + ";" + END
    text = qvrtest.run_sh(TREE, ["-Timeout", "600", "-QuitOn", "aglod census", "-Filter",
                                 "aglod census|Host_Error|SV_Error|TIMEOUT", "-Script", script], timeout=900)
    m = re.search(r"aglod census: " + r"\s+".join(f + r"\s+(-?[0-9.]+)" for f in FIELDS), text)  # (a long line may wrap)
    return dict(zip(FIELDS, m.groups())) if m else None


def main():
    out, scens, seeds = sys.argv[1], sys.argv[2].split(","), sys.argv[3].split(",")
    same = differ = 0
    with open(out, "a", encoding="utf-8") as f:
        for sc in scens:
            for s in seeds:
                r = {l: run(sc, s, l) for l in ("0", "1")}
                for l in ("0", "1"):
                    row = r[l]
                    f.write("\t".join([sc, s, l] + ([row[k] for k in FIELDS[1:]] if row else ["NO RESULT"])) + "\n")
                f.flush()
                ok = r["0"] and r["1"] and r["0"]["game"] == r["1"]["game"] and r["0"]["dmg"] == r["1"]["dmg"]
                same += 1 if ok else 0
                differ += 0 if ok else 1
                print("aglod_balance: %s seed %s: %s (off %s / on %s)" % (sc, s, "same" if ok else "DIFFER", r["0"], r["1"]))
    print("aglod_balance: %d same, %d differ" % (same, differ))


main()
