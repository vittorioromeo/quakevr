#!/bin/bash
# playersteps_test.sh <agent>: the player's footsteps on the drawn feet (ROUND23.md, "The player's walk";
# vr_playersteps.cpp, vr_body_step_report). On vrarena's flat floor: a walk, a run and a strafe (by the mock's stick, counted
# frames for a dozen steps or more each): one sound from a foot per touchdown, within a frame of the foot drawn down, nothing from
# the old trigger, and louder at a run. Head turns, stick turns and small room moves: silent; with their tunable up, spaced. vr_body_step_sounds 0 and vr_body_walk 0: the old trigger alone. Ends on
# "playersteps_test: N passed, M failed".
KIT=C:/OHWorkspace/qvr-kit
AGENT=${1:-phases}

# phase <label> <legs|old> <stick x> <stick y> <yaw>: along vrarena's long flat floor (x -770 .. 770).
phase() {
    echo "vr_mock_look 0 0;wait5;setpos -720 600 24 0 $5 0;noclip;wait10;vr_mock_stick off $3 $4;wait50;vr_body_step_report reset;wait216;vr_body_step_report $1 $2;vr_mock_stick off 0 0;wait20"
}
S="sv_random_seed 7;map vrarena;wait30;god 1;notarget 1;vr_body_step_rate 4.9;vr_body_step_report reset"
S="$S;$(phase slow legs 0 0.15 0);$(phase walk legs 0 0.4 0);$(phase run louder 0 1 0);$(phase strafe legs 0.8 0 90)"
# The body's small adjustments: the head turning, the stick turning, the head moving a little round the room: none heard.
HEADTURN="vr_mock_look 0 0;wait40;vr_body_step_report reset"
for Y in 45 90 135 180 225 270 315 360; do HEADTURN="$HEADTURN;vr_mock_look 0 $Y;wait30"; done
STICKTURN="vr_mock_look 0 0;wait40;vr_body_step_report reset;vr_mock_stick main 0.7 0;wait216;vr_mock_stick main 0 0;wait30"
ROOM="vr_mock_look 0 0;wait40;vr_body_step_report reset"
for X in 0.3 0 -0.3 0 0.3 0 -0.3 0; do ROOM="$ROOM;vr_mock_hand head $X 1.6 0;wait40"; done
ROOM="$ROOM;vr_mock_hand head;wait20"
S="$S;$HEADTURN;vr_body_step_report headturn quiet;$STICKTURN;vr_body_step_report stickturn quiet;$ROOM;vr_body_step_report roommove quiet"
S="$S;tune vr_footstep_player_adjust_vol 0.2;$STICKTURN;vr_body_step_report stickturn_on spaced;tune_reset vr_footstep_player_adjust_vol"
S="$S;vr_body_step_sounds 0;$(phase off_run old 0 1 0)"
S="$S;vr_body_step_sounds 1;vr_body_walk 0;$(phase nolegs_run old 0 1 0)"
S="$S;vr_body_walk 1;vr_body_step_report done"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "${PSF:-playersteps}" -QuitOn "playersteps_test: \d+ passed" -Timeout 200
