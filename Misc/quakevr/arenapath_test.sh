#!/bin/bash
# arenapath_test.sh <agent>: how long the waves' walkers take between vrarena's floor and its dais, and where they stall
# (vr_arena_test 36, vr_arena_test.qc). One foe at a time (grunt at fists, knight, ogre, dog), up and down from 8 places,
# in the fixed game clock. Ends on the test's own verdict line; a foe not there within 25 game-seconds fails it.
# APF overrides the output filter.
KIT=C:/OHWorkspace/qvr-kit
AGENT=${1:-armor}
export QVR_AUTHOR_WAIT_ALWAYS=1
bash $KIT/run.sh $AGENT -Script "sv_random_seed 7;developer 1;map vrarena;wait30;god;vr_arena_test 36" -Filter "${APF:-arenapath|arena: (FAIL|test 36)}" -QuitOn "arena: test 36 over" -Timeout ${APT:-900}
