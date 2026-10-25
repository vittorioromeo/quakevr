#!/bin/bash
# carry_water_test.sh <agent> [release] -- a grunt knocked down on vrtesthall's floor beside the pool, its ragdoll taken by
# the main hand at its head and carried into the pool (the player walks in); release: let go of in the water at once
# (else held until it gets up). Logs where it lay and where it stood up (handback lines).
AGENT=${1:?}
MODE=${2:-hold}
KIT=C:/OHWorkspace/qvr-kit
if [ "$MODE" = release ]; then LET="wait120;-grabmain;echo RELEASED;vr_knockdown_test 5;wait900"; else LET="wait900;-grabmain"; fi
PRE="wait30;god;notarget;developer 1;vr_knockdown_debug ${KDD:-1};vr_ragdoll_active_seed 0;sv_random_seed 7"
S="map vrtesthall;$PRE;vr_mock_walk_to -20 -380 16;wait150;vr_mock_walk_to 40 -380 16;wait100;vr_mock_walk_to off;wait5;vr_foegrab_status;vr_test_spawn 0;vr_test_spawn_dist 80;impulse 241;wait60"
S="$S;vr_knockdown_push 0;vr_knockdown_time_min ${KDT:-5};vr_knockdown_time_max ${KDT:-5};vr_knockdown_test 0;wait60"
S="$S;vr_mock_walk_to nearest monster_army 24;wait200;vr_mock_walk_to off;wait30;vr_knockdown_test 5"
S="$S;vr_mock_hand_to main ragdoll head 0;wait3;+grabmain;wait30;echo GRABBED;vr_foegrab_status"
S="$S;vr_mock_walk_to 400 -380 16;wait240;vr_mock_walk_to off;echo INWATER;vr_knockdown_test 5;vr_foegrab_status;$LET"
S="$S;vr_knockdown_test 5;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "handback|getup|knockdown:|swim|walk_to|taken by|let go|flail|GRABBED|INWATER|RELEASED|foegrab|rror|CRASH|TIMEOUT" -Timeout 300
