#!/bin/bash
# flataimsens_test.sh <agent> -- the flatscreen ironsight's look speed (vr_flat_aim_sens, vr_flat_aim_sens_match;
# Quake/vr/vr_flataimsens.cpp), headless, fast mode: vr_flat_aim_sens_test sights in and out at fov 95 and 75 with the
# match and the multiplier set and not, feeds the mouse 200 counts each time and checks the degrees turned against
# hipfire's: match 0 the old tangent ratio (its factor exactly 1), 1.78 and the multiplier their formulas, unsighted
# unchanged; the stick's scale and the menu's readout the same. Ends on its verdict line; exits 1 on a failure.
AGENT=${1:-selfshot}
QT_NAME=flataimsens_sh
source "$(dirname "$0")/qvrtest.sh"
out=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -QuitOn "^flataimsens: [0-9]+ passed" -Timeout 120 \
    -Filter "^flataimsens" \
    -Script "map e1m1; god; notarget; r_norefresh 0; setpos 480 -352 88 0 0 0; impulse 154; wait2; vr_flat_aim_sens_test; wait9000; toggleconsole; quit" 2>&1)
echo "$out" | grep -a "^flataimsens: \(FAIL\|step\)"
expect_verdict "$out" "flataimsens" "ironsight look speed: match 0 the old ratio, the match and the multiplier as asked, hipfire unchanged"
verdict; exit $fail
