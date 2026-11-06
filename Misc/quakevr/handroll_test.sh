#!/bin/bash
# handroll_test.sh <agent> -- the drawn hands and weapons stay rigid with the controllers as they roll and pitch (his
# notes 2026-11-06: tuned with the wrist upright, palm sideways, everything lined up with Show Controller's ghost; palm
# to the floor or the ceiling, the gun and the hand came off the ghost and the lasers apart; agent/rollaim: the
# weapon's angle offsets were added to the hand's Euler angles, view::weaponTurn).
# A nonzero hand calibration on both hands (Gun Angle, Gun Yaw, roll and move; the off hand its own); three pairs: the
# super shotgun with Pitch, Yaw, Roll set (main) and a knight's sword (off, melee); the enforcer's rifle (main) and the
# super shotgun (off, mirrored); the axe (main, melee) and an empty off hand. Both controllers at pitch 0, +40, -30 by
# roll 0, +-45, +-90 (the off hand's mirrored); vr_hand_check prints each hand's aim frame, drawn weapon and drawn hand
# (turn and place) in the controller's own grip frame (the one Show Controller's ghost is drawn from). Prints
# "handroll: N passed, M failed" (handroll_check.py: each within 0.5 degrees and 0.3 units of its pose at p0 r0); the
# run quits itself after its last step.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
T=C:/OHWorkspace/qvr-agents/$AGENT/scratch; mkdir -p "$T"
CAL="vr_gunangle 30;vr_gunyaw 6;vr_handcal_roll 10;vr_handcal_x 1;vr_handcal_y -1.5;vr_handcal_z 2;vr_handcal_off_mirror 0"
CAL="$CAL;vr_offhandpitch 35;vr_offhandyaw -3;vr_handcal_off_roll -8;vr_handcal_off_x 0.5;vr_handcal_off_y 1;vr_handcal_off_z -1"
SPRING="vr_weight_spring_sag 0" # (no world sag on the held weight's spring: the pose itself; it settles in 30 frames)
SSG="vr_wofs_pitch_03 -3;vr_wofs_yaw_03 4;vr_wofs_roll_03 12;vr_wofs_shot_pitch_03 2.5;vr_wofs_shot_yaw_03 -1.5"
S="map e1m1;wait60;god;notarget;vr_weapon_grip_mode 1;$CAL;$SSG;$SPRING;impulse 9;wait2"
S="$S;vr_mock_hand main 0.25 1.2 -0.35 70 0 0;vr_mock_hand off -0.25 1.2 -0.35 70 0 0"
poses() { # <case>
    for p in 0 40 -30; do
        for r in 0 45 -45 90 -90; do
            S="$S;vr_mock_hand main 0.25 1.2 -0.35 $((p + 70)) 0 $r;vr_mock_hand off -0.25 1.2 -0.35 $((p + 70)) 0 $((0 - r))"
            S="$S;wait30;echo handroll case $1 p$p r$r;vr_hand_check"
        done
    done
    S="$S;vr_mock_hand main 0.25 1.2 -0.35 70 0 0;vr_mock_hand off -0.25 1.2 -0.35 70 0 0"
}
S="$S;vr_test_give_wid 5;impulse 149;wait5;impulse 183;wait30" # super shotgun, sword (170 + 13)
poses ssg_sword
S="$S;vr_test_give_wid 16;impulse 149;wait5;impulse 175;wait30" # enforcer's rifle, super shotgun
poses enf_ssg
S="$S;vr_test_give_wid 2;impulse 149;wait5;impulse 170;wait30" # axe, empty
poses axe_empty
S="$S;echo handroll done;toggleconsole;quit"
bash "$KIT/run.sh" "$AGENT" -Script "$S" -Filter "^handroll|^hand_check|rror" -QuitOn "handroll done" -Timeout 300 > "$T/handroll.log" 2>&1
python "C:/OHWorkspace/qvr-agents/$AGENT/Misc/quakevr/handroll_check.py" "$T/handroll.log"
