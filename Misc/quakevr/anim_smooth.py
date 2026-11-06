#!/usr/bin/env python3
"""anim_smooth.py -- smooths an animation recording (quakevr/anims/<name>.anim; docs/vr-port/ANIMATIONS.md) offline.

The same smoothing as the game's vr_ar_smooth (Quake/vr/vr_animrec_data.cpp: keep the two alike):
  1. a jitter filter: exponential smoothing forwards then backwards (no lag), time constant --tau seconds;
  2. keyframe reduction: the ends, each event's frame and each weapon change are kept; then the frame where a cubic
     Hermite spline (Catmull-Rom tangents over the keys' times) through the keys strays most from the filtered motion is
     added, until it strays no more than --pos metres, --ang degrees and --curl (fingers, grip, trigger) anywhere.
Writes <name>.smooth.anim beside the input (or --out) and prints the frames kept and the largest errors.

  python Misc/quakevr/anim_smooth.py quakevr/anims/melee_1h.anim [--tau 0.02 --pos 0.004 --ang 1.5 --curl 0.04]
  python Misc/quakevr/anim_smooth.py --check quakevr/anims/x.anim   (exit 1 if the result breaks a tolerance)
"""
import argparse
import math
import sys

HAND_ORDER = ("main", "off")  # the file's column order


def basis_of(pitch, yaw, roll):
    """Quake angles' axes (forward, left, up) as columns (hands::basisOf: AngleVectors' forward, -right, up)."""
    p, y, r = math.radians(pitch), math.radians(yaw), math.radians(roll)
    sp, cp, sy, cy, sr, cr = math.sin(p), math.cos(p), math.sin(y), math.cos(y), math.sin(r), math.cos(r)
    fwd = (cp * cy, cp * sy, -sp)
    right = (-sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp)
    up = (cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp)
    left = tuple(-c for c in right)
    return fwd, left, up


def quat_of(angles):
    f, l, u = basis_of(*angles)
    m = [[f[0], l[0], u[0]], [f[1], l[1], u[1]], [f[2], l[2], u[2]]]  # m[row][col]
    tr = m[0][0] + m[1][1] + m[2][2]
    if tr > 0:
        s = math.sqrt(tr + 1.0) * 2
        w, x, y, z = 0.25 * s, (m[2][1] - m[1][2]) / s, (m[0][2] - m[2][0]) / s, (m[1][0] - m[0][1]) / s
    elif m[0][0] > m[1][1] and m[0][0] > m[2][2]:
        s = math.sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2
        w, x, y, z = (m[2][1] - m[1][2]) / s, 0.25 * s, (m[0][1] + m[1][0]) / s, (m[0][2] + m[2][0]) / s
    elif m[1][1] > m[2][2]:
        s = math.sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2
        w, x, y, z = (m[0][2] - m[2][0]) / s, (m[0][1] + m[1][0]) / s, 0.25 * s, (m[1][2] + m[2][1]) / s
    else:
        s = math.sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2
        w, x, y, z = (m[1][0] - m[0][1]) / s, (m[0][2] + m[2][0]) / s, (m[1][2] + m[2][1]) / s, 0.25 * s
    n = math.sqrt(w * w + x * x + y * y + z * z)
    return [w / n, x / n, y / n, z / n]


def angles_of(q):
    """A quaternion back to Quake angles (hands::anglesFromVectors of its forward and up)."""
    w, x, y, z = q
    n = math.sqrt(w * w + x * x + y * y + z * z)
    w, x, y, z = w / n, x / n, y / n, z / n
    fwd = (1 - 2 * (y * y + z * z), 2 * (x * y + w * z), 2 * (x * z - w * y))
    up = (2 * (x * z + w * y), 2 * (y * z - w * x), 1 - 2 * (x * x + y * y))
    pitch = -math.degrees(math.atan2(fwd[2], math.hypot(fwd[0], fwd[1])))
    yaw = math.degrees(math.atan2(fwd[1], fwd[0]))
    # roll: the up vector against the one with no roll
    _, l0, u0 = basis_of(pitch, yaw, 0.0)
    roll = math.degrees(math.atan2(-sum(a * b for a, b in zip(up, l0)), sum(a * b for a, b in zip(up, u0))))
    return pitch, yaw, roll


# Channels of a frame, as the game's (vr_animrec_data.cpp toChans): head pos 3, head quat 4, crouch; per hand (off, main):
# head pos 3, head quat 4, body pos 3, body quat 4, curls 5, grip, trigger.
def hand_base(h):
    return 8 + h * 21


GROUPS = [(0, "pos", 3), (3, "rot", 4), (7, "scalar", 1)]
for _h in (0, 1):
    _b = hand_base(_h)
    GROUPS += [(_b, "pos", 3), (_b + 3, "rot", 4), (_b + 7, "pos", 3), (_b + 10, "rot", 4), (_b + 14, "scalar", 7)]
CHANS = 8 + 2 * 21


def parse(path):
    header, frames = [], []
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\r\n")
        if line.startswith("f "):
            frames.append([float(v) for v in line.split()[1:]])
        else:
            header.append(line)
    return header, frames


def to_chans(row):
    """A file row (t, head 6, crouch, main 21+..., off ...) to (t, channels, wids)."""
    t = row[0]
    c = [0.0] * CHANS
    c[0:3] = row[1:4]
    c[3:7] = quat_of(row[4:7])
    c[7] = row[7]
    wids = [0, 0]
    at = 8
    for name in HAND_ORDER:
        h = 1 if name == "main" else 0
        b = hand_base(h)
        c[b:b + 3] = row[at:at + 3]
        c[b + 3:b + 7] = quat_of(row[at + 3:at + 6])
        c[b + 7:b + 10] = row[at + 6:at + 9]
        c[b + 10:b + 14] = quat_of(row[at + 9:at + 12])
        c[b + 14:b + 21] = row[at + 12:at + 19]
        wids[h] = int(row[at + 19])
        at += 20
    return t, c, wids


def row_of(t, c, wids):
    out = [f"{t:.4f}"]
    out += [f"{v:.5f}" for v in c[0:3]] + [f"{v:.3f}" for v in angles_of(c[3:7])] + [f"{c[7]:.3f}"]
    for name in HAND_ORDER:
        h = 1 if name == "main" else 0
        b = hand_base(h)
        out += [f"{v:.5f}" for v in c[b:b + 3]] + [f"{v:.3f}" for v in angles_of(c[b + 3:b + 7])]
        out += [f"{v:.5f}" for v in c[b + 7:b + 10]] + [f"{v:.3f}" for v in angles_of(c[b + 10:b + 14])]
        out += [f"{v:.3f}" for v in c[b + 14:b + 21]] + [str(wids[h])]
    return "f " + " ".join(out)


def keep_sides(xs):
    for i in range(1, len(xs)):
        for at, kind, _ in GROUPS:
            if kind == "rot" and sum(xs[i][at + k] * xs[i - 1][at + k] for k in range(4)) < 0:
                for k in range(4):
                    xs[i][at + k] = -xs[i][at + k]


def normalise(c):
    for at, kind, _ in GROUPS:
        if kind == "rot":
            n = math.sqrt(sum(c[at + k] ** 2 for k in range(4)))
            if n > 1e-6:
                for k in range(4):
                    c[at + k] /= n


def hermite(x, t, prev, a, b, nxt, time):
    h = t[b] - t[a]
    if h <= 1e-9:
        return list(x[a])
    s = min(max((time - t[a]) / h, 0.0), 1.0)
    s2, s3 = s * s, s * s * s
    h00, h10, h01, h11 = 2 * s3 - 3 * s2 + 1, s3 - 2 * s2 + s, -2 * s3 + 3 * s2, s3 - s2
    pa = prev if prev >= 0 else a
    nb = nxt if nxt >= 0 else b
    da, db = t[b] - t[pa], t[nb] - t[a]
    out = [0.0] * CHANS
    xa, xb, xp, xn = x[a], x[b], x[pa], x[nb]
    for c in range(CHANS):
        ma = (xb[c] - xp[c]) / da if da > 1e-9 else 0.0
        mb = (xn[c] - xa[c]) / db if db > 1e-9 else 0.0
        out[c] = h00 * xa[c] + h10 * h * ma + h01 * xb[c] + h11 * h * mb
    normalise(out)
    return out


def errors(got, want):
    pos = ang = sc = 0.0
    for at, kind, size in GROUPS:
        if kind == "pos":
            pos = max(pos, math.dist(got[at:at + 3], want[at:at + 3]))
        elif kind == "rot":
            d = min(abs(sum(got[at + k] * want[at + k] for k in range(4))), 1.0)
            ang = max(ang, 2 * math.degrees(math.acos(d)))
        else:
            sc = max(sc, max(abs(got[at + k] - want[at + k]) for k in range(size)))
    return pos, ang, sc


def smooth(times, raw, wids, event_times, tau, pos_tol, ang_tol, curl_tol):
    n = len(raw)
    keep_sides(raw)
    x = [list(c) for c in raw]
    if tau > 0:
        for i in range(1, n):
            k = 1 - math.exp(-(times[i] - times[i - 1]) / tau)
            x[i] = [p + k * (v - p) for p, v in zip(x[i - 1], x[i])]
        for i in range(n - 2, -1, -1):
            k = 1 - math.exp(-(times[i + 1] - times[i]) / tau)
            x[i] = [p + k * (v - p) for p, v in zip(x[i + 1], x[i])]
        for c in x:
            normalise(c)
    jitter = max(errors(x[i], raw[i])[0] for i in range(n))

    must = {0, n - 1}
    for et in event_times:
        must.add(min(range(n), key=lambda i: abs(times[i] - et)))
    for i in range(1, n):
        if wids[i] != wids[i - 1]:
            must.update((i - 1, i))
    keys = sorted(must)

    def norm_err(e):
        return max(e[0] / max(pos_tol, 1e-6), e[1] / max(ang_tol, 1e-3), e[2] / max(curl_tol, 1e-4))

    def measure(j):
        kp = keys[j - 1] if j > 0 else -1
        kn = keys[j + 2] if j + 2 < len(keys) else -1
        worst, at = 0.0, -1
        for i in range(keys[j] + 1, keys[j + 1]):
            e = norm_err(errors(hermite(x, times, kp, keys[j], keys[j + 1], kn, times[i]), x[i]))
            if e > worst:
                worst, at = e, i
        return worst, at

    seg = [measure(j) for j in range(len(keys) - 1)]
    for _ in range(n):
        worst_j, worst = -1, 1.0
        for j, (e, at) in enumerate(seg):
            if e > worst and at >= 0:
                worst_j, worst = j, e
        if worst_j < 0:
            break
        keys.insert(worst_j + 1, seg[worst_j][1])
        seg.insert(worst_j + 1, (0.0, -1))
        for j in range(max(0, worst_j - 2), min(len(seg) - 1, worst_j + 3) + 1):
            seg[j] = measure(j)

    mp = ma = mc = 0.0
    for j in range(len(keys) - 1):
        kp = keys[j - 1] if j > 0 else -1
        kn = keys[j + 2] if j + 2 < len(keys) else -1
        for i in range(keys[j] + 1, keys[j + 1]):
            e = errors(hermite(x, times, kp, keys[j], keys[j + 1], kn, times[i]), x[i])
            mp, ma, mc = max(mp, e[0]), max(ma, e[1]), max(mc, e[2])
    return keys, x, (mp, ma, mc, jitter)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("anim")
    ap.add_argument("--out")
    ap.add_argument("--tau", type=float, default=0.02)
    ap.add_argument("--pos", type=float, default=0.004)
    ap.add_argument("--ang", type=float, default=1.5)
    ap.add_argument("--curl", type=float, default=0.04)
    ap.add_argument("--check", action="store_true", help="exit 1 if a tolerance is broken or no frame was dropped")
    a = ap.parse_args()

    header, rows = parse(a.anim)
    if not rows:
        sys.exit(f"{a.anim}: no frames")
    times, raw, wids = [], [], []
    for r in rows:
        t, c, w = to_chans(r)
        times.append(t)
        raw.append(c)
        wids.append(tuple(w))
    event_times = [float(l.split()[1]) for l in header if l.startswith("event ")]
    keys, x, (mp, ma, mc, jitter) = smooth(times, raw, wids, event_times, a.tau, a.pos, a.ang, a.curl)

    info = (f"keys {len(keys)} of {len(rows)}; tau {a.tau:.3f} s, tolerances {a.pos:.4f} m {a.ang:.2f} deg {a.curl:.3f}; "
            f"max error {mp:.4f} m {ma:.2f} deg {mc:.3f}; jitter removed {jitter:.4f} m (anim_smooth.py)")
    out = []
    for line in header:
        if line.startswith("frames "):
            raw_n = line.split()[2] if len(line.split()) > 2 else str(len(rows))
            line = f"frames {len(keys)} {raw_n}"
        elif line.startswith("smoothed "):
            line = "smoothed 1 " + info
        out.append(line)
    out += [row_of(times[k], x[k], list(wids[k])) for k in keys]
    path = a.out or (a.anim[:-5] if a.anim.endswith(".anim") else a.anim) + ".smooth.anim"
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    print(f"anim_smooth: {path}: {len(rows)} frames -> {len(keys)} keys; max error {mp * 1000:.2f} mm, {ma:.2f} deg, "
          f"{mc:.3f}; jitter removed {jitter * 1000:.2f} mm")
    if a.check:
        ok = len(keys) < len(rows) and mp <= a.pos * 1.0001 and ma <= a.ang * 1.0001 and mc <= a.curl * 1.0001
        print(f"anim_smooth: {'PASS' if ok else 'FAIL'}")
        sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
