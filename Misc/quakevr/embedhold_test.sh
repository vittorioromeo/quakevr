#!/bin/bash
# embedhold_test.sh <agent> -- vr_embed_test 64 (QC/vr_flatembedwalk_test.qc): flat play's stuck blade keeps its hands on their arms
# and weapon+hand locked together during release ease (embedhold2/embedholdfix). Seven cases (crate, ragdoll, live grunt, ogre, spear,
# sword with free off hand, swung axe): standing still after the stab/swing the drawn wrist stays within its arm (.vr_embed_armpast <= 3 cm),
# the hold stays on and the blade doesn't creep out; walking away keeps hands on wrists, draws pointed blades out of light hosts into the hand
# (checking weapon+hand lock during vr_embed_free_ease), lets go on heavy hosts (ogre) and swung edge embeds (axe) so the blade stays in.
# Two runs: the shipped defaults (vr_flat_embed_arm 1) and the rule off (0). Ends `embedhold: N passed, M failed`.
AGENT=${1:?worktree name}
QT_NAME=embedhold
source "$(dirname "$0")/qvrtest.sh"

run()
{
    bash $KIT/run.sh $AGENT -Clean -ExtraArgs "-flatscreenmode -loopbackonly" -Timeout 300 \
        -Script "map vrtesthall; god; notarget; wait5; setpos 0 300 25 0 90 0; wait5; vr_flat_embed_arm $1; vr_embed_test 64" \
        -QuitOn "^flatembedwalk: [0-9]+ passed" -Filter "^(flatembedwalk: [0-9]+ passed|flatembedwalk: FAIL|flatembedwalk: .*: walked|exit=)" 2>&1
}

ON=$(run 1)
echo "$ON"
expect_verdict "$ON" flatembedwalk "vr_flat_embed_arm 1"

OFF=$(run 0)
echo "$OFF"
expect_verdict "$OFF" flatembedwalk "vr_flat_embed_arm 0"

verdict
exit $fail
