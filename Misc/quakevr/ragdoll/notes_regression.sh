#!/bin/bash
# notes_regression.sh <agent>: visual-size shove reach, selective blunt corpse impacts, and physical self-collision.
# The corpse impact cases (vr_limb_test 24-28, and 29 after a save/load) assert: any FAIL or missing PASS fails the script.
set -e
AGENT=${1:?worktree name}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
OUT="$TREE/scratch/notes_regression_corpse.txt"
mkdir -p "$TREE/scratch"
for d in 64 100 160 240; do
    echo "VORE $d"
    bash "$KIT/run.sh" "$AGENT" -Script "developer 1;sv_random_seed 7;map vrtesthall;wait30;god;vr_enemy_shove 1;vr_enemy_shove_vore 1;vr_enemy_shove_range 52;vr_enemy_shove_damage 0;vr_enemy_shove_distance 0;vr_test_spawn 10;vr_test_spawn_dist $d;impulse 241;wait260;toggleconsole;quit" -Filter "enemy shove:|Error|CRASH" -Timeout 180
done
SETUP="developer 1;sv_random_seed 7;map vrfiringrange;wait30;god;notarget;vr_limbs 1;vr_limbs_corpses 1;vr_ragdoll 1;vr_ragdoll_self_collide 1;vr_test_spawn 0;vr_test_spawn_dist 90;impulse 241;wait30;vr_test_kill 1;wait160"
: > "$OUT"
for n in 24 25 26 27 28; do
    echo "CORPSE $n"
    bash "$KIT/run.sh" "$AGENT" -Script "$SETUP;vr_limb_test $n;wait20;vr_ragdoll_list;toggleconsole;quit" -Filter "corpse limb:|limbtest:|^ragdoll [0-9]|self:|unstable|Error|CRASH" -Timeout 180 | tee -a "$OUT"
done
echo "CORPSE 28 + save/load + 29"
bash "$KIT/run.sh" "$AGENT" -Script "$SETUP;vr_limb_test 28;wait20;save notes_limbdmg;wait10;load notes_limbdmg;wait40;vr_limb_test 29;wait10;toggleconsole;quit" -Filter "limbtest: 2[89]|Error|CRASH" -Timeout 180 | tee -a "$OUT"
PASSES=$(grep -c "PASS" "$OUT" || true)
if grep -q "FAIL" "$OUT" || [ "$PASSES" -lt 7 ]; then
    echo "CORPSE IMPACTS FAILED ($PASSES/7 PASS)"
    exit 1
fi
echo "CORPSE IMPACTS PASS ($PASSES/7)"
bash "$TREE/Misc/quakevr/ragdoll/selfcollide_test.sh" "$AGENT" overlap rest
# Driven reaches must stay stable when their hands meet the torso, including the single-arm and forearm stumps.
MONS=0 bash "$TREE/Misc/quakevr/ragdoll/dismember_test.sh" "$AGENT" arm forearm
SELF=1 bash "$TREE/Misc/quakevr/ragdoll/ragtest_test.sh" "$AGENT" stagger getup pullup guard
SELF=1 MON=5 bash "$TREE/Misc/quakevr/ragdoll/ragtest_test.sh" "$AGENT" getup
