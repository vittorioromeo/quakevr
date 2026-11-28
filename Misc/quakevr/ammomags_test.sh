#!/bin/bash
# ammomags_test.sh <agent> -- the ammo follows the rounds loaded (ammomags; ROUND26.md "ammomags"), headless:
#   vr_reload_test 51: the QC checks at once (switch refused, pouch halves, lava magazine fires lava, no mixed launcher
#     loads, the simple mode and flat play keep the switch);
#   52-55 by the mock's hands: an empty nailgun in the off hand; the main hand takes from the pouch's left half
#     (vr_mock_hand_to main ammopouch 2) and seats it at the gun's port: the gun takes lava nails (53); its trigger held
#     (+offhandattack): lava nails fly (54); the main hand takes from the right half (-2): a plain magazine (55).
# Prints each part's verdict ("ammomags: N passed, M failed"); exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
SET="map e1m1;wait60;developer 1;vr_reload_debug 1;god;notarget;vr_weapon_grip_mode 1;vr_holster_mode 0;vr_reload_mode 3;wait10"
QC="vr_reload_test 51;impulse 125;wait3"
MOCK="vr_reload_test 52;impulse 125;wait5;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait3"
LEFT="vr_mock_hand_to main ammopouch 2;wait3;vr_mock_hand_to main ammopouch 2;wait3;+grabmain;vr_mock_button main grip 1;wait3"
SEAT="vr_mock_hand_to main lport;wait3;vr_mock_hand_to main lport;wait5;vr_reload_test 53;impulse 125;wait3"
FIRE="vr_mock_button main grip 0;-grabmain;wait5;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait3;vr_mock_hand off -0.1 1.69 -0.20 70 0 0;wait5;+offhandattack;wait12;vr_reload_test 54;impulse 125;wait1;-offhandattack;wait3"
RIGHT="vr_mock_button main grip 0;-grabmain;wait10;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait3;vr_mock_hand_to main ammopouch -2;wait3;vr_mock_hand_to main ammopouch -2;wait3;+grabmain;vr_mock_button main grip 1;wait3;vr_reload_test 55;impulse 125;wait3"
log=$(bash "$KIT/run.sh" "$AGENT" -Script "$SET;$QC;$MOCK;$LEFT;$SEAT;$FIRE;$RIGHT;toggleconsole;quit" -Filter "^ammomags:|taken from the pouch|seated|takes the round|refused" 2>&1)
echo "$log" | grep -E "^ammomags: (FAIL|[0-9]+ passed)|taken from the pouch|takes the round"
verdicts=$(echo "$log" | grep -cE "^ammomags: [0-9]+ passed, 0 failed")
if [ "$verdicts" = 5 ]; then echo "ammomags_test: PASS"; exit 0; fi
echo "ammomags_test: FAIL ($verdicts of 5 parts clean)"; exit 1
