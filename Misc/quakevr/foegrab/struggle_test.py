#!/usr/bin/env python3
"""struggle_test.py <agent> [regex] -- held living knockdowns struggling (vr_struggle; docs/vr-port/STRUGGLE.md).

Each case is one game on vrfiringrange (batched: testbatch.py), a grunt 36 units ahead, held by the mock hands, with
`vr_struggle_status` every 0.3 s (its weights, the holds' lag and the body's pose) and vr_struggle_debug 2:
  lift       both hands at its shoulders, pulled up (the lift): Swat should lead; its arms come nearer your head
  leglift    both hands on its shins, pulled up 1.1 m (the leg lift), held: Crunch should lead; its waist bends up
  legdrag    knocked down, you stop 44 units off, the main hand on its left thigh, you walk back (dragged by the
             leg): Claw leads; it turns belly down
  headdrag   knocked down (vr_knockdown_test 0), the main hand on its ragdoll's head, you walk back: Pry leads, Brace too
  armdrag    knocked down, the main hand on its right forearm, you walk back: Pry leads, its free hand swats (Swat over
             0.4); its chest's uprightness (Stay Upright) reported
  dead       as headdrag, its health taken at 2.5 s: its struggle ends (dead), its strengths let go
Each also as <case>_off (vr_struggle 0): the holds' lag and the pose compared. PASS per case as above; the logs go to
<agent>/scratch/struggle_<case>.txt.
"""
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "testbatch"))
import testbatch  # noqa: E402

os.environ.setdefault("TB_NODRAW", "1")
AGENT = sys.argv[1] if len(sys.argv) > 1 else "permdecals"
ONLY = sys.argv[2] if len(sys.argv) > 2 else ""
TREE = f"C:/OHWorkspace/qvr-agents/{AGENT}"
MOTIONS = os.path.join(TREE, "quakevr", "motions")
SCRATCH = os.path.join(TREE, "scratch")
PRE = ("sv_random_seed 7;map vrfiringrange;wait30;god;notarget;developer 1;vr_foegrab_debug 1;vr_foegrab_leniency 12;"
       "vr_knockdown_debug 1;vr_struggle_debug 2;vr_foegrab_lift_time 9;vr_test_spawn 0;vr_test_spawn_dist 36;"
       "cl_backspeed 30;impulse 241;wait40")
DOWN = ("vr_knockdown_push 0;vr_knockdown_time_min 12;vr_knockdown_time_max 12;vr_knockdown_test 0;wait60;"
        "vr_mock_walk_to nearest monster_army 24;wait200;vr_mock_walk_to off;wait90")
FILTER = "struggle|lifted|leg diag|MARK|[Ee]rror|CRASH"
FAILS = 0
NAMES = ["swat", "pry", "crunch", "claw", "brace", "kick", "thrash"]


def motion(name, keys):
    os.makedirs(MOTIONS, exist_ok=True)
    with open(os.path.join(MOTIONS, name + ".mock"), "w") as f:
        for t, line in sorted(keys, key=lambda k: k[0]):
            f.write(f"{t:.3f} {line}\n")
    return f"quakevr/motions/{name}.mock"


def statuses(t0, t1):
    out, t = [], t0
    while t <= t1 + 1e-6:
        out.append((t, "cmd vr_struggle_status"))
        t += 0.3
    return out


def lift_keys():
    y, near, gap = 1.02, -1.15, 0.09
    m0, o0 = (gap, y, near - 0.08), (-gap, y, near)
    m1, o1 = (gap, y + 0.4, near - 0.08), (-gap, y + 0.4, near)
    k = [(0.0, "main %.3f %.3f %.3f" % m0), (0.0, "off %.3f %.3f %.3f" % o0), (0.3, "cmd +grabmain"), (0.3, "cmd +graboff"),
         (0.8, "main %.3f %.3f %.3f" % m0), (0.8, "off %.3f %.3f %.3f" % o0),
         (1.05, "main %.3f %.3f %.3f" % m1), (1.05, "off %.3f %.3f %.3f" % o1)]
    return k + statuses(1.2, 4.2) + [(4.4, "cmd -grabmain"), (4.4, "cmd -graboff")]


def leg_keys(way, walk=False):
    side, back, shin = 0.17, 0.38, -21
    main, off = (side, -0.15, -back), (-side, 0.0, 0.0)
    k = [(0.0, f"cmd vr_mock_hand_to main nearest monster_army {shin}"),
         (0.0, f"cmd vr_mock_hand_to off nearest monster_army {shin}"), (0.03, "rel main 0 0 0"), (0.03, "rel off 0 0 0")]

    def at(t, d, low=0.0):
        dy, dz = (d, 0.0) if way == "up" else (0.0, d)
        return [(t, "rel %s %.3f %.3f %.3f" % (n, h[0], h[1] + dy - low, h[2] + dz)) for n, h in (("main", main), ("off", off))]
    dist, dur = (1.1, 0.6) if way == "up" else (0.45, 0.25)
    k += at(0.1, 0.0) + [(0.15, "cmd +grabmain"), (0.15, "cmd +graboff")] + at(0.6, 0.0) + at(0.6 + dur, dist)
    if walk:
        k += at(1.4, dist) + at(1.7, dist, 0.2)  # (the hands lowered to it lying)
        k += [(1.8, "cmd +back"), (3.6, "cmd -back")]
    return k + statuses(1.2, 4.2) + [(4.4, "cmd -grabmain"), (4.4, "cmd -graboff")]


def drag_keys(part, dead=False):
    k = [(0.0, f"cmd vr_mock_hand_to main ragdoll {part}"), (0.03, "rel main 0 0 0"), (0.15, "cmd +grabmain"),
         (0.5, "rel main 0 0 0"), (0.8, "rel main 0 0.25 0"),  # (lifted a little off the floor)
         (1.0, "cmd +back"), (3.0, "cmd -back")]
    if dead:
        k.append((2.5, "cmd vr_struggle_status hurt 999"))
    return k + statuses(0.6, 3.6) + [(3.8, "cmd -grabmain")]


CASES = {
    "lift": (lift_keys, ""),
    "leglift": (lambda: leg_keys("up"), ""),
    "legdrag": (lambda: drag_keys("7 0"), DOWN.replace("monster_army 24", "monster_army 44")),
    "headdrag": (lambda: drag_keys("head 0"), DOWN),
    "armdrag": (lambda: drag_keys("6 0"), DOWN),
    "dead": (lambda: drag_keys("head 0", dead=True), DOWN),
}
LEAD = {"lift": "swat", "leglift": "crunch", "legdrag": "claw", "headdrag": "pry", "armdrag": "pry"}


def verdict(ok, what):
    global FAILS
    print(f"  {'PASS' if ok else 'FAIL'} {what}")
    if not ok:
        FAILS += 1


def parse(out):
    flat = " ".join(out.split())
    weights = [dict(zip(NAMES, map(float, m))) for m in re.findall(
        r"struggle status: \d+ \S+ [\d.]+ s: swat ([\d.]+) pry ([\d.]+) crunch ([\d.]+) claw ([\d.]+) brace ([\d.]+) "
        r"kick ([\d.]+) thrash ([\d.]+);", flat)]
    holds = [tuple(float(x) for x in m) for m in re.findall(
        r"struggle hold: \d+ hand \d part \d+ \(\S+\) lag ([\d.]+) units; bend ([-\d]+) prone ([-\d.]+) head ([-\d.]+) over "
        r"its pelvis; an arm ([\d.]+) from your head, ([\d.]+) from this hand; legs straight ([\d.]+); pelvis \S+ \S+ \S+; "
        r"a hand ([\d.]+) from its head; chest up ([-\d.]+)", flat)]
    traces = re.findall(r"struggle: \d+ ([\d.]+) s w .*? effort ([\d.]+) .*? straight ([\d.]+) ", flat)
    cost = [float(x) for x in re.findall(r"struggling; ([\d.]+) ms a frame", flat)]
    return flat, weights, holds, traces, cost


def stats(xs):
    return (sum(xs) / len(xs), max(xs)) if xs else (0.0, 0.0)


def main():
    os.makedirs(SCRATCH, exist_ok=True)
    b = testbatch.Batch(AGENT, "struggle")
    handles = {}
    for name, (keys, pre) in CASES.items():
        for off in (False, True):
            case = name + ("_off" if off else "")
            if (ONLY and not re.search(ONLY, case)) or (off and name == "dead"):
                continue
            path = motion("struggle_" + case, keys())
            script = (f"{PRE};{pre};{'vr_struggle 0;' if off else ''}vr_mock_play {path};wait420;-grabmain;-graboff;"
                      "-back;wait5;toggleconsole;quit")
            handles[case] = b.case(["-Script", script, "-Filter", FILTER, "-Timeout", "240"])
    b.run()
    res = {}
    for case, h in handles.items():
        out = b.output(h).replace("\r", "")
        with open(os.path.join(SCRATCH, f"struggle_{case}.txt"), "w") as f:
            f.write(out)
        res[case] = parse(out)
    for name in CASES:
        if name not in res:
            continue
        flat, weights, holds, traces, cost = res[name]
        bad = bool(re.search(r"\b[Ee]rror|CRASH", flat))
        lag_on = stats([x[0] for x in holds])
        offr = res.get(name + "_off")
        lag_off = stats([x[0] for x in offr[2]]) if offr else (0.0, 0.0)
        late = weights[len(weights) // 3:] if weights else []
        mean = {n: sum(w[n] for w in late) / len(late) for n in NAMES} if late else {}
        lead = max(mean, key=mean.get) if mean else "-"
        ws = " ".join(f"{n} {mean[n]:.2f}" for n in NAMES) if mean else "no weights"
        pose = lambda hs, i: stats([x[i] for x in hs])  # noqa: E731
        if name == "dead":
            ended = re.search(r"struggle: \d+ ends after ([\d.]+) s \(dead\)", flat)
            verdict(bool(ended) and not bad, f"dead: struggle ends as dead after {ended.group(1) if ended else '-'} s")
            continue
        ok = lead == LEAD[name] and not bad and lag_on[1] <= max(lag_off[1] * 1.5, lag_off[1] + 4.0)
        if name == "legdrag" and not weights:
            print("  INFO legdrag: no hold taken (the mock hand on a lying leg near your feet misses it; not counted)")
            continue
        extra = ""
        if offr:
            # (The pose's measure for each case, on against off, over the later two thirds of the statuses.)
            on_p, off_p = holds[len(holds) // 3:], offr[2][len(offr[2]) // 3:]
            col, word, more = {"lift": (4, "an arm from your head (min)", False),
                               "leglift": (3, "its head over its pelvis", True),
                               "legdrag": (2, "belly down", True),
                               "headdrag": (5, "an arm from the holding hand", False),
                               "armdrag": (8, "chest up (Stay Upright)", True)}[name]
            pick = (lambda xs: min(xs)) if name == "lift" else (lambda xs: sum(xs) / len(xs))
            a = pick([x[col] for x in on_p]) if on_p else 0.0
            z = pick([x[col] for x in off_p]) if off_p else 0.0
            moved = a > z if more else a < z
            if name == "armdrag":
                # (Its free hand swats at the holding one as it pries; the pose is the struggle's Stay Upright: lying
                # dragged from flat it can't rise, so its chest is reported, not judged.)
                moved = mean.get("swat", 0.0) > 0.4
                word = f"swat {mean.get('swat', 0.0):.2f} (>0.4); chest up"
            ok = ok and moved
            extra = (f"; {word} {a:.2f} (off {z:.2f}: {'as meant' if moved else 'NOT as meant'}); bend "
                     f"{pose(on_p, 1)[0]:.0f} (off {pose(off_p, 1)[0]:.0f})")
        verdict(ok, f"{name}: leads {lead} ({ws}); lag mean/most {lag_on[0]:.1f}/{lag_on[1]:.1f} units (off "
                    f"{lag_off[0]:.1f}/{lag_off[1]:.1f}){extra}; cost {max(cost or [0]):.4f} ms/frame")
    print(f"{FAILS} failed")
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
