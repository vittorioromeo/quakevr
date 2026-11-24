#!/bin/bash
# spear_test.sh <agent> -- the spear (QC VR_SWORDKIND_SPEAR, a pole weapon on the Crystal Lance's systems; ROUND25.md "spear"):
# vr_embed_test 53 (QC/vr_spear_test.qc) twice, headless, fast mode: with the mock headset (a thrust sticks by its point no
# deeper than its head and comes out drawn back; a swing; thrown at a wall and a grunt point first; the flip grip; no
# ammo) and in flat-screen play (-flatscreenmode: vr_flatplay_status's "spear:" line, flat play's spear pose, every pole
# weapon's; the fire button's punch is the spear's thrust ("spear thrust"), fire presses pull it out, B flips its grip).
# Each run ends on `embedspear: N passed, M failed`; the script ends `spear: N passed, M failed`.
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
START="map vrtesthall; god; notarget; developer 1; vr_flatplay_debug 1; wait5; setpos 0 300 25 0 90 0; wait5"
F="(^embedspear|^ *spear:|^dwlance:|^flatplay: main punch|^embedhold: flat pull)"
vr=$(bash $KIT/run.sh $AGENT -Timeout 240 -Script "$START; vr_embed_test 53" -QuitOn "^embedspear: [0-9]+ passed" -Filter "$F")
echo "$vr"
flat=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Timeout 240 \
    -Script "$START; vr_debug_axestick 1; vr_embed_test 53" -QuitOn "^embedspear: [0-9]+ passed" -Filter "$F")
echo "$flat"
expect_verdict "$vr" embedspear "VR mock: the spear's checks"
expect_verdict "$flat" embedspear "flat: the spear's checks"
check $(grep -aq "spear: main" <<<"$flat" && echo 1 || echo 0) "flat: the spear posed as a spear (a pole weapon)"
check $(grep -aq "spear thrust" <<<"$flat" && echo 1 || echo 0) "flat: the fire button's punch is the spear's thrust"
verdict
exit $fail
