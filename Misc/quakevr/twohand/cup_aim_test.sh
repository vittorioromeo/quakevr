#!/bin/bash
# cup_aim_test.sh <agent> -- the Widowmaker's cup on its front handle as a two-handed aim point (his voice notes
# 2026-10-26; Weapon Offsets > Hotspots: Two-Handed Aim, vr_wofs_hs1_aim2h_35). The Widowmaker in the main hand, the off
# hand's palm on its hotspot 1 (a cup) and gripping; then the main hand raised and lowered 4 cm (vr_mock_hand_lift, the
# off hand still: the line between the hands tilts) with Two-Handed Aim on, then the same with it off. On, the barrel's
# pitch follows the line (it moves); off, only the main hand aims it (its turn never changes: the pitch stays). The shots
# follow the barrel throughout (vr_aim_check's shot-barrel angle). Prints "cupaim: N passed, M failed"; the run quits
# itself after its last step.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
T=C:/OHWorkspace/qvr-agents/$AGENT/scratch; mkdir -p "$T"
S="map e1m1;wait30;god;notarget;developer 1;vr_weapon_grip_mode 1;vr_debug_2h_grip 2;vr_mock_hand main 0.2 1.3 -0.3 70 0 0;vr_mock_hand off -0.2 1.1 -0.3 70 0 0;+grabright;vr_mock_button main grip 1;vr_ad_test 98;wait60"
S="$S;${EXTRA:-};vr_wofs_hs1_aim2h_35 1;vr_mock_hand_to off heldspot 0 palm;wait5;+graboff;vr_mock_button off grip 1;wait40"
for set in 1 0; do
    S="$S;vr_wofs_hs1_aim2h_35 $set;vr_mock_hand_lift 0;wait40;echo cupaim step on$set mid;vr_aim_check"
    S="$S;vr_mock_hand_lift 0.04;wait40;echo cupaim step on$set up;vr_aim_check"
    S="$S;vr_mock_hand_lift -0.04;wait40;echo cupaim step on$set down;vr_aim_check"
done
S="$S;toggleconsole;quit"
bash "$KIT/run.sh" "$AGENT" -Script "$S" -Filter "^cupaim|^aim_check: main|^2h grip|rror" -Timeout 180 > "$T/cupaim.log" 2>&1
python "C:/OHWorkspace/qvr-agents/$AGENT/Misc/quakevr/twohand/cup_aim_check.py" "$T/cupaim.log"
