#!/bin/bash
# attach_holster_test.sh <agent> -- holsterattach: the holstered guns' attachments drawn on them (view::placeAttachment, the
# same mount math as the held gun's). vr_embed_test 88 (QC/vr_attach_test.qc) holsters the shotgun with a bayonet in the right
# hip and the super shotgun with one, broken open, in the left hip (its bayonet carried down with its barrels); the client's
# vr_attach_report (gap=: its mount's distance from the gun's mount as drawn) read by Misc/quakevr/attach_holster_check.py:
#   flat-screen, in your eyes and from the fly camera (F8: vr_flat_flycam_place) beside the hip; VR (the mock headset).
# Another player's holstered guns: mp_test.py attach (other_holster). Each run ends on its marker line (-QuitOn); the script
# ends `attach_holster: N passed, M failed`.
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
START="map vrtesthall; god; notarget; wait5; setpos 0 300 25 0 90 0; wait5; vr_embed_test 88; wait10"
FILTER="^attach|rror"
END="echo attachholster: done"
flat=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Timeout 150 \
    -Script "$START; vr_attach_report; vr_flat_flycam_place 4 -30 -30 0 -90; wait5; vr_attach_report; $END" \
    -QuitOn "^attachholster: done" -Filter "$FILTER")
echo "$flat"
vr=$(bash $KIT/run.sh $AGENT -Timeout 150 -Script "$START; vr_attach_report; $END" -QuitOn "^attachholster: done" -Filter "$FILTER")
echo "$vr"
fc=$(python "$(dirname "$0")/attach_holster_check.py" flat - <<< "$flat")
echo "$fc"
vc=$(python "$(dirname "$0")/attach_holster_check.py" vr - <<< "$vr")
echo "$vc"
expect_verdict "$fc" attachholster_flat "the holstered guns' bayonets at their mounts (flat, in the eyes and the fly camera)"
expect_verdict "$vc" attachholster_vr "the holstered guns' bayonets at their mounts (VR)"
verdict
exit $fail
