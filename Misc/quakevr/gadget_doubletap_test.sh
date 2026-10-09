#!/bin/bash
# gadget_doubletap_test.sh <agent> -- the screen tap's Double Tap gesture (vr_bullettime_tap_gesture 1; ROUND21.md, "The
# screen tap: a double tap"), headless. Taps are glided (vr_mock_hand_glide) from 10 cm over the screen onto it and back.
# Expected: J1 (two quick taps), J4 (two lighter ones, 1.0 m/s: under the single tap's 1.2), K (gun: the hand and
# the butt) and L (holding a health pack) say "double tap: screen tapped" and turn bullet time on; J2 (one tap) and J3
# (two taps 0.8 s apart) only "first tap" and "no second tap"; J5 (two swings across 3 cm over) nothing.
AGENT=${1:-gadget}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
S="map e1m1;wait60;vr_debug_bullettime 1;vr_bullettime_cooldown 0;vr_bullettime_duration 600;vr_bullettime_scale 1"
# The thresholds the cases are measured against (as gadget_tap_test.sh's), not the author's shipped since config 109.
S="$S;vr_bullettime_tap_speed 1.2;vr_bullettime_tap_angle 40;vr_bullettime_tap_margin 2;vr_bullettime_tap_depth 6"
S="$S;vr_bullettime_tap_height 1;vr_bullettime_tap_z 0;vr_bullettime_tap_stop 0.5;vr_bullettime_tap_window 0.25"
S="$S;vr_bullettime_trigger_cooldown 0.5;vr_bullettime_tap_gesture 1;vr_bullettime_tap_double_window 0.4;vr_bullettime_tap_double_speed 0.8;vr_gadget_button 0"
S="$S;vr_bullettime_tap_width 1;vr_bullettime_tap_butt_depth 4" # the zone and butt of before config 110
settle() { # <point>: the hand settled 10 cm over the screen's middle (a slow glide)
    S="$S;vr_mock_hand_glide 1;vr_mock_hand_to main $1 10 0;wait120;vr_mock_hand_to main $1 10 0;wait100"
}
tap() { # <point> <seconds down>: down onto the screen, then back up
    S="$S;vr_mock_hand_glide $2;vr_mock_hand_to main $1 0 0;wait6;vr_mock_hand_glide 0.08;vr_mock_hand_to main $1 10 0;wait8"
}
# J1: two taps 0.2 s apart (1.7 m/s each). Each case ends turned off again by a double tap of its own if it turned on.
S="$S;echo === J1 two quick taps"; settle screen; tap screen 0.06; tap screen 0.06; S="$S;wait60;echo === END"
S="$S;echo === J1b again: off"; settle screen; tap screen 0.06; tap screen 0.06; S="$S;wait60;echo === END"
S="$S;echo === J2 one tap"; settle screen; tap screen 0.06; S="$S;wait90;echo === END"
S="$S;echo === J3 two taps 0.8 s apart"; settle screen; tap screen 0.06; S="$S;wait44"; tap screen 0.06; S="$S;wait90;echo === END"
S="$S;echo === J4 two lighter taps 1.0 m/s"; settle screen; tap screen 0.1; tap screen 0.1; S="$S;wait60;echo === END"
S="$S;echo === J4b again: off"; settle screen; tap screen 0.06; tap screen 0.06; S="$S;wait60;echo === END"
S="$S;echo === J5 two swings across 3 cm over;vr_mock_hand_glide 1;vr_mock_hand_to main screen 3 -40;wait120;vr_mock_hand_to main screen 3 -40;wait60"
S="$S;vr_mock_hand_glide 0.08;vr_mock_hand_to main screen 3 40;wait8;vr_mock_hand_to main screen 3 -40;wait8;wait60;echo === END"
# K: a gun in the main hand (holster 2: e1m1's start), held: the hand and the gun's butt strike.
S="$S;vr_mock_hand_glide 0;vr_mock_hand_to main holster 2;wait3;vr_mock_hand_to main holster 2;wait3;+grabmain;vr_mock_button main grip 1;wait20"
S="$S;echo === K gun: two quick taps with the butt"; settle screenbutt; tap screenbutt 0.06; tap screenbutt 0.06; S="$S;wait60;echo === END"
S="$S;echo === Kb again: off"; settle screenbutt; tap screenbutt 0.06; tap screenbutt 0.06; S="$S;wait60;echo === END"
S="$S;-grabmain;vr_mock_button main grip 0;wait20;impulse 1;wait20;vr_mock_hand main;wait20"
# L: holding a health pack (placed in the hand): the hand and the pack strike.
S="$S;+grabmain;vr_mock_button main grip 1;wait5;vr_rigid_place item_health main 0 3 0;wait40"
S="$S;echo === L prop: two quick taps"; settle screen; tap screen 0.06; tap screen 0.06; S="$S;wait60;echo === END"
S="$S;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "===|screen tapped|first tap|no second|never stopped|bullet time: (on|off)|rror|no gadget|usage|vr_rigid_place" "$@"
