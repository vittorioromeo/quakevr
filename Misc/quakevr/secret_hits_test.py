"""Actual e1m1 secret doors/Quad trigger: tracked fists and a shotgun; enemy blood controls.
Run after building: python Misc/quakevr/secret_hits_test.py <worktree-name>
The kit's quakevr junction keeps all test saves inside that worktree.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

name = sys.argv[1]
tree = Path(f"C:/OHWorkspace/qvr-agents/{name}")
kit = "C:/OHWorkspace/qvr-kit"
steps = ["vr_enabled 1", "vr_fixed_frames 1", "developer 1", "vr_weapon_grip_mode 1",
         "vr_gunangle 0", "vr_gunyaw 0", "vr_handcal_x 0", "vr_handcal_y 0", "vr_handcal_z 0",
         "vr_handcal_roll 0", "vr_handcal_off_mirror 0", "vr_handcal_off_x 0", "vr_handcal_off_y 0",
         "vr_handcal_off_z 0", "vr_handcal_off_roll 0"]
cases = []
for fixture, pos, yaw, direction, target in [
    ("quad", (490, 2016, -88), 180, (-1, 0, 0), 40),
    ("door", (544, 2290, -88), 270, (0, -1, 0), 39),
    ("enemy", (480, -352, 84), 90, (0, 1, 0), None),
]:
    for attack in ("shot", "punch"):
        case = f"{fixture}-{attack}"
        cases.append((case, target))
        x, y, z = pos
        # The hand goes through the torso; keep the player's feet at their original height.
        player_z = 88 if fixture == "enemy" else z
        steps += ["-attack", "-grabmain", "map e1m1", "wait90", "god", "notarget", "noclip",
                  f"echo SECRET_CASE_{case}", f"setpos {x} {y} {player_z} 0 {yaw} 0", "wait20",
                  f"impulse {154 if attack == 'shot' else 150}", "wait10",
                  "vr_mock_hand main -0.05 1.3 -0.3 0 0 0", "wait10",
                  f"vr_mock_hand_to main {x} {y} {z}", "wait30"]
        if fixture == "enemy":
            steps += ["vr_test_spawn 0", "vr_test_spawn_dist 48", "impulse 241", "wait30"]
        if attack == "punch":
            steps += ["+grabmain", "wait10"]
        steps += [f"save secretfx-{case}-before", "wait10"]
        if attack == "shot":
            steps += ["+attack", "wait5", "-attack"]
        else:
            for d in range(8, 57 if fixture == "enemy" else 41, 8):
                p = tuple(pos[i] + d * direction[i] for i in range(3))
                steps += [f"vr_mock_hand_to main {p[0]} {p[1]} {p[2]}", "wait2"]
        steps += ["wait80", f"save secretfx-{case}-after", "wait20", "-grabmain"]
steps += ["echo SECRET_HITS_DONE", "toggleconsole", "quit"]
env = dict(os.environ, PSExecutionPolicyPreference="Bypass")
command = (f"bash {kit}/run.sh {shlex.quote(name)} -Script {shlex.quote(';'.join(steps))} "
           '-Filter "SECRET_|hit:|melee event:|rror|ENGINE|exit="')
if '--check' not in sys.argv:
    result = subprocess.run(["C:/Program Files/Git/bin/bash.exe", "-lc", command], env=env,
                            text=True, capture_output=True, timeout=100)
    print(result.stdout.strip())
    if result.returncode:
        print(result.stderr)
        raise SystemExit(result.returncode)
    log = Path(f"{kit}/bases/{name}/qbase/qconsole.log").read_text(errors="replace")
    (tree / "secretfx-test.log").write_text(log)
else:
    log = (tree / 'secretfx-test.log').read_text()
assert "SECRET_HITS_DONE" in log, "test did not finish"

def snapshot(case, stage):
    text = (tree / "quakevr" / f"secretfx-{case}-{stage}.sav").read_text()
    return [dict(re.findall(r'"([^\"]+)"\s+"([^\"]*)"', block))
            for block in re.findall(r"\{([^}]*)\}", text)]

for case, target in cases:
    before, after = snapshot(case, "before"), snapshot(case, "after")
    blood = float(after[0]["vr_blood_sent"]) - float(before[0]["vr_blood_sent"])
    case_log = log.split(f"SECRET_CASE_{case}", 1)[1].split("SECRET_CASE_", 1)[0]
    if target is not None:
        # Both fixtures retain their brush identity; use bounds because a closed door's origin is zero.
        moved = after[target + 1]["absmin"] != before[target + 1]["absmin"]
        sparks = len(re.findall(r"hit: .* is metal: sparks", case_log))
        assert blood == 0, f"{case} bled {blood} times"
        for counter in ("vr_gore_sent", "vr_wound_sent"):
            assert after[0][counter] == before[0][counter], f"{case} emitted {counter}"
        assert moved, f"{case} did not activate its secret door"
        assert sparks > 0, f"{case} emitted no spark effects"
        print(f"PASS {case}: blood={blood:g}, spark effects={sparks}, secret moved={moved}")
    else:
        assert blood > 0, f"{case} did not preserve enemy blood"
        assert "is metal: sparks" not in case_log, f"{case} misclassified the enemy"
        print(f"PASS {case}: enemy blood={blood:g}")
