#!/bin/bash
# rope_flat_test.sh <agent> -- flatscreen play's climbable ropes (-flatscreenmode; vr_flat_rope; Quake/vr/vr_climb.cpp
# flatRope, flatRopeFrame; docs/vr-port/ROPES.md, "Climbing (flat-screen)"), headless, in vrropes. Prints PASS/FAIL lines
# and a verdict, exits 1 on a failure.
#   vr_rope_flat_test: a rope hung near the floor walked into with forward held (taken), climbed with forward looking
#   level (the feet up 80, hand over hand, both hands drawn on it), climbed down with back looking up (off it standing;
#   ropes8a: the keys climb whatever the pitch); then rope to rope on the swinging row: set in the air by its first rope
#   (taken), facing across the row the strafe key pumps the swing towards the second, jump at
#   the swing's forward peak lets go, the second caught in the air. (Climbing stamina off: the pump takes a while.)
# rope_flat_test.sh <agent> pole (ropes7): a wooden pole standing there instead: walked into, up, down, it never moved.
# rope_flat_test.sh <agent> strung (ropes7): a rope strung level overhead: jumped up to, hand over hand along it both
#   ways (the forward and back keys), the legs hanging, dropped from (crouch), jumped off.
# rope_flat_test.sh <agent> strungpole (ropes8a): the same on a level pole (it never sags).
# rope_flat_test.sh <agent> chain (ropes8b): a chain walked into and climbed (it clinks), held still 2 s (no clink).
# (ropes8b) Every mode hanging still checks the hip holsters stay on the hips; strung, the hands overhand under it.
# `vr_rope_flat_test <mode> shots` (in the console) screenshots the body from outside while hanging.
AGENT=${1:-posebake}
MODE=${2:-}
QT_NAME=rope_flat${MODE:+_$MODE}
source "$(dirname "$0")/../qvrtest.sh"
out=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -QuitOn "^vr_rope_flat_test: [0-9]+ passed" -Timeout 240 \
    -Filter "^vr_rope_flat_test" -Script "map vrropes; wait60; god; notarget; vr_climb_stamina 0; vr_rope_flat_test $MODE" 2>&1)
echo "$out" | grep -a "^vr_rope_flat_test: \(PASS\|FAIL\)"
expect_verdict "$out" "vr_rope_flat_test" "flat ropes ${MODE:-rope}: walked into, up, down, rope to rope"
verdict; exit $fail
