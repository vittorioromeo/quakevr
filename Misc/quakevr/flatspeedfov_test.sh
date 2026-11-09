#!/bin/bash
# flatspeedfov_test.sh <agent> -- flatscreen play's speed FOV (vr_flat_speed_fov; docs/vr-port/FLATPLAY.md, "The speed
# FOV"; Quake/vr/vr_flatspeedfov.cpp), headless, in vrtesthall: vr_flat_speed_fov_test walks and runs (the view's own
# field of view), slides (it widens), stands (it eases back, no frame's step past what the ease allows), then
# strafe-jumps a bunny hop chain (it widens) and stops (it eases back). Prints the measured speeds and fields of view
# and a verdict, exits 1 on a failure.
AGENT=${1:-blendmodels}
QT_NAME=flatspeedfov_sh
source "$(dirname "$0")/qvrtest.sh"
out=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -QuitOn "^flatspeedfov: [0-9]+ passed" -Timeout 180 \
    -Filter "^flatspeedfov" \
    -Script "map vrtesthall; wait60; god; notarget; vr_stamina_speed 0; wait10; vr_flat_speed_fov_test; wait9000; toggleconsole; quit" 2>&1)
echo "$out" | grep -a "^flatspeedfov: \(FAIL\|measured\)"
expect_verdict "$out" "flatspeedfov" "speed fov: none walking or running, wider sliding and bunny hopping, eased back, no pop"
verdict; exit $fail
