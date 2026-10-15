#!/bin/bash
# melee_arc_test.sh <agent> [id|ad] [arc ...]: monsters' melee blows against the melee arc (vr_enemy_melee_arc; ROUND22.md,
# "Melee arc"; QC vr_meleearc_test.qc). Per monster, a fresh map each: the monster set 56 units from the player and after
# him (vr_test_meleearc): 1 facing him, 2 its back to him, 3 facing him and he steps to its side (100 degrees) as its
# attack starts; the knight also from 100 units (1001, 1003: its run swing). One "meleearc: result" line each: hits on
# him in 4 s, the first one's time and the monster's angle off its facing then, and the hits after the step.
# id: vrtesthall (knight, hell knight, ogre, fiend, rottweiler); ad: AD's start (the same through AD's rules, AD's death
# knight 200). Run with arc 0 (Quake's blows) and 1 (default: both). PASS (arc 1): front hits > 0; behind: the first
# hit's angle within the arc; step aside: no hit after the step.
AGENT=${1:?worktree name}
SET=${2:-id}
shift 2 2>/dev/null
ARCS=${*:-0 1}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
mkdir -p "$TREE/scratch"
if [ "$SET" = ad ]; then
    PRE="skill 1;vr_campaign_native ad;wait30;map start;wait60"
    MONSTERS="5 200 1 9 7"
else
    PRE="skill 1;map vrtesthall;wait30"
    MONSTERS="5 6 1 9 7"
fi
for A in $ARCS; do
    S="developer 1;sv_random_seed 7;vr_enemy_melee_arc $A"
    for M in $MONSTERS; do
        MODES="1 2 3"
        [ "$M" = 5 ] && MODES="1 2 3 1001 1003"
        for MO in $MODES; do
            S="$S;$PRE;vr_test_spawn $M;vr_test_spawn_dist 128;impulse 241;wait20;vr_test_meleearc $MO;wait450"
        done
    done
    S="$S;toggleconsole;quit"
    bash "$KIT/run.sh" "$AGENT" -Script "$S" -Filter "meleearc: (result|start|stepped)|CRASH|Host_Error|SYS_ERROR" -Timeout 1200 > "$TREE/scratch/melee_arc_${SET}_$A.txt" 2>&1
    grep "meleearc: result\|meleearc: stepped\|CRASH\|Error" "$TREE/scratch/melee_arc_${SET}_$A.txt" | sed 's/^.*meleearc: //'
done
