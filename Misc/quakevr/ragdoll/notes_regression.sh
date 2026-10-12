#!/bin/bash
# notes_regression.sh <agent>: visual-size shove reach, selective blunt corpse impacts, and physical self-collision.
set -e
AGENT=${1:?worktree name}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
for d in 64 100 160 240; do
    echo "VORE $d"
    bash "$KIT/run.sh" "$AGENT" -Script "developer 1;sv_random_seed 7;map vrtesthall;wait30;god;vr_enemy_shove_damage 0;vr_enemy_shove_distance 0;vr_test_spawn 10;vr_test_spawn_dist $d;impulse 241;wait260;toggleconsole;quit" -Filter "enemy shove:|Error|CRASH" -Timeout 180
done
for n in 24 25 26 27; do
    echo "CORPSE $n"
    bash "$KIT/run.sh" "$AGENT" -Script "developer 1;sv_random_seed 7;map vrfiringrange;wait30;god;notarget;vr_test_spawn 0;vr_test_spawn_dist 90;impulse 241;wait30;vr_test_kill 1;wait160;vr_limb_test $n;wait20;vr_ragdoll_list;toggleconsole;quit" -Filter "corpse limb:|limbtest:|^ragdoll [0-9]|self:|unstable|Error|CRASH" -Timeout 180
done
bash "$TREE/Misc/quakevr/ragdoll/selfcollide_test.sh" "$AGENT" overlap rest
# Driven reaches must stay stable when their hands meet the torso, including the single-arm and forearm stumps.
MONS=0 bash "$TREE/Misc/quakevr/ragdoll/dismember_test.sh" "$AGENT" arm forearm
