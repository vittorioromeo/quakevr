#!/bin/bash
# gib_2h_throw_test.sh <agent> [out dir] [extra console commands] -- gibs thrown with both hands (ROUND21.md, "Two-handed
# gib throws that burst"; NOTES.md vrfiringrange_2026-10-02_15-38-26): a gib (impulse 252, destroyable:
# vr_test_held_destroy 1) or a small gib (vr_smallgibs_test 9) taken in the off hand, then the main hand, and thrown by
# both along several arcs (both hands let go together, or the off hand 40 ms first). Prints one line a throw:
#   <throw> <kind>: burst <0|1> (what burst it), its speed as it left
# and the totals. The hands are 14 cm apart (HALF), the gib between them. KINDS="gib0 gib2 gib3 ...": that model of
# impulse 252 (vr_test_held_pick: 0 gib1, 1 h_player, 2 gib2 (the torso), 3 gib3 (the big chunk), 4-8 heads).
# CFG=his.cfg: exec'd first (a config in the game folder: the author's settings, his gib masses).
# STAGGERS ("0 1"): 0 both let go together, 1 the off hand 40 ms first, 2 the main hand 22 ms first, 3 the off hand a
# frame (11 ms) first. LAG: the off hand's path that much later (s); OSCALE: its length times this (the hands apart).
#   push     from the chest straight ahead, 5 degrees up, 6 m/s at the peak
#   hard     the same at 9 m/s
#   up       from the chest 50 degrees up, 5 m/s
#   vertical from the chest straight up (80 degrees), 4 m/s
#   over     overhead (a throw-in): the arms from behind the head (150 degrees) to 40, let go at 95
#   under    underarm: -120 to -30, let go at -60
#   down     overhead down across the body (100 to -10, let go at 30: a wrist-snap's path, into the chest)
#   back     (not a throw: THROWS=back) from arm's length back into the chest, 7 m/s (the body's collision)
AGENT=$1; OUT=${2:-$(mktemp -d)}; EXTRA=${3:-}; HERE=$(cd "$(dirname "$0")" && pwd -W 2>/dev/null || pwd)
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd -W 2>/dev/null || pwd)
python - "$HERE" "$OUT" <<'PY'
import sys, math, importlib.util
spec = importlib.util.spec_from_file_location("tp", sys.argv[1] + "/throw_plays.py"); tp = importlib.util.module_from_spec(spec); spec.loader.exec_module(tp)
import os
HALF = float(os.environ.get("HALF", "0.07"))  # each hand's x off the middle (HALF=0.03: a small gib)
def line(elev, peak, duration, p0=(0.0, 1.25, -0.25)):
    keys, rel = [], 0.5 * duration
    n = int(duration * tp.RATE); length = peak * duration / 1.875; e = math.radians(elev)
    for i in range(n + 1):
        s = i / n; d = length * tp.minjerk(s)
        keys.append([s * duration, p0[0], p0[1] + d * math.sin(e), p0[2] - d * math.cos(e), 0.0])
    return keys, rel
def arc(a0, a1, arel, duration, radius=0.55):
    keys, rel = tp.arc_throw("x", a0, a1, arel, 0, 0, duration, radius)
    for k in keys:
        k[1] = 0.0
    return keys, rel
throws = {
    "push": line(5, 6.0, 0.30), "hard": line(5, 9.0, 0.25), "up": line(50, 5.0, 0.30), "vertical": line(80, 4.0, 0.30),
    "over": arc(150, 40, 95, 0.35), "under": arc(-120, -30, -60, 0.35), "down": arc(100, -10, 30, 0.30),
    "back": line(170, 7.0, 0.25, (0.0, 1.25, -0.65)),
}
LAG = float(os.environ.get("LAG", "0"))      # the off hand's path this much later (s): the hands not quite together
OSCALE = float(os.environ.get("OSCALE", "1"))  # and its path's length times this (a weaker arm)
for name, (keys, rel) in throws.items():
    for stagger in (0, 1, 2, 3):
        L = []
        for hand, sx in (("main", 1), ("off", -1)):
            k0 = keys[0]
            lag, sc = (LAG, OSCALE) if hand == "off" else (0.0, 1.0)
            L.append(f"0.000 {hand} {k0[1] + sx * HALF:.4f} {k0[2]:.4f} {k0[3]:.4f} {k0[4] + 70:.2f} 0 0")
            L += [f"{0.3 + lag + k[0]:.4f} {hand} {k0[1] + (k[1] - k0[1]) * sc + sx * HALF:.4f} {k0[2] + (k[2] - k0[2]) * sc:.4f} "
                  f"{k0[3] + (k[3] - k0[3]) * sc:.4f} {k[4] + 70:.2f} 0 0" for k in keys]
        # 0 together, 1 the off hand 40 ms first, 2 the main hand 22 ms first, 3 the off hand a frame (11 ms) first
        offrel = rel - {0: 0.0, 1: 0.04, 2: -0.022, 3: 0.011}[stagger]
        L += [f"{0.3 + offrel:.4f} cmd -graboff", f"{0.3 + offrel:.4f} button off grip 0"]
        L += [f"{0.3 + rel:.4f} cmd -grabright", f"{0.3 + rel:.4f} button main grip 0"]
        open(f"{sys.argv[2]}/{name}{stagger}.txt", "w", newline="\n").write("\n".join(L) + "\n")
        if stagger == 0:
            k0 = keys[0]
            open(f"{sys.argv[2]}/{name}.start", "w").write(f"{k0[1] - HALF:.4f} {k0[2]:.4f} {k0[3]:.4f} {k0[4] + 70:.2f} 0 0|"
                                                         f"{k0[1] + HALF:.4f} {k0[2]:.4f} {k0[3]:.4f} {k0[4] + 70:.2f} 0 0")
PY
SUMMARY="$OUT/summary.txt"; : > "$SUMMARY"
run() { # <throw> <stagger 0|1> <kind: gib|small|gib<pick>: vr_test_held_pick's model>
    local start; start=$(cat "$OUT/$1.start"); local offp=${start%%|*} mainp=${start##*|}
    local take="impulse 252"
    [ "$3" = small ] && take="vr_smallgibs_test 9"
    case $3 in gib[0-9]*) take="vr_test_held_pick ${3#gib};impulse 252";; esac
    local cfg=""; [ -n "$CFG" ] && cfg="exec $CFG;"
    bash $KIT/run.sh $AGENT -Script "map vrfiringrange;wait60;${cfg}god;notarget;developer 1;vr_test_held_destroy 1;vr_debug_box3d 1;$EXTRA;vr_mock_hand off $offp;vr_mock_hand main $mainp;wait10;+graboff;vr_mock_button off grip 1;$take;wait20;+grabright;vr_mock_button main grip 1;wait20;vr_mock_play $OUT/$1$2.txt;wait200;toggleconsole;quit" \
        -Filter "carry:|gib: |test: |throw: two|box3d: [0-9]+ .*thrown by|throw (grace|watched)|blow|smallgib: a gib burst|sgibtest: held" > "$OUT/$1$2_$3.log" 2>&1
    local both=$(grep -c "carry: both hands$" "$OUT/$1$2_$3.log")
    local burst=$(grep -c "gib: burst" "$OUT/$1$2_$3.log")
    local why=$(grep -m1 -B1 "gib: burst" "$OUT/$1$2_$3.log" | head -1 | cut -c1-110)
    local thrown=$(grep -m1 "gib: thrown at\|thrown by" "$OUT/$1$2_$3.log" | cut -c1-90)
    echo "$1$2 $3: both $both burst $burst ($why) | $thrown" | tee -a "$SUMMARY"
}
for kind in ${KINDS:-gib small}; do
    for t in ${THROWS:-push hard up vertical over under down}; do
        for s in ${STAGGERS:-0 1}; do run $t $s $kind; done
    done
done
echo "total bursts: $(grep -c "burst [1-9]" "$SUMMARY") of $(wc -l < "$SUMMARY") throws ($(grep -c "both 0" "$SUMMARY") not held in both)"
