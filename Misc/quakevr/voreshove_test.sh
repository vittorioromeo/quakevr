#!/bin/bash
# voreshove_test.sh <agent>: a vore spawned touching the player (vrtesthall, god, the author's shove settings), the
# player walking into it for ~20 s of game time: it must shove him again and again (ROUND22.md, "The vore never
# shoves"), not only throw balls. Fails below 4 shoves (before the fix: 2, one at the start and one 16 s later).
AGENT=${1:?worktree name}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
OUT="$TREE/scratch/voreshove_test.txt"
mkdir -p "$TREE/scratch"
bash "$KIT/run.sh" "$AGENT" -Script "developer 1;sv_random_seed 7;map vrtesthall;wait30;god;vr_stealth_meter 1;vr_enemy_shove 1;vr_enemy_shove_vore 1;vr_enemy_shove_range 50;vr_enemy_shove_delay 0.5;vr_enemy_shove_cooldown 1.5;vr_enemy_shove_distance 128;vr_test_spawn 10;vr_test_spawn_dist 52;impulse 241;wait5;+forward;wait1500;toggleconsole;quit" -Filter "enemy shove:|Error|CRASH" -Timeout 300 > "$OUT"
SHOVES=$(grep -c "shoves the player" "$OUT" || true)
grep -v "can't shove" "$OUT" | head -12
echo "vore shoves: $SHOVES"
if [ "$SHOVES" -lt 4 ]; then
    echo "VORE SHOVE FAILED"
    exit 1
fi
echo "VORE SHOVE PASSED"
