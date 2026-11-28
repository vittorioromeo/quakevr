#!/usr/bin/env python3
"""qc_cpp_compare.py <tree> [scenario...] -- the C++ QuakeC against the interpreter (docs/vr-port/QC_TO_CPP.md).

Runs each scenario twice through the kit's run.sh, vr_qc_cpp 0 (the interpreter) then 1 (the built-in C++ from
FTEQCC -cppout; the engine built with QvrQcCpp=1), from the same seed in fixed frames, and compares what they print:
vr_bench_statehash (every edict's whole state) at fixed frames and the `profile` total (QuakeC statements run since
the map started). Equal lines mean the two ran the same QuakeC to the same state, statement for statement.
Ends with "qc_cpp_compare: N passed, M failed". Scenarios: the names below (default: all).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qvrtest  # noqa: E402

MIXED = ("monster_army", "monster_ogre", "monster_knight", "monster_wizard")


def grid(count, kinds, x0=180, dx=48, per_row=8, dy=48):
    return ";".join(f"vr_physics_spawn {kinds[i % len(kinds)]} {x0 + (i // per_row) * dx} "
                    f"{(i % per_row - (per_row - 1) / 2) * dy:g}" for i in range(count))


HASH = "vr_bench_statehash"
# (name, body after the seed and the map's first frames); each prints its hashes and the profile total
SCENARIOS = {
    "e1m1": "map e1m1;wait 300;" + HASH + ";wait 300;" + HASH,
    "combat_48": "map vrfiringrange;wait 10;god;" + grid(48, MIXED) + ";wait 300;" + HASH + ";impulse 4;wait 300;" + HASH,
    "hip1m1": "skill 2;map hip1m1;wait 10;god;vr_test_monsters 2;wait 300;" + HASH + ";wait 300;" + HASH,
    "arena_wave6": "vr_arena_seed 7;map vrarena;wait 10;god;vr_arena_goto 6;wait 300;" + HASH + ";wait 300;" + HASH,
    "bots_dm4": "maxplayers 4;deathmatch 1;map dm4;wait 10;impulse 100;impulse 100;wait 300;" + HASH + ";wait 300;" + HASH,
    "saveload":"map e1m1;wait 120;save qc2cpp_cmp;wait 60;" + HASH + ";load qc2cpp_cmp;wait 120;" + HASH,
}


def run(tree, cpp, body):
    script = f"vr_qc_cpp {cpp};sv_random_seed 7;{body};profile 0;vr_qc_cpp_info;toggleconsole;quit"
    out = qvrtest.run_sh(tree, ["-Script", script, "-Timeout", "600",
                                "-Filter", "vr_bench_statehash:|profile total|vr_qc_cpp |rror|runaway"], timeout=900)
    lines = [l.strip() for l in out.splitlines()]
    keep = [l for l in lines if "vr_bench_statehash:" in l or "profile total" in l]
    info = [l for l in lines if "vr_qc_cpp " in l or "rror" in l or "runaway" in l]
    return keep, info


def main():
    tree = sys.argv[1]
    names = sys.argv[2:] or list(SCENARIOS)
    tally = qvrtest.Tally("qc_cpp_compare")
    for name in names:
        vm, vminfo = run(tree, 0, SCENARIOS[name])
        cpp, cppinfo = run(tree, 1, SCENARIOS[name])
        ok = bool(vm) and vm == cpp and any("matches the loaded progs" in l for l in cppinfo)
        tally.check(ok, name)
        for a, b in zip(vm, cpp):
            print(f"  vm  {a}\n  cpp {b}" if a != b else f"  ==  {a}")
        if len(vm) != len(cpp):
            print(f"  line counts differ: vm {len(vm)}, cpp {len(cpp)}")
        for l in cppinfo + [l for l in vminfo if "rror" in l]:
            print(f"  info {l}")
    return tally.verdict()


if __name__ == "__main__":
    sys.exit(main())
