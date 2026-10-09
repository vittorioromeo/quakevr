#!/bin/bash
# gadget_tap_feedback_test.sh <agent> -- a screen tap's feedback (ROUND21.md, "The screen tap's click and glitch"),
# headless: as each tap registers, the gadget clicks from its screen and its screen glitches for a moment. Expected:
# M1 (a double tap, the defaults) "first tap" feedback (vr/gadget_tap.wav, glitch 0.70 for 0.12 s) then "activation"
# (vr/gadget_tap_on.wav, 1.00 for 0.15 s), each glitch drawn in some frames; M2 (Single Tap) the activation's only; M3
# (vr_bullettime_tap_sound 0, vr_bullettime_tap_glitch 0) click none, glitch 0, nothing drawn; M4 the test command.
AGENT=${1:-defaults}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
S="map e1m1;wait60;vr_debug_bullettime 1;vr_bullettime_cooldown 0;vr_bullettime_duration 600;vr_bullettime_scale 1"
S="$S;vr_gadget_button 0"
settle() { S="$S;vr_mock_hand_glide 1;vr_mock_hand_to main screen 10 0;wait120;vr_mock_hand_to main screen 10 0;wait100"; }
tap() { S="$S;vr_mock_hand_glide 0.06;vr_mock_hand_to main screen 0 0;wait6;vr_mock_hand_glide 0.08;vr_mock_hand_to main screen 10 0;wait8"; }
S="$S;echo === M1 double tap"; settle; tap; tap; S="$S;wait60;echo === END"
S="$S;echo === M1b again: off"; settle; tap; tap; S="$S;wait60;echo === END"
S="$S;echo === M2 single tap;vr_bullettime_tap_gesture 0"; settle; tap; S="$S;wait60;echo === END"
S="$S;echo === M2b again: off"; settle; tap; S="$S;wait60;echo === END"
S="$S;echo === M3 feedback off;vr_bullettime_tap_sound 0;vr_bullettime_tap_glitch 0"; settle; tap; S="$S;wait60;echo === END"
S="$S;vr_bullettime_tap_sound 0.6;vr_bullettime_tap_glitch 1;vr_bullettime_tap_gesture 1"
S="$S;echo === M4 the test command;vr_bullettime_tap_feedback_test;wait30;vr_bullettime_tap_feedback_test 1;wait30;echo === END"
S="$S;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "===|tapped|first tap|no second|bullet time: (on|off)|gadget: tap|rror|t load|not found" "$@"
