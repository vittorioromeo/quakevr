#!/bin/bash
# corpse_notes_test.sh <agent>: AD artifacts, shoulder contact, repeated dummy recovery.
set -eu
AGENT=${1:?worktree name}
KIT=C:/OHWorkspace/qvr-kit
TREE=C:/OHWorkspace/qvr-agents/$AGENT
OUT=$TREE/scratch/corpse_notes_test.txt
S='developer 1;sv_random_seed 7;map vrtesthall;wait30;god;notarget;setpos -150 0 24 0 90 0;vr_test_spawn_dist 110;vr_ragdoll_self_collide 1'
for m in 211 212 217 220 223 227 232 235; do
 S="$S;echo AD_CORPSE_$m;vr_knockdown_test 20;vr_scene_clean all;wait5;vr_test_spawn $m;impulse 241;wait40;vr_test_kill 1;wait700;vr_ragdoll_list 1"
done
S="$S;toggleconsole;quit"
bash "$KIT/run.sh" "$AGENT" -Games '-game ad -game quakevr' -ExtraArgs '-nomapindex' -Script "$S" -Filter 'AD_CORPSE_|adcorpse:|ragdoll:|hidden in|ragdoll [0-9]|self:|arm interiors:|kg at|unstable|Error|CRASH|exit=' -Timeout 420 > "$OUT"
S='developer 1;sv_random_seed 7;map vrtesthall;wait30;god;notarget;setpos -150 0 24 0 90 0;vr_test_spawn_dist 110'
for c in 0 1; do
 for hit in 1 2 3 4; do
  S="$S;echo SHOULDER_${c}_$hit;vr_knockdown_test 20;vr_scene_clean all;wait5;vr_ragdoll_self_collide $c;vr_test_spawn 0;impulse 241;wait40;vr_test_kill $hit;wait600;vr_ragdoll_arm_test;vr_ragdoll_list;wait400;vr_ragdoll_list 1"
 done
done
S="$S;toggleconsole;quit"
bash "$KIT/run.sh" "$AGENT" -ExtraArgs '-nomapindex' -Script "$S" -Filter 'SHOULDER_|armtest:|ragdoll [0-9]|self:|arm interiors:|kg at|unstable|Error|CRASH|exit=' -Timeout 420 >> "$OUT"
S='developer 1;sv_random_seed 7;map vrmelee-recording;wait30;god;notarget;vr_dummy_type 0;vr_ragdoll_self_collide 1;vr_dummy_test 4;wait5'
for active in 0 1; do
 S="$S;vr_ragdoll_active_getup $active"
 for attempt in 1 2 3; do
  S="$S;echo DUMMY_${active}_$attempt;vr_knockdown_test 0;wait100;vr_knockdown_test 1;wait900;vr_dummy_test 4;wait5"
 done
done
S="$S;toggleconsole;quit"
bash "$KIT/run.sh" "$AGENT" -ExtraArgs '-nomapindex' -Script "$S" -Filter 'DUMMY_|dummytest:|knockdown:|Error|CRASH|exit=' -Timeout 420 >> "$OUT"
grep -E 'AD_CORPSE_|adcorpse:|^ragdoll [0-9]|SHOULDER_|arm interiors:|DUMMY_|dummytest: recovery|Error|CRASH|exit=' "$OUT"
