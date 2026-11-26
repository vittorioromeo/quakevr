#!/bin/bash
# flatmelee4_test.sh <agent> -- flat-screen V's blade thrust windup, the empty-handed overhand, the flipped grips' stab
# (flatmelee4, his words 2026-11-25; ROUND26.md "flatmelee4"). One headless flat run on vrtesthall, fast mode, the probe on
# (vr_flat_melee_probe 1: a "flatmelee:" line and a "flatwindup:" line, back / up / out at the windup's top and the thrust's
# line at the strike's end, as each blow ends):
#   1. V with empty hands: the overhand blow (the engine's "overhand blow"), the fist over the eye, a blow on a grunt 16
#      ahead (a melee event, marked an action); the fire button's punch still the straight punch;
#   2. the sword's V thrust: its windup draws the fist back further than the old 8 (vr_flat_thrust_1h_wind_back 16), and
#      by what the tunables say: _wind_back 26 draws it 10 further, _wind_out 6 takes it 6 out;
#   3. the sword, the dagger and the spear in a flipped grip: the thrust's line goes down (under -8 degrees) at the
#      strike's end; the upright sword's stays level.
# The run ends on "flatmelee: done" (-QuitOn).
AGENT=${1:?agent}
fail=0
QT_NAME=flatmelee4
source "$(dirname "$0")/qvrtest.sh"
log=$TREE/scratch/flatmelee4_test.log
S="map vrtesthall; god; notarget; developer 1; vr_flatplay_debug 1; vr_handsource_debug 1; vr_flat_melee_probe 1; wait5; setpos 0 260 25 0 90 0; wait5"
S="$S; vr_test_spawn 0; vr_test_spawn_dist 16; impulse 241; wait5; vr_flat_punch main v; wait60; vr_test_kill 1; wait5"
S="$S; vr_flat_punch main; wait60"
give() { echo "vr_test_give_wid 13; vr_test_give_flags $1; impulse 149; wait30"; }
S="$S; $(give 0); vr_flat_punch main v; wait60"
S="$S; tune vr_flat_thrust_1h_wind_back 26; vr_flat_punch main v; wait60; tune_reset vr_flat_thrust_1h_wind_back"
S="$S; tune vr_flat_thrust_1h_wind_out 6; vr_flat_punch main v; wait60; tune_reset vr_flat_thrust_1h_wind_out"
S="$S; $(give 8192); vr_flat_punch main v; wait60; $(give 11264); vr_flat_punch main v; wait60; $(give 11520); vr_flat_punch main v; wait60"
S="$S; vr_flat_melee_probe done; wait2; toggleconsole; quit"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Script "$S" -QuitOn "flatmelee: done" -Timeout 200 \
    -Filter "^flatmelee:|^flatwindup:|^flatplay: main punch|^melee event:|^handsource: main hand action" > "$log" 2>&1
grep -a "^flatmelee: main\|^flatwindup:\|^flatplay: main punch" "$log" | sed 's/^/  /'
ok() { awk "BEGIN { exit !(${1}) }" && echo 1 || echo 0; }
# `wind <n> <field>`: the n-th flatwindup line's field (back up out tip).
wind() { grep -a "^flatwindup: main" "$log" | sed -n "${1}p" | grep -ao " $2 [-0-9.]*" | head -1 | awk '{ print $NF + 0 }'; }
eye=$(grep -a "^flatmelee: main overhand" "$log" | head -1 | grep -ao " eye [-0-9.]*" | awk '{ print $2 + 0 }')
check $(grep -aq "^flatplay: main punch (vr_flat_punch (V)): .*overhand blow" "$log" && echo 1 || echo 0) "empty hands: V is the overhand blow"
check $(ok "${eye:--99} > 0") "the overhand: the fist goes over the eye (${eye:-?} units)"
check $(grep -aq "^melee event:.*m/s" "$log" && grep -aq "^handsource: main hand action" "$log" && echo 1 || echo 0) "the overhand: a blow lands on the grunt, as a punch (an action)"
check $(grep -aq "^flatmelee: main punch" "$log" && echo 1 || echo 0) "the fire button with an empty hand: still the straight punch"
b1=$(wind 2 back); b2=$(wind 3 back); o1=$(wind 2 out); o3=$(wind 4 out)
echo "  sword windup back: default ${b1:-?}, wind_back 26: ${b2:-?}; out: default ${o1:-?}, wind_out 6: ${o3:-?}"
check $(ok "${b1:-0} > 10") "the sword's windup goes further back than before (${b1:-?} units; 8 along the blade before)"
check $(ok "${b2:-0} - ${b1:-0} > 7 && ${b2:-0} - ${b1:-0} < 12") "vr_flat_thrust_1h_wind_back 26: 10 further back"
check $(ok "${o3:-0} - ${o1:-0} > 4.5 && ${o3:-0} - ${o1:-0} < 7.5") "vr_flat_thrust_1h_wind_out 6: 6 further out"
t0=$(wind 2 tip); ts=$(wind 5 tip); td=$(wind 6 tip); tp=$(wind 7 tip)
echo "  thrust line at the strike's end (degrees): sword ${t0:-?}, flipped sword ${ts:-?}, dagger ${td:-?}, spear ${tp:-?}"
check $(ok "${t0:--99} > -8") "the upright sword thrusts level"
check $(ok "${ts:-0} < -8 && ${td:-0} < -8 && ${tp:-0} < -8") "flipped: the sword, the dagger and the spear stab down"
verdict
exit $fail
