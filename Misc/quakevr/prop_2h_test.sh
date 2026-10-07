#!/bin/bash
# prop_2h_test.sh <agent> [out dir] [extra console commands] -- a barrel or a crate held in both hands (mock hands), thrown
# or swung (ROUND21.md, "Heavy props thrown and slammed"; NOTES.md vrfiringrange_2026-10-07_23-00-55 and 23-08-07).
# In the firing range (god, notarget): an ogre TARGET units ahead (0: none), the prop (vr_test_spawn 111 a barrel, 107 a
# small crate, 108 a large one: KINDS="111 107") put in the main hand, the off hand taking it too, then one motion:
#   push     from the chest straight ahead, 5 degrees up, PEAK m/s (6) at the peak, let go at it: at the ogre
#   over     overhead throw: the arms from behind the head (150 degrees) to 40, let go at 95
#   slam     lifted overhead (the hands 30 cm over the eyes), then swung down and forward onto what is ahead (the
#            ogre, or the floor) at about PEAK m/s, held on
#   down     lifted overhead, then thrown down onto the floor ahead (let go halfway)
#   low      the same swing down from the chest, not overhead (no slam: the control)
# Prints one line a run: the prop, the motion, what it hit and for how much, whether it broke (the console's lines).
# REPEAT=n: the motion n times on the same prop (a crate thrown down again: how many throws break it).
AGENT=$1; OUT=${2:-C:/OHWorkspace/qvr-agents/$1/scratch/prop2h}; EXTRA=${3:-}; HERE=$(cd "$(dirname "$0")" && pwd -W 2>/dev/null || pwd)
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd -W 2>/dev/null || pwd)
python - "$HERE" "$OUT" <<'PY'
import sys, math, os, importlib.util
spec = importlib.util.spec_from_file_location("tp", sys.argv[1] + "/throw_plays.py"); tp = importlib.util.module_from_spec(spec); spec.loader.exec_module(tp)
HALF = float(os.environ.get("HALF", "0.25"))  # each hand's x off the middle (m)
PEAK = float(os.environ.get("PEAK", "6"))
REPEAT = int(os.environ.get("REPEAT", "1"))
RATE = 250.0
def path(points, duration):
    # min-jerk through a straight line from points[0] to points[1] (x, y, z, pitch)
    n = int(duration * RATE); a, b = points; keys = []
    for i in range(n + 1):
        s = tp.minjerk(i / n); keys.append([i / n * duration] + [a[k] + (b[k] - a[k]) * s for k in range(4)])
    return keys
CHEST = (0.0, 1.25, -0.30, 0.0)
OVER = (0.0, 2.00, -0.15, 30.0)
def line(elev, peak, duration):
    length = peak * duration / 1.875; e = math.radians(elev)
    return path([CHEST, (0.0, CHEST[1] + length * math.sin(e), CHEST[2] - length * math.cos(e), 0.0)], duration), 0.5 * duration
def arc(a0, a1, arel, duration, radius=0.55):
    keys, rel = tp.arc_throw("x", a0, a1, arel, 0, 0, duration, radius)
    return [[k[0], 0.0, k[2], k[3], k[4]] for k in keys], rel
def lifted(motion):
    # chest -> overhead (0.6 s), a pause, then the motion from overhead
    up = path([CHEST, OVER], 0.6)
    keys = up + [[0.75] + list(OVER)]
    m, rel = motion
    return keys + [[0.75 + k[0]] + k[1:] for k in m], (None if rel is None else 0.75 + rel)
down_len = 1.1  # m: overhead to the floor ahead, roughly
slam_end = (0.0, 0.70, -0.75, -60.0)
throws = {
    "push": line(5, PEAK, 0.30),
    "over": arc(150, 40, 95, 0.35),
    "slam": lifted((path([OVER, slam_end], down_len * 1.875 / PEAK), None)),
    "down": lifted((path([OVER, slam_end], down_len * 1.875 / PEAK), 0.5 * down_len * 1.875 / PEAK)),
    "low": (path([CHEST, slam_end], 0.8 * 1.875 / PEAK), None),
}
for name, (keys, rel) in throws.items():
    L = []
    t0 = 0.3
    for r in range(REPEAT):
        for hand, sx in (("main", 1), ("off", -1)):
            L += [f"{t0 + k[0]:.4f} {hand} {k[1] + sx * HALF:.4f} {k[2]:.4f} {k[3]:.4f} {k[4] + 70:.2f} 0 0" for k in keys]
        end = t0 + keys[-1][0]
        if rel is not None:
            L += [f"{t0 + rel:.4f} cmd -graboff", f"{t0 + rel:.4f} button off grip 0",
                  f"{t0 + rel:.4f} cmd -grabright", f"{t0 + rel:.4f} button main grip 0"]
        if r + 1 < REPEAT:
            # back to the chest, then (the prop lying ahead) the next round takes it again: impulse 252's pick 13
            L += [f"{end + 1.5:.4f} cmd echo AGAIN {r + 2}", f"{end + 2.5:.4f} cmd vr_test_held_pick 13;impulse 252",
                  f"{end + 2.7:.4f} cmd +graboff", f"{end + 2.7:.4f} button off grip 1",
                  f"{end + 2.9:.4f} cmd +grabright", f"{end + 2.9:.4f} button main grip 1"]
            for hand, sx in (("main", 1), ("off", -1)):
                L.append(f"{end + 2.0:.4f} {hand} {CHEST[0] + sx * HALF:.4f} {CHEST[1]:.4f} {CHEST[2]:.4f} {CHEST[3] + 70:.2f} 0 0")
            t0 = end + 3.2
    open(f"{sys.argv[2]}/{name}.txt", "w", newline="\n").write("\n".join(sorted(L, key=lambda l: float(l.split()[0]))) + "\n")
    open(f"{sys.argv[2]}/{name}.len", "w").write(f"{t0 + keys[-1][0] + 3:.2f}")
    k0 = keys[0]
    open(f"{sys.argv[2]}/{name}.start", "w").write(f"{k0[1] - HALF:.4f} {k0[2]:.4f} {k0[3]:.4f} {k0[4] + 70:.2f} 0 0|"
                                                 f"{k0[1] + HALF:.4f} {k0[2]:.4f} {k0[3]:.4f} {k0[4] + 70:.2f} 0 0")
PY
TARGET=${TARGET:-120}
run() { # <motion> <kind: vr_test_spawn 111 107 108>
    local start; start=$(cat "$OUT/$1.start"); local offp=${start%%|*} mainp=${start##*|}
    local len; len=$(cat "$OUT/$1.len"); local frames; frames=$(python -c "print(int($len * 90) + 60)")
    local ogre=""; [ "$TARGET" != 0 ] && ogre="vr_test_spawn 1;vr_test_spawn_dist $TARGET;impulse 241;wait5;"
    bash $KIT/run.sh $AGENT -Script "map vrfiringrange;wait60;god;notarget;developer 1;vr_fixed_frames 1;$EXTRA;vr_mock_hand off $offp;vr_mock_hand main $mainp;wait10;${ogre}+grabright;vr_mock_button main grip 1;wait5;vr_test_spawn $2;vr_test_spawn_dist 24;vr_test_spawn_hold 1;impulse 241;wait10;+graboff;vr_mock_button off grip 1;wait10;vr_mock_play $OUT/$1.txt;wait$frames;toggleconsole;quit" \
        -Timeout 300 -Filter "carry:|test|thrown into|crate: |crate at|slam|AGAIN|test spawn|prop: flung|melee|T_Damage|ogre" > "$OUT/$1_$2.log" 2>&1
    local both=$(grep -c "carry: both hands$" "$OUT/$1_$2.log")
    local hits=$(grep -o "thrown into [a-z_]* at [0-9.]* u/s ([0-9.]* m/s against it): [0-9.]*\|slam[^,]*: [^\n]*" "$OUT/$1_$2.log" | cut -c1-90 | tr '\n' ';')
    local broke=$(grep -c "crate: breaks" "$OUT/$1_$2.log")
    local dmg=$(grep -o "crate at [^:]*: hit [a-z_]* at [0-9.]* m/s: [0-9.]*\|hit by [a-z_]* for [0-9.]*" "$OUT/$1_$2.log" | sed 's/crate at [^:]*: //' | tr '\n' ';' | cut -c1-200)
    echo "$2 $1: both $both | $hits | prop damaged: $dmg | broke $broke"
}
for kind in ${KINDS:-111}; do
    for m in ${MOTIONS:-push}; do run $m $kind; done
done
