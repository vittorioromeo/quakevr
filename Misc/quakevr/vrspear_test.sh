#!/bin/bash
# vrspear_test.sh <agent> [extra run.sh args] -- vrspear (his VR notes 2026-11-26 20:07-20:10), mock VR, headless, fast mode, map
# vrtesthall (QC/vr_poleembed_test.qc; needs Dwell's data for the Crystal Lance):
#   vr_embed_test 73: the spear's and the Crystal Lance's off hand put on the pole with an offset closes round the drawn shaft
#     (palm on it, fingers on it); vr_pole_grip_fixed 1 keeps it where it took hold, 0 lets it slide; a two-handed stab sticks
#     in a grunt and the off hand stays on the stuck shaft where it held it (`vrspear: N passed, M failed`).
# The run ends on its verdict line (-QuitOn); the script ends `vrspear_test: N passed, M failed`.
AGENT=${1:?agent}
shift
source "$(dirname "$0")/qvrtest.sh"
START="map vrtesthall; god; notarget; developer 1; vr_debug_axestick 1; wait5; setpos 0 300 25 0 90 0; wait5"
out=$(bash $KIT/run.sh $AGENT -Timeout 300 -Script "$START; vr_embed_test 73" -QuitOn "^vrspear: [0-9]+ passed" \
    -Filter "^vrspear|embedpin: hand . stays|lets go|let go|rror" "$@")
echo "$out"
expect_verdict "$out" vrspear "the pole's off hand on the shaft, fixed, through a stab that sticks (VR)"
verdict
exit $fail
