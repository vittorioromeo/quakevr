#!/usr/bin/env python3
"""qc_cpp_compare.py <tree> [scenario...] -- the C++ QuakeC against the interpreter (docs/vr-port/QC_TO_CPP.md).

Runs each scenario twice through the kit's run.sh, vr_qc_cpp 0 (the interpreter) then 1 (the built-in C++ from
FTEQCC -cppout; the engine built with QvrQcCpp=1), from the same seed in fixed frames, and compares what they print:
vr_bench_statehash fields (a hash of each QuakeC field over every edict, the real-clock fields left out: NOISY) at
fixed frames and the `profile` total (QuakeC statements run since the map started). Equal means the two ran the same
QuakeC to the same state, statement for statement. --control: the interpreter against itself.
Ends with "qc_cpp_compare: N passed, M failed". Scenarios: the names below (default: all).
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qvrtest  # noqa: E402

MIXED = ("monster_army", "monster_ogre", "monster_knight", "monster_wizard")


def grid(count, kinds, x0=180, dx=48, per_row=8, dy=48):
    return ";".join(f"vr_physics_spawn {kinds[i % len(kinds)]} {x0 + (i // per_row) * dx} "
                    f"{(i % per_row - (per_row - 1) / 2) * dy:g}" for i in range(count))


HASH = "vr_bench_statehash"
# (run.sh's "waitN": N frames, each 1/72 s of game time; a bare "wait 300" would be one frame)
H3 = f"wait300;{HASH};wait300;{HASH};wait300;{HASH}"
# (name, body after the seed and the map's first frames); each prints its hashes and the profile total
SCENARIOS = {
    "e1m1": f"map e1m1;wait10;{H3}",
    "combat_48": f"map vrfiringrange;wait10;god;{grid(48, MIXED)};wait300;{HASH};impulse 4;{H3}",
    "hip1m1": f"skill 2;map hip1m1;wait10;god;vr_test_monsters 2;{H3}",
    "arena_wave6": f"vr_arena_seed 7;map vrarena;wait10;god;vr_arena_goto 6;{H3}",
    "bots_dm4": f"maxplayers 4;deathmatch 1;map dm4;wait10;impulse 100;impulse 100;impulse 100;{H3}",
    "saveload": f"map e1m1;wait120;save qc2cpp_cmp;wait60;{HASH};load qc2cpp_cmp;wait120;{HASH};wait300;{HASH}",
}


# Fields fed from the real clock (the mock headset's pose times: vr_posetime; the hands' motion history mh_*, its
# ring and speeds from those times), different between runs even in the interpreter (--control shows them on e1m1
# and arena_wave6): left out of the comparison.
NOISY = re.compile(r"^(vr_posetime|mh_\w+(\[\d+\])?)$")


def run(tree, cpp, body):
    """Each checkpoint as (its line without the whole-state hash, {field: hash} without the noisy fields), the profile
    line, and the info lines."""
    body = body.replace(HASH, HASH + " fields")
    script = f"vr_qc_cpp {cpp};sv_random_seed 7;{body};profile 0;vr_qc_cpp_info;toggleconsole;quit"
    out = qvrtest.run_sh(tree, ["-Script", script, "-Timeout", "600",
                                "-Filter", "vr_bench_statehash:|statefield |profile total|vr_qc_cpp |rror|runaway"],
                         timeout=900)
    points, info = [], []
    for line in (l.strip() for l in out.splitlines()):
        if line.startswith("vr_bench_statehash:"):
            points.append([line.rsplit(",", 1)[0], {}])
        elif line.startswith("statefield ") and points:
            name, h = line[len("statefield "):].rsplit(" ", 1)
            if not NOISY.match(name):
                points[-1][1][name] = h
        elif "profile total" in line:
            points.append([line, {}])
        elif "vr_qc_cpp " in line or "rror" in line or "runaway" in line:
            info.append(line)
    return points, info


def main():
    tree = sys.argv[1]
    control = "--control" in sys.argv  # (the interpreter against itself: is the scenario repeatable at all?)
    names = [a for a in sys.argv[2:] if a != "--control"] or list(SCENARIOS)
    tally = qvrtest.Tally("qc_cpp_compare")
    for name in names:
        vm, vminfo = run(tree, 0, SCENARIOS[name])
        cpp, cppinfo = run(tree, 0 if control else 1, SCENARIOS[name])
        ok = bool(vm) and vm == cpp and (control or any("matches the loaded progs" in l for l in cppinfo))
        tally.check(ok, name)
        for a, b in zip(vm, cpp):
            if a == b:
                print(f"  ==  {a[0]} ({len(a[1])} fields)")
                continue
            print(f"  vm  {a[0]}\n  cpp {b[0]}")
            diff = sorted(k for k in set(a[1]) | set(b[1]) if a[1].get(k) != b[1].get(k))
            if diff:
                print(f"  {len(diff)} fields differ: {' '.join(diff[:12])}")
        if len(vm) != len(cpp):
            print(f"  line counts differ: vm {len(vm)}, cpp {len(cpp)}")
        for l in cppinfo + [l for l in vminfo if "rror" in l]:
            print(f"  info {l}")
    return tally.verdict()


if __name__ == "__main__":
    sys.exit(main())
