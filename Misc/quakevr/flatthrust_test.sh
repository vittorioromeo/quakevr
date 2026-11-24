#!/bin/bash
# flatthrust_test.sh <agent> -- flat-screen V thrusts with every stabbing blade (flatthrust, his decision 2026-11-24;
# ROUND25.md "flatthrust"): two headless flat runs on vrtesthall, fast mode.
#   1. vr_embed_test 58 (QC/vr_flatthrust_test.qc): the knight's sword, the dagger, the spear and the dagger in a flipped
#      grip (a stab down), each V'd at a grunt ahead: it hurts it, sticks by its point, fire presses pull it out, back in
#      the hand; the engine's punch line for each is the "spear thrust"; the axe's V is still its "overhead swing".
#   2. vr_embed_test 59 with Dwell's Crystal Lance in the hand (vr_ad_test 207; needs Dwell in the base dir): the same.
# Each run ends on its "flatthrust: N passed" verdict (-QuitOn).
AGENT=${1:?agent}
fail=0
QT_NAME=flatthrust_test
source "$(dirname "$0")/qvrtest.sh"
START="map vrtesthall; god; notarget; developer 1; vr_flatplay_debug 1; vr_debug_axestick 1; wait5; setpos 0 300 25 0 90 0; wait5"
F="(^flatthrust|^flatplay: main punch|^embedhold: flat pull|^dwmodels)"
one=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Timeout 240 -Script "$START; vr_embed_test 58" \
    -QuitOn "^flatthrust: [0-9]+ passed" -Filter "$F")
echo "$one"
expect_verdict "$one" flatthrust "blades: V hurts, sticks by the point, fire pulls out"
# (Each case's V and the engine's punch line after it.)
pairs=$(awk '/^flatthrust: case /{c=substr($0,18)} /^flatplay: main punch \(V/{print c "=" ($0 ~ /spear thrust/ ? "thrust" : ($0 ~ /overhead swing/ ? "swing" : "other"))}' <<<"$one")
for c in "sword" "dagger" "spear" "dagger flipped"; do
    check $(grep -qx "$c=thrust" <<<"$pairs" && echo 1 || echo 0) "$c: V is the spear's thrust"
done
check $(grep -qx "axe=swing" <<<"$pairs" && echo 1 || echo 0) "axe: V still its overhead swing"
two=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Timeout 240 -Script "$START; vr_ad_test 207; vr_embed_test 59" \
    -QuitOn "^flatthrust: [0-9]+ passed" -Filter "$F")
echo "$two"
expect_verdict "$two" flatthrust "the Lance: V hurts, sticks by the point, fire pulls out"
verdict
exit $fail
