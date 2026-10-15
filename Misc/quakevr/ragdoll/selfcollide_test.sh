#!/bin/bash
# selfcollide_test.sh <agent> [cases] -- ragdolls' self-collision (vr_ragdoll_self_collide; ROUND22.md, "Ragdoll tests,
# tuning, self-collision"), each case with it off (0) then on (1):
#   cost   8 grunts in a row on vrfiringrange killed at once (active deaths; --exclusive): the physics step's ms (mean,
#          p95, worst; its pairs phase, where the self-collision filter runs) while they fall (60 frames) and settle (300)
#   pile   4 grunts on one spot killed together, ragdolls meeting each other: the step's ms, then after 10 s each
#          ragdoll's parts awake, its pelvis's height and lowest/highest points (vr_ragdoll_list: at rest, nothing
#          sunk, no part flung)
#   overlap 8 monsters (grunts, enforcers, knights, a death knight, an ogre) killed by body, head, arm and leg shots: after
#          8 s, how many of each ragdoll's unjointed part pairs overlap (vr_ragdoll_list's self: lines, counted)
#   rest   one grunt knocked down (alive, lying): vr_knockdown_test 9's motion from 1.5 to 2.5 s (its parts' average and
#          most movement and spin while it lies struggling: a jitter would add to it)
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
CASES=${*:-cost pile overlap rest}
for c in $CASES; do
    for s in 0 1; do
        echo "== $c, vr_ragdoll_self_collide $s"
        case $c in
        cost)
            S="sv_random_seed 7;map vrfiringrange;wait30;god;notarget;vr_ragdoll_max 8;vr_ragdoll_self_collide $s;vr_test_spawn 0"
            for d in 80 120 160 200 240 280 320 360; do S="$S;vr_test_spawn_dist $d;impulse 241;wait2"; done
            S="$S;wait40"
            for i in 1 2 3 4 5 6 7 8; do S="$S;vr_test_kill 1;wait1"; done
            S="$S;wait4;vr_physics_steptime;wait60;vr_physics_steptime bins;wait300;vr_physics_steptime bins;toggleconsole;quit"
            bash $KIT/run.sh --exclusive $AGENT -Script "$S" -Filter "vr_physics_steptime:|profile|rror|CRASH" -Timeout 300 2>&1 |
                tr -d '\r' | grep -v "^$" | sed -E 's/\(setup.*\)$//' ;;
        pile)
            S="sv_random_seed 7;map e1m1;wait30;god;notarget;vr_ragdoll 1;vr_ragdoll_self_collide $s;vr_ragdoll_collide_each 1"
            S="$S;vr_test_spawn 0;vr_test_spawn_dist 90"
            for i in 1 2 3 4; do S="$S;impulse 241;wait2;vr_test_kill 1;wait3"; done
            S="$S;wait60;vr_physics_steptime;wait700;vr_ragdoll_list;toggleconsole;quit"
            bash $KIT/run.sh $AGENT -Script "$S" -Filter "vr_physics_steptime:|^ragdoll |self:|rror|CRASH" -Timeout 300 2>&1 |
                tr -d '\r' | grep -v "^$" | head -12 ;;
        overlap)
            S="sv_random_seed ${SEED:-7};map vrfiringrange;wait30;god;notarget;vr_ragdoll_max 8;vr_ragdoll_self_collide $s"
            d=80
            for m in 0 8 5 6 1 0 5 8; do S="$S;vr_test_spawn $m;vr_test_spawn_dist $d;impulse 241;wait2"; d=$((d + 60)); done
            S="$S;wait40"
            for i in 1 2 3 4 5 6 7 8; do S="$S;vr_test_kill $(( (i % 4) + 1 ));wait1"; done
            S="$S;wait600;vr_ragdoll_list;toggleconsole;quit"
            bash $KIT/run.sh $AGENT -Script "$S" -Filter "self:|^ragdoll [0-9]|rror|CRASH" -Timeout 300 2>&1 | tr -d '\r' |
                grep -E "self:|rror|CRASH" | sed -E 's/ \(Limbs.*//' | sort | uniq -c ;;
        rest)
            S="sv_random_seed 7;map vrfiringrange;wait30;god;notarget;vr_ragdoll_self_collide $s;vr_test_spawn 0;vr_test_spawn_dist 110"
            S="$S;impulse 241;wait40;vr_knockdown_test 0;wait108;vr_knockdown_test 9;wait72;vr_knockdown_test 9;toggleconsole;quit"
            bash $KIT/run.sh $AGENT -Script "$S" -Filter "strugglemotion|wigglecheck|rror|CRASH" -Timeout 300 2>&1 |
                tr -d '\r' | grep -v "^$" | tail -3 ;;
        esac
    done
done
