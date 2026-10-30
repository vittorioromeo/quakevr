#!/bin/bash
# bodytrack_solve_test.sh <agent> [--update] -- full body tracking's solve (vr_bodytrack.cpp, vr_avatar.cpp) over a
# recorded take (quakevr/scenarios/bodytrack/take.csv: 300 frames of the author's Quest 3 capture through Virtual
# Desktop, replayed by vr_bodytrack_replay: head, controllers and the tracked joints) with his body settings, against
# stored reference positions (solve_ref.csv: the pelvis, the neck's top, each shoulder, elbow and wrist, and the
# regions' weights, per frame). Fails when any frame's joint moves more than 0.5 units (about 1.3 cm) from the reference.
# --update writes the reference from this run instead (after a deliberate change to the solve; say why in the commit).
# The run ends after the counted frames the dump needs (30 to start the replay, 240 dumped, a few to spare).
AGENT="$1"
KIT=C:/OHWorkspace/qvr-kit
TREE=C:/OHWorkspace/qvr-agents/$AGENT
DIR=$TREE/quakevr/scenarios/bodytrack
OUT=bodytrack_solve_dump.csv
S="map e1m1;wait60;vr_bodytrack 1;vr_bodytrack_arms 1;vr_bodytrack_arm_fit 0.25;vr_bodytrack_legs_walk 1"
S="$S;vr_body_build 3;vr_body_elbow_back 0.35;vr_body_elbow_hand 0.5;vr_body_elbow_lift 8;vr_height_calibration 1.560722"
S="$S;vr_bodycal_upper_arm 25.2;vr_bodycal_forearm 24.5;vr_bodycal_shoulders_back -0.074;vr_bodycal_shoulders_up -0.017"
S="$S;vr_bodycal_shoulders_out -0.018;vr_bodycal_shoulder_rise 28.8;vr_bodycal_shoulder_swing 0"
S="$S;vr_bodytrack_replay scenarios/bodytrack/take.csv;wait30;vr_bodytrack_dump 999 $OUT;wait250"
S="$S;vr_bodytrack_dump 0;vr_bodytrack_replay \"\";toggleconsole;quit"
bash "$KIT/run.sh" "$AGENT" -Filter "vr_bodytrack|rror" -Timeout 120 -Script "$S" | tr -d '\r' | grep -E "can't|rror|not a"
DUMP="C:/OHWorkspace/qvr-kit/bases/$AGENT/qbase/quakevr/$OUT"
if [ "$2" = "--update" ]; then
    python "$TREE/Misc/quakevr/bodytrack_solve_check.py" ref "$DUMP" "$DIR/solve_ref.csv" 240
    echo "bodytrack_solve: reference written"
    exit 0
fi
python "$TREE/Misc/quakevr/bodytrack_solve_check.py" compare "$DUMP" "$DIR/solve_ref.csv" 0.5
