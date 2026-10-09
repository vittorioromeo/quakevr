#!/bin/bash
# throw_2h_damage_test.sh <agent> [out dir] [extra console commands] -- two-handed throws hurt more (ROUND21.md,
# "Two-handed throws hurt more"; NOTES.md vrfiringrange_2026-10-09_15-44-39): the same push (prop_2h_test.sh's: both
# hands straight ahead, 5 degrees up, PEAK m/s (6) at the peak, let go at it) at an ogre TARGET units ahead, with the
# thing held in the main hand alone (hands 1) and in both (hands 2). vr_throw_2h_strength 1 and vr_2h_throw_velocity_mult
# 1 (both hands give no extra speed), so the two throws hit at the same speed: the two-handed one's damage
# vr_throw_2h_damage (1.25) times the other's. KINDS: 101 a box of shells (vr_test_spawn 101 put in the main hand, the
# off hand 10 cm beside it), shotgun (impulse 154 into the gripping main hand, the off hand on its foregrip: vr_debug_2h_grip
# says where). Prints per thing and hands: whether both held it, the hit's speed, damage (before where) and the 2h
# multiplier, from the throw hit's debug line.
AGENT=$1; OUT=${2:-C:/OHWorkspace/qvr-agents/$1/scratch/throw2hdmg}; EXTRA=${3:-}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd -W 2>/dev/null || pwd)
plays() { # <name> <main x y z> <off x y z>: push1/push2 play files from those starts (tracking space, metres)
python - "$OUT" "$@" <<'PY'
import sys, math, os
out, name = sys.argv[1], sys.argv[2]; main = [float(v) for v in sys.argv[3:6]]; off = [float(v) for v in sys.argv[6:9]]
PEAK = float(os.environ.get("PEAK", "6")); RATE = 250.0; dur = 0.30; rel = 0.5 * dur; t0 = 0.3
def minjerk(s): return s * s * s * (10 - 15 * s + 6 * s * s)
length = PEAK * dur / 1.875; e = math.radians(5); n = int(dur * RATE)
for hands in (1, 2):
    L = []
    for hand, p in (("main", main), ("off", off)):
        for i in range(n + 1):
            d = length * minjerk(i / n)
            L.append(f"{t0 + i / n * dur:.4f} {hand} {p[0]:.4f} {p[1] + d * math.sin(e):.4f} {p[2] - d * math.cos(e):.4f} 70.00 0 0")
    if hands == 2:
        L += [f"{t0 + rel:.4f} cmd -graboff", f"{t0 + rel:.4f} button off grip 0"]
    L += [f"{t0 + rel:.4f} cmd -grabright", f"{t0 + rel:.4f} button main grip 0"]
    open(f"{out}/{name}_push{hands}.txt", "w", newline="\n").write("\n".join(sorted(L, key=lambda l: float(l.split()[0]))) + "\n")
PY
}
TARGET=${TARGET:-120}
run() { # <kind> <hands 1|2>
    local main off take
    if [ "$1" = shotgun ]; then
        main="0.15 1.25 -0.3"; off="0.09 1.2 -0.645"; take="+grabright;vr_mock_button main grip 1;impulse 9;wait2;impulse 154;wait10"
    else
        main="0.05 1.25 -0.3"; off="-0.05 1.25 -0.3"
        take="+grabright;vr_mock_button main grip 1;wait5;vr_test_spawn $1;vr_test_spawn_dist 24;vr_test_spawn_hold 1;impulse 241;wait10"
    fi
    plays "$1" $main $off
    local ogre="vr_test_spawn_hold 0;vr_test_spawn ${TKIND:-1};vr_test_spawn_dist $TARGET;impulse 241;wait5;"
    local both=""; [ "$2" = 2 ] && both="+graboff;vr_mock_button off grip 1;"
    local log="$OUT/$1_push$2.log"
    bash $KIT/run.sh $AGENT -Script "map vrfiringrange;wait60;god;notarget;developer 1;vr_fixed_frames 1;vr_throw_2h_strength 1;vr_2h_throw_velocity_mult 1;vr_debug_2h_grip 1;$EXTRA;vr_mock_hand main $main 70 0 0;vr_mock_hand off $off 70 0 0;wait10;${ogre}$take;${both}wait20;vr_mock_play $OUT/$1_push$2.txt;wait150;toggleconsole;quit" \
        -Timeout 300 -Filter "carry: both|throw hit|thrown into| hands [12]:|throw: two|2h grip: off hand took" > "$log" 2>&1
    local nboth=$(grep -c "carry: both hands$\|2h grip: off hand took" "$log")
    local hit=$(grep -m1 -o "thrown into [a-z_]* at [0-9.]* u/s ([0-9.]* m/s against it): [0-9.]* (hands [12]: x[0-9.]*)" "$log")
    [ -z "$hit" ] && hit=$(grep -m1 -A1 "throw hit: .* into monster" "$log" | tr -d '\n' | grep -o "into [a-z_]* at [0-9]* u/s.*" | cut -c1-150)
    echo "$1 hands $2: held in both $nboth | ${hit:-no hit}"
}
for kind in ${KINDS:-101 shotgun}; do
    for h in 1 2; do run $kind $h; done
done
