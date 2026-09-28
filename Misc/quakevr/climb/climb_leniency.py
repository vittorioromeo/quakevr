# climb_leniency.py: the grab leniency's tests on vrclimb (ROUND21.md, "Climbing: hand placement and grab leniency").
#   python climb_leniency.py script         prints the console script (commands joined by ';'): for each site, the
#                                           player put there, then "vr_climb_try" at each hand point, at each leniency
#   python climb_leniency.py table <log>    reads the "climbtry" lines of the run's log: a table per site, a column per
#                                           leniency: E<top>@<cm> taken where the hand is (exact), L<top>@<cm> taken
#                                           leniently (the hold's top, and how far it is from the hand), - none, and
#                                           -(...) none, with the holds seen but turned down: l too low (Lowest Ledge),
#                                           b lower under the hand than a hold may be, f too far, h behind the head,
#                                           w through a wall
import re
import sys

LENIENCIES = [0, 5, 10, 15, 20, 30]  # cm

# (name, setpos, [hand points (x y z [vx vy vz: the hand's velocity, m/s])])
SITES = [
    ("A1 ledge (top 48, face x 96): in front of the lip, 2 units apart", "78 176 24 0 0 0",
     [(96 - d, 173, 46) for d in range(0, 17, 2)]),
    ("A2 ledge: under the lip, at the face", "78 176 24 0 0 0", [(94, 173, 48 - h) for h in (4, 8, 12, 14, 16, 18, 20, 24)]),
    ("A3 ledge: over the lip, above the top", "78 176 24 0 0 0", [(98, 173, 48 + h) for h in (6, 8, 10, 12, 14, 16, 18)]),
    ("B1 rung (top 56, face x 88): in front of it, 2 units apart", "71 0 24 0 0 0", [(88 - d, -3, 54) for d in range(0, 17, 2)]),
    ("B2 rung: past its end (y 96)", "71 80 24 0 0 0", [(92, 96 + d, 57) for d in range(0, 11, 2)]),
    ("B3 rung: between rungs 56 and 76, 2 units in front", "71 0 24 0 0 0", [(86, -3, z) for z in range(58, 75, 2)]),
    ("B4 rung: in front of it, the body to the side (y 60)", "71 60 24 0 0 0", [(88 - d, -3, 54) for d in range(0, 9, 2)]),
    ("V 8 units in front of the rungs, between 56 and 76: still, moving up, moving down", "71 0 24 0 0 0",
     [(80, -3, 66, 0, 0, 0), (80, -3, 66, 0, 0, 1), (80, -3, 66, 0, 0, -1)]),
    ("C tower wall, no ledge (x 96, y -200)", "71 -200 24 0 0 0", [(x, -200, z) for x in (94, 90) for z in (30, 40, 50, 60)]),
    ("D stairs out of the trench (16-unit steps): at steps 1-3's nosings", "-60 700 -184 0 180 0",
     [p for i in (1, 2, 3) for p in ((-112 - 24 * i + 2, 700, -176 + 16 * i + 2), (-112 - 24 * i + 2, 700, -176 + 16 * i - 2),
                                     (-112 - 24 * i - 4, 700, -176 + 16 * i + 3), (-112 - 24 * i + 6, 700, -176 + 16 * i - 6))]),
    ("E thin wall (x -304..-300) with a ledge (top 48) 6 units behind it", "-270 -260 24 0 180 0",
     [(x, -260, z) for x in (-298, -299.5) for z in (40, 46, 50, 56)]),
]


def script():
    cmds = ["vr_fixed_frames 1", "vr_climb 1", "map vrclimb", "wait20"]
    for i, (name, pos, pts) in enumerate(SITES):
        cmds += [f"setpos {pos}", "noclip", "wait20", f"echo SITE {i}"]
        for lenient in LENIENCIES:
            cmds.append(f"vr_climb_leniency {lenient}")
            for p in pts:
                cmds.append("vr_climb_try %g %g %g" % p[:3] + (" main %g %g %g" % p[3:] if len(p) > 3 else ""))
    cmds += ["toggleconsole", "quit"]
    print(";".join(cmds))


def table(log):
    site = None
    results = {}
    ms = []
    for line in open(log, encoding="utf-8", errors="replace"):
        line = line.strip()
        if line.startswith("SITE "):
            site = int(line.split()[1])
            continue
        m = re.match(r"climbtry (\S+) (\S+) (\S+) main: (.*)", line)
        if not m or site is None:
            continue
        r = m.group(4)
        t = re.search(r"([\d.]+) ms", r)
        if t:
            ms.append((float(t.group(1)), int(re.search(r"(\d+) points", r).group(1))))
        if r.startswith("none"):
            c = "-"
            rej = re.search(r"turned down: (\d+) low, (\d+) below, (\d+) far, (\d+) behind, (\d+) through", r)
            if rej and any(int(x) for x in rej.groups()):
                c = "-(" + ",".join(k + x for k, x in zip("lbfhw", rej.groups()) if int(x)) + ")"
        else:
            top = float(re.search(r"top (\S+)", r).group(1))
            cm = float(re.search(r"\(([\d.]+) cm\)", r).group(1))
            c = ("E" if r.startswith("exact") else "L") + f"{top:g}@{cm:.0f}"
            out = re.search(r"out (\S+) (\S+),", r)
            if abs(float(out.group(1))) < 0.9 and abs(float(out.group(2))) < 0.9:
                c += "!"  # the hold's way out not square to the map's axes (vrclimb's edges all are)
        results.setdefault(site, []).append(c)
    for i, (name, pos, pts) in enumerate(SITES):
        cells = results.get(i, [])
        print(f"\n{name} (setpos {pos}); leniency, cm: " + "".join(f"{x:>11}" for x in LENIENCIES))
        for j, p in enumerate(pts):
            row = [cells[k * len(pts) + j] if k * len(pts) + j < len(cells) else "?" for k in range(len(LENIENCIES))]
            label = "hand %g %g %g" % p[:3] + (" v %g %g %g" % p[3:] if len(p) > 3 else "")
            print(f"  {label:<32}" + "".join(f"{c:>11}" for c in row))
    for n in sorted({p for _, p in ms}):
        times = sorted(t for t, q in ms if q == n)
        if n:
            print(f"lenient search, {n} points: median {times[len(times) // 2]:.3f} ms, max {times[-1]:.3f} ms")


if __name__ == "__main__":
    script() if sys.argv[1] == "script" else table(sys.argv[2])
