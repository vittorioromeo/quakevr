# visor_check.py <log>: visor_test.sh's verdicts, from the vr_flat_visor_report lines after each "visor_test: <phase>".
import re
import sys

log = open(sys.argv[1], encoding="utf-8", errors="replace").read().replace("\r", "")
# The console wraps long lines in the run's output: joined back (a report line ends at "log lines N").
log = re.sub(r" +", " ", re.sub(r"\n(?!visor|composed|exit|$)", " ", log))

phases = {}
phase = None
for line in log.split("\n"):
    m = re.match(r"visor_test: (.+)$", line.strip())
    if m:
        phase = m.group(1).strip()
        phases.setdefault(phase, {"panels": {}})
        continue
    if phase is None:
        continue
    m = re.match(r"visor: active (\d) frame \d+; sway ([-\d.]+) ([-\d.]+) \(lag ([-\d.]+) ([-\d.]+), bob ([-\d.]+)\); "
                 r"glitch ([-\d.]+) fog ([-\d.]+) underwater (\d) fade ([-\d.]+) alarm ([-\d.]+) boot ([-\d.]+) ([-\d.]+) ([-\d.]+)",
                 line.strip())
    if m and "active" not in phases[phase]:
        v = [float(x) for x in m.groups()]
        phases[phase].update(active=int(v[0]), sway=(v[1], v[2]), glitch=v[6], fog=v[7], underwater=int(v[8]),
                             boot=(v[11], v[12], v[13]))
        continue
    m = re.match(r"visor: panel (\w+) drawn (\d) rect ([-\d.]+) ([-\d.]+) ([-\d.]+) ([-\d.]+) \(view (\d+) x (\d+)\)", line.strip())
    if m and m.group(1) not in phases[phase]["panels"]:
        phases[phase]["panels"][m.group(1)] = (int(m.group(2)), *(float(x) for x in m.groups()[2:6]),
                                               int(m.group(7)), int(m.group(8)))

passed = failed = 0


def check(ok, what):
    global passed, failed
    print(("PASS " if ok else "FAIL ") + what)
    if ok:
        passed += 1
    else:
        failed += 1


def get(p, key, default=None):
    return phases.get(p, {}).get(key, default)


def sway(p):
    s = get(p, "sway", (99.0, 99.0))
    return max(abs(s[0]), abs(s[1]))


rest = phases.get("rest", {"panels": {}})
inside = len(rest["panels"]) == 3 and all(
    d == 1 and 0 <= x0 < x1 <= w and 0 <= y0 < y1 <= h for (d, x0, y0, x1, y1, w, h) in rest["panels"].values())
check(get("rest", "active") == 1 and inside, f"rest: the visor on by default, its three panels inside the view ({rest['panels']})")
if inside:
    v, a, lo = rest["panels"]["vitals"], rest["panels"]["ammo"], rest["panels"]["log"]
    check(v[1] < v[5] * 0.2 and v[2] < v[6] * 0.2 and a[3] > a[5] * 0.8 and a[2] < a[6] * 0.2 and lo[4] > lo[6] * 0.85
          and abs((lo[1] + lo[3]) * 0.5 - lo[5] * 0.5) < 2,
          "rest: health top left, ammo top right, the low panel at the bottom's middle")
check(sway("rest") == 0, f"rest: standing still, no sway ({get('rest', 'sway')})")
check(0.05 < sway("turn") <= 0.6001, f"turn: the panels lag the turn a fraction of a degree ({get('turn', 'sway')})")
check(sway("settle") == 0, f"settle: let go, back to rest within half a second ({get('settle', 'sway')})")
walk = max(sway("walk"), sway("walk") if "walk" in phases else 0)
check(0.005 < walk <= 0.35, f"walk: the walk's bob moves them a little ({get('walk', 'sway')})")
check(sway("still") == 0, f"still: standing again, still ({get('still', 'sway')})")
check(get("hit", "glitch", 0) > 0.3 and get("hit later", "glitch", 1) == 0,
      f"hit: the glitch at once ({get('hit', 'glitch')}), gone half a second later ({get('hit later', 'glitch')})")
check(get("water", "underwater") == 1 and get("water", "fog", 0) > 0.9,
      f"water: under water the visor fogs up ({get('water', 'fog')})")
check(0 < get("surfaced", "fog", 0) < get("water", "fog", 0) and get("dry", "fog", 1) == 0,
      f"water: surfaced it clears ({get('surfaced', 'fog')}, then {get('dry', 'fog')})")
b0, b1 = get("boot", "boot", (1, 1, 1)), get("booted", "boot", (0, 0, 0))
check(min(b0) < 1 and min(b1) == 1, f"boot: the power-on sweep under way {b0}, then done {b1}")
check(get("off", "active") == 0, "status: vr_flat_visor 0 turns it off (the status bar back)")
print(f"visor_test: {passed} passed, {failed} failed")
sys.exit(1 if failed else 0)
