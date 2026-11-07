#!/bin/bash
# flatslide_test.sh <agent> -- flatscreen play's slide and lean (-flatscreenmode; docs/vr-port/FLATPLAY.md, "Sliding" and
# "Leaning"; Quake/vr/vr_flatmove.cpp), headless. Prints PASS/FAIL lines and a verdict, exits 1 on a failure.
#   slide  vr_flatslide_selftest (the slide's step on made-up floors: flat friction, downhill to the top speed, uphill
#          to a stop), then vr_flatslide_test in vrtesthall: running, the crouch starts a slide (the boost), it lasts
#          what its friction gives, ends in a crouch; slide again and jump: the speed kept in the air.
#   lean   vr_flatlean_test in vrtesthall: a full lean right moves the eye vr_flat_lean_dist and rolls the view; a shot
#          across the leant head out of his box meets him (and not with his box alone); walking leant is
#          vr_flat_lean_speed of upright; leant into a wall at his side the eye stays vr_flat_lean_margin off it.
#   sights e1m1, the shotgun on the ironsight: the sights on the drawn ray upright, leant right, leant right crouched
#          and leant left (vr_flat_sight_check), and the shot along the view (flataim).
#   keys   E and Q lean (vr_mock_key), 3 swaps the hands, R force-grabs; an old config's binds on E and 3 do not run
#          while the mode reads those keys (vr_flat_keys_own 1), and do with vr_flat_keys_own 0.
AGENT=${1:-selfshot}
QT_NAME=flatslidelean
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch; mkdir -p "$S"
F=(-ExtraArgs "-flatscreenmode")
H="map vrtesthall; wait60; god; notarget; cl_alwaysrun 1; vr_stamina_speed 0; wait10"

out=$(bash $KIT/run.sh $AGENT "${F[@]}" -QuitOn "^flatslide: [0-9]+ passed" -Timeout 120 -Filter "^flatslide" \
    -Script "vr_flatslide_selftest; $H; +forward; vr_flatslide_test; wait3000; toggleconsole; quit" 2>&1)
echo "$out" | grep -a "^flatslide.*\(FAIL\|passed\)"
expect_verdict "$out" "flatslide_selftest" "slide: the step on flat, downhill and uphill floors"
expect_verdict "$out" "flatslide" "slide: started from a run, its length, into a crouch, the jump keeps the speed"

out=$(bash $KIT/run.sh $AGENT "${F[@]}" -QuitOn "^flatlean: [0-9]+ passed" -Timeout 120 -Filter "^flatlean" \
    -Script "$H; vr_flatlean_test; wait3000; toggleconsole; quit" 2>&1)
echo "$out" | grep -a "^flatlean.*FAIL"
expect_verdict "$out" "flatlean" "lean: the eye and the roll, the hit box, the slower walk, the wall's clamp"

P="map e1m1; god; notarget; developer 1; r_norefresh 0; setpos 480 -352 88 0 0 0; impulse 154; wait30; vr_flat_aim 1; wait30"
out=$(bash $KIT/run.sh $AGENT "${F[@]}" -Timeout 120 -Filter "^flatsightcheck|^flataim: main" \
    -Script "$P; vr_flat_sight_check; +flatleanright; wait30; vr_flat_sight_check; vr_flatplay_debug 2; wait2; vr_flatplay_debug 0; +flatcrouch; wait30; vr_flat_sight_check; -flatcrouch; -flatleanright; +flatleanleft; wait30; vr_flat_sight_check; -flatleanleft; vr_flat_aim 0; wait2; toggleconsole; quit" 2>&1)
n=$(echo "$out" | grep -ac "^flatsightcheck: .* rear off 0\.00[0-9]* front off 0\.00")
check $([ "$n" = 4 ] && echo 1 || echo 0) "sights: on the drawn ray upright, leant right, leant right crouched, leant left ($n of 4 within 0.01)"
aim=$(echo "$out" | grep -a "^flataim: main" | head -1)
check $(grep -q "off its point 0\.0[0-9]* deg" <<<"$aim" && echo 1 || echo 0) "sights: leant, the shot goes where the crosshair is (${aim:-no line})"

KEY() { echo "vr_mock_key $1 down; wait${2:-3}; vr_mock_key $1 up; wait10"; }
K="$H; impulse 154; wait10; bind e \"echo BOUND_E\"; bind 3 \"echo BOUND_3\"; vr_flatplay_debug 1"
out=$(bash $KIT/run.sh $AGENT "${F[@]}" -Timeout 120 -Filter "^flatmove: lean|^BOUND_|^flatplay: [3r]:" \
    -Script "$K; vr_mock_key e down; wait30; vr_flatmove_status; vr_mock_key e up; wait30; vr_mock_key q down; wait30; vr_flatmove_status; vr_mock_key q up; wait30; $(KEY 3); $(KEY r); echo OWN_OFF; vr_flat_keys_own 0; $(KEY e); $(KEY 3); vr_flat_keys_own 1; bind e +moveup; bind 3 \"impulse 3\"; toggleconsole; quit" 2>&1)
check $(echo "$out" | grep -qa "^flatmove: lean 1\.00" && echo 1 || echo 0) "keys: E held leans right"
check $(echo "$out" | grep -qa "^flatmove: lean -1\.00" && echo 1 || echo 0) "keys: Q held leans left"
check $(echo "$out" | grep -qa "^flatplay: 3: swap hands" && echo 1 || echo 0) "keys: 3 swaps the hands"
check $(echo "$out" | grep -qa "^flatplay: r: force grab" && echo 1 || echo 0) "keys: R force-grabs"
nb=$(echo "$out" | grep -ac "^BOUND_")
check $([ "$nb" = 2 ] && echo 1 || echo 0) "keys: E's and 3's own binds ran only with vr_flat_keys_own 0 ($nb lines, 2 wanted)"
verdict; exit $fail
