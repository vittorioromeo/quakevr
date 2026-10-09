#!/bin/bash
# gadget_sync_test.sh <agent> -- the wrist gadget's screen tap zone and side button against the gadget as drawn, while
# moving and turning at full speed (ROUND21.md, "The gadget's screen tap and side button in sync"; his notes
# vrfiringrange_2026-10-09_11-01-21, 11-05-38). vr_debug_gadget_button 3 prints, every frame the view tests the tap,
# how far the zone is from the screen on the drawn gadget entity (expected 0: the same frame's), and how far off the old
# test at the frame's start was (the gadget a frame old there: the lag he saw). Phases: standing, running, strafing while
# smooth turning, snap turning, jumping while running; then taps and button presses while running and turning (each
# expected to count: "screen tapped", "gadget button: pressed").
AGENT=${1:-gadget}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
S="map e1m1;wait60;vr_bullettime_cooldown 0;vr_bullettime_duration 600;vr_bullettime_scale 1;vr_debug_bullettime 1"
S="$S;vr_bullettime_tap_gesture 0" # each tap counts alone (the double tap is the default since config 110)
S="$S;vr_debug_gadget_button 3;echo === STILL;wait30"
S="$S;echo === RUN;vr_mock_stick off 0 1;wait90"
S="$S;echo === STRAFE AND SMOOTH TURN;vr_mock_stick off 1 0;vr_mock_stick main 1 0;wait90;vr_mock_stick main 0 0"
S="$S;echo === SNAP TURN;vr_snap_turn 45;vr_mock_stick off 0 1"
for i in 1 2 3 4; do S="$S;vr_mock_stick main 1 0;wait8;vr_mock_stick main 0 0;wait8"; done
S="$S;vr_snap_turn 0;echo === JUMP WHILE RUNNING;+jump;wait40;-jump;wait40;vr_mock_stick off 0 0;vr_mock_stick main 0 0"
S="$S;vr_debug_gadget_button 1;echo === END"
# Taps while running and turning: settled 20 cm over the screen, then a glided strike (3.2 m/s) onto it.
tap() { # <label>
    S="$S;echo === $1;vr_mock_hand_glide 0;vr_mock_hand_to main screen 20 0;wait30;vr_mock_hand_to main screen 20 0;wait2"
    S="$S;vr_mock_hand_glide 0.06;vr_mock_hand_to main screen 1 0;wait40;vr_mock_hand_glide 0;vr_mock_hand_to main screen 20 0;wait60"
}
S="$S;map e1m1;wait60;vr_mock_stick off 0 1;wait30" # (from the start again: a wall ahead pushes the hand off)
tap "TAP RUNNING"
S="$S;vr_mock_stick main 1 0"
tap "TAP RUNNING AND TURNING"
S="$S;vr_mock_stick off 1 0"
tap "TAP STRAFING AND TURNING"
# The side button while running and turning: from 3 units off its face onto it.
S="$S;echo === BUTTON RUNNING AND TURNING;vr_mock_hand_to main button 3;wait30;vr_mock_hand_to main button 3;wait2;vr_mock_hand_to main button;wait10"
S="$S;vr_mock_stick off 0 0;vr_mock_stick main 0 0;echo === END;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "===|gadget sync|screen tapped|never stopped|gadget button: press|rror|no gadget" "$@" |
    python "$(dirname "$0")/gadget_sync_summary.py"
