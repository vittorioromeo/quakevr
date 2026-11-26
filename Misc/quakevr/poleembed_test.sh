#!/bin/bash
# poleembed_test.sh <agent> -- flatembed3/4 (his flat items 7 and 10), flat-screen play, headless, fast mode, map vrtesthall
# (QC/vr_poleembed_test.qc; needs Dwell's data for the Crystal Lance):
#   vr_embed_test 71: the spear's and the Crystal Lance's other hand round the drawn shaft held upright, flipped, guarding and
#     stuck in a grunt (`poleembed: N passed, M failed`);
#   vr_embed_test 72: a held sword stuck in a ragdoll, vr_embed_ragdoll_smooth_time as set and 0: less jitter drawn, bounded
#     lag, the hand on the handle (`poleembedjit: N passed, M failed`).
# Each run ends on its verdict line (-QuitOn); the script ends `poleembed_test: N passed, M failed`.
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
START="map vrtesthall; god; notarget; developer 1; wait5; setpos 0 300 25 0 90 0; wait5"
pole=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Timeout 240 -Script "$START; vr_embed_test 71" \
    -QuitOn "^poleembed: [0-9]+ passed" -Filter "^poleembed")
echo "$pole"
jit=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Timeout 240 -Script "$START; vr_embed_test 72" \
    -QuitOn "^poleembedjit: [0-9]+ passed" -Filter "^poleembedjit")
echo "$jit"
expect_verdict "$pole" poleembed "the pole's other hand on the shaft in four states"
expect_verdict "$jit" poleembedjit "a held blade's ragdoll smoothing"
verdict
exit $fail
