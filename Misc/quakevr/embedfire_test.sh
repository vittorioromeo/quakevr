#!/bin/bash
# embedfire_test.sh <agent> -- embedfire (his VR notes 2026-11-26 20:24-20:27): the Crystal Lance fires while it is stuck in a
# body and its hand holds it (QC/vr_propfire.qc, QC/vr_embedfire_test.qc; map vrtesthall, needs Dwell's data):
#   vr_embed_test 77 (VR, mock headset): charged in the hand then stuck, and stuck then charged: one shot from the stuck Lance's
#     tip, 6 cells, the grunt hit, one charge carried through; the hand off it fires nothing; pulled out charging, fired from
#     the hand (`embedfire: N passed, M failed`);
#   vr_embed_test 78 (flat-screen): the fire button held then V's thrust, the thrust then the fire button: the same, no pull; a
#     tap of it pulls, no shot (`embedfireflat: N passed, M failed`).
# The client's charge look on the stuck Lance (vr_debug_gunfx): never "gone with the Lance" (the charge kept from the hand to
# the stuck Lance and back), and the stuck Lance's bolt drawn from its tip.
# Each run ends on its verdict line (-QuitOn); the script ends `embedfire_test: N passed, M failed`.
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
START="map vrtesthall; god; notarget; developer 1; vr_debug_axestick 1; vr_debug_gunfx 1; wait5; setpos 0 300 25 0 90 0; wait5"
FILTER="^embedfire|propfire:|gone with the Lance|bolt from the stuck|rror"
vr=$(bash $KIT/run.sh $AGENT -Timeout 300 -Script "$START; vr_embed_test 77" -QuitOn "^embedfire: [0-9]+ passed" -Filter "$FILTER")
echo "$vr"
flat=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Timeout 300 -Script "$START; vr_embed_test 78" \
    -QuitOn "^embedfireflat: [0-9]+ passed" -Filter "$FILTER")
echo "$flat"
expect_verdict "$vr" embedfire "the Lance fired while stuck (VR)"
expect_verdict "$flat" embedfireflat "the Lance fired while stuck (flat)"
lostvr=$(grep -c "gone with the Lance" <<< "$vr")
lostflat=$(grep -c "gone with the Lance" <<< "$flat")
bolts=$(grep -c "bolt from the stuck Lance" <<< "$vr")
check $([ "$lostvr" = 0 ] && echo 1 || echo 0) "the charge drawn throughout, the hand to the stuck Lance and back (VR: $lostvr lost)"
check $([ "$lostflat" = 0 ] && echo 1 || echo 0) "the charge drawn throughout (flat: $lostflat lost)"
check $([ "$bolts" -ge 2 ] && echo 1 || echo 0) "the stuck Lance's bolts drawn from its tip (VR: $bolts)"
verdict
exit $fail
