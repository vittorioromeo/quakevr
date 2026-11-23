#!/bin/bash
# weaponwheel_test.sh <agent> -- the headset's weapon wheel (vr_weaponwheel; Quake/vr/vr_weaponwheel.cpp, QC
# vr_flatholster.qc VR_Wheel_Pick; docs/vr-port/ROUND25.md, "Weapon wheel"). Headless mock headset, fast mode; the run
# ends on the test's own verdict line (-QuitOn "weaponwheel_test: N passed, M failed").
#   vr_weaponwheel_test: a nailgun in the right hand (its grip held), a shotgun in the right shoulder holster; B held
#   opens the wheel round the right hand (seven slots) and time slows to vr_weaponwheel_timescale; the hand moved onto the
#   right shoulder's slot lights it (a haptic tick), B let go: the shotgun in the right hand, the nailgun into a holster;
#   the left hand's Y held, the hand moved off the wheel, let go: nothing moves; Y again onto the nailgun's slot: the left
#   hand (its grip not held) takes it and keeps it; the right hand's EMPTY HAND: the shotgun back in its shoulder holster;
#   time back at 1. Two screenshots (the lit slot; the left hand off the wheel) composed into -Out.
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/weaponwheel
mkdir -p "$S"
OUT=${2:-$S/wheel.png}
SCRIPT="map vrtesthall; developer 1; vr_weaponwheel_test"
bash $KIT/run.sh $AGENT -Script "$SCRIPT" -QuitOn "^weaponwheel_test: [0-9]+ passed" -Timeout 180 \
    -Filter "^(weaponwheel_test|weapon wheel|holster give|flat holsters)" -Out "$OUT" > "$S/run.log" 2>&1
grep -E "^(weaponwheel_test|weapon wheel)" "$S/run.log" | tail -40
