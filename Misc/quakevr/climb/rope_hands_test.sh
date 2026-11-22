#!/bin/bash
# rope_hands_test.sh <agent> -- flat-screen play's drawn hands on a rope and two chains (ropes5; Quake/vr/vr_climb.cpp
# drawnHand, climb::ropePalm; docs/vr-port/ROPES.md, "Tests"), headless, in vrropes, the client drawing faster than the
# server (144 Hz, as a monitor or a headset: the case that showed the jitter). Prints PASS/FAIL lines and a verdict,
# exits 1 on a failure.
#   vr_rope_hands_test: the row's first rope, its first chain and the heavy chain, each taken in the air, hanging still
#   then pumped: each drawn frame, each hand's palm and arm (palm less shoulder) second difference (the jitter) and the
#   palm's middle off the rope as drawn.
AGENT=${1:-posebake}
QT_NAME=rope_hands
source "$(dirname "$0")/../qvrtest.sh"
out=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -QuitOn "^vr_rope_hands_test: [0-9]+ passed" -Timeout 240 \
    -Filter "^vr_rope_hands_test" -Script "vr_fixed_frames_rate 144; map vrropes; wait60; god; notarget; vr_rope_hands_test" 2>&1)
echo "$out" | grep -a "^vr_rope_hands_test: \(PASS\|FAIL\)"
expect_verdict "$out" "vr_rope_hands_test" "flat hands on ropes and chains: no jitter, on the drawn rope"
verdict; exit $fail
