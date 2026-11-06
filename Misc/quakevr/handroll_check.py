"""handroll_check.py <log> -- handroll_test.sh's verdict: per case, hand and thing (aim frame, drawn weapon, drawn hand),
its turn and place in the controller's grip frame (vr_hand_check) within 0.5 degrees and 0.3 units of the level pose's
(p0 r0) at every pose. Prints "handroll: N passed, M failed"."""
import math
import re
import sys

TOL_DEG = 0.5
TOL_POS = 0.3
CASES = ("ssg_sword", "enf_ssg", "axe_empty")
EMPTY = {("axe_empty", "off")}  # (no weapon drawn: its weapon line is not printed)


def basis(pitch_up, yaw, roll):
    """Quake angles (pitch up here, as vr_hand_check prints) to forward, left, up (AngleVectors' convention)."""
    p, y, r = math.radians(-pitch_up), math.radians(yaw), math.radians(roll)
    sp, cp, sy, cy, sr, cr = math.sin(p), math.cos(p), math.sin(y), math.cos(y), math.sin(r), math.cos(r)
    f = (cp * cy, cp * sy, -sp)
    right = (-sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp)
    u = (cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp)
    return (f, tuple(-c for c in right), u)


def angle_between(a, b):
    """The rotation angle (degrees) between two bases."""
    tr = sum(sum(a[i][k] * b[i][k] for k in range(3)) for i in range(3))
    return math.degrees(math.acos(max(-1.0, min(1.0, (tr - 1.0) / 2.0))))


def main():
    step = None
    data = {}
    slots = {}
    pat = re.compile(r"hand_check: (main|off) (aim|weapon|hand) (\S+) (\S+) (\S+) at (\S+) (\S+) (\S+)(?: slot (-?\d+))?")
    for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
        m = re.match(r"handroll case (\S+) (p\S+ r\S+)", line)
        if m:
            step = m.groups()
            continue
        m = pat.match(line)
        if m and step:
            v = [float(x) for x in m.groups()[2:8]]
            data.setdefault((step[0], m.group(1), m.group(2)), {})[step[1]] = v
            if m.group(9) is not None:
                slots[(step[0], m.group(1))] = m.group(9)
    passed = failed = 0
    for case in CASES:
        for hand in ("main", "off"):
            for thing in ("aim", "weapon", "hand"):
                poses = data.get((case, hand, thing))
                what = "%s %s (slot %s) %s" % (case, hand, slots.get((case, hand), "?"), thing)
                if not poses:
                    if thing == "weapon" and (case, hand) in EMPTY:
                        continue  # (an empty hand: no weapon)
                    print("handroll: FAIL %s: not measured" % what)
                    failed += 1
                    continue
                ref = poses.get("p0 r0")
                if ref is None or len(poses) < 15:
                    print("handroll: FAIL %s: %d poses measured" % (what, len(poses)))
                    failed += 1
                    continue
                rb = basis(*ref[:3])
                worst_t = worst_p = 0.0
                at_t = at_p = "-"
                for pose, v in poses.items():
                    t = angle_between(rb, basis(*v[:3]))
                    d = math.dist(ref[3:], v[3:])
                    if t > worst_t:
                        worst_t, at_t = t, pose
                    if d > worst_p:
                        worst_p, at_p = d, pose
                ok = worst_t <= TOL_DEG and worst_p <= TOL_POS
                print("handroll: %s %s: worst turn %.2f deg (%s), place %.2f units (%s)" % (
                    "ok" if ok else "FAIL", what, worst_t, at_t, worst_p, at_p))
                if ok:
                    passed += 1
                else:
                    failed += 1
    print("handroll: %d passed, %d failed" % (passed, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
