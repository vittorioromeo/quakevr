"""haze_preset_check.py <log> -- the verdict of haze_preset_test.sh."""
import re
import sys

lines = open(sys.argv[1], encoding="utf-8", errors="replace").read().splitlines()
names = ["Off (Quake)", "Low", "Medium", "High", "Ultra"]
want_half = {0: "1", 1: "1", 2: "1", 3: "0", 4: "0"}
want_migrate = {"medium": "1", "high": "0", "custom": "0"}
step, got_half, got_state, got_mig = None, {}, {}, {}
for l in lines:
    m = re.search(r"hazepreset (preset|migrate) (\w+)", l)
    if m:
        step = (m.group(1), m.group(2))
        continue
    m = re.search(r'"vr_heat_haze_half" is "([^"]*)"', l)
    if m and step:
        (got_half if step[0] == "preset" else got_mig)[step[1]] = m.group(1)
        continue
    m = re.search(r"^graphics preset: (.*?) \(rows", l)
    if m and step and step[0] == "preset":
        got_state[step[1]] = m.group(1)
passed = failed = 0


def check(what, ok, got):
    global passed, failed
    print("hazepreset: %s: %s (%s)" % (what, "PASS" if ok else "FAIL", got))
    if ok:
        passed += 1
    else:
        failed += 1


for p in range(5):
    h, s = got_half.get(str(p)), got_state.get(str(p), "")
    check("preset %s sets half %s and is recognised" % (names[p], want_half[p]), h == want_half[p] and s.startswith(names[p]),
          "half %s, state %s" % (h, s))
for k, v in want_migrate.items():
    check("migration from 204 at the old 1, %s" % k, got_mig.get(k) == v, "half %s, want %s" % (got_mig.get(k), v))
print("hazepreset: %d passed, %d failed" % (passed, failed))
