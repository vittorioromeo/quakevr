#!/usr/bin/env python3
"""flatsweep_check.py <rows.csv>... [--known flatsweep_known.txt] [--baseline <rows.csv>...] [--table]

Judges vr_flat_sweep's rows (docs/vr-port/FLATPLAY.md, "The pose sweep"). Each check has a limit, and fails on a stance
(weapon, crouch, ads) if any pitch breaks it. A failure listed in the known file (check, stance, the phase that fixes it, the
worst value it may reach) is reported as known, as long as it is no worse than listed: worse, or not listed, is a new
regression. Prints `flatsweep: N passed, K known failing, M failed` and exits non-zero on M > 0.

--baseline: also compares each check's worst value with those rows: worse by more than the check's noise limit is printed
and counts as failed. --table: each stance's worst value per check. --write-known: prints a known file from these rows' failures.
"""
import argparse
import csv
import math
import sys

# name, column or function of a row, the worse direction, limit, noise limit (what a run may differ by), the rows it applies to,
# the phase that fixes it when it fails (0: must pass now), what it says.
def has_sight(r):
    return float(r["rear_off"]) >= 0.0


def camera_rows(r):
    return True


def gun_rows(r):
    return float(r["gun_torso"]) > -90.0


def hipfire(r):
    return r["ads"] == "0"


def aiming(r):
    return r["weapon"] != "melee" and float(r["shot_apart"]) >= 0.0


def sighted(r):
    return r["ads"] == "1" and has_sight(r)


def two_hand_rows(r):
    return r["weapon"] in ("pistol", "long")


def spot_off(r):
    if r["has_spot"] == "0" or float(r["spot_off"]) < 0.0:
        return 99.0
    return float(r["spot_off"])


CHECKS = [
    # The camera in the body (the neck pivot).
    ("cam_inner_pct", "inner_pct", "high", 0.5, 0.2, camera_rows, 1, "the screen seeing the body's inside (first surface from behind), %"),
    ("cam_hidden_pct", "hidden_pct", "high", 0.5, 0.2, camera_rows, 1, "the screen seeing the collapsed neck and head, %"),
    ("cam_inside", "inside", "high", 0.0, 0.0, camera_rows, 1, "the eye inside the mesh"),
    ("cam_surface", "eye_torso", "low", 1.0, 0.05, camera_rows, 1, "the eye's distance from the torso's surface"),
    ("cam_neck", "eye_neck", "low", 1.0, 0.05, camera_rows, 1, "the eye's distance from the neck's surface"),
    ("cam_hull", "hull_m", "low", 0.0, 0.05, camera_rows, 0, "the eye inside his box"),
    ("cam_solid", "solid", "high", 0.0, 0.0, camera_rows, 0, "the eye in solid"),
    ("cam_clear", "clear", "low", 1.0, 0.0, camera_rows, 0, "the level between the head and the eye"),
    # What later phases fix.
    ("arm_at_eye_pct", "arm_inner_pct", "high", 0.5, 0.2, camera_rows, 2, "the screen seeing an arm's inside, %"),
    ("arm_at_eye", "arm_inside", "high", 0.0, 0.0, camera_rows, 2, "the eye inside an arm"),
    ("arm_surface", "eye_arm", "low", 1.0, 0.05, camera_rows, 2, "the eye's distance from an arm's surface"),
    ("gun_torso", "gun_torso", "high", 0.5, 0.05, gun_rows, 2, "the gun in the torso, depth"),
    ("gun_head", "gun_head", "high", 0.5, 0.05, lambda r: gun_rows(r) and hipfire(r), 2, "the gun in the head and neck, hipfire, depth"),
    ("hand_torso", lambda r: max(float(r["hand0_torso"]), float(r["hand1_torso"])), "high", 0.5, 0.05, camera_rows, 2, "a hand in the torso, depth"),
    ("fore_torso", lambda r: max(float(r["fore0_torso"]), float(r["fore1_torso"])), "high", 1.0, 0.05, camera_rows, 2, "a forearm in the torso, depth"),
    ("hand_head", lambda r: max(float(r["hand0_head"]), float(r["hand1_head"])), "high", 0.5, 0.05, camera_rows, 2, "a hand in the head and neck, depth"),
    ("arm_reach", lambda r: max(float(r["arm0"]), float(r["arm1"])), "high", 1.0, 0.01, camera_rows, 2, "shoulder to drawn hand, share of the arm"),
    ("hand_gap", lambda r: max(float(r["gap0"]), float(r["gap1"])), "high", 6.0, 0.05, camera_rows, 2, "the arm's wrist to the drawn hand (5.4 at rest): the hand detached"),
    ("offhand_spot", spot_off, "high", 0.3, 0.05, two_hand_rows, 3, "the off hand off its hotspot (99: this weapon has none or it is not known)"),
    # What must stay exact.
    # (Today's shot_apart failures: the gun at a wall or the floor, nearer than the aim's 12 units or with its muzzle past the surface; the
    # gun's own place decides those, so phase 2's. shot_deg stays exact in the ironsight at all of them.)
    ("shot_apart", "shot_apart", "high", 1.0, 0.1, aiming, 2, "the shot's line off the crosshair's point"),
    ("sight_rear", "rear_off", "high", 0.05, 0.01, sighted, 0, "the rear sight off the ray"),
    ("sight_front", "front_off", "high", 0.05, 0.01, sighted, 0, "the front sight off the ray"),
    # (Today's: the free hand snapping to and from the gun at the edge of what its arm can take, swing column: 9 units a frame.)
    ("settled", "settled", "low", 1.0, 0.0, camera_rows, 3, "the pose settled at the step"),
]

# Largest jump between neighbouring pitches (per step, in units), per degree of step: the eye turning about a neck moves
# smoothly, a hand that lets go or snaps does not.
JUMPS = [
    ("jump_eye", ("eye_x", "eye_y", "eye_z"), 0.25, 0.02, 1, "the eye's move a degree"),
    ("jump_gun", ("gun_x", "gun_y", "gun_z"), 1.0, 0.05, 2, "the gun's move a degree"),
    ("jump_hand0", ("hand0_x", "hand0_y", "hand0_z"), 1.0, 0.05, 2, "the off hand's move a degree"),
    ("jump_hand1", ("hand1_x", "hand1_y", "hand1_z"), 1.0, 0.05, 2, "the main hand's move a degree"),
]


def stance_of(r):
    return f'{r["label_group"]}/{r["weapon"]}/{"crouch" if r["crouch"] == "1" else "stand"}/{"ads" if r["ads"] == "1" else "hip"}'


def read_rows(paths):
    rows = []
    for p in paths:
        with open(p, newline="") as f:
            for r in csv.DictReader(f):
                r["label_group"] = "wall" if "wall" in r["label"] else "low" if "low" in r["label"] else "open"
                rows.append(r)
    return rows


def value(r, col):
    return col(r) if callable(col) else float(r[col])


def worst_values(rows):
    """(check, stance) -> (worst value, pitch at it)"""
    worst = {}
    for name, col, way, _limit, _noise, applies, _phase, _say in CHECKS:
        for r in rows:
            if not applies(r):
                continue
            v = value(r, col)
            key = (name, stance_of(r))
            if key not in worst or (way == "high" and v > worst[key][0]) or (way == "low" and v < worst[key][0]):
                worst[key] = (v, float(r["pitch"]))
    by_stance = {}
    for r in rows:
        by_stance.setdefault(stance_of(r), []).append(r)
    for stance, group in by_stance.items():
        group.sort(key=lambda r: float(r["pitch"]))
        for name, cols, _limit, _noise, _phase, _say in JUMPS:
            for a, b in zip(group, group[1:]):
                step = float(b["pitch"]) - float(a["pitch"])
                if step <= 0.0:
                    continue
                d = math.dist([float(a[c]) for c in cols], [float(b[c]) for c in cols]) / step
                key = (name, stance)
                if key not in worst or d > worst[key][0]:
                    worst[key] = (d, float(a["pitch"]))
    return worst


def spec(name):
    for c in CHECKS:
        if c[0] == name:
            return {"way": c[2], "limit": c[3], "noise": c[4], "phase": c[6], "say": c[7]}
    for c in JUMPS:
        if c[0] == name:
            return {"way": "high", "limit": c[2], "noise": c[3], "phase": c[4], "say": c[5]}
    raise KeyError(name)


def breaks(name, v):
    s = spec(name)
    return v > s["limit"] if s["way"] == "high" else v < s["limit"]


def read_known(path):
    known = {}
    if not path:
        return known
    with open(path) as f:
        for line in f:
            line = line.split("#")[0].strip()
            if line:
                check, stance, phase, worst = line.split()
                known[(check, stance)] = (int(phase), float(worst))
    return known


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rows", nargs="+")
    ap.add_argument("--known")
    ap.add_argument("--baseline", nargs="*")
    ap.add_argument("--table", action="store_true")
    ap.add_argument("--write-known", action="store_true")
    a = ap.parse_args()

    worst = worst_values(read_rows(a.rows))
    known = read_known(a.known)
    if a.write_known:
        print("# check stance phase worst: rows failing today, by the phase that fixes them. Generated by flatsweep_check.py --write-known;")
        print("# a row may not get worse than its number (plus the check's noise limit). Fix a row: take its line out.")
        for (name, stance), (v, _p) in sorted(worst.items(), key=lambda kv: (spec(kv[0][0])["phase"], kv[0][0], kv[0][1])):
            if breaks(name, v):
                print(f'{name} {stance} {spec(name)["phase"]} {v:.3f}')
        return 0

    passed = known_failing = failed = 0
    fixed = []
    for (name, stance), (v, pitch) in sorted(worst.items()):
        s = spec(name)
        k = known.get((name, stance))
        if not breaks(name, v):
            passed += 1
            if k:
                fixed.append(f"{name} {stance}")
            continue
        if k:
            over = v > k[1] + s["noise"] if s["way"] == "high" else v < k[1] - s["noise"]
            if over:
                failed += 1
                print(f"FAIL worse than known: {name} {stance}: {v:.3f} at pitch {pitch:.0f}, listed {k[1]:.3f} ({s['say']})")
            else:
                known_failing += 1
            continue
        failed += 1
        print(f"FAIL new: {name} {stance}: {v:.3f} at pitch {pitch:.0f}, limit {s['limit']} ({s['say']})")
    for f in fixed:
        print(f"note: known row passes now, take it out: {f}")

    if a.baseline:
        base = worst_values(read_rows(a.baseline))
        for key, (v, pitch) in sorted(worst.items()):
            if key not in base:
                continue
            s = spec(key[0])
            b = base[key][0]
            worse = v > b + s["noise"] if s["way"] == "high" else v < b - s["noise"]
            if worse and breaks(key[0], v):
                failed += 1
                print(f"FAIL worse than the baseline: {key[0]} {key[1]}: {b:.3f} -> {v:.3f} at pitch {pitch:.0f} ({s['say']})")

    if a.table:
        names = [c[0] for c in CHECKS] + [c[0] for c in JUMPS]
        stances = sorted({k[1] for k in worst})
        for n in names:
            cells = []
            for st in stances:
                if (n, st) in worst:
                    v, p = worst[(n, st)]
                    cells.append(f'{st.replace("open/", "")}={v:.2f}@{p:.0f}{"*" if breaks(n, v) else ""}')
            print(f"{n}: " + " ".join(cells))

    print(f"flatsweep: {passed} passed, {known_failing} known failing, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
