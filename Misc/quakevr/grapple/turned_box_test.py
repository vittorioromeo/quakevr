"""Grappling hook vs a turned explosive box: fire at its middle, near its real corners and into its box's empty corners;
compare each bite with the exact ray / turned-box crossing (and with where Quake's box round it would have bitten)."""
import math, re, subprocess, sys

KIT = "C:/OHWorkspace/qvr-kit/run.sh"
NAME = sys.argv[1] if len(sys.argv) > 1 else "grapple5"
MID_XY = (156.0, -556.0)
FLOOR = 17.61  # (the spawn's origin z for an upright box: its bottom, 0.5 over the floor)
HALF = (16.0, 16.0, 32.0)
NUM = r"(-?[\d.]+(?:e[-+]?\d+)?)"
VEC = r"'?\s*" + NUM + r"\s+" + NUM + r"\s+" + NUM + r"\s*'?"


def run(script):
    out = subprocess.run(["C:/Program Files/Git/usr/bin/bash.exe", KIT, NAME, "-Filter", "grapple|test spawn", "-Script", script], capture_output=True,
                         text=True, errors="replace").stdout

    return out


def v(m, i):
    return tuple(float(m.group(i + k)) for k in range(3))


def sub(a, b): return tuple(x - y for x, y in zip(a, b))
def add(a, b): return tuple(x + y for x, y in zip(a, b))
def mul(a, s): return tuple(x * s for x in a)
def dot(a, b): return sum(x * y for x, y in zip(a, b))
def norm(a): return math.sqrt(dot(a, a))


def slab(o, d, centre, axes, half):
    """Ray o + t d (t >= 0) against the box (centre, unit axes, half sizes): entry t or None."""
    t0, t1 = 0.0, 1e9
    rel = sub(o, centre)
    for ax, h in zip(axes, half):
        p, q = dot(rel, ax), dot(d, ax)
        if abs(q) < 1e-9:
            if abs(p) > h:
                return None
            continue
        a, b = (-h - p) / q, (h - p) / q
        if a > b:
            a, b = b, a
        t0, t1 = max(t0, a), min(t1, b)
        if t0 > t1:
            return None
    return t0


def case_mid_z(tilt):
    r = math.radians(tilt)
    return FLOOR + HALF[2] * math.cos(r) + HALF[1] * math.sin(r)


def script(yaw, tilt, targets):
    s = ("developer 1;vr_grapple_debug 2;vr_weapon_grip_mode 1;vr_grapple_trigger_release 1;map vrfiringrange;wait60;god;"
         "impulse 151;wait10;vr_test_spawn 104;vr_test_spawn_dist 160;")
    s += f"sv_gravity {0 if tilt else 800};vr_test_spawn_yaw {yaw};vr_test_spawn_tilt {tilt};impulse 241;wait90;"
    for t in targets:
        aim = f"vr_grapple_test_aim {t[0]:.3f} {t[1]:.3f} {t[2]:.3f};wait1;"
        s += "+attack;" + aim * 6 + "wait60;-attack;wait300;"
    return s + "toggleconsole;quit"


def shots(out):
    """Each shot: (from, dir, what it bit, at, box) in order."""
    res = []
    cur = None
    for line in out.splitlines():
        m = re.search(r"grapple test aim: hook \d+ from (\S+) (\S+) (\S+) dir (\S+) (\S+) (\S+)", line)
        if m:
            if cur is None or cur["bit"] is not None:
                cur = {"bit": None}
                res.append(cur)
            cur["from"], cur["dir"] = v(m, 1), v(m, 4)
            continue
        m = re.search(r"grapple: hook \d+ \(hand \S+\) bit (.*?) \(\S+ kg\) at " + VEC, line)
        if m and cur and cur["bit"] is None:
            cur["bit"] = m.group(1)
            cur["at"] = v(m, 2)
            continue
        m = re.search(r"bitten box: (origin|x|y|z) " + VEC, line)
        if m and cur:
            cur.setdefault("raw", {})[m.group(1)] = v(m, 2)
            r = cur["raw"]
            if len(r) == 4:
                # (the explosive box's model: 0..32, 0..32, 0..64 from its origin)
                centre = add(add(add(r["origin"], mul(r["x"], HALF[0])), mul(r["y"], HALF[1])), mul(r["z"], HALF[2]))
                cur["box"] = (centre, HALF, r["x"], r["y"], r["z"])
    return res


def main():
    cases = [(0, 0), (22, 0), (45, 0), (0, 22), (0, 45)]
    if len(sys.argv) > 2:
        cases = [tuple(int(x) for x in c.split(",")) for c in sys.argv[2:]]
    bad = 0
    for yaw, tilt in cases:
        zm = case_mid_z(tilt)
        mid = (MID_XY[0], MID_XY[1], zm)
        # Discovery: its middle, for its pose.
        first = shots(run(script(yaw, tilt, [mid])))
        if not first or "box" not in first[0]:
            print(f"yaw {yaw} tilt {tilt}: no box bitten at its middle: {first}")
            bad += 1
            continue
        centre, half, ax, ay, az = first[0]["box"]
        axes = [mul(a, 1 / norm(a)) for a in (ax, ay, az)]
        # Quake's box round it.
        ext = [sum(abs(a[k]) * h for a, h in zip(axes, half)) for k in range(3)]
        # Targets: its middle; its corners on the side facing you pulled 1.5 in (the real surface near a corner); the
        # box round it's corners 1 in from them (empty unless upright).
        targets = [centre]
        toward = (1.0, 0.0, 0.0)  # (you are along +x)
        for sy in (-1, 1):
            for sz in (-1, 1):
                for sx in (-1, 1):
                    corner = centre
                    for a, h, s_ in zip(axes, half, (sx, sy, sz)):
                        corner = add(corner, mul(a, s_ * (h - 1.5)))
                    if dot(sub(corner, centre), toward) > 0:
                        targets.append(corner)
        for sy in (-1, 1):
            for sz in (-1, 1):
                targets.append((centre[0] + ext[0] - 1, centre[1] + sy * (ext[1] - 1), centre[2] + sz * (ext[2] - 1)))
        res = shots(run(script(yaw, tilt, targets)))
        # (A hook that flew to its rope's end lies loose out there: the gun fires no more that run. The rest again.)
        for _ in range(4):
            if len(res) >= len(targets):
                break
            res += shots(run(script(yaw, tilt, targets[len(res):])))
        if len(res) != len(targets):
            print(f"  ({len(targets)} targets, {len(res)} shots seen)")
        print(f"yaw {yaw} tilt {tilt}: box middle {tuple(round(c, 2) for c in centre)} round-box half "
              f"{tuple(round(e, 1) for e in ext)}; {len(res)} shots")
        for i, s in enumerate(res):
            o, d = s["from"], s["dir"]
            t = slab(o, d, centre, axes, half)
            tb = slab(o, d, centre, [(1, 0, 0), (0, 1, 0), (0, 0, 1)], ext)
            exp = add(o, mul(d, t)) if t is not None else None
            boxat = add(o, mul(d, tb)) if tb is not None else None
            bitbox = s["bit"] is not None and s["bit"] in ("misc_explobox", "maps/b_explob.bsp", "")
            if exp is not None:
                err = norm(sub(s["at"], exp)) if bitbox else None
                ok = bitbox and err < 0.75
                verdict = f"bit the box {err:.2f} from the real surface" if bitbox else f"missed it ({s['bit']})"
            else:
                ok = not bitbox
                verdict = f"bit the box at {s.get('at')}" if bitbox else f"missed it (bit {s['bit']})"
            old = "the old box would bite" if boxat is not None else "the old box missed too"
            if boxat is not None and exp is not None:
                old += f" {norm(sub(boxat, exp)):.1f} off"
            k = max(range(len(targets)), key=lambda j: dot(d, mul(sub(targets[j], o), 1 / max(norm(sub(targets[j], o)), 1e-6))))
            kind = f"t{k} " + ("middle" if k == 0 else ("real corner" if k < len(targets) - 4 else "empty corner"))
            print(f"  {i} {kind:16s} {'ok ' if ok else 'BAD'} expected {'hit' if exp else 'miss'}: {verdict}; {old}")
            bad += 0 if ok else 1
    print("FAILURES", bad)


main()
