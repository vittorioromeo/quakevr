#!/bin/bash
# aglod_test.sh -- a far, unseen armed grunt wakes and fights as before (vr_agrunt_test 228, QC/vr_armedgrunt_lod_test.qc;
# ROUND25.md "Armed grunts' cost on crowded maps"). On vrarena, three runs from one seed: vr_agrunt_pose_lod_frames 8
# (the default), 2 (as before 1.1.1) and vr_agrunt_pose_lod 0 (posed every frame). Each run's checks must pass, and its
# two traces (seen, shot: its place, frame, weapon and his health each frame for 3 s from the wake) must match the
# others'. Last line: `aglod_test: N passed, M failed`.
#   bash Misc/quakevr/aglod_test.sh
set -u
K=C:/OHWorkspace/qvr-kit
W=$(cd "$(dirname "$0")/../.." && pwd)
NAME=$(basename "$W")
OUT=$W/scratch/aglod_test
mkdir -p "$OUT"
S="developer 0;skill 1;coop 0;maxplayers 1"
for c in "vr_agrunt_pose_lod 1;vr_agrunt_pose_lod_frames 8" "vr_agrunt_pose_lod 1;vr_agrunt_pose_lod_frames 2" "vr_agrunt_pose_lod 0;vr_agrunt_pose_lod_frames 8" "vr_agrunt_think_lod 0;vr_agrunt_pose_lod 1;vr_agrunt_pose_lod_frames 8"; do
    S="$S;$c;echo aglod_test: run $c;sv_random_seed 7;map vrarena;wait30;god;vr_agrunt_test 228;vr_waitprint 3000 \"test 228 over\""
done
# (229: a worn pack's LOD as its wearer walks into view; the pack's near distance set here, QC's cvar_set does not take)
S="$S;vr_agrunt_think_lod 1;vr_agrunt_pose_lod 1;vr_agrunt_pose_lod_frames 8;sv_random_seed 7;map vrarena;wait30;god;vr_worn_backpack_lod_near 128"
S="$S;vr_agrunt_test 229;vr_waitprint 3000 \"test 229 over\";vr_worn_backpack_lod_near 1500;echo aglod_test: done;toggleconsole;quit"
bash "$K/run.sh" "$NAME" -Timeout 600 -QuitOn "aglod_test: done" -Script "$S" \
    -Filter "aglod|agtest: (PASS|FAIL)|test 22[89] over|Host_Error|SV_Error|TIMEOUT|timed out" > "$OUT/run.log" 2>&1
python -I "$W/Misc/quakevr/aglod_test.py" "$OUT/run.log"
