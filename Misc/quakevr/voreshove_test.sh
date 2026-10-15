#!/bin/bash
# voreshove_test.sh <agent>: a vore spawned touching the player (vrtesthall, god, the author's shove settings), the
# player walking into it for ~35 s of game time (margin: at Body Inset 4 the vores stand closer and die to their own splash, 3-4 shoves in 20 s): it must shove him again and again (ROUND22.md, "The vore never
# shoves"), not only throw balls. Fails below 4 shoves (before the fix: 2, one at the start and one 16 s later).
AGENT=${1:?worktree name}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
OUT="$TREE/scratch/voreshove_test.txt"
mkdir -p "$TREE/scratch"
# The walk: 1500 frames (~21 s) as before with TB_SLOW=1; batched, until the 4th shove (vr_waitprint 2500x4: at
# most as long; the count printed is then 4, the verdict the same).
WALK=wait2500
export TB_FIXED=${TB_FIXED-0} # (real-time frames batched too: at fixed frames the vore shoves 3 times with seed 7, at the wall
#  clock 4 as alone; marginal, TESTING.md "Closer to enemies")
[ "$TB_SLOW" = "1" ] || WALK="vr_waitprint 2500x4 shoves the player"
# (The body: tb_main plays its run as a batch, testbatch.sh; TB_SLOW=1: alone by run.sh, as before.)
tb_body() {
tb_run_sh -Script "developer 1;sv_random_seed 7;map vrtesthall;wait30;god;vr_stealth_meter 1;vr_enemy_shove 1;vr_enemy_shove_vore 1;vr_enemy_shove_range 50;vr_enemy_shove_delay 0.5;vr_enemy_shove_cooldown 1.5;vr_enemy_shove_distance 128;vr_test_spawn 10;vr_test_spawn_dist 52;impulse 241;wait5;+forward;$WALK;toggleconsole;quit" -Filter "enemy shove:|Error|CRASH" -Timeout 300 > "$OUT"
SHOVES=$(grep -c "shoves the player" "$OUT" || true)
grep -v "can't shove" "$OUT" | head -12
echo "vore shoves: $SHOVES$([ "$TB_SLOW" = "1" ] || echo " (batched: the walk ends at the 4th)")"
if [ "$SHOVES" -lt 4 ]; then
    echo "VORE SHOVE FAILED"
    exit 1
fi
echo "VORE SHOVE PASSED"
}
source "$(dirname "$0")/testbatch/testbatch.sh"
tb_main "$AGENT" tb_body "$@"
