#!/bin/bash
# weapon_catch_test.sh <agent> -- a weapon is caught or picked up only by the closed fist on it (the author's notes
# hip1m1_2026-10-07_22-33-15, vrfiringrange_2026-10-07_22-49-20: the hand's 5-unit box at its point against the box round
# the handle took a weapon with the fist ~20 cm off). Headless, a crowbar (impulse 217), the mock main hand's point
# lowered onto its middle (vr_mock_hand_to main weapon 0.5 <cm>) a step at a time, the grip pressed fresh at each,
# vr_debug_carry 2 printing the server's fist gap, developer 1 the take (vr_box3d_hand_props 0: the hand doesn't push it):
#   1. in the air (sv_gravity 0, so it floats; not resting on anything): not taken while the fist is off it (no slack),
#      taken once the fist touches it (gap <= 0).
#   2. the same with vr_weapon_grab_box 1 (the old catch): taken from 12 cm over it (by the boxes, the fist far off).
#   3. lying on the floor (vrfiringrange): taken with the fist within vr_weapon_grab_slack (5 cm) of it, as before
#      (vr_weapon_grab_handle_leniency 0: the crowbar is gripped at its middle, its handle).
#   4. a shotgun lying as a prop, taken at its handle within vr_weapon_grab_handle_leniency more (below).
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
FLOOR="vr_weapon_grab_handle_leniency 0;map vrfiringrange;wait60;setpos 221.2 -656.7 41 0 0 0;wait10;impulse 217;wait90;setpos 250 -656.7 41 0 0 0;wait10"
log=$(run "$FLOOR" "12 9 6 4 2 0 -1 -2")
t=$(echo "$log" | grep "^taken")
check $(echo "$t" | awk '{print ($2 <= 5 && $2 > 0 && $3 == 5) ? 1 : 0}') "lying on the floor: taken within the slack ($t)"
check $(echo "$log" | grep -q "error" && echo 0 || echo 1) "no error"
# 4. (the author's note of 2026-10-08: "very hard to grab guns by the main handle while they're in prop form") a shotgun
#    let go of (lying as a Box3D prop, vr_reload_test 20), the off hand stepped down over its handle (its origin), the
#    closing hand nudging it as in the game (vr_box3d_hand_props as shipped): taken by the handle with the fist farther
#    off it than vr_weapon_grab_slack (5) and within it plus vr_weapon_grab_handle_leniency (5); with the leniency 0, the
#    allowed gap at the handle is the slack's alone.
handle() { # <setup> <heights in units over the handle>: "taken <gap> <allowed>", "allowed <first allowed>"
    local S="map e1m1;wait60;developer 1;vr_weapon_grip_mode 1;impulse 9;wait2;impulse 154;wait3;vr_test_weaponinst 7;impulse 120;wait3;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;vr_mock_hand off -0.15 1.0 -0.40 80 0 0;wait10;vr_reload_test 20;impulse 125;wait120;vr_mock_hand off -0.15 1.0 -0.40 0 0 0;wait10;vr_mock_walk_to nearest thrown_weapon 20;wait200;vr_mock_walk_to off;wait10;$1;vr_debug_carry 2"
    for h in $2; do S="$S;vr_mock_hand_to off nearest thrown_weapon $h;wait6;+graboff;vr_mock_button off grip 1;wait3;-graboff;vr_mock_button off grip 0;wait15"; done
    bash $KIT/run.sh $AGENT -Script "$S;toggleconsole;quit" -Filter "Shotgun taken into|grab: off hand, thrown|rror" 2>&1 |
        awk '/the fist/{match($0, /, -?[0-9.]+ cm/); g=substr($0, RSTART+2, RLENGTH-5); match($0, /allowed [0-9.]+/); a=substr($0, RSTART+8, RLENGTH-8); if(!f){print "allowed " a; f=1}}
             /taken into/{print "taken " g " " a; exit} /rror/{print "error"}'
}
log=$(handle "" "8 6 5 4 3 2 1 0")
t=$(echo "$log" | grep "^taken")
check $(echo "$t" | awk '{print ($2 > 5 && $2 <= 10 && $3 == 10) ? 1 : 0}') "lying shotgun at its handle: taken with the fist past the slack, within the handle leniency ($t)"
log=$(handle "vr_weapon_grab_handle_leniency 0" "6 4")
check $(echo "$log" | grep -q "^allowed 5.00" && ! echo "$log" | grep -q "^taken" && echo 1 || echo 0) "leniency 0: the slack's 5 cm alone at the handle, not taken from 10 cm over it ($(echo "$log" | paste -sd' '))"
exit $fail
