#!/bin/bash
# The small gibs' tests (docs/vr-port/ROUND21.md, "Small gibs"), with the agent kit's run.sh: bash Misc/quakevr/smallgibs_tests.sh <agent>
A=${1:-smallgibs}
K=C:/OHWorkspace/qvr-kit
F="sgibtest|rror|ENGINE|runaway"
echo "== live grunt: rates, quad, chainsaw, burst, cap, grace"
bash $K/run.sh $A -Filter "$F" -Script "map e1m1;god;notarget;vr_test_spawn 0;vr_test_spawn_dist 96;impulse 241;wait40;vr_smallgibs_test 1;wait3;vr_smallgibs_test 2;wait3;vr_smallgibs_test 3;wait3;vr_smallgibs_test 4;wait3;vr_smallgibs_test 5;wait3;vr_smallgibs_test 6;wait90;vr_smallgibs_test 7;wait60;vr_smallgibs_test 8;wait3;vr_smallgibs_test 10;wait80;vr_smallgibs_test 11;wait3;toggleconsole;quit" 2>&1 | grep "sgibtest: [a-z0-9]" | grep -v "sgibtest: test "
echo "== corpse: chainsaw, super shotgun"
bash $K/run.sh $A -Filter "$F" -Script "map e1m1;god;notarget;vr_test_spawn 0;vr_test_spawn_dead 1;vr_test_spawn_dist 96;impulse 241;wait300;vr_smallgibs_test 6;wait90;vr_smallgibs_test 2;wait30;toggleconsole;quit" 2>&1 | grep "sgibtest: [a-z0-9]" | grep -v "sgibtest: test "
echo "== held, thrown into a wall"
bash $K/run.sh $A -Filter "$F" -Script "map e1m1;god;notarget;+graboff;wait5;vr_smallgibs_test 9;wait216;-graboff;wait700;vr_smallgibs_test 12;wait90;toggleconsole;quit" 2>&1 | grep "sgibtest: [a-z0-9]" | grep -v "sgibtest: test "
echo "== flight by situation: launch speeds and where they lie 2.5 s on (a grunt, then a corpse's gibbings)"
bash $K/run.sh $A -Filter "$F" -Script "map e1m1;god;notarget;vr_test_spawn 0;vr_test_spawn_dist 96;impulse 241;wait40;vr_smallgibs_test 13;wait1500;toggleconsole;quit" 2>&1 | grep "sgibtest: speeds" | sed 's/sgibtest: speeds: //'
bash $K/run.sh $A -Filter "$F" -Script "map e1m1;god;notarget;vr_test_spawn 0;vr_test_spawn_dead 1;vr_test_spawn_dist 96;impulse 241;wait300;vr_smallgibs_test 13;wait1500;toggleconsole;quit" 2>&1 | grep "sgibtest: speeds" | sed 's/sgibtest: speeds: //' | grep -A1 "gibbed by"
