#!/bin/bash
# walknudge_test.sh <agent>: walking through a (non-blocking, "Walk Through") ragdoll nudges its limbs gently
# (vr_box3d_walk_nudge 1): it moves a little along his pace, but not far (no excessive push); with 0 it stays put.
set -eu
AGENT=${1:?worktree name}
KIT=C:/OHWorkspace/qvr-kit
TREE=C:/OHWorkspace/qvr-agents/$AGENT
OUT=$TREE/scratch/walknudge_test.txt
: > "$OUT"
for n in 1 0; do
  S="developer 1;sv_random_seed 7;map vrtesthall;wait30;god;notarget;vr_test_spawn_dist 70;vr_corpse_collide_player 0;vr_box3d_walk_nudge $n;vr_test_spawn 0;impulse 241;wait40;vr_test_kill 1;wait600;echo NUDGE_${n}_BEFORE;vr_ragdoll_list;vr_foegrab_status;vr_mock_stick off 0 1;wait25;vr_foegrab_status;wait25;vr_foegrab_status;wait25;vr_foegrab_status;vr_mock_stick off 0 0;wait100;echo NUDGE_${n}_AFTER;vr_ragdoll_list;toggleconsole;quit"
  bash "$KIT/run.sh" "$AGENT" -ExtraArgs '-nomapindex' -Script "$S" -Filter "NUDGE_|ragdoll [0-9]|foegrab status: player|Error|CRASH" -Timeout 240 >> "$OUT" 2>&1
done
cat "$OUT" | cut -c1-200
