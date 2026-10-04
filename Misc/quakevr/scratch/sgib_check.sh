#!/bin/bash
# Test runs for brain chunks' own throw and the per-enemy gib count multipliers.
# Each measurement is on a fresh map (the QC counters start again); every cvar under test is set explicitly.
K=C:/OHWorkspace/qvr-kit
F='sgibtest|sgibtrace|decaptest|rror'
DEF="vr_smallgibs_mult_grunt 1; vr_smallgibs_brains_mult_grunt 1; vr_smallgibs_mult_enforcer 1; vr_smallgibs_brains_speed 3; vr_smallgibs_brains_up 7; vr_smallgibs_speed 3; vr_smallgibs_up 7"
G="vr_test_spawn 0; vr_test_spawn_dist 96; impulse 241; wait60; vr_smallgibs_test 16; wait60;"
E="vr_test_spawn 8; vr_test_spawn_dist 96; impulse 241; wait60; vr_smallgibs_test 16; wait60;"
R() { bash $K/run.sh gib-mults -Timeout 400 -Filter "$F" -Script "$1"; }

echo "=== 1: counts (fresh map each): grunt 1x, enforcer 1x, grunt 2x, enforcer 2x ==="
R "$DEF; map e1m1; wait60; $G map e1m1; wait60; $E map e1m1; wait60; vr_smallgibs_mult_grunt 2; vr_smallgibs_brains_mult_grunt 2; $G map e1m1; wait60; vr_smallgibs_mult_grunt 2; vr_smallgibs_brains_mult_grunt 2; $E toggleconsole; quit"

echo "=== 2: brain chunk flight, defaults (brains_speed 3, brains_up 7) ==="
R "$DEF; map e1m1; wait60; vr_smallgibs_trace 1; vr_smallgibs_test 21; wait60; toggleconsole; quit"

echo "=== 3: brain chunk flight, brains_speed 6, brains_up 0 ==="
R "$DEF; vr_smallgibs_brains_speed 6; vr_smallgibs_brains_up 0; map e1m1; wait60; vr_smallgibs_trace 1; vr_smallgibs_test 21; wait60; toggleconsole; quit"

echo "=== 4: a grunt head pop (decap test 12), defaults ==="
R "$DEF; map e1m1; wait60; vr_test_spawn 0; vr_test_spawn_dist 96; impulse 241; wait60; vr_decap_test 12; wait120; vr_smallgibs_test 11; wait60; toggleconsole; quit"

echo "=== 5: a grunt head pop, mult_grunt 2 and brains_mult_grunt 2 ==="
R "$DEF; vr_smallgibs_mult_grunt 2; vr_smallgibs_brains_mult_grunt 2; map e1m1; wait60; vr_test_spawn 0; vr_test_spawn_dist 96; impulse 241; wait60; vr_decap_test 12; wait120; vr_smallgibs_test 11; wait60; toggleconsole; quit"
