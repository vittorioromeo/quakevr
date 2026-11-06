#!/bin/bash
# rollaim_test.sh <agent> -- the shots, the drawn barrel and the sight line stay rigid with the hand as it rolls (his
# note 2026-11-06: Show Controller Laser's red (the shots, weapons::shotAngles) and green (the barrel, hotspotFrame)
# lined up with the gun level came apart as he rolled his wrist: the weapon's angle offsets were added to the hand's
# Euler angles, view::weaponTurn, instead of composed with its turn).
# Guns with angle offsets and Shot Pitch/Yaw: the enforcer's rifle (Shot Pitch 7 against its model's base pitch), the
# super shotgun with Pitch, Yaw and Roll and Shot Pitch and Yaw set (main hand, then off hand: mirrored), and the
# heavy revolver (AD guns need AD's own models: not in the test's game). Each held at controller pitch 0, +40, -30 and rolls 0, +-45, +-90;
# vr_aim_check prints each one's shot, barrel and sight line in the frame of the hand it was drawn from. Prints
# "rollaim: N passed, M failed" (rollaim_check.py: every direction within 0.5 degrees of its level pose's); the run
# quits itself after its last step.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
T=C:/OHWorkspace/qvr-agents/$AGENT/scratch; mkdir -p "$T"
SSG="vr_wofs_pitch_03 -3;vr_wofs_yaw_03 4;vr_wofs_roll_03 12;vr_wofs_shot_pitch_03 2.5;vr_wofs_shot_yaw_03 -1.5"
S="map e1m1;wait60;god;notarget;vr_weapon_grip_mode 1;impulse 9;wait2"
poses() { # <case> <hand> <x>
    for p in 0 40 -30; do
        for r in 0 45 -45 90 -90; do
            S="$S;vr_mock_hand $2 $3 1.3 -0.3 $((p + 70)) 0 $r;wait2;echo rollaim case $1 $2 p$p r$r;vr_aim_check"
        done
    done
    S="$S;vr_mock_hand $2 $3 1.3 -0.3 70 0 0;wait2"
}
S="$S;vr_test_give_wid 16;impulse 149;wait5;vr_mock_hand main 0.2 1.3 -0.3 70 0 0;wait30"
poses enforcer main 0.2
S="$S;$SSG;vr_test_give_wid 5;impulse 149;wait30"
poses ssg main 0.2
S="$S;impulse 175;wait5;vr_mock_hand off -0.2 1.3 -0.3 70 0 0;wait30"
poses ssg off -0.2
S="$S;vr_test_give_wid 31;impulse 149;wait30"
poses revolver main 0.2
S="$S;echo rollaim done;toggleconsole;quit"
bash "$KIT/run.sh" "$AGENT" -Script "$S" -Filter "^rollaim|^aim_check|rror" -QuitOn "rollaim done" -Timeout 240 > "$T/rollaim.log" 2>&1
python "C:/OHWorkspace/qvr-agents/$AGENT/Misc/quakevr/rollaim_check.py" "$T/rollaim.log"
