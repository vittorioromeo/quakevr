"""Parry balance (ROUND22.md, "Parry balance"): headless checks of a parry's outcome by its strength over the monster's
Parry Poise. Run: python Misc/quakevr/parrybalance/test.py <worktree name> [examples|pose|repeat|chances].

examples: forced parries (vr_kdtrig_test_parry: the tool, the grip, the timing; the pose not looked at) on a fresh
monster each (vr_kdtrig_test 39 / 41, one trial): the outcome table of the plan.
pose: the real crossed-arms pose (mock hands) against a knight: raised long before the blow (plain) or just before it
(well timed).
repeat: diminishing returns: three parries of one knight, then of one ogre, two seconds apart.
chances: the poise factor on hits unbalancing (vr_knockdown_test 40's chance lines), a grunt and an ogre."""
import re
import subprocess
import sys
from pathlib import Path

NAME = sys.argv[1]
ONLY = sys.argv[2:]
TREE = Path("C:/OHWorkspace/qvr-agents") / NAME
KIT = Path("C:/OHWorkspace/qvr-kit")
BASH = "C:/Program Files/Git/bin/bash.exe"
CFG = TREE / "quakevr/test_parrybalance.cfg"
LOG = KIT / "bases" / NAME / "qbase/qconsole.log"
FILTER = "ENGINE|CASE|hit stagger|parry outcome|parry interrupt|kdtrig t|parry: none|parry ragdoll"


def wait(n):
    return "\n".join(["wait"] * n)


def selected(section):
    return not ONLY or section in ONLY


def run(label, commands):
    CFG.write_text("\n".join(commands + ["toggleconsole", "disconnect", wait(5), "quit"]) + "\n")
    result = subprocess.run([BASH, str(KIT / "run.sh"), NAME, "-Script", "exec test_parrybalance.cfg",
                             "-Filter", FILTER, "-Timeout", "240"], capture_output=True, text=True)
    log = LOG.read_text(errors="replace")
    assert result.returncode == 0 and "ENGINE ERROR" not in log and "ENGINE CRASH" not in log, result.stdout[-2000:]
    (TREE / "scratch").mkdir(exist_ok=True)
    (TREE / "scratch" / ("parrybalance_" + label + ".log")).write_text(log)  # (scratch: git-ignored)
    return log


def setup():
    return ["map vrtesthall", wait(60), "setpos 0 0 24 0 0 0", "developer 1", "notarget", "give h 250",
            "vr_enemy_shove 1", "vr_stealth_meter 0", "vr_parry 1", "vr_parry_balance 1", "vr_parry_stamina 0",
            "vr_parry_drop_chance 0", "vr_parry_push_enemy 0", "vr_parry_push_player 0", "vr_kdtrig_test_x 0",
            "vr_kdtrig_test_y 0", "vr_kdtrig_test_z 0", "vr_kdtrig_test_n 1", "vr_kdtrig_test_wait 0.3",
            "vr_weapon_grip_mode 1", "impulse 167", wait(10), LOWER]  # (the crowbar in the main hand: Heft 0)


LOWER = "vr_mock_hand main 0.15 0.2 0.1 70 0 0"
GUARD = "vr_mock_hand main 0.15 1.25 -0.4 0 90 0"  # the crowbar level across (TESTING.md: parryinterrupt's)


def cases(log):
    """Each CASE's lines, by its name."""
    out = {}
    name = None
    for line in log.splitlines():
        m = re.match(r"CASE (\S+)", line)
        if m:
            name = m.group(1)
            out[name] = []
        elif name:
            out[name].append(line)
    return out


def outcome(lines):
    """(kind, seconds or None, ratio, ragdoll?) from a case's parry outcome line."""
    text = "\n".join(lines)
    m = re.search(r"parry outcome: \S+ strength ([\d.]+)( \(well timed\))? poise\s+(\S+) ratio ([\d.]+): (.*)", text)
    assert m, text[-1500:]
    rest = m.group(5)
    if rest.startswith("blocked"):
        return "blocked", None, float(m.group(4)), False, m.group(2) is not None
    k = re.match(r"(flinch|stagger|heavy stagger) ([\d.]+) s", rest)
    assert k, rest
    rag = "parry ragdoll:" in text and "(no ragdoll)" not in rest
    return k.group(1), float(k.group(2)), float(m.group(4)), rag, m.group(2) is not None


if selected("examples"):
    # (name, test class, forced parry, trial test, expected kind, expected seconds, ragdoll expected)
    table = [
        ("bare_vs_grunt_shove", 0, 1, 41, "stagger", 1.2, True),
        ("bare_vs_knight", 6, 1, 39, "flinch", 0.3, False),
        ("sword1h_vs_knight", 6, 2, 39, "stagger", 0.8, True),
        ("sword2h_vs_knight", 6, 3, 39, "heavy stagger", 1.24, True),
        ("weapon1h_vs_deathknight", 3, 2, 39, "stagger", 0.5, True),
        ("bare_vs_ogre", 4, 1, 39, "blocked", None, False),
        ("timed1h_vs_ogre", 4, 12, 39, "flinch", 0.3, False),
        ("timed2h_vs_shambler", 8, 13, 39, "blocked", None, False),
    ]
    cmds = setup()
    for name, cls, forced, test, *_ in table:
        cmds += ["echo CASE " + name, "vr_kdtrig_test_class %d" % cls, "vr_kdtrig_test_parry %d" % forced,
                 "vr_kdtrig_test %d" % test, wait(110)]
    log = run("examples", cmds + ["echo CASE end"])
    got = cases(log)
    print("| case | strength/poise | outcome | ragdoll | health |")
    for name, cls, forced, test, kind, secs, rag in table:
        lines = got[name]
        k, s, ratio, r, timed = outcome(lines)
        hp = re.search(r"health ([\d.]+) -> ([\d.]+)", "\n".join(lines))
        assert k == kind, (name, k, kind)
        assert secs is None or abs(s - secs) < 0.02, (name, s, secs)
        assert r == rag, (name, r, rag, "\n".join(lines)[-1500:])
        took = "%s" % (float(hp.group(1)) - float(hp.group(2))) if hp else "-"
        print("| %s | %.2f | %s%s | %s | %s |" % (name, ratio, k, " %.2f s" % s if s else "", "yes" if r else "no", took))
        if name == "bare_vs_ogre":
            assert hp and 0 < float(hp.group(1)) - float(hp.group(2)) < 10, hp and hp.groups()
    print("examples: PASS")

if selected("pose"):
    cmds = setup() + ["vr_kdtrig_test_parry 0", "vr_kdtrig_test_class 6"]
    # Held up all along: raised, two seconds, the blow.
    cmds += ["echo CASE held", "vr_kdtrig_test 20", wait(60), GUARD, wait(150), "vr_kdtrig_test 9", wait(150),
             "vr_kdtrig_test 22", LOWER, wait(30)]
    # Raised just before the blow.
    cmds += ["echo CASE timed", "vr_kdtrig_test 20", wait(60), GUARD, wait(3), "vr_kdtrig_test 9", wait(150)]
    log = run("pose", cmds + ["echo CASE end"])
    got = cases(log)
    held = outcome(got["held"])
    timed = outcome(got["timed"])
    print("held up: %s, ratio %.2f, %s; raised just before: %s, ratio %.2f, %s" %
          (held[0], held[2], "well timed" if held[4] else "plain", timed[0], timed[2], "well timed" if timed[4] else "plain"))
    assert not held[4] and held[0] == "stagger" and abs(held[2] - 1 / 0.7) < 0.01, held
    assert timed[4] and timed[0] == "stagger" and abs(timed[2] - 1.25 / 0.7) < 0.01, timed
    print("pose: PASS")

if selected("repeat"):
    cmds = setup()
    for name, cls, forced in [("knight", 6, 3), ("ogre", 4, 13)]:
        cmds += ["echo CASE " + name, "vr_kdtrig_test_class %d" % cls, "vr_kdtrig_test_parry %d" % forced,
                 "vr_kdtrig_test 20", wait(60)]
        for i in range(3):
            cmds += ["vr_kdtrig_test 9", wait(150)]
        cmds += ["vr_kdtrig_test 22", wait(10)]  # (22: every monster gone)
    log = run("repeat", cmds + ["echo CASE end"])
    got = cases(log)
    for name in ["knight", "ogre"]:
        text = "\n".join(got[name])
        secs = [float(s) for s in re.findall(r"parry outcome: .*?(?:flinch|stagger) ([\d.]+) s", text)]
        rec = [float(s) for s in re.findall(r"recovered \(.*?([\d.]+) s\)", text)]
        print("%s: staggers %s, recovered after %s" % (name, secs, rec))
        assert len(secs) == 3 and secs[1] < secs[0] and secs[2] < secs[1], secs
        if name == "knight":
            assert abs(secs[1] / secs[0] - 0.7) < 0.02 and abs(secs[2] / secs[0] - 0.49) < 0.02, secs
        else:
            assert abs(secs[1] / secs[0] - 0.7 ** 1.6) < 0.03, secs
    print("repeat: PASS")

if selected("chances"):
    cmds = setup()
    for name, cls in [("grunt", 0), ("ogre", 4)]:
        cmds += ["vr_kdtrig_test_class %d" % cls, "vr_kdtrig_test 20", wait(30)]
        for k in [0, 1]:
            cmds += ["echo CASE %s_%d" % (name, k), "vr_poise_chances %d" % k, "vr_knockdown_test 40", wait(3)]
        cmds += ["vr_kdtrig_test 22", wait(5)]
    log = run("chances", cmds + ["echo CASE end", "vr_poise_chances 1"])
    got = cases(log)

    def chances(case):
        m = re.search(r"weak tap ([\d.]+), strong blow ([\d.]+), head punch ([\d.]+), heavy throw ([\d.]+), weapon throw "
                      r"([\d.]+)", "\n".join(got[case]))
        assert m, got[case]
        return [float(x) for x in m.groups()]
    for name, factor in [("grunt", 1), ("ogre", 1 / 1.6)]:
        off, on = chances(name + "_0"), chances(name + "_1")
        print("%s: hits unbalance off %s, on %s" % (name, off, on))
        assert any(off) and all(abs(b - a * factor) < 0.002 for a, b in zip(off, on)), (off, on)
    print("chances: PASS")
