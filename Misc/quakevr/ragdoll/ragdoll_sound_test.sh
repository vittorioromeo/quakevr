#!/bin/bash
# Ragdoll impact sounds (ROUND21.md, "Ragdoll impact sounds"): each case's knocks (vr_debug_physsound 1; DBG=2 the
# skipped ones too), their times and volumes. Usage: ragdoll_sound_test.sh [flat stairs blast pile]; the agent name in AGENT.
KIT=C:/OHWorkspace/qvr-kit
PRE="wait30;god;notarget;vr_ragdoll 1;vr_debug_physsound ${DBG:-1};vr_test_spawn 0"
DEAD="vr_test_spawn_dead 1;impulse 241;wait3;vr_test_spawn_dead 0"
run() { bash $KIT/run.sh ${AGENT:-audioengine} -Script "$1" -Filter "physsound:|rror|PILE|backpack|TIME" -Timeout 300 2>&1 | grep -v "^$"; }
for c in ${*:-flat stairs blast pile}; do
  echo "== $c"
  case $c in
  flat) run "map e1m1;$PRE;vr_test_spawn_dist 90;$DEAD;wait720;toggleconsole;quit" ;;
  stairs) run "map vrclimb;$PRE;setpos -430 704 24 0 0 0;wait5;vr_test_spawn_dist 90;$DEAD;wait720;toggleconsole;quit" ;;
  blast) run "map e1m1;$PRE;vr_test_spawn_dist 90;impulse 241;wait30;vr_physics_blast 500 -255 50 60;wait720;toggleconsole;quit" ;;
  pile) S="map e1m1;$PRE;vr_ragdoll_collide_each 1;vr_test_spawn_dist 70"; for i in 1 2 3 4 5 6; do S="$S;$DEAD;wait40"; done; run "$S;echo PILE_SETTLING;wait720;impulse 243;wait60;toggleconsole;quit" ;;
  esac
done
