"""The author's melee takes swung into a wall: how many blows each swing lands on it (ROUND22.md "One wall blow a swing").

    python melee_wall_takes.py gen <takes folder> <list out> [--sets axe,crowbar,shotgun,sword,fists] [--per N]
    python melee_wall_takes.py report <run log> [<another run's log, compared>]

gen writes a vr_motion_eval list (melee_wall_takes.sh runs it): each melee take with its weapon (the axe takes also with
the crowbar and the shotgun, as gun bash) replayed in vrmelee-recording with the take's dummy put at its north wall
(its surface y=640, facing -y): the wall at the dummy's front (front0), 8, 16 and 24 units nearer (in8..in24: the blow
lands deeper, the player stepped back off the wall where his box would be in it) and 10 units past it (miss10).
report counts each replay's wall blows (the console's "melee event: ..., a wall", developer 1) per weapon: replays with
one or more, with two or more (one swing struck the wall twice: a doubled thunk).
The axe slash takes are also swung twice (double_take: two real blows 0.7 s apart, set <weapon>2x, the doubled takes
in a wall_doubles folder beside the list): both must land (two blows each, as many as the takes once struck).
"""
import csv
import os
import re
import sys

WALL_Y = 640.0  # vrmelee-recording's north wall (make_vrmelee_recording_map.py)
HALF = 12.0     # the training dummy's box half width (the take's target)
Z = 24.0        # the dummy's origin height (the takes': 24.03)
SPOTS = [("front0", 0), ("in8", 8), ("in16", 16), ("in24", 24), ("miss10", -10)]
# weapon name: (QC WID_*, the takes' main weapon it replays)
WEAPONS = {"axe": (2, 2), "crowbar": (17, 2), "shotgun": (4, 2), "sword": (13, 13), "fists": (0, 0)}
# Not swings: poses, misses, grabs, charges.
SKIP = re.compile(r"^(no_|not_|parry_pose|neck_snap|lift|judo_throw|shoulder_charge|leg_|eval_status)")
FIST_LABELS = re.compile(r"^(punch_|slap_)")


def header(path):
    h = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if not line.startswith("#"):
                break
            k, _, v = line[1:].strip().partition(":")
            h[k.strip()] = v.strip()
    return h


TIME_COLS = ("t", "realtime", "xr_time", "sv_time")


def double_take(src, dst):
    """The take's blow swung twice (two real blows 0.7 s apart): its frames from 0.45 s before its recorded hit to 0.25 s
    after it played again straight after (the hand jumps back to its wind-up), the rest after them. None without a hit."""
    with open(src, newline="", encoding="utf-8", errors="replace") as f:
        lines = f.read().splitlines(True)
    head = [l for l in lines if l.startswith("#")]
    rows = list(csv.reader([l for l in lines if not l.startswith("#")]))
    cols, rows = rows[0], rows[1:]
    ti, ei, fi = cols.index("t"), cols.index("events"), cols.index("frame")
    hit = next((float(r[ti]) for r in rows if "melee:" in r[ei]), None)
    if hit is None:
        return False
    block = [r for r in rows if hit - 0.45 <= float(r[ti]) <= hit + 0.25]
    before = [r for r in rows if float(r[ti]) <= hit + 0.25]
    after = [r for r in rows if float(r[ti]) > hit + 0.25]
    step = float(after[0][ti]) - float(before[-1][ti]) if after else 0.0139
    shift = float(before[-1][ti]) + step - float(block[0][ti])
    out = list(before)
    for part, d in ((block, shift), (after, shift + float(block[-1][ti]) + step - float(after[0][ti]) if after else 0)):
        for r in part:
            r = list(r)
            for c in TIME_COLS:
                if c in cols and r[cols.index(c)] not in ("", "-"):
                    r[cols.index(c)] = "%.6f" % (float(r[cols.index(c)]) + d)
            r[ei] = ""
            out.append(r)
    for k, r in enumerate(out):
        r[fi] = str(k)
    with open(dst, "w", newline="", encoding="utf-8") as f:
        f.writelines(head)
        w = csv.writer(f, lineterminator="\n")
        w.writerow(cols)
        w.writerows(out)
    return True


def gen(takes, out, sets, per):
    lines = []
    counts = {}
    # Two real blows in a row must both land: the axe slash takes swung twice, at the wall's front (set <weapon>2x).
    ddir = os.path.join(os.path.dirname(os.path.abspath(out)), "wall_doubles")
    os.makedirs(ddir, exist_ok=True)
    for name in sorted(os.listdir(takes)):
        if not name.startswith("slash_") or not name.endswith(".csv"):
            continue
        h = header(os.path.join(takes, name))
        if int(h.get("main weapon", "-1").split()[0]) != 2:
            continue
        dst = os.path.join(ddir, "double_" + name).replace("\\", "/")
        if not double_take(os.path.join(takes, name), dst):
            continue
        for w in sets:
            wid, src = WEAPONS[w]
            if src == 2:
                lines.append(f"{dst} | at 0 {WALL_Y + HALF:g} {Z:g} 270 weapon {wid} spot front0 set {w}2x")
    for name in sorted(os.listdir(takes)):
        if not name.endswith(".csv") or SKIP.match(name):
            continue
        path = os.path.join(takes, name).replace("\\", "/")
        h = header(path)
        main = int(h.get("main weapon", "-1").split()[0])
        off = int(h.get("off weapon", "-1").split()[0])
        for w in sets:
            wid, src = WEAPONS[w]
            if main != src or (w == "fists" and (off != 0 or not FIST_LABELS.match(name))):
                continue
            if counts.get(w, 0) >= per:
                continue
            counts[w] = counts.get(w, 0) + 1
            for spot, k in SPOTS:
                y = WALL_Y + HALF + k
                lines.append(f"{path} | at 0 {y:g} {Z:g} 270 weapon {wid} spot {spot} set {w}")
    with open(out, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("melee_wall_takes: %d replays (%s)" % (len(lines), ", ".join(f"{w} {counts.get(w, 0)} takes" for w in sets)))


def load(log):
    """The run's console (developer 1): each replay's "melee event: ..., a wall" lines, then its verdict line
    ("FAIL  <take> | at ... set <weapon>: ...")."""
    rows = {}
    walls = 0
    verdict = re.compile(r"^(\S+)\s+(\S+\.csv \| at [^:]*): ")
    with open(log, encoding="utf-8", errors="replace") as f:
        for line in f:
            if line.startswith("melee event: ") and line.rstrip().endswith("a wall"):
                walls += 1
                continue
            m = verdict.match(line)
            if m:
                key = m.group(2)
                w = re.search(r"set (\w+)", key)
                s = re.search(r"spot (\w+)", key)
                rows[key] = (w.group(1) if w else "?", s.group(1) if s else "?", walls, m.group(1))
                walls = 0
    return rows


def summary(rows):
    out = {}
    for w, spot, walls, _ in rows.values():
        t = out.setdefault(w, [0, 0, 0, 0])  # replays, struck, twice or more, errors
        t[0] += 1
        t[1] += walls >= 1
        t[2] += walls >= 2
    return out


def report(table, other=None):
    rows = load(table)
    s = summary(rows)
    errors = sum(1 for r in rows.values() if r[3] == "ERROR")
    if other:
        rows2 = load(other)
        s2 = summary(rows2)
        print("weapon     replays  struck (A -> B)  two+ blows (A -> B)")
        for w in sorted(set(s) | set(s2), key=lambda k: (k.endswith("2x"), list(WEAPONS).index(k.replace("2x", "")))):
            if True:
                a, b = s.get(w, [0] * 4), s2.get(w, [0] * 4)
                print(f"{w:10} {a[0]:7}  {a[1]:5} -> {b[1]:<5}    {a[2]:5} -> {b[2]:<5}")
        moved = [(f, rows[f][2], rows2[f][2]) for f in rows if f in rows2 and rows[f][2] != rows2[f][2]]
        for f, a, b in moved:
            print(f"  {a} -> {b}: {os.path.basename(f.split(' | ')[0])} {rows[f][0]} {rows[f][1]}")
    else:
        print("weapon     replays  struck  two+ blows")
        for w, t in s.items():
            print(f"{w:10} {t[0]:7}  {t[1]:6}  {t[2]:6}")
        for f, (w, spot, walls, _) in rows.items():
            if walls >= 2:
                print(f"  {walls} blows: {os.path.basename(f.split(' | ')[0])} {w} {spot}")
    if errors:
        print(f"melee_wall_takes: {errors} replays could not play")


if __name__ == "__main__":
    a = sys.argv[1:]
    if len(a) >= 3 and a[0] == "gen":
        sets = list(WEAPONS)
        per = 999
        if "--sets" in a:
            sets = a[a.index("--sets") + 1].split(",")
        if "--per" in a:
            per = int(a[a.index("--per") + 1])
        gen(a[1], a[2], sets, per)
    elif len(a) >= 2 and a[0] == "report":
        report(a[1], a[2] if len(a) >= 3 else None)
    else:
        print(__doc__)
        sys.exit(1)
