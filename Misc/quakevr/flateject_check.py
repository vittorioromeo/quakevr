"""flateject_check.py <dir> -- checks flateject_test.sh's logs; prints each check and "flateject: N passed, M failed"."""
import os
import re
import sys

D = sys.argv[1]
results = []


def check(name, ok, detail=""):
    results.append(ok)
    print("%s %s%s" % ("PASS" if ok else "FAIL", name, (": " + detail) if detail else ""))


def log(tag):
    p = os.path.join(D, tag + ".log")
    return open(p, encoding="utf-8", errors="replace").read() if os.path.exists(p) else ""


VEL = re.compile(r"shells velocity hand (\d): forward (-?[\d.]+) right (-?[\d.]+) up (-?[\d.]+) m/s")
BEFORE = re.compile(r"shells velocity hand \d: .*before the face's rule (-?[\d.]+) (-?[\d.]+) (-?[\d.]+)\)")


def befores(text):
    return [tuple(float(x) for x in m.groups()) for m in BEFORE.finditer(text)]


def vels(text):
    return [(float(m.group(2)), float(m.group(3)), float(m.group(4))) for m in VEL.finditer(text)]


def ended(tag, text):
    check("%s: ended on its own line" % tag, "flateject_test: %s done" % tag in text)


# ssg: two shells, thrown sideways (outward: right, the main hand) and up, out once drawn open + the delay.
t = log("ssg")
ended("ssg", t)
v = vels(t)
check("ssg: two shells", len(v) == 2, str(v))
check("ssg: thrown off sideways (>= 1.2 m/s right) and up (> 0.3 m/s)", len(v) == 2 and all(r >= 1.2 and u > 0.3 for f, r, u in v),
      str(v))
m = re.search(r"drawn open ([\d.]+) s after the break \(barrels ([\d.]+) of ([\d.]+) deg, pose ([\d.]+)\)", t)
check("ssg: drawn fully open (barrels down, pose eased in, >= 0.18 s) before its shells leave",
      bool(m) and float(m.group(1)) >= 0.18 and float(m.group(2)) >= float(m.group(3)) - 0.01 and float(m.group(4)) >= 0.999,
      m.group(0) if m else "no line")
m = re.search(r"x2 t [\d.]+ \(([\d.]+) s after the break, ([\d.]+) s after it was drawn open\)", t)
check("ssg: its shells out vr_flat_ssg_eject_delay (0.05 s) after it was drawn open", bool(m) and 0.04 <= float(m.group(2)) <= 0.08,
      m.group(0) if m else "no line")

# ssgold: the control: the old ejection dropped them (hardly any speed sideways, none up).
v = vels(log("ssgold"))
ended("ssgold", log("ssgold"))
check("ssgold: the old ejection drops them (< 0.8 m/s sideways)", len(v) == 2 and all(abs(r) < 0.8 for f, r, u in v), str(v))

# rv: six shells thrown off sideways; the revolver's open pose (the defaults) in the report.
t = log("rv")
ended("rv", t)
v = vels(t)
check("rv: six shells", len(v) == 6, str(len(v)))
check("rv: thrown off sideways (>= 0.9 m/s right)", len(v) == 6 and all(r >= 0.9 for f, r, u in v), str(v))
m = re.search(r"flatreload: main hand: open 1 kind 2 share 1\.00, turned (\S+) (\S+) (\S+), moved (\S+) (\S+) (\S+)", t)
check("rv: the main hand in the revolver's open pose (-15 15 -30, moved 0 3 -1)",
      bool(m) and [float(x) for x in m.groups()] == [-15.0, 15.0, -30.0, 0.0, 3.0, -1.0], m.group(0) if m else "no line")
t = log("rvold")
ended("rvold", t)
v = vels(t)
check("rvold: the old ejection drops them (< 0.8 m/s sideways)", len(v) == 6 and all(abs(r) < 0.8 for f, r, u in v), str(v))

# vr: the mock headset: the same shared ejection keeps their level speed (turned past the face, not lost: >= 80%).
t = log("vr")
ended("vr", t)
v = vels(t)
b = befores(t)
lv = lambda f, r: (f * f + r * r) ** 0.5
check("vr: two shells keep their level speed past the face (>= 80%)",
      len(v) == 2 and len(b) == 2 and all(lv(x[0], x[1]) >= 0.8 * lv(y[0], y[1]) for x, y in zip(v, b)), str(v) + " from " + str(b))

print("flateject: %d passed, %d failed" % (sum(results), len(results) - sum(results)))
sys.exit(0 if all(results) else 1)
