#!/bin/bash
# vrspear_test.sh <agent> -- vrspear (his VR notes 2026-11-26 20:07-20:10), mock VR, headless, fast mode, map vrtesthall
# (QC/vr_poleembed_test.qc; needs Dwell's data for the Crystal Lance):
#   vr_embed_test 75: the Crystal Lance (the main hand's grip loose: it slides along the pole, the off hand kept on its point)
#     and the spear with vr_pole_grip_fixed 0 (the drawn off hand slides);
#   vr_embed_test 76: the spear with vr_pole_grip_fixed 1 (a run of its own: a third case in one run never took the pole).
#   Each: the off hand put on the pole with an offset closes round the drawn shaft (palm and fingers on it); moved along it,
#   the drawn hand stays (fixed) or slides; a two-handed stab sticks in a grunt and the off hand stays on the stuck shaft where
#   it held it (`vrspear: N passed, M failed`).
# Each run ends on its verdict line (-QuitOn); the script ends `vrspear_test: N passed, M failed`.
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
START="map vrtesthall; god; notarget; developer 1; vr_debug_axestick 1; wait5; setpos 0 300 25 0 90 0; wait5"
for_test() {
    bash $KIT/run.sh $AGENT -Timeout 300 -Script "$START; vr_embed_test $1" -QuitOn "^vrspear: [0-9]+ passed" \
        -Filter "^vrspear|embedpin: hand . stays|lets go|let go|rror"
}
a=$(for_test 75)
echo "$a"
b=$(for_test 76)
echo "$b"
expect_verdict "$a" vrspear "the Lance and the sliding spear: the off hand on the shaft, through a stab that sticks (VR)"
expect_verdict "$b" vrspear "the fixed spear: the off hand on the shaft, kept on it, through a stab that sticks (VR)"
verdict
exit $fail
