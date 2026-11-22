#!/bin/bash
# rope_sound_test.sh <agent> -- a climbable chain's own sounds, a rope's kept (ropes5; Quake/vr/vr_physsound.cpp ropeGrab,
# chainMoves; vr_climbrope.cpp grabbed; docs/vr-port/ROPES.md, "Tests"), headless, in vrropes. Prints PASS/FAIL lines and
# a verdict, exits 1 on a failure.
#   vr_rope_sound_test: a rope and a chain made ahead; still: quiet; a headset's grip (the rope silent, the chain's metal
#   grip), a flat-screen one (the rope's slap, the chain's metal grip; a second at once held back); both pushed swinging
#   (the chain's links clink, 2 to 9 in 2 s; the rope quiet).
AGENT=${1:-posebake}
QT_NAME=rope_sound
source "$(dirname "$0")/../qvrtest.sh"
out=$(bash $KIT/run.sh $AGENT -QuitOn "^vr_rope_sound_test: [0-9]+ passed" -Timeout 180 \
    -Filter "^vr_rope_sound_test" -Script "map vrropes; wait60; vr_rope_sound_test" 2>&1)
echo "$out" | grep -a "^vr_rope_sound_test: \(PASS\|FAIL\)"
expect_verdict "$out" "vr_rope_sound_test" "a chain's metal grip and clinks, a rope's slap"
verdict; exit $fail
