#!/bin/bash
# throw_spin_test.sh <agent> [out dir] -- a thrown axe spins about the axis of the flick that threw it (ROUND21.md, "A
# thrown axe's spin: the runtime's angular velocity frame"). The axe held upright in the main hand, flicked down in the
# vertical plane ahead (the wrist from 10 degrees past upright to 30 up, the arm 12 cm down and forward), let go at 55%
# of the flick, facing the play space's front (yaw 0) and turned 90 and 180 degrees in it. Each run prints the "thrown
# spin:" line (developer 1): how far its spin's axis is off end over end, its tip going down (about the hand's left
# axis: 0; about the handle or a cartwheel: 90; backwards: 180). vr_mock_angvel_local 1 reports the angular velocity in
# the controller's frame, as VirtualDesktopXR 1.0.10 does: with vr_throw_spin_from_pose 0 (before the fix) a flick
# turned 90 degrees in the play space spun the axe as a cartwheel, turned 180 backwards; with 1 (the default) every
# case is within a few degrees.
# Prints "PASS" when every case with vr_throw_spin_from_pose 1 is under 10 degrees off.
AGENT=$1; OUT=${2:-$(mktemp -d)}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd -W 2>/dev/null || pwd)
python - "$OUT" <<'PY'
import sys, math
GA, RATE = 70.0, 90.0  # vr_gunangle 70: the controller pitched up 70 degrees from the hand
def mj(s): return s * s * s * (10 - 15 * s + 6 * s * s)
for yaw in (0, 90, 180):
    c, s_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    def at(x, z): return x * c + z * s_, -x * s_ + z * c  # turned left by yaw about the vertical (tracking: -z ahead)
    def key(t, y, x, z, p):
        X, Z = at(x, z)
        return f"{t:.4f} main {X:.4f} {y:.4f} {Z:.4f} {p + GA:.2f} {yaw} 0"
    hx, hz = at(0, 0)
    L = [f"0.000 head {hx:.3f} 1.6 {hz:.3f} 0 {yaw} 0", key(0, 1.50, 0.2, -0.30, 100)]
    dur, n = 0.25, int(0.25 * RATE)
    for i in range(n + 1):
        u = mj(i / n)
        L.append(key(0.3 + i / n * dur, 1.50 - 0.12 * u, 0.2, -0.30 - 0.12 * u, 100 - 70 * u))
    rel = 0.3 + 0.55 * dur
    L += [f"{rel:.4f} cmd -grabright", f"{rel:.4f} button main grip 0"]
    open(f"{sys.argv[1]}/spin_yaw{yaw}.txt", "w", newline="\n").write("\n".join(L) + "\n")
PY
fails=0
for local in 0 1; do
    for frompose in 0 1; do
        for yaw in 0 90 180; do
            play="$OUT/spin_yaw$yaw.txt"
            head=$(head -1 "$play" | cut -d' ' -f3-); start=$(sed -n 2p "$play" | cut -d' ' -f3-)
            line=$(bash $KIT/run.sh $AGENT -Script "map vrfiringrange;wait60;god;notarget;developer 1;vr_gunangle 70;vr_gunyaw 0;vr_weapon_grip_mode 0;vr_mock_grip_velocity 1;vr_mock_angvel_local $local;vr_throw_spin_from_pose $frompose;vr_mock_hand head $head;vr_mock_hand main $start;wait10;impulse 9;wait5;+grabright;vr_mock_button main grip 1;impulse 152;wait40;vr_mock_play $play;wait100;toggleconsole;quit" -Filter "^thrown spin:" 2>&1 | grep "^thrown spin:" | head -1)
            off=$(echo "$line" | sed -n 's/.* rad\/s, \([0-9.]*\) deg off.*/\1/p')
            echo "local $local from_pose $frompose yaw $yaw: $line"
            if [ "$frompose" = 1 ]; then
                python -c "import sys; sys.exit(0 if '$off' and float('$off') < 10 else 1)" || fails=$((fails + 1))
            fi
        done
    done
done
[ $fails = 0 ] && echo PASS || echo "FAIL ($fails)"
