#!/bin/bash
# playersteps_test.sh <agent>: the player's footsteps on the drawn feet (ROUND23.md, "The player's walk";
# vr_playersteps.cpp, vr_body_step_report). On vrarena's flat floor: a walk, a run and a strafe (by the mock's stick, counted
# frames for a dozen steps or more each): one sound from a foot per touchdown, within a frame of the foot drawn down, nothing from
# the old trigger. Turning on the spot: the standing steps, heard. vr_body_step_sounds 0 and vr_body_walk 0: the old trigger alone. Ends on
# "playersteps_test: N passed, M failed".
KIT=C:/OHWorkspace/qvr-kit
AGENT=${1:-phases}

# phase <label> <legs|old> <stick x> <stick y> <yaw>: along vrarena's long flat floor (x -770 .. 770).
phase() {
    echo "vr_mock_look 0 0;wait5;setpos -720 600 24 0 $5 0;noclip;wait10;vr_mock_stick off $3 $4;wait50;vr_body_step_report reset;wait216;vr_body_step_report $1 $2;vr_mock_stick off 0 0;wait20"
}
S="sv_random_seed 7;map vrarena;wait30;god 1;notarget 1;vr_body_step_rate 4.9;vr_body_step_report reset"
S="$S;$(phase walk legs 0 0.4 0);$(phase run legs 0 1 0);$(phase strafe legs 0.8 0 90)"
S="$S;vr_mock_look 0 0;wait40;vr_body_step_report reset;vr_mock_stick main 0.7 0;wait216;vr_mock_stick main 0 0;wait30;vr_body_step_report turn standing"
S="$S;vr_body_step_sounds 0;$(phase off_run old 0 1 0)"
S="$S;vr_body_step_sounds 1;vr_body_walk 0;$(phase nolegs_run old 0 1 0)"
S="$S;vr_body_walk 1;vr_body_step_report done"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "${PSF:-playersteps}" -QuitOn "playersteps_test: \d+ passed" -Timeout 200
