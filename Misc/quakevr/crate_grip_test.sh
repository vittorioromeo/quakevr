#!/bin/bash
# crate_grip_test.sh <agent>: a small crate (vr_test_spawn 107) taken in the main hand (its grip fitted out to the crate's
# side, 10 units from its middle), then in the off hand too, each grip held still for a second (72 fixed frames) with each
# hand's distance from its grip point measured every server frame (vr_carry2h_grip_test). The off hand is put at a point
# in the crate's frame, with vr_hand_collide_props_keep_out as asked (0: the grip fit moves the grip at the grab):
#   close        beside the main hand on the same side
#   close_in     the same, pushed 10 cm into the crate
#   close_fit    the same with the keep-out off: the fit moves the grip out
#   wide         on the far side, the keep-out off, nothing to fit (the control: the same before and after the fix)
#   wide_fit     the same pushed in, so the fit moves the grip out
# Each hand must stay within LIMIT cm (0.5) of its grip. Ends on the verdict line.
AGENT=${1:?agent}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
LIMIT=${LIMIT:-0.5}
F='vr_carry2h_grip_test|carry: the other|gripfit: [0-9]|ENGINE CRASH|Host_Error'
MAIN='0.16 1.25 -0.55 0 0 0'
AWAY='-0.35 1.05 -0.1 0 0 0'
SETUP='map vrfiringrange;wait60;god;notarget;developer 1;vr_debug_carry 1;vr_grip_fit_reach 3;vr_carry2h_grip_test start'
case_() { # <label> <keep-out> <the off hand's way there, a point in the crate's frame> <its point>
    S="$S;vr_hand_collide_props_keep_out $2;vr_mock_hand off $AWAY;vr_mock_hand main $MAIN;wait10;+grabright;vr_mock_button main grip 1;wait5"
    S="$S;vr_test_spawn 107;vr_test_spawn_dist 24;vr_test_spawn_hold 1;impulse 241;wait10"
    S="$S;vr_carry2h_grip_test hand off $3;wait3;vr_carry2h_grip_test hand off $4;wait3;+graboff;vr_mock_button off grip 1;wait5;vr_carry2h_grip_test sample;wait80;vr_carry2h_grip_test check $1 $LIMIT 72"
    S="$S;-graboff;vr_mock_button off grip 0;-grabright;vr_mock_button main grip 0;wait30"
}
S="$SETUP"
case_ close     1 '2 14 0' '2 10.5 0'
case_ close_in  1 '2 14 0' '2 7 0'
case_ close_fit 0 '2 14 0' '2 7 0'
case_ wide      0 '0 -24 0' '0 -19.5 0'
case_ wide_fit  0 '0 -24 0' '0 -12 0'
S="$S;vr_hand_collide_props_keep_out 1;vr_grip_fit_reach 0.75;vr_carry2h_grip_test end;toggleconsole;quit"
bash "$KIT/run.sh" "$AGENT" -Timeout 300 -QuitOn 'vr_carry2h_grip_test: [0-9]+ passed' -Script "$S" -Filter "$F"
