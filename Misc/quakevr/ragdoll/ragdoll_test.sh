#!/bin/bash
# ragdoll_test.sh <agent> [flat stairs blast shot gib cap save] -- ragdolls (ROUND21.md, "Ragdolls"; vr_ragdoll 1): a grunt
# killed ahead goes limp and is followed to rest; each scenario prints its ragdolls (vr_ragdoll_list: parts awake, where
# the pelvis lies, how high the parts' lowest and highest points are) and what happened to them:
#   flat   e1m1's start: killed 90 units ahead (a side eyeshot from the firing range: EYES=1)
#   stairs vrclimb's stairs (12 steps of 16): killed at their top, he falls down them and rests across steps
#   blast  e1m1: a live grunt killed by a 60-point blast beside him: the blast throws his ragdoll as it is made
#   shot   e1m1: his ragdoll shot with the shotgun (each pellet's push, his precise hit's corpse damage)
#   gib    e1m1: his ragdoll blown up (200): gibbed, the ragdoll gone
#   cap    e1m1: 10 grunts killed, at most 8 ragdolls (vr_ragdoll_max): 8 ragdolls and 2 corpses; the step's cost
#   save   e1m1: saved and loaded with a ragdoll lying there: made again from his frame where he lay
# EYES=1 also takes eyeshots (flat, stairs, blast) into the kit's scratch (ragdoll_flat.png, ragdoll_stairs.png,
# ragdoll_blast.png).
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
CASES=${*:-flat stairs blast shot gib cap save}
PRE="wait30;god;notarget;vr_ragdoll 1;vr_debug_ragdoll 1;vr_test_spawn 0"
DEAD="vr_test_spawn_dead 1;impulse 241;wait3;vr_test_spawn_dead 0"
FILTER="^ragdoll|^vr_ragdoll_list|^vr_physics_steptime|corpses in the physics|gibbed|corpse: .* hit by|rror|CRASH|pushed at"
run() { bash $KIT/run.sh $AGENT -Script "$1" -Filter "$FILTER" -Timeout 180 ${2:+-Out $2} 2>&1 | grep -v "^$" | grep -v "part [0-9]* blasted"; }
for c in $CASES; do
    echo "== $c"
    case $c in
    flat)
        run "map e1m1;$PRE;vr_test_spawn_dist 90;$DEAD;wait20;vr_ragdoll_list;wait300;vr_ragdoll_list;toggleconsole;quit"
        if [ -n "$EYES" ]; then
            run "map vrfiringrange;$PRE;vr_test_spawn_dist 100;$DEAD;vr_mock_camera 1.6 0.4 -3.2 15 80;wait6;screenshot;wait12;screenshot;wait12;screenshot;wait24;screenshot;wait300;screenshot;toggleconsole;quit" ragdoll_flat.png
        fi ;;
    stairs)
        S="map vrclimb;$PRE;setpos -430 704 24 0 0 0;wait5;vr_test_spawn_dist 90;$DEAD;wait20;vr_ragdoll_list;wait400;vr_ragdoll_list 1"
        [ -n "$EYES" ] && S="$S;r_fullbright 1;vr_mock_camera 0.3 -1.8 -5.6 -17 180;wait3;screenshot;vr_debug_physics_shapes 1;wait2;screenshot"
        run "$S;toggleconsole;quit" ${EYES:+ragdoll_stairs.png} ;;
    blast)
        run "map e1m1;$PRE;vr_test_spawn_dist 90;impulse 241;wait30;vr_physics_blast 500 -255 50 60;wait20;vr_ragdoll_list 1;wait300;vr_ragdoll_list;toggleconsole;quit"
        if [ -n "$EYES" ]; then
            run "map vrfiringrange;$PRE;vr_test_spawn_dist 100;impulse 241;wait30;vr_mock_camera 1.6 0.4 -3.2 15 80;wait3;screenshot;vr_physics_blast 222 -585 30 65;wait12;screenshot;wait12;screenshot;wait24;screenshot;wait300;vr_mock_camera 0.3 1.7 0 18 12;wait2;screenshot;vr_ragdoll_list;toggleconsole;quit" ragdoll_blast.png
        fi ;;
    shot)
        run "map e1m1;$PRE;vr_weapon_grip_mode 1;impulse 9;wait5;impulse 154;wait5;vr_mock_hand main -0.05 1.3 -0.3 40 0 0;vr_test_spawn_dist 70;$DEAD;wait300;vr_ragdoll_list;+attack;wait3;-attack;wait100;vr_ragdoll_list;toggleconsole;quit" ;;
    gib)
        run "map e1m1;$PRE;vr_test_spawn_dist 70;$DEAD;wait300;vr_ragdoll_list;vr_physics_blast 475 -281 50 200;wait10;vr_ragdoll_list;toggleconsole;quit" ;;
    cap)
        SP="vr_test_spawn_dead 1;impulse 241;wait3"
        run "map e1m1;$PRE;vr_debug_ragdoll 0;vr_ragdoll_max 8;vr_test_spawn_dist 90;wait60;vr_physics_steptime;wait120;vr_physics_steptime;$SP;$SP;$SP;$SP;$SP;$SP;$SP;$SP;$SP;$SP;vr_test_spawn_dead 0;wait20;vr_physics_steptime;wait120;vr_physics_steptime;vr_ragdoll_list;wait400;vr_physics_steptime;wait120;vr_physics_steptime;vr_ragdoll_list;vr_corpse_list;toggleconsole;quit" | sed 's/^vr_physics_steptime/  steptime/' ;;
    save)
        run "map e1m1;$PRE;vr_test_spawn_dist 90;$DEAD;wait300;vr_ragdoll_list;save ragdolltest;wait10;load ragdolltest;wait60;vr_ragdoll_list;wait300;vr_ragdoll_list;toggleconsole;quit" ;;
    esac
done
