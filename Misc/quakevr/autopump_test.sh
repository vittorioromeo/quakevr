#!/bin/bash
# autopump_test.sh <agent> -- the shotgun's auto pump (vr_autopump.cpp; ROUND21.md, "Shotgun auto pump"), headless: the
# shotgun in the main hand (impulse 154), one shot (+attack), vr_debug_weaponfx 1 printing the stroke's start, back and
# home and the spent shell's eject:
#   1. at 72 and 120 Hz (vr_fixed_frames_rate): the shell leaves in the frame the stroke reaches the back (within 15 ms of
#      it), the stroke home on time (0.3 s), long before the shotgun can fire again (0.5 s);
#   2. in bullet time at 0.25x: the same game times, four times as long in real time (the stroke is game time);
#   3. Auto Pump off: no stroke, the shell 0.22 s after the shot as before;
#   4. the off hand's shotgun (mirrored): the same.
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
SETUP="map e1m1;wait60;vr_weapon_grip_mode 1;impulse 9;wait2;impulse 154;wait3;vr_mock_hand main 0.15 1.2 -0.45 70 0 0;wait10"
FIRE="vr_debug_weaponfx 1;+attack;wait3;-attack;wait90"
run() { bash $KIT/run.sh $AGENT -Script "$1;toggleconsole;quit" -Filter "^autopump hand|^shells eject|^weaponfx fired" 2>&1; }
# The numbers: the stroke's start, back, home and the eject (game and real times).
stroke() {
    echo "$1" | awk '
        /autopump hand [01] start/ { s = $6; sr = $8; sub(/\):?/, "", sr) }
        /autopump hand [01] back/ { b = $6; br = $8; sub(/;/, "", br); bd = $10; sub(/\),?/, "", bd) }
        /autopump hand [01] home/ { h = $6; hr = $8; sub(/;/, "", hr); hd = $10; sub(/\)/, "", hd) }
        /shells eject/ { e = $6 }
        END { printf "%s %s %s %s %s %s %s %s %s\n", s, sr, b, br, bd, h, hr, hd, e }'
}

for rate in 72 120; do
    log=$(run "vr_fixed_frames_rate $rate;$SETUP;$FIRE")
    read s sr b br bd h hr hd e <<< "$(stroke "$log")"
    echo "  $rate Hz: shot $s, back $b (due $bd), shell $e, home $h (due $hd)"
    check $(awk -v e="$e" -v bd="$bd" 'BEGIN { print (e != "" && e >= bd && e - bd < 0.015) ? 1 : 0 }') "$rate Hz: the shell leaves as the fore-end reaches the back"
    check $(awk -v b="$b" -v e="$e" 'BEGIN { print (b != "" && b == e) ? 1 : 0 }') "$rate Hz: in the same frame"
    check $(awk -v s="$s" -v h="$h" 'BEGIN { d = h - s; print (s != "" && d >= 0.3 && d < 0.32) ? 1 : 0 }') "$rate Hz: home 0.3 s after the shot, before the next is possible (0.5 s)"
done

log=$(run "$SETUP;vr_bullettime_scale 0.25;vr_bullettime_duration 60;vr_bullettime;wait30;$FIRE")
read s sr b br bd h hr hd e <<< "$(stroke "$log")"
echo "  bullet time: game $s -> $h, real $sr -> $hr"
check $(awk -v s="$s" -v h="$h" -v sr="$sr" -v hr="$hr" 'BEGIN { g = h - s; r = hr - sr; print (s != "" && g < 0.32 && r > 3.5 * g && r < 4.5 * g) ? 1 : 0 }') "bullet time 0.25x: the stroke takes 0.3 s of game time, four times that in real time"
check $(awk -v e="$e" -v bd="$bd" 'BEGIN { print (e != "" && e >= bd && e - bd < 0.015) ? 1 : 0 }') "bullet time: the shell leaves at the back"

log=$(run "$SETUP;vr_autopump 0;vr_debug_weaponfx 1;+attack;wait3;-attack;wait90")
f=$(echo "$log" | awk '/weaponfx fired/ { print $4; exit }')
e=$(echo "$log" | awk '/shells eject/ { print $6; exit }')
echo "  off: shot $f, shell $e"
check $(echo "$log" | grep -q "^autopump hand" && echo 0 || echo 1) "Auto Pump off: no stroke"
check $(awk -v f="$f" -v e="$e" 'BEGIN { d = e - f; print (f != "" && e != "" && d >= 0.22 && d < 0.24) ? 1 : 0 }') "Auto Pump off: the shell 0.22 s after the shot, as before"

log=$(run "$SETUP;vr_test_weaponinst 7;impulse 120;wait3;vr_mock_hand off -0.15 1.2 -0.45 70 0 0;vr_mock_hand main 0.3 1.0 -0.2 0 0 0;wait10;vr_debug_weaponfx 1;+offhandattack;wait3;-offhandattack;wait90")
read s sr b br bd h hr hd e <<< "$(stroke "$log")"
echo "  off hand: shot $s, back $b, shell $e, home $h"
check $(echo "$log" | grep -q "autopump hand 0 start" && awk -v e="$e" -v bd="$bd" 'BEGIN { print (e != "" && e >= bd && e - bd < 0.015) ? 1 : 0 }') "off hand: its stroke, the shell at the back"

[ $fail = 0 ] && echo "autopump: all passed" || echo "autopump: FAILED"
exit $fail
