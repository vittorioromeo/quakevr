#!/bin/bash
# The ranged aim cone test (QC vr_aimcone_test.qc, vr_test_aimcone; ROUND22.md, "Ranged aim cone"): for each test
# monster (vr_test_spawn kinds) and each request (distance * 1000 + degrees), the monster's ranged attack with the player
# held that many degrees off its facing; prints each "aimcone: result" line (shots, the first's angle off and bend,
# waits, hits) and, with LOG=1, every shot's line.
# Usage: aim_cone_test.sh <worktree> [id|ad] [monster kinds] [requests]
#   aim_cone_test.sh dequant id "0" "300000 300030 300060 300090 64010"
AGENT=${1:?worktree name}
SET=${2:-id}
MONSTERS=${3:-0}
REQS=${4:-300000 300030 300060 300090}
ON=${ON:-1}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
mkdir -p "$TREE/scratch"
if [ "$SET" = ad ]; then
    PRE="skill 1;vr_campaign_native ad;wait30;map start;wait60"
else
    PRE="skill 1;map vrtesthall;wait30"
fi
S="developer 1;sv_random_seed 7;vr_enemy_aim_cone $ON;vr_enemy_aim_debug ${LOG:-0}"
for M in $MONSTERS; do
    for R in $REQS; do
        S="$S;$PRE;vr_test_spawn $M;vr_test_spawn_dist 200;impulse 241;wait20;vr_test_aimcone $R;wait400"
    done
done
S="$S;toggleconsole;quit"
OUT="$TREE/scratch/aim_cone_${SET}_$ON.txt"
bash "$KIT/run.sh" "$AGENT" -Script "$S" -Filter "aimcone:|CRASH|Host_Error|SYS_ERROR" -Timeout 1200 > "$OUT" 2>&1
grep "aimcone: \(result\|start\|.*waits\|.*waited\|.*shot\|no \)\|CRASH\|Error" "$OUT" | sed 's/^.*aimcone: //'
