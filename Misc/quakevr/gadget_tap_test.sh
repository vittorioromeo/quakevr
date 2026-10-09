#!/bin/bash
# gadget_tap_test.sh <agent> -- the wrist gadget's screen tap (bullet time) and side button (gear lights), headless
# (ROUND21.md, "The gadget's side button: gear lights"). Taps and swings are glided (vr_mock_hand_glide) so that the
# hand reports their exact speed; each case settles at its start (a slow glide), then makes one move. Bullet time runs
# at 1x here (its slowed hands would change the next case). Expected: A, D2 and E tap ("screen tapped"), B, B2, C, C2
# and D don't; F presses (lights off), ignores the press at once after it (cooling down), presses again (on); the
# stealth light's dynamic share lower while off. H and I (below): the hand's parts.
AGENT=${1:-gadget}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
S="map e1m1;wait60;vr_debug_bullettime ${DBG:-1};vr_debug_gadget_button 1;vr_bullettime_cooldown 0;vr_bullettime_duration 600;vr_bullettime_scale 1"
# The tap's thresholds the cases are measured against (B2 under 1.2 m/s, D2 within 40 degrees), not the author's
# shipped since config 109 (0.5 m/s, 10 degrees, the zone 0.75 high and 3.5 cm in: ROUND21.md).
S="$S;vr_bullettime_tap_speed 1.2;vr_bullettime_tap_angle 40;vr_bullettime_tap_margin 2;vr_bullettime_tap_depth 6"
S="$S;vr_bullettime_tap_height 1;vr_bullettime_tap_z 0;vr_bullettime_tap_stop 0.5;vr_bullettime_tap_window 0.25"
# The single tap (the double tap is the default since config 110) and the zone and butt of before it.
S="$S;vr_bullettime_tap_gesture 0;vr_bullettime_tap_width 1;vr_bullettime_tap_butt_depth 4"
# case <label> <screen|screenbutt> <start cm over> <start cm across> <end cm over> <end cm across> <seconds>
case_() {
    S="$S;echo === $1;vr_mock_hand_glide 1;vr_mock_hand_to main $2 $3 $4;wait120;vr_mock_hand_to main $2 $3 $4;wait120"
    S="$S;vr_mock_hand_glide $7;vr_mock_hand_to main $2 $5 $6;wait60;vr_mock_hand_glide 1;vr_mock_hand_to main $2 20 0;wait120;echo === END"
}
case_ "A hard hand tap 3.2 m/s straight" screen 20 0 1 0 0.06
case_ "B soft touch 0.23 m/s" screen 15 0 1 0 0.6
case_ "B2 firm touch 0.93 m/s under 1.2" screen 15 0 1 0 0.15
case_ "C swing across 10 m/s 3 cm over" screen 3 -40 3 40 0.08
case_ "C2 swing across stopping on it 8 m/s" screen 3 -40 3 0 0.05
case_ "D diagonal 50 deg off 3.1 m/s" screen 22 -26 2 -2 0.1
case_ "D2 diagonal 31 deg off 2.3 m/s" screen 21 -12 1 0 0.1
# The whole hand strikes (its drawn surface): the hand turned so that one part leads (vr_mock_hand main ... <pitch yaw
# roll>), that part's point furthest towards the screen placed (screen <cm> <side> <part>). Expected: H1..H5 tap, by the
# part named (H1, a flat slap: the fingers' pads land with the palm, a little before it); I1, I2 (swings across with the
# back of the hand, the knuckles) and I3 (the fingertips 50 deg off) don't. The side button is off meanwhile (the hand
# glides past it between the cases).
# case_part <label> <pitch yaw roll> <part> <start over> <start across> <end over> <end across> <seconds>
case_part() {
    S="$S;echo === $1;vr_mock_hand_glide 0;vr_mock_hand main 0.2 1.1 -0.4 $2;wait3;vr_mock_hand_glide 1;vr_mock_hand_to main screen $4 $5 $3;wait120"
    S="$S;vr_mock_hand_to main screen $4 $5 $3;wait120;vr_mock_hand_glide $8;vr_mock_hand_to main screen $6 $7 $3;wait60"
    S="$S;vr_mock_hand_glide 1;vr_mock_hand_to main screen 20 0 $3;wait120;echo === END"
}
S="$S;vr_gadget_button 0"
case_part "H1 flat palm slap" "-90 0 0" palmskin 20 0 0 0 0.06
case_part "H2 back of the hand" "90 0 0" back 20 0 0 0 0.06
case_part "H3 fingertips" "0 -37 0" tips 20 0 0 0 0.06
case_part "H4 thumb" "0 0 90" thumb 20 0 0 0 0.06
S="$S;vr_mock_fingers main 1 1 1 1;wait20"
case_part "H5 knuckles (a fist)" "0 -37 0" knuckles 20 0 0 0 0.06
case_part "I2 knuckles swing across 3 cm over" "0 -37 0" knuckles 3 -40 3 40 0.08
S="$S;vr_mock_fingers main 0 0 0 0;wait20"
case_part "I1 back of the hand swing across 3 cm over" "90 0 0" back 3 -40 3 40 0.08
case_part "I3 fingertips 50 deg off" "0 -37 0" tips 22 -26 0 0 0.1
S="$S;vr_mock_hand main;vr_gadget_button 1"
# A gun in the main hand (holster 2: e1m1's start), held.
S="$S;vr_mock_hand_glide 0;vr_mock_hand_to main holster 2;wait3;vr_mock_hand_to main holster 2;wait3;+grabmain;vr_mock_button main grip 1;wait20"
case_ "E gun butt tap 3.2 m/s" screenbutt 20 0 1 0 0.06
# E2: the gun turned so that its butt leads (expected: tapped by the gun's butt).
S="$S;vr_gadget_button 0"
case_part "E2 gun butt leading" "30 90 0" butt 20 0 0 0 0.06
S="$S;vr_mock_hand main;vr_gadget_button 1;wait30"
# F: the side button: pressed (lights off, a click), again at once (ignored: the cooldown), again after it (on).
S="$S;echo === F BUTTON;vr_mock_hand_glide 0;vr_mock_hand_to main button 3;wait30;vr_mock_hand_to main button;wait5;vr_gear_lights_info"
S="$S;vr_mock_hand_to main button 3;wait5;vr_mock_hand_to main button;wait5;vr_gear_lights_info"
S="$S;vr_mock_hand_to main button 3;wait60;vr_mock_hand_to main button;wait30;vr_gear_lights_info;echo === END"
# G: the stealth light on the player with the gadget raised to be read, gear lights on and off (the toggle command).
S="$S;echo === G STEALTH;vr_mock_hand off -0.1 1.3 -0.3 60 30 0;wait30;vr_gear_lights_info;vr_gear_lights_toggle;wait30;vr_gear_lights_info;echo === END"
S="$S;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "===|screen tapped|never stopped|bullet time: (on|off)|gadget button: press|gear lights: (on|off)$|stealth light|rror|no gadget" "$@"
