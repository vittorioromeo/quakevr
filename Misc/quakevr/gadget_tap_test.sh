#!/bin/bash
# gadget_tap_test.sh <agent> -- the wrist gadget's screen tap (bullet time) and side button (gear lights), headless
# (ROUND21.md, "The gadget's side button: gear lights"). Taps and swings are glided (vr_mock_hand_glide) so that the
# hand reports their exact speed; each case settles at its start (a slow glide), then makes one move. Bullet time runs
# at 1x here (its slowed hands would change the next case). Expected: A, D2 and E tap ("screen tapped"), B, B2, C, C2
# and D don't; F presses (lights off), ignores the press at once after it (cooling down), presses again (on); the
# stealth light's dynamic share lower while off.
AGENT=${1:-gadget}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
S="map e1m1;wait60;vr_debug_bullettime ${DBG:-1};vr_debug_gadget_button 1;vr_bullettime_cooldown 0;vr_bullettime_duration 600;vr_bullettime_scale 1"
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
# A gun in the main hand (holster 2: e1m1's start), held.
S="$S;vr_mock_hand_glide 0;vr_mock_hand_to main holster 2;wait3;vr_mock_hand_to main holster 2;wait3;+grabmain;vr_mock_button main grip 1;wait20"
case_ "E gun butt tap 3.2 m/s" screenbutt 20 0 1 0 0.06
# F: the side button: pressed (lights off, a click), again at once (ignored: the cooldown), again after it (on).
S="$S;echo === F BUTTON;vr_mock_hand_glide 0;vr_mock_hand_to main button 3;wait30;vr_mock_hand_to main button;wait5;vr_gear_lights_info"
S="$S;vr_mock_hand_to main button 3;wait5;vr_mock_hand_to main button;wait5;vr_gear_lights_info"
S="$S;vr_mock_hand_to main button 3;wait60;vr_mock_hand_to main button;wait30;vr_gear_lights_info;echo === END"
# G: the stealth light on the player with the gadget raised to be read, gear lights on and off (the toggle command).
S="$S;echo === G STEALTH;vr_mock_hand off -0.1 1.3 -0.3 60 30 0;wait30;vr_gear_lights_info;vr_gear_lights_toggle;wait30;vr_gear_lights_info;echo === END"
S="$S;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "===|screen tapped|never stopped|bullet time: (on|off)|gadget button: press|gear lights: (on|off)$|stealth light|rror|no gadget" "$@"
