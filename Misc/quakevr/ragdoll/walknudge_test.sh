#!/bin/bash
# walknudge_test.sh <agent>: walking through a (non-blocking, "Walk Through") ragdoll nudges its limbs gently
# (vr_box3d_walk_nudge 1): its pelvis moves a little along his pace (0.5 to 12 units: no excessive push); with 0 it
# stays put (under 0.1). His walk is the same either way (where he ends, within 4 units). Before the fix (ROUND22.md,
# "Walking nudges") the nudge was weaker than the floor's friction and the pelvis moved 0.0 with it on.
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
# The pelvis before and after each walk, and where he ended.
awk '
/NUDGE_[01]_BEFORE/ { n = substr($1, 7, 1); when = "b" }
/NUDGE_[01]_AFTER/ { when = "a" }
/^ragdoll [0-9]/ { for (i = 1; i <= NF; i++) if ($i == "pelvis") { x[n when] = $(i + 1); y[n when] = $(i + 2) } }
/foegrab status: player 1 at/ { end[n] = $7 }
END {
  ok = 1
  for (k = 1; k >= 0; k--) {
    d = sqrt((x[k "a"] - x[k "b"]) ^ 2 + (y[k "a"] - y[k "b"]) ^ 2)
    printf "walk_nudge %d: pelvis moved %.1f units, he ended at y %s\n", k, d, end[k]
    if (k == 1 && (d < 0.5 || d > 12)) ok = 0
    if (k == 0 && d > 0.1) ok = 0
  }
  dy = end[1] - end[0]; if (dy < 0) dy = -dy
  if (dy > 4 || end[1] == "") ok = 0
  print (ok ? "WALKNUDGE PASSED" : "WALKNUDGE FAILED")
  exit !ok
}' "$OUT"
