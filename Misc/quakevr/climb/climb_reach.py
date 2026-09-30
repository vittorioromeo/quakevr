# climb_reach.py: how far from a ledge a grip takes it, against vr_climb_leniency (ROUND21.md, "Climbing: the leniency
# is the one reach"). vrclimb's ledge (top 48, face x 96) and rung 56 (face x 88): the hand 0, 2, 5, 10 and 20 cm from
# them, in front of the face (just under the lip), above the top (over it, 2 units in), and out and up from the lip's
# corner (at 45 degrees), at leniencies 0, 6 and 15 cm.
#   python climb_reach.py script        the console script (vr_climb_try at each point, at each leniency)
#   python climb_reach.py table <log>   the grab / no-grab table from the run's "climbtry" lines (E exact, L lenient, - none)
import re
import sys

CM = [0, 2, 5, 10, 20]
LENIENCIES = [0, 6, 15]
WORLD_SCALE = 1.25  # vr_world_scale (the default; set by the script)
U = WORLD_SCALE / 3.81  # units per cm

def pts_ledge(face, y, top, below):
    return {
        "front": [(face - c * U, y, top - below) for c in CM],
        "above": [(face + 2, y, top + c * U) for c in CM],
        "corner": [(face - c * U * 0.7071, y, top + c * U * 0.7071) for c in CM],
    }

SITES = [("ledge", "78 176 24 0 0 0", pts_ledge(96, 173, 48, 1)), ("rung", "71 0 24 0 0 0", pts_ledge(88, -3, 56, 1))]

def script():
    cmds = ["vr_fixed_frames 1", "vr_climb 1", f"vr_world_scale {WORLD_SCALE:g}", "vr_climb_min_height 0", "map vrclimb", "wait20"]
    for name, pos, dirs in SITES:
        cmds += [f"setpos {pos}", "noclip", "wait20"]
        for lenient in LENIENCIES:
            cmds.append(f"vr_climb_leniency {lenient}")
            for d, pts in dirs.items():
                for c, p in zip(CM, pts):
                    cmds.append(f"echo REACH {name} {lenient} {d} {c}")
                    cmds.append("vr_climb_try %.3f %.3f %.3f" % p)
    cmds += ["toggleconsole", "quit"]
    print(";".join(cmds))

def table(log):
    key = None
    res = {}
    for line in open(log, encoding="utf-8", errors="replace"):
        m = re.match(r"REACH (\S+) (\S+) (\S+) (\S+)", line.strip())
        if m:
            key = m.groups()
            continue
        if line.startswith("climbtry") and key:
            r = line.split(": ", 1)[1]
            res[key] = "-" if r.startswith("none") else ("E" if r.startswith("exact") else "L")
            key = None
    print("site   dir     len | " + " ".join(f"{c:>3}cm" for c in CM))
    for name, _, dirs in SITES:
        for d in dirs:
            for l in LENIENCIES:
                row = [res.get((name, str(l), d, str(c)), "?") for c in CM]
                print(f"{name:6} {d:7} {l:3} | " + " ".join(f"{x:>5}" for x in row))

if __name__ == "__main__":
    script() if sys.argv[1] == "script" else table(sys.argv[2])
