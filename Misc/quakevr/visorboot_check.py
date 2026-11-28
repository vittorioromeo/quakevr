# visorboot_check.py <log>: visorboot_test.sh's verdicts, from the "visorboot: boot" trace lines (vr_visor.cpp traceBoot)
# and the vr_flat_visor_report lines after each "visorboot_test: start <phase>".
import re
import sys

log = open(sys.argv[1], encoding="utf-8", errors="replace").read().replace("\r", "")
# The console wraps long lines in the run's output: joined back.
log = re.sub(r" +", " ", re.sub(r"\n(?!visor|composed|exit|$)", " ", log))
passed = failed = 0


def check(ok, what):
    global passed, failed
    print(("PASS " if ok else "FAIL ") + what)
    passed, failed = passed + (1 if ok else 0), failed + (0 if ok else 1)


# The report through the first start: each label's boot phase and panels drawn.
phases, label = {}, None
for line in log.split("\n"):
    line = line.strip()
    m = re.match(r"visorboot_test: (.+)$", line)
    if m:
        label = m.group(1).strip()
        phases.setdefault(label, {"drawn": 0})
        continue
    if label is None:
        continue
    m = re.match(r"visor: boot (\w+) seq (\d) in ([-\d.]+) glitch ([-\d.]+)", line)
    if m and "phase" not in phases[label]:
        phases[label].update(phase=m.group(1), seq=int(m.group(2)), fadein=float(m.group(3)), glitch=float(m.group(4)))
    m = re.match(r"visor: panel (\w+) drawn (\d)", line)
    if m and m.group(1) != "center":
        phases[label]["drawn"] += int(m.group(2))
for name, want, drawn in (("dark", "dark", lambda n: n == 0), ("fading", "fading", lambda n: n > 0),
                          ("sweeping", "sweeping", lambda n: n > 0), ("booted", "booted", lambda n: n > 0)):
    p = phases.get("start " + name, {})
    check(p.get("phase") == want and drawn(p.get("drawn", -1)),
          f"start {name}: the boot {p.get('phase')} (want {want}), panels drawn {p.get('drawn')}")
p = phases.get("start fading", {})
check(0.0 < p.get("fadein", 0) < 1.0 and p.get("glitch", 0) > 0.0,
      f"start fading: the fade in {p.get('fadein')} under way, glitching {p.get('glitch')}")

# The traces: one boot each, in order (start, change, death, coop, off).
pat = (r"visorboot: boot (\d+) \((\w+)\) seq (\d): dark (\d+) frames \(shown (\d+)\), fading (\d+) \(rises (\d+) falls (\d+), "
       r"glitching (\d+), peak ([-\d.]+)\), sweeping (\d+) \(fade ([-\d.]+) at its start\), booted ([-\d.]+) s \(fade ([-\d.]+)\), "
       r"sounds (\d+)")
traces = []
for line in log.split("\n"):
    m = re.match(pat, line.strip())
    if m:
        g = m.groups()
        traces.append(dict(n=int(g[0]), cause=g[1], seq=int(g[2]), dark=int(g[3]), shown=int(g[4]), fading=int(g[5]),
                           rises=int(g[6]), falls=int(g[7]), glitching=int(g[8]), peak=float(g[9]), sweeping=int(g[10]),
                           fadeAtSweep=float(g[11]), booted=float(g[12]), fadeEnd=float(g[13]), sounds=int(g[14])))
# (A boot cut short by another, as the level restarts after a death: no verdict; the next one is.)
names = ["start", "change", "death", "coopmap", "coop", "off"]
causes = {"start": ("signon", "map"), "change": ("signon", "map"), "death": ("signon", "map", "respawn"),
          "coopmap": ("signon", "map"), "coop": ("respawn",), "off": ("fx",)}
check(len(traces) == len(names), f"traces: {len(traces)} boots traced (want {len(names)}: {', '.join(names)})")
for name, t in zip(names, traces):
    if name == "off":
        ok = (t["seq"] == 0 and t["dark"] == 0 and t["fading"] == 0 and t["sounds"] == 0 and 50 <= t["sweeping"] <= 64
              and t["fadeEnd"] > 0.99)
        check(ok, f"off: the sweep alone: {t}")
        continue
    check(t["cause"] in causes[name], f"{name}: its cause {t['cause']} (want one of {causes[name]})")
    check(t["seq"] == 1 and 19 <= t["dark"] <= 23 and t["shown"] == 0,
          f"{name}: dark {t['dark']} frames (0.3 s: about 22), nothing shown ({t['shown']})")
    check(33 <= t["fading"] <= 38 and t["rises"] == t["fading"] and t["falls"] == 0,
          f"{name}: fading in {t['fading']} frames (0.5 s: about 36), rising every frame ({t['rises']} rises, {t['falls']} falls)")
    check(t["glitching"] >= t["fading"] - 1 and 0.2 <= t["peak"] <= 0.61,
          f"{name}: glitching {t['glitching']} frames, peak {t['peak']} (0.6 at the fade's start)")
    check(50 <= t["sweeping"] <= 64 and t["fadeAtSweep"] > 0.99,
          f"{name}: sweeping {t['sweeping']} frames (0.8 s: about 58) with the panels faded in ({t['fadeAtSweep']})")
    check(1.5 <= t["booted"] <= 1.7 and t["fadeEnd"] > 0.99, f"{name}: booted at {t['booted']} s, shown {t['fadeEnd']}")
    check(t["sounds"] == 1, f"{name}: the power-up sound started once ({t['sounds']})")
print(f"visorboot_test: {passed} passed, {failed} failed")
sys.exit(1 if failed else 0)
