"""Missiles and shots vs a turned explosive box (vr_box3d_shot_shape): a rocket, a nail, a grenade and a shotgun pellet
fired (vr_physics_fire) at its middle, near its real corners and into the empty corners of Quake's box round it; each hit
compared with the exact ray / turned-box crossing.

python Misc/quakevr/shot_shape_test.py <worktree name> [yaw,tilt ...] [--off] [--more]
(--off: vr_box3d_shot_shape 0, the old box; --more: a super nail and an enforcer's laser instead)
"""
import math, re, subprocess, sys

KIT = "C:/OHWorkspace/qvr-kit/run.sh"
BASH = "C:/Program Files/Git/usr/bin/bash.exe"
NUM = r"(-?[\d.]+(?:e[-+]?\d+)?)"
V3 = NUM + r" " + NUM + r" " + NUM
KINDS = {0: "rocket", 1: "nail", 2: "grenade", 10: "pellet"}


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


def run(name, body, shape):
    s = ("developer 1;vr_debug_missiles 1;map vrfiringrange;wait60;god;notarget;sv_gravity 0;vr_shot_push 0;"
         f"vr_dmg_rocket 0;vr_dmg_grenade 0;vr_box3d_shot_shape {shape};vr_test_spawn 104;vr_test_spawn_dist 160;"
         + body + "toggleconsole;quit")
    return subprocess.run([BASH, KIT, name, "-Filter", "missile hit|vr_physics_fire|test fire", "-Script", s],
                          capture_output=True, text=True, errors="replace").stdout


def pose_of(lines, start):
    """The prop's box printed after line `start`: (centre, half, axes)."""
    c = h = None
    axes = {}
    for line in lines[start:start + 6]:
        m = re.search(r"missile hit: prop \d+ centre " + V3 + r" half " + V3, line)
        if m:
            g = [float(x) for x in m.groups()]
            c, h = tuple(g[:3]), tuple(g[3:])
        m = re.search(r"missile hit: prop \d+ axis (\d) " + V3, line)
        if m:
            axes[int(m.group(1))] = tuple(float(x) for x in m.groups()[1:])
    return (c, h, [axes[k] for k in range(3)]) if c and len(axes) == 3 else None


def parse(out):
    """Each shot in order: {kind, from, dir, what, at, pose}."""
    lines = out.splitlines()
    shots, pending = [], {}
    for i, line in enumerate(lines):
        m = re.search(r"vr_physics_fire: (\S+) (\d+) from " + V3 + r" vel " + V3, line)
        if m:
            v = tuple(float(x) for x in m.groups()[5:8])
            s = {"kind": m.group(1), "from": tuple(float(x) for x in m.groups()[2:5]), "dir": mul(v, 1 / norm(v)),
                 "what": None}
            shots.append(s)
            pending[m.group(2)] = s
            continue
        m = re.search(r"test fire: shot from '?" + V3 + r"'? dir '?" + V3 + r"'? hit (.*?) at '?" + V3, line)
        if m:
            g = m.groups()
            d = tuple(float(x) for x in g[3:6])
            shots.append({"kind": "pellet", "from": tuple(float(x) for x in g[:3]), "dir": mul(d, 1 / norm(d)),
                          "what": g[6], "at": tuple(float(x) for x in g[7:10]), "pose": None})
            continue
        m = re.search(r"missile hit: (\S+) (\d+) hit (.*?) (\d+) at " + V3, line)
        if m and m.group(2) in pending:
            s = pending.pop(m.group(2))
            s["what"] = m.group(3)
            s["at"] = tuple(float(x) for x in m.groups()[4:7])
            s["pose"] = pose_of(lines, i + 1)
    return shots


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    shape = 0 if "--off" in sys.argv else 1
    kinds = ((0, 30), (1, 30), (2, 200), (10, 5))
    if "--more" in sys.argv:
        kinds = ((3, 30), (4, 60))  # a super nail, an enforcer's laser (MOVETYPE_FLY)
    name = args[0] if args else "missileshape"
    cases = [tuple(int(x) for x in c.split(",")) for c in args[1:]] or [(0, 0), (22, 0), (45, 0), (0, 45)]
    bad = total = 0
    worst = 0.0
    for yaw, tilt in cases:
        spawn = f"vr_test_spawn_yaw {yaw};vr_test_spawn_tilt {tilt};impulse 241;wait90;"
        first = parse(run(name, spawn + "vr_physics_fire 1;wait30;", shape))
        if not first or not first[0].get("pose"):
            print(f"yaw {yaw} tilt {tilt}: no box hit at its middle: {first}")
            bad += 1
            continue
        centre, half, axes = first[0]["pose"]
        ext = [sum(abs(a[k]) * h for a, h in zip(axes, half)) for k in range(3)]
        o = first[0]["from"]
        toward = mul(sub(o, centre), 1 / norm(sub(o, centre)))
        targets = [("middle", centre)]
        for sy in (-1, 1):
            for sz in (-1, 1):
                for sx in (-1, 1):
                    corner = centre
                    for a, h, s_ in zip(axes, half, (sx, sy, sz)):
                        corner = add(corner, mul(a, s_ * (h - 1.5)))
                    if dot(sub(corner, centre), toward) > 0:
                        targets.append(("real corner", corner))
        # The box round it: its corners on your side, 1 unit in (empty unless it is upright and square to you).
        side = 0 if abs(toward[0]) > abs(toward[1]) else 1
        sgn = 1 if toward[side] > 0 else -1
        for su in (-1, 1):
            for sz in (-1, 1):
                p = list(centre)
                p[side] += sgn * (ext[side] - 1)
                p[1 - side] += su * (ext[1 - side] - 1)
                p[2] += sz * (ext[2] - 1)
                targets.append(("empty corner", tuple(p)))
        body = spawn
        for k, wait in kinds:
            for _, t in targets:
                body += f"vr_physics_fire {k} {t[0]:.3f} {t[1]:.3f} {t[2]:.3f};wait{wait};"
        res = parse(run(name, body, shape))
        print(f"yaw {yaw} tilt {tilt}: box middle {tuple(round(c, 2) for c in centre)}, round-box half "
              f"{tuple(round(e, 1) for e in ext)}; {len(res)} shots (shape {shape})")
        if len(res) != len(kinds) * len(targets):
            print(f"  expected {len(kinds) * len(targets)} shots")
            bad += 1
        for i, s in enumerate(res):
            label = targets[i % len(targets)][0]
            c, h, ax = s.get("pose") or (centre, half, axes)
            t = slab(s["from"], s["dir"], c, ax, h)
            tb = slab(s["from"], s["dir"], c, [(1, 0, 0), (0, 1, 0), (0, 0, 1)],
                      [sum(abs(a[k]) * hh for a, hh in zip(ax, h)) for k in range(3)])
            hitbox = s["what"] in ("misc_explobox", "")
            if t is not None:
                exp = add(s["from"], mul(s["dir"], t))
                err = norm(sub(s["at"], exp)) if hitbox else None
                ok = hitbox and err < 0.5
                verdict = f"hit {err:.2f} from the real surface" if hitbox else f"missed it ({s['what']})"
                if hitbox:
                    worst = max(worst, err)
            else:
                ok = not hitbox
                verdict = (f"hit it at {tuple(round(x, 2) for x in s['at'])}" if hitbox else
                           f"went by ({s['what']})")
            old = ("the round box: " + (f"{norm(sub(add(s['from'], mul(s['dir'], tb)), add(s['from'], mul(s['dir'], t)))):.1f} off"
                   if t is not None and tb is not None else ("hit" if tb is not None else "missed too")))
            print(f"  {s['kind']:14s} {label:12s} {'ok ' if ok else 'BAD'} expected {'hit' if t is not None else 'miss'}: "
                  f"{verdict}; {old}")
            bad += 0 if ok else 1
            total += 1
    print(f"SHOTS {total} FAILURES {bad} worst {worst:.2f}")


main()
