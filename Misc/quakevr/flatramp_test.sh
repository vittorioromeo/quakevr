#!/bin/bash
# flatramp_test.sh <agent> -- a flat-screen slide down e1m1's ramp (vr_flatslide_ramp_test, Quake/vr/vr_flatmove.cpp;
# ROUND24.md, "Flat fixes, round 2"), headless, fast mode: the view's height, his origin's and the drawn gun from the
# view, frame by frame, must not bend by more than 0.5 a frame (a shake does). Ends on the test's verdict line.
# KNOWN FAIL (2026-11-09): the gun jumps 3.4 units forward from the view in one frame where the crouch's ease ends
# (vr_flat_crouch_time 0.25: the slide's 16th frame), then eases back over 10 frames; with vr_flat_crouch_time 0.6 it
# does not happen within the ramp. The origin and the view are smooth (no ground bounce).
AGENT=${1:-blendmodels}
QT_NAME=flatramp
source "$(dirname "$0")/qvrtest.sh"
out=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -QuitOn "^flatslide_ramp: [0-9]+ passed" -Timeout 120 \
    -Filter "^flatslide_ramp: (PASS|FAIL|[0-9])" \
    -Script "map e1m1; wait60; god; notarget; cl_alwaysrun 1; vr_stamina_speed 0; impulse 2; wait20; vr_flatslide_ramp_test; wait3000; toggleconsole; quit" 2>&1)
echo "$out" | grep -a "^flatslide_ramp: .*\(FAIL\|passed\)"
expect_verdict "$out" "flatslide_ramp" "a slide down e1m1's ramp: on the ground, the view and the gun without a shake"
verdict; exit $fail
