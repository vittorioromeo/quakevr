#!/bin/bash
# flatramp_test.sh <agent> [-RealTime] -- flat-screen slides down e1m1's ramps (vr_flatslide_ramp_test, Quake/vr/vr_flatmove.cpp;
# ROUND24.md, "Ramp slide jitter"), headless: the spiral room's ramp and the button ramp under the bridge (off its lip
# and down). On the slope the view's height and his origin's, and the whole slide the drawn gun and the drawn body (the
# main shoulder, the pelvis) from the view, must not bend by more than 0.5 a frame (a shake does). Fast mode by default;
# -RealTime runs it at the real clock's pace (his play: frame times as the machine gives them). Ends on the test's
# verdict line.
AGENT=${1:-blendmodels}
REAL=${2:-}
QT_NAME=flatramp
source "$(dirname "$0")/qvrtest.sh"
out=$(bash $KIT/run.sh $AGENT $REAL -ExtraArgs "-flatscreenmode" -QuitOn "^flatslide_ramp: [0-9]+ passed" -Timeout 120 \
    -Filter "^flatslide_ramp: (PASS|FAIL|[0-9])" \
    -Script "map e1m1; wait60; god; notarget; cl_alwaysrun 1; vr_stamina_speed 0; impulse 2; wait20; vr_flatslide_ramp_test; wait9000; toggleconsole; quit" 2>&1)
echo "$out" | grep -a "^flatslide_ramp: .*\(FAIL\|passed\)"
expect_verdict "$out" "flatslide_ramp" "slides down e1m1's ramps: the view, the gun and the body without a shake"
verdict; exit $fail
