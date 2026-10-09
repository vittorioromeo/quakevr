#!/bin/bash
# autopump_test.sh <agent> -- the shotgun's auto pump (vr_autopump.cpp; ROUND21.md, "Shotgun auto pump"), headless: the
# shotgun in the main hand (impulse 154), one shot (+attack), vr_debug_weaponfx 1 printing the stroke's start, back and
# home and the spent shell's eject:
#   1. at 72 and 120 Hz (vr_fixed_frames_rate): the shell leaves in the frame the stroke reaches the back (within 15 ms of
#      it), the stroke home on time (0.3 s), long before the shotgun can fire again (0.5 s);
#   2. in bullet time at 0.25x: the same game times, four times as long in real time (the stroke is game time);
#   3. Auto Pump off: no stroke, the shell 0.22 s after the shot as before;
#   4. the off hand's shotgun (mirrored): the same;
#   5. the delay (vr_autopump_delay 0.15; the author's note, 2026-10-08 13:58): the stroke starts 0.15 s after the shot,
#      home by 0.45 s; Delay 0 starts it with the shot; a long stroke (0.45 s) cuts the delay to 0.03 s (home by 0.48);
#      the shipped delay (0.25, config 107) is cut to 0.18 (home by 0.48).
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
# (Times are printed to the millisecond: their differences are compared with half a millisecond to spare, awk's
# 2.775 - 2.475 being 0.29999999999999982, not 0.3: the stroke home on its due time read as early.)
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
SETUP="map e1m1;wait60;vr_weapon_grip_mode 1;impulse 9;wait2;impulse 154;wait3;vr_mock_hand main 0.15 1.2 -0.45 70 0 0;wait10"
FIRE="vr_debug_weaponfx 1;+attack;wait3;-attack;wait90"
run() { bash $KIT/run.sh $AGENT -Script "$1;toggleconsole;quit" -Filter "^autopump hand|^shells eject|^weaponfx fired" 2>&1; }
# The numbers: the stroke's start, back, home and the eject (game and real times).
stroke() {
    echo "$1" | awk '
        /autopump hand [01] start/ { s = $6; sr = $8; sub(/\):?/, "", sr); shot = $NF }
        /autopump hand [01] back/ { b = $6; br = $8; sub(/;/, "", br); bd = $10; sub(/\),?/, "", bd) }
        /autopump hand [01] home/ { h = $6; hr = $8; sub(/;/, "", hr); hd = $10; sub(/\)/, "", hd) }
        /shells eject/ { e = $6 }
        END { printf "%s %s %s %s %s %s %s %s %s %s\n", s, sr, b, br, bd, h, hr, hd, e, shot }'
}

for rate in 72 120; do
    log=$(run "vr_fixed_frames_rate $rate;vr_autopump_delay 0.15;$SETUP;$FIRE") # (the delay the checks are about: the shipped one is 0.25, below)
    read s sr b br bd h hr hd e shot <<< "$(stroke "$log")"
    echo "  $rate Hz: shot $s, back $b (due $bd), shell $e, home $h (due $hd)"
    check $(awk -v e="$e" -v bd="$bd" 'BEGIN { print (e != "" && e >= bd && e - bd < 0.015) ? 1 : 0 }') "$rate Hz: the shell leaves as the fore-end reaches the back"
    check $(awk -v b="$b" -v e="$e" 'BEGIN { print (b != "" && b == e) ? 1 : 0 }') "$rate Hz: in the same frame"
    check $(awk -v s="$s" -v h="$h" 'BEGIN { d = h - s; print (s != "" && d >= 0.2995 && d < 0.32) ? 1 : 0 }') "$rate Hz: home 0.3 s after the stroke's start"
    check $(awk -v s="$s" -v t="$shot" -v h="$h" 'BEGIN { d = s - t; print (t != "" && d >= 0.1495 && d < 0.1505 && h - t < 0.48) ? 1 : 0 }') "$rate Hz: the stroke 0.15 s after the shot ($shot -> $s), home before the next is possible (0.5 s)"
done

# The shipped delay (0.25 since config 107, the author's): longer than the 0.18 the stroke (0.3 s) leaves before the next
# shot (0.5 s), so cut to 0.18: home by 0.48.
log=$(run "$SETUP;$FIRE")
read s sr b br bd h hr hd e shot <<< "$(stroke "$log")"
check $(awk -v s="$s" -v t="$shot" -v h="$h" 'BEGIN { d = s - t; print (t != "" && d >= 0.1795 && d < 0.1805 && h - t < 0.4805) ? 1 : 0 }') "the shipped delay 0.25 cut to 0.18 ($shot -> $s), home by 0.48 ($h)"

log=$(run "$SETUP;vr_bullettime_scale 0.25;vr_bullettime_duration 60;vr_bullettime;wait30;$FIRE;wait120")
read s sr b br bd h hr hd e shot <<< "$(stroke "$log")"
echo "  bullet time: game $s -> $h, real $sr -> $hr"
# (The start line's real time is the shot's: the delay and the stroke, 0.45 s of game time, four times that in real time.)
check $(awk -v s="$s" -v h="$h" -v t="$shot" -v sr="$sr" -v hr="$hr" 'BEGIN { g = h - t; r = hr - sr; print (s != "" && h - s < 0.32 && r > 3.5 * g && r < 4.5 * g) ? 1 : 0 }') "bullet time 0.25x: the stroke takes 0.3 s of game time, it and the delay four times their game time in real time"
check $(awk -v e="$e" -v bd="$bd" 'BEGIN { print (e != "" && e >= bd && e - bd < 0.015) ? 1 : 0 }') "bullet time: the shell leaves at the back"

log=$(run "$SETUP;vr_autopump 0;vr_debug_weaponfx 1;+attack;wait3;-attack;wait90")
f=$(echo "$log" | awk '/weaponfx fired/ { print $4; exit }')
e=$(echo "$log" | awk '/shells eject/ { print $6; exit }')
echo "  off: shot $f, shell $e"
check $(echo "$log" | grep -q "^autopump hand" && echo 0 || echo 1) "Auto Pump off: no stroke"
check $(awk -v f="$f" -v e="$e" 'BEGIN { d = e - f; print (f != "" && e != "" && d >= 0.2195 && d < 0.24) ? 1 : 0 }') "Auto Pump off: the shell 0.22 s after the shot, as before"

log=$(run "$SETUP;vr_test_weaponinst 7;impulse 120;wait3;vr_mock_hand off -0.15 1.2 -0.45 70 0 0;vr_mock_hand main 0.3 1.0 -0.2 0 0 0;wait10;vr_debug_weaponfx 1;+offhandattack;wait3;-offhandattack;wait90")
read s sr b br bd h hr hd e shot <<< "$(stroke "$log")"
echo "  off hand: shot $s, back $b, shell $e, home $h"
check $(echo "$log" | grep -q "autopump hand 0 start" && awk -v e="$e" -v bd="$bd" 'BEGIN { print (e != "" && e >= bd && e - bd < 0.015) ? 1 : 0 }') "off hand: its stroke, the shell at the back"

log=$(run "vr_autopump_delay 0;$SETUP;$FIRE")
read s sr b br bd h hr hd e shot <<< "$(stroke "$log")"
check $(awk -v s="$s" -v t="$shot" 'BEGIN { print (t != "" && s == t) ? 1 : 0 }') "Delay 0: the stroke starts with the shot ($shot -> $s)"
log=$(run "vr_autopump_time 0.45;$SETUP;$FIRE")
read s sr b br bd h hr hd e shot <<< "$(stroke "$log")"
check $(awk -v s="$s" -v t="$shot" -v h="$h" -v e="$e" -v bd="$bd" 'BEGIN { d = s - t; print (t != "" && d >= 0.0295 && d < 0.0305 && h - t < 0.5 && e >= bd && e - bd < 0.015) ? 1 : 0 }') "a 0.45 s stroke: the delay cut to 0.03 s, home before the next shot ($shot -> $s -> $h), the shell at the back"

[ $fail = 0 ] && echo "autopump: all passed" || echo "autopump: FAILED"
exit $fail
