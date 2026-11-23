#!/bin/bash
# pole_fall_test.sh <agent> [upright|slant] [vr] -- falling beside a climbing pole (ropes11; Quake/vr/vr_climbrope.cpp
# vr_pole_fall_test, solidInside, solidLedge; docs/vr-port/ROPES.md, "Tests"), headless, in vrropes, flat-screen play
# (or the mock headset with `vr`: no jump cases). Prints PASS/FAIL lines and a verdict, exits 1 on a failure.
#   Each case jumps off the pole (taken in the air, a key held: none, forward, back, strafe; from four sides) or is set
#   beside it in the air (touching its boxes' faces, or 0.3 / 1 unit in them as a hold leaves him) with a key held: every
#   airborne frame the speed down grows by gravity's step and he goes down by as much (no slow slide against its boxes),
#   landing on the floor (not on a pole box), or taken again (only with a key held towards it). The regrab case: jumped
#   off, then set back within reach in the air past vr_flat_rope_retake: taken again.
AGENT=${1:-posebake}
POLE=${2:-upright}
MODE=${3:-}
QT_NAME=pole_fall_${POLE}${MODE:+_$MODE}
source "$(dirname "$0")/../qvrtest.sh"
FLAT=(-ExtraArgs "-flatscreenmode")
if [ "$MODE" = vr ]; then FLAT=(); fi
out=$(bash $KIT/run.sh $AGENT "${FLAT[@]}" -QuitOn "^vr_pole_fall_test: [0-9]+ passed" -Timeout 300 \
    -Filter "^vr_pole_fall_test" -Script "map vrropes; wait60; god; notarget; vr_climb_stamina 0; vr_pole_fall_test $POLE" 2>&1)
echo "$out" | grep -a "^vr_pole_fall_test: \(FAIL\|HELD\)"
expect_verdict "$out" "vr_pole_fall_test" "falling beside a pole ($POLE${MODE:+, $MODE}): at gravity's pace, retaken after the jump"
verdict; exit $fail
