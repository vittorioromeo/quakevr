"""rollaim_check.py <log> -- rollaim_test.sh's verdict: per case and hand, the shot, barrel and sight line directions in
the drawing hand's frame (vr_aim_check's "in the hand's frame" lines) within 0.5 degrees of the level pose's (p0 r0).
Prints "rollaim: N passed, M failed"."""
import math
import re
import sys

TOL = 0.5

def main():
    lines = open(sys.argv[1], encoding="utf-8", errors="replace").read().splitlines()
    step = None
    data = {}  # (case, hand) -> {pose: {"shot": (up, left), ...}}
    slots = {}
    pat = re.compile(r"aim_check: (main|off) hand in the hand's frame \(up, left\): shot (\S+) (\S+) barrel (\S+) (\S+) "
                     r"sight (\S+) (\S+)( \(none\))?;")
    slotpat = re.compile(r"aim_check: (main|off) hand slot (\d+):")
    for line in lines:
        m = re.match(r"rollaim case (\S+) (\S+) (p\S+ r\S+)", line)
        if m:
            step = m.groups()
            continue
        if step is None:
            continue
        sm = slotpat.match(line)
        if sm and sm.group(1) == step[1]:
            slots[(step[0], step[1])] = sm.group(2)
        m = pat.match(line)
        if m and m.group(1) == step[1]:
            v = [float(x) for x in m.groups()[1:7]]
            d = {"shot": (v[0], v[1]), "barrel": (v[2], v[3])}
            if not m.group(8):
                d["sight"] = (v[4], v[5])
            data.setdefault((step[0], step[1]), {})[step[2]] = d
    passed = failed = 0
    def fail(msg):
        nonlocal failed
        failed += 1
        print("rollaim: FAIL " + msg)
    for case in ("enforcer main", "ssg main", "ssg off", "revolver main"):
        key = tuple(case.split())
        poses = data.get(key)
        if not poses or "p0 r0" not in poses or len(poses) < 15:
            fail("%s: %d poses measured (unarmed?)" % (case, len(poses or {})))
            continue
        ref = poses["p0 r0"]
        worst = {}
        for pose, d in poses.items():
            for k, (up, left) in d.items():
                if k not in ref:
                    continue
                e = math.hypot(up - ref[k][0], (left - ref[k][1]) * math.cos(math.radians(up)))
                if e > worst.get(k, (0.0, ""))[0]:
                    worst[k] = (e, pose)
        for k in ("shot", "barrel", "sight"):
            if k not in ref:
                print("rollaim: %s slot %s has no sight line (not checked)" % (case, slots.get(key, "?")))
                continue
            e, pose = worst.get(k, (0.0, "-"))
            msg = "%s slot %s %s: level (up %.2f, left %.2f), worst %.2f deg off (%s)" % (
                case, slots.get(key, "?"), k, ref[k][0], ref[k][1], e, pose)
            if e <= TOL:
                passed += 1
                print("rollaim: ok " + msg)
            else:
                fail(msg)
    print("rollaim: %d passed, %d failed" % (passed, failed))
    return 1 if failed else 0

if __name__ == "__main__":
    sys.exit(main())
