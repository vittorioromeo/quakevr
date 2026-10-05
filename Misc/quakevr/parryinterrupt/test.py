"""Focused mock checks for parry interruption; run with a qvr-kit worktree name."""
import re
import subprocess
import sys
from pathlib import Path

NAME = sys.argv[1]
ONLY = sys.argv[2:]

def selected(section):
    return not ONLY or section in ONLY
TREE = Path("C:/OHWorkspace/qvr-agents") / NAME
KIT = Path("C:/OHWorkspace/qvr-kit")
BASH = "C:/Program Files/Git/bin/bash.exe"
CFG = TREE / "quakevr/test_parryinterrupt.cfg"
LOG = KIT / "bases" / NAME / "qbase/qconsole.log"
GUARD = "vr_mock_hand main 0.15 1.25 -0.4 70 90 0"
LOWER = "vr_mock_hand main 0.15 0.2 0.1 70 0 0"


def wait(n):
    return "\n".join(["wait"] * n)


def run(label, commands):
    CFG.write_text("\n".join(commands + ["toggleconsole", "disconnect", wait(5), "quit"]) + "\n")
    result = subprocess.run([BASH, str(KIT / "run.sh"), NAME, "-Script", "exec test_parryinterrupt.cfg",
                             "-ExtraArgs", "-noaddons", "-Filter", "ENGINE|parry interrupt|menu paths",
                             "-Timeout", "90"], capture_output=True, text=True)
    log = LOG.read_text(errors="replace")
    assert result.returncode == 0 and "ENGINE ERROR" not in log and "ENGINE CRASH" not in log, result.stdout
    (TREE / ("parryinterrupt_" + label + ".log")).write_text(log)
    return log


def setup(enabled, guard=True, stationary=True):
    return ["map vrcalibration", wait(60), "setpos 0 0 24 0 0 0", "developer 1",
            "vr_enemy_shove 0", "vr_parry 1", "vr_parry_stamina 0", "vr_parry_drop_chance 0",
            "vr_parry_push_enemy 0", "vr_parry_push_player 0", "vr_parry_reduction 0.75",
            "vr_parry_interrupt " + str(enabled), "vr_parry_stagger 0.35", "vr_weapon_grip_mode 1",
            "impulse 154", wait(5), GUARD if guard else LOWER, wait(20)] + (
            ["notarget"] if stationary else []) + ["vr_test_spawn 5", "vr_test_spawn_dist 48", "impulse 241", wait(30)]


def health(log):
    return [float(h) for h in re.findall(r"^health\s+([-\d.]+)", log, re.M)]


# Real parry, then lower the weapon. Follow-up damage must stay cancelled until recovery.
for enabled, guarded, expected in [(1, True, [97, 97, 87]), (0, True, [97, 87, 77]),
                                   (1, False, [90, 80, 70]), (0, False, [90, 80, 70])] if selected("forced") else []:
    label = "forced_%s_%s" % (enabled, "guard" if guarded else "fail")
    log = run(label, setup(enabled, guarded) + ["impulse 242", wait(1), "edict 1", LOWER,
              wait(2), "impulse 242", wait(1), "edict 1", wait(35), "impulse 242", wait(1), "edict 1"])
    got = health(log)
    assert got == expected, (label, got, expected)
    assert ("remaining melee hit cancelled" in log) == bool(enabled and guarded), label
    assert ("monster_knight recovered" in log) == bool(enabled and guarded), label
    print(label + ": PASS health " + str(got))

# Let an actual knight animation run. Enabled: one hit per attack, no cooldown hits.
for enabled in [1, 0] if selected("animations") else []:
    label = "animation_%s" % enabled
    log = run(label, setup(enabled, stationary=False) + [wait(150), "edict 1"])
    parries = len(re.findall(r"^parry: monster_knight with hand", log, re.M))
    repeats = len(re.findall(r"^parry: monster_knight again", log, re.M))
    interruptions = log.count("staggered 0.35 s")
    recoveries = log.count("monster_knight recovered")
    assert parries > 0 and len(health(log)) == 1, (label, health(log), parries, repeats)
    hits = parries + repeats
    assert 100 - 3 * hits <= health(log)[0] <= 100 - hits, (label, health(log), hits)
    if enabled:
        assert repeats == 0 and interruptions == parries and recoveries > 0, (label, interruptions, recoveries)
    else:
        assert repeats > 0 and interruptions == 0 and recoveries == 0, (label, repeats)
    print(label + ": PASS first parries %s, remaining hits %s, recoveries %s, health %s" %
          (parries, repeats, recoveries, health(log)))

if selected("timing"):
    log = run("timing", setup(1) + ["vr_parry_stagger 0.8", "impulse 242", wait(1), "edict 1", LOWER,
              wait(35), "impulse 242", wait(1), "edict 1", wait(30), "impulse 242", wait(1), "edict 1"])
    assert health(log) == [97, 97, 87], health(log)
    assert "staggered 0.80 s" in log and "monster_knight recovered" in log
    print("0.8 s stagger: PASS health " + str(health(log)))

if selected("smoke"):
    log = run("smoke", ["map e1m1", wait(60), "vr_menu_path_check maps/vrcalibration.map"])
    assert re.search(r"menu paths: \d+ found, 0 missing", log), "calibration menu paths"
    print("e1m1 smoke / calibration menu paths: PASS")


if selected("dragon"):
    for enabled, guarded in [(1, True), (0, True), (1, False)]:
        label = "dragon_%s_%s" % (enabled, "guard" if guarded else "fail")
        log = run(label, setup(enabled, guarded)[:-5] +
                  ["vr_physics_spawn VR_Parry_DragonTest 120", wait(65)])
        match = re.search(r"dragon parry test: first health (\d+), after follow-ups (\d+), interrupted (\d+), attacking (\d+), recovery think (\d+)", log)
        assert match, log[-2000:]
        first, after, interrupted, attacking, recovering = map(int, match.groups())
        if enabled and guarded:
            assert (first, after, interrupted, attacking, recovering) == (492, 492, 1, 0, 1), match.groups()
            assert "dragon parry test: recovered 1, attacking 0, route 1, moved 1, next think 1" in log, log[-2000:]
        elif guarded:
            assert first == 492 and after < first and not interrupted and not recovering, match.groups()
        else:
            assert first == 470 and after < first and not interrupted and not recovering, match.groups()
        print(label + ": PASS first/after health %s/%s, interruption %s, safe route recovery %s" %
              (first, after, interrupted, bool(enabled and guarded)))

if selected("callback"):
    for enabled in [1, 0]:
        label = "callback_%s" % enabled
        log = run(label, setup(enabled)[:-5] +
                  ["vr_physics_spawn VR_Parry_SameCallbackTest 48", wait(1)])
        expected = "first health 497, after follow-ups %s, self/vectors restored 1, recovery think %s" % (
            497 if enabled else 491, enabled)
        assert "parry callback test: " + expected in log, log[-2000:]
        print(label + ": PASS " + expected)
