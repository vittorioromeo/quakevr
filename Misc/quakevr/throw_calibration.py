# throw_calibration.py <take.csv|folder> [...]: what the hand calibration did to throws, on real hand motions (ROUND21.md,
# "Climbing: sliding along the wall; throw angle after calibration"). For each take (docs/vr-port/MOTIONS.md; its raw_m_*
# columns: the main hand's controller, its palm's velocity), at the palm's fastest moment, the throw estimate of
# vr_throw.cpp (releasePeak: the mean velocity within 17 ms of the peak, its direction from the 40 ms before it, and
# above 6 rad/s the wrist's spin through a 0.1 m lever along the hand, times 0.7) as the code before the fix made it:
#   old: Gun Angle 39.5, Gun Yaw 4, no move (the calibration before 2026-09-28)
#   new: Gun Angle 70, Gun Yaw 0, move X -4 Y 0.58 Z -2.5 cm (the author's since); its palm's velocity then had the
#        wrist's turn through the move added (w x grip*move), and the lever turned 30.5 degrees down with the hand
# and the same with the fix (the controller's own palm velocity, the lever on a fixed frame: the old one), which is
# the old for both. Prints each take's elevation (degrees up) and speed, old and new, and a summary per category.
import csv, math, os, sys
from statistics import median

LEVER, FACTOR, THRESHOLD, SPAN, LOOKBACK = 0.1, 0.7, 6.0, 0.017, 0.04
# The runtime's angular velocity (raw_m_w*) as the game reads it (vr_angvel.cpp, vr_angvel_frame -1): VirtualDesktopXR
# gives it in the controller's frame: the grip pose (GRIP_TURN below) turned LOCAL_PITCH degrees about x.
LOCAL_PITCH = -57.5


def qmul(a, b):
    aw, ax, ay, az = a; bw, bx, by, bz = b
    return (aw * bw - ax * bx - ay * by - az * bz, aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx, aw * bz + ax * by - ay * bx + az * bw)


def qrot(q, v):
    w, x, y, z = q
    p = qmul(qmul(q, (0.0, *v)), (w, -x, -y, -z))
    return p[1:]


def axis(ax, deg):
    h = math.radians(deg) / 2
    return (math.cos(h), *(math.sin(h) * c for c in ax))


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def add(a, b, s=1.0):
    return tuple(x + s * y for x, y in zip(a, b))


def length(a):
    return math.sqrt(sum(x * x for x in a))


def elevation(v):  # tracking space: y up
    return math.degrees(math.atan2(v[1], math.hypot(v[0], v[2])))


GRIP_TURN = axis((1, 0, 0), 20.6)            # legacyGripInRaw(Touch): the grip in the raw pose's frame
CAL = {"old": (39.5, 4.0, (0.0, 0.0, 0.0)), "new": (70.0, 0.0, (-4.0, 0.58, -2.5)),
       "gun": (70.0, 0.0, (0.0, 0.0, 0.0)), "move": (39.5, 4.0, (-4.0, 0.58, -2.5))}  # each part alone


def tracking_from_quake(v):
    return (-v[1], v[2], -v[0])


def estimate(rows, peak, cal, fixed):
    pitch, yaw, move = CAL["old" if fixed else cal]
    m = tracking_from_quake(tuple(c * 0.01 for c in move))

    def vel(r):
        v = r["g"]
        if m != (0.0, 0.0, 0.0):
            v = add(v, cross(r["w"], qrot(qmul(r["q"], GRIP_TURN), m)))
        return v

    t0 = rows[peak]["t"]
    near = [r for r in rows if abs(r["t"] - t0) <= SPAN]
    v = tuple(sum(vel(r)[k] for r in near) / len(near) for k in range(3))
    near2 = [r for r in rows if abs(r["t"] - t0) <= 2 * SPAN]
    w = tuple(sum(r["w"][k] for r in near2) / len(near2) for k in range(3))
    back = [r for r in rows if t0 - LOOKBACK <= r["t"] <= t0]
    d = tuple(sum(vel(r)[k] for r in back) for k in range(3))
    if length(d) > 1e-4:
        v = tuple(c / length(d) * length(v) for c in d)
    hand = qmul(qmul(rows[peak]["q"], axis((0, 1, 0), yaw)), axis((1, 0, 0), -pitch))
    f = qrot(hand, (0.0, 0.0, -1.0))
    if length(w) > THRESHOLD:
        v = add(v, cross(w, tuple(c * LEVER for c in f)), FACTOR)
    return v


def take(path):
    with open(path, encoding="utf-8") as fh:
        text = fh.readlines()
    lines = [l for l in text if not l.startswith("#")]
    local = any(l.startswith("# source:") and "VirtualDesktopXR" in l for l in text)
    turn = qmul(GRIP_TURN, axis((1, 0, 0), LOCAL_PITCH))
    rows = []
    for r in csv.DictReader(lines):
        try:
            if r.get("raw_m_gvalid", "0") not in ("1", "1.0"):
                continue
            q = tuple(float(r[f"raw_m_q{c}"]) for c in "wxyz")
            w = tuple(float(r[f"raw_m_w{c}"]) for c in "xyz")
            rows.append({"t": float(r["t"]), "q": q, "w": qrot(qmul(q, turn), w) if local else w,
                         "g": tuple(float(r[f"raw_m_g{c}"]) for c in "xyz")})
        except (KeyError, ValueError):
            continue
    if len(rows) < 10:
        return None
    peak = max(range(len(rows)), key=lambda i: length(rows[i]["g"]))
    if length(rows[peak]["g"]) < 2.0:
        return None  # not a throw's speed
    return {k: estimate(rows, peak, k, False) for k in CAL} | {"fixed": estimate(rows, peak, "new", True)}


def main():
    paths = []
    for a in sys.argv[1:]:
        paths += sorted(os.path.join(a, f) for f in os.listdir(a) if f.endswith(".csv")) if os.path.isdir(a) else [a]
    cats = {}
    for p in paths:
        r = take(p)
        if not r:
            continue
        name = os.path.basename(p)[:-4]
        cat = name.rsplit("_2026", 1)[0]
        de = elevation(r["new"]) - elevation(r["old"])
        sr = length(r["new"]) / length(r["old"])
        df = elevation(r["fixed"]) - elevation(r["old"])
        cats.setdefault(cat, []).append((de, sr, df, elevation(r["gun"]) - elevation(r["old"]),
                                         elevation(r["move"]) - elevation(r["old"])))
        print(f"{name}: old {elevation(r['old']):+6.1f} deg {length(r['old']):5.2f} m/s, new {elevation(r['new']):+6.1f} deg "
              f"{length(r['new']):5.2f} m/s ({de:+5.1f} deg, x{sr:.2f}); fixed {df:+.1f} deg")
    print("\ncategory: takes, elevation change new-old (median, 10%..90%), speed ratio (median); of it, Gun Angle alone and the"
          " move alone (medians); the fix's largest change from old")
    alls = []
    for cat, v in sorted(cats.items()):
        des = sorted(x[0] for x in v); alls += v
        print(f"{cat:28s} {len(v):3d}  {median(des):+5.1f} ({des[len(des) // 10]:+5.1f}..{des[(9 * len(des)) // 10]:+5.1f})"
              f"  x{median(x[1] for x in v):.2f}  gun {median(x[3] for x in v):+5.1f} move {median(x[4] for x in v):+5.1f}"
              f"   {max(abs(x[2]) for x in v):.1f}")
    des = sorted(x[0] for x in alls)
    print(f"{'all':28s} {len(alls):3d}  {median(des):+5.1f} ({des[len(des) // 10]:+5.1f}..{des[(9 * len(des)) // 10]:+5.1f})"
          f"  x{median(x[1] for x in alls):.2f}; lower: {sum(d < 0 for d in des)} of {len(des)}")


if __name__ == "__main__":
    main()
