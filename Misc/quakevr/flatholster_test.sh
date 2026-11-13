#!/bin/bash
# flatholster_test.sh <agent> -- flat-screen play's holsters and their menu (vr_flat_holsters; Quake/vr/vr_flatholster.cpp,
# QC vr_flatholster.qc; docs/vr-port/FLATPLAY.md, "Holsters"). Headless, fast mode; the run ends on the test's own verdict
# line (-QuitOn "flatholster_test: N passed, M failed").
#   vr_flatholster_test: the shotgun in the right hand, the nailgun in the left; the menu held open (+flatholsters) and
#   driven by the mouse's counts and the left button's click: time at vr_flat_holster_menu_timescale; every holster emptied
#   by its DROP; h0 L, h0 R (a holster trading with either hand); SWAP; h0 R into the empty hand; with both hands full the
#   weapon keys (2, 4, the wheel's cycle) change nothing and a super shotgun walked over is left lying (QC's line,
#   "flatholster pickup: ... left lying"); H (impulse 47) drops the off (left) hand's gun first, then the main hand's;
#   DROP BOTH dim with both hands empty; the left hand's DROP; DROP BOTH empties both hands; drag and drop with the left
#   button held (hand -> holster, holster -> hand, hand <-> hand, holster <-> holster, holster -> empty holster; a drop
#   on nothing or on its own slot cancelled, an empty holster picks nothing up, the places a too-big weapon or a carrying
#   hand may not go greyed; a second screenshot mid-drag); vr_flat_holster_menu_mode 0:
#   let go over no button just closes, let go over h1 R clicks it; mode 1 (toggle): a press opens it, three clicks with
#   it still open, the next press closes it, Esc closes it; at the end the menu closed and time back at 1. A screenshot
#   of the menu with the nailgun in the left shoulder holster (-Out).
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flatholster
mkdir -p "$S"
SCRIPT="vr_flat_holsters 1; map vrtesthall; wait60; god; notarget; developer 1; impulse 154; wait5; impulse 176; wait10; vr_flatholster_test"
bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode -Script "$SCRIPT" -QuitOn "^flatholster_test: [0-9]+ passed" -Timeout 180 \
    -Filter "^(flatholster|flat holsters|vr_flat_mouse_move)" -Out "$S/menu.png" > "$S/run.log" 2>&1
grep -E "^(flatholster|flat holsters)" "$S/run.log" | tail -40
grep -q "flatholster pickup: .* left lying" "$S/run.log" && echo "flatholster_test: PASS pickup with both hands full left lying" \
    || echo "flatholster_test: FAIL pickup with both hands full (no 'left lying' line)"
