#!/bin/bash
# flatslide_test.sh <agent> -- flatscreen play's slide and lean (-flatscreenmode; docs/vr-port/FLATPLAY.md, "Sliding" and
# "Leaning"; Quake/vr/vr_flatmove.cpp), headless. Prints PASS/FAIL lines and a verdict, exits 1 on a failure.
#   slide  vr_flatslide_selftest (the slide's step on made-up floors: flat friction, downhill to the top speed, uphill
#          to a stop), then vr_flatslide_test in vrtesthall: running, the crouch starts a slide (the boost), it lasts
#          what its friction gives, ends in a crouch; slide again and jump: the speed kept in the air. Then
#          vr_flatslide_rough_test: up vrtesthall's stairs and ramp, no step nor ramp ending it; A alone and D alone
#          curve it, the view turned 10 degrees off its way.
#   lean   vr_flatlean_test in vrtesthall: a full lean right moves the eye vr_flat_lean_dist and rolls the view; a shot
#          across the leant head out of his box meets him (and not with his box alone); walking leant is
#          vr_flat_lean_speed of upright; leant into a wall at his side the eye stays vr_flat_lean_margin off it; leant
#          away from it, and facing a wall he stands against (either way), the eye goes all of vr_flat_lean_dist out.
#   prone  vr_flatprone_test in vrtesthall, the shotgun: lying down a full lean moves the eye vr_flat_lean_prone_dist
#          and rolls the view and the body vr_flat_lean_prone_angle; the crouch pressed lying down gets up into the
#          crouch (the gun without a jump); looking down lying down the gun lies flush on the floor (no shot, the
#          ironsight waits), looking level the ironsight comes.
#   mantle vr_flatmantle_test in vrmantle (Misc/quakevr/climb/make_vrmantle_map.py): a jump at each block, the jump key
#          held, looking up 35 degrees (vr_flat_mantle_look 30): mantled onto the 48, 64, 80 and 96 high ones, not the 128 and 160 ones; looking up
#          only 20 at the 64 one, no mantle (vr_climb.cpp flatMantle). Each mantle: its empty hands on the ledge (the
#          drawn hands' hold bits, STAT_QVR_CLIMB), the others not (flatMantleHands): both hands empty, both; then the
#          64 one alone with a gun in the main hand (the off hand on it), and dual wielding (none).
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

out=$(bash $KIT/run.sh $AGENT "${F[@]}" -QuitOn "^flatslide_rough: [0-9]+ passed" -Timeout 120 -Filter "^flatslide_rough" \
    -Script "$H; vr_flatslide_rough_test; wait3000; toggleconsole; quit" 2>&1)
echo "$out" | grep -a "^flatslide_rough.*FAIL"
expect_verdict "$out" "flatslide_rough" "slide: up stairs and a ramp (no step, no ramp ends it), A alone and D alone curve it"

out=$(bash $KIT/run.sh $AGENT "${F[@]}" -QuitOn "^flatlean: [0-9]+ passed" -Timeout 120 -Filter "^flatlean" \
    -Script "$H; vr_flatlean_test; wait3000; toggleconsole; quit" 2>&1)
echo "$out" | grep -a "^flatlean.*FAIL"
expect_verdict "$out" "flatlean" "lean: the eye and the roll, the hit box, the slower walk, the wall's clamp, away from and facing a wall"

out=$(bash $KIT/run.sh $AGENT "${F[@]}" -QuitOn "^flatprone: [0-9]+ passed" -Timeout 120 -Filter "^flatprone" \
    -Script "$H; impulse 154; wait30; vr_flatprone_test; wait3000; toggleconsole; quit" 2>&1)
echo "$out" | grep -a "^flatprone.*FAIL"
expect_verdict "$out" "flatprone" "prone: the smaller lean and body roll, the crouch out of it, the gun flush on the floor"

out=$(bash $KIT/run.sh $AGENT "${F[@]}" -QuitOn "^flatmantle: [0-9]+ passed" -Timeout 120 -Filter "^flatmantle|^climb: flat" \
    -Script "map vrmantle; wait60; god; notarget; vr_climb_debug 1; vr_flatmantle_test; wait3000; toggleconsole; quit" 2>&1)
echo "$out" | grep -a "^flatmantle.*FAIL"
expect_verdict "$out" "flatmantle" "mantle: after a jump, looking up 35, onto ledges 48..96 high; not 128 or 160, nor looking up only 20"
check $([ "$(echo "$out" | grep -ac "^flatmantle: PASS lip [0-9]*: hands on the ledge off 1 main 1")" = 4 ] && echo 1 || echo 0) "mantle: both hands empty, both on the ledge at each of the 4 mantles"
for lo in "154; wait5; impulse 170|off 1 main 0|a gun in the main hand: the off hand alone" "152; wait5; impulse 172|off 0 main 0|dual wielding: no hand"; do
    out=$(bash $KIT/run.sh $AGENT "${F[@]}" -QuitOn "^flatmantle: [0-9]+ passed" -Timeout 120 -Filter "^flatmantle" \
        -Script "map vrmantle; wait60; god; notarget; impulse ${lo%%|*}; wait20; vr_flatmantle_test 1; wait3000; toggleconsole; quit" 2>&1)
    rest=${lo#*|}
    check $(echo "$out" | grep -aq "^flatmantle: PASS lip 64: hands on the ledge ${rest%%|*} " && echo 1 || echo 0) "mantle: ${rest#*|} on the ledge (${rest%%|*})"
done

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
