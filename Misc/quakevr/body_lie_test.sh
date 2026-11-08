#!/bin/bash
# body_lie_test.sh <agent> -- the lying body (ROUND24.md, "Lying body: slide and prone"; Quake/vr/vr_avatar_lie.cpp), headless.
#   flat  vr_body_lie_test in vrtesthall, flat-screen, the super shotgun held (the two-handed stance): lying down puts the
#         hips vr_prone_hip_floor over the floor, the legs straight, knees and feet on it, the gear with its bones; a slide
#         has no stance turn, straighter legs, feet on the floor, no bob (body and view), nothing worn in front of the
#         camera, its sound (started once, fading as it slows, stopped after); a second slide with the view turned 40
#         degrees: the legs point the slide's way, the chest the view's.
#   vr    vr_body_lie_test vr: the headset's mock head lowered to 0.28 m: the same lying-down checks.
AGENT=${1:-embed5}
QT_NAME=bodylie
source "$(dirname "$0")/qvrtest.sh"
H="map vrtesthall; wait60; god; notarget; cl_alwaysrun 1; vr_stamina_speed 0; impulse 9; wait5; impulse 3; wait30"

out=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -QuitOn "^body_lie_test: [0-9]+ passed" -Timeout 120 \
    -Filter "^body_lie_test: (PASS|FAIL|[0-9])" -Script "$H; vr_body_lie_test; wait5000; toggleconsole; quit" 2>&1)
echo "$out" | grep -a "^body_lie_test: .*\(FAIL\|passed\)"
expect_verdict "$out" "body_lie_test" "flat: prone on the floor, the slide's pose, bob, gear, sound and legs"

out=$(bash $KIT/run.sh $AGENT -QuitOn "^body_lie_test: [0-9]+ passed" -Timeout 120 \
    -Filter "^body_lie_test: (PASS|FAIL|[0-9])" -Script "map vrtesthall; wait60; god; notarget; vr_body_lie_test vr; wait5000; toggleconsole; quit" 2>&1)
echo "$out" | grep -a "^body_lie_test: .*\(FAIL\|passed\)"
expect_verdict "$out" "body_lie_test" "vr: lying down on the floor, straight legs, the gear with its bones"
verdict; exit $fail
