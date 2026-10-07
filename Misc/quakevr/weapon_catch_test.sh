#!/bin/bash
# weapon_catch_test.sh <agent> -- a weapon is caught or picked up only by the closed fist on it (the author's notes
# hip1m1_2026-10-07_22-33-15, vrfiringrange_2026-10-07_22-49-20: the hand's 5-unit box at its point against the box round
# the handle took a weapon with the fist ~20 cm off). Headless, a crowbar (impulse 217), the mock main hand's point
# lowered onto its middle (vr_mock_hand_to main weapon 0.5 <cm>) a step at a time, the grip pressed fresh at each,
# vr_debug_carry 2 printing the server's fist gap, developer 1 the take (vr_box3d_hand_props 0: the hand doesn't push it):
#   1. in the air (sv_gravity 0, so it floats; not resting on anything): not taken while the fist is off it (no slack),
#      taken once the fist touches it (gap <= 0).
#   2. the same with vr_weapon_grab_box 1 (the old catch): taken from 12 cm over it (by the boxes, the fist far off).
#   3. lying on the floor (vrfiringrange): taken with the fist within vr_weapon_grab_slack (5 cm) of it, as before.
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
run() { # <setup> <heights>: the server's gap (cm) at each take ("taken <gap> <allowed>"), and "first <gap>" the gap seen first
    local S="vr_fixed_frames 1;$1;developer 1;vr_debug_carry 2;vr_box3d_hand_props 0"
    for h in $2; do S="$S;vr_mock_hand_to main weapon 0.5 $h;wait6;+grabmain;vr_mock_button main grip 1;wait3;-grabmain;vr_mock_button main grip 0;wait15"; done
    bash $KIT/run.sh $AGENT -Script "$S;toggleconsole;quit" -Filter "weapon: Crowbar taken|grab: main hand, thrown|rror" 2>&1 |
        awk '/the fist/{match($0, /, -?[0-9.]+ cm/); g=substr($0, RSTART+2, RLENGTH-5); match($0, /allowed [0-9.]+/); a=substr($0, RSTART+8, RLENGTH-8); if(!f){print "first " g; f=1}}
             /taken into/{print "taken " g " " a; exit} /rror/{print "error"}'
}
AIR="map e1m1;wait60;sv_gravity 0;impulse 217;wait30"
log=$(run "$AIR" "12 9 6 4 2 1 0 -1 -2 -3")
t=$(echo "$log" | grep "^taken")
check $(echo "$t" | awk '{print ($2 <= 0 && $3 == 0) ? 1 : 0}') "in the air: taken only with the fist on it, no slack ($t)"
log=$(run "vr_weapon_grab_box 1;$AIR" "12")
t=$(echo "$log" | grep "^taken")
check $([ -n "$t" ] && echo 1 || echo 0) "in the air, vr_weapon_grab_box 1: the old catch takes it from 12 cm over it ($t)"
FLOOR="map vrfiringrange;wait60;setpos 221.2 -656.7 41 0 0 0;wait10;impulse 217;wait90;setpos 250 -656.7 41 0 0 0;wait10"
log=$(run "$FLOOR" "12 9 6 4 2 0 -1 -2")
t=$(echo "$log" | grep "^taken")
check $(echo "$t" | awk '{print ($2 <= 5 && $2 > 0 && $3 == 5) ? 1 : 0}') "lying on the floor: taken within the slack ($t)"
check $(echo "$log" | grep -q "error" && echo 0 || echo 1) "no error"
exit $fail
