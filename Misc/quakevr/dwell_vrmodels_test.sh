#!/bin/bash
# dwell_vrmodels_test.sh <agent> -- Dwell's weapons drawn as their own VR models (DWELL.md 9.6; vr_ad_test 205-207,
# dwell/dw_test.qc; engine vr_adweapons.cpp's recipes made from Dwell's view models). Needs Dwell v2.2 in the base dir
# (<base>/dwellv2p2). Headless on e1m1:
#  1. vr_ad_test 205: the Rotary Shotgun and the Crystal Lance in the hands drawn with their models, a plain shotgun
#     (the retired Rifle's variant) in a holster ours, AD's Widowmaker still its own; the models made from Dwell's
#     files (vr_adweapon_models: 7 made, 0 not);
#     the holster menu (flat and the wrist's: vr_flat_holster_menu_dump) names them by their models.
#  2. The Rotary's drawn barrel spin (vr_ad_widowmaker_spin_info), in mock VR and in flat play (-flatscreenmode): turning
#     with the trigger held 40 frames, then at rest with a barrel on top (a multiple of 90 degrees) 200 frames after
#     its release (the coast's frames: vr_ad_widowmaker_spindown).
#  3. Screenshots for review (no check): each of the two in the main hand, mock VR (a side view, the eyes' view) to
#     scratch/dwell_vrmodels_vr.png, flat to scratch/dwell_vrmodels_flat.png (and the holster menu open).
# Exits 1 on a failure.
AGENT=$1
source "$(dirname "$0")/qvrtest.sh"
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait10;god;notarget;vr_adweapon_models;vr_ad_test 205;wait10;vr_flat_holster_menu_dump;echo dwmodels-test: done" \
    -QuitOn "dwmodels-test: done" -Timeout 300 -Filter "dwmodels|vr_adweapon_models:|holster 0 \(|right hand:|rror|exit=" 2>&1 | tr -d '\r')
echo "$log" | grep -E "FAIL|vr_adweapon_models:|holster 0 \(|right hand:" | head -8
expect_verdict "$log" dwmodels "Dwell's weapons drawn with their own models"
check $(grep -q "vr_adweapon_models: 7 made, 0 not made" <<<"$log" && echo 1 || echo 0) "the seven models made (AD's three, Dwell's four)"
check $(grep -q "holster 0 (.*): shotgun$" <<<"$log" && echo 1 || echo 0) "the holster menu names the holstered plain Shotgun (the Rifle retired)"
check $(grep -qE "left hand: Crystal Lance.*right hand: Rotary Shotgun" <<<"$log" && echo 1 || echo 0) "the holster menu names the Lance and the Rotary in the hands"
check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "models: the run ended on its last line"
SPIN="vr_ad_test 206;wait5;+attack;wait40;vr_ad_widowmaker_spin_info;-attack;wait200;vr_ad_widowmaker_spin_info;echo dwmodels-test: done"
for mode in vr flat; do
    extra=""
    [ "$mode" = flat ] && extra="-flatscreenmode"
    log=$(bash $KIT/run.sh $AGENT -ExtraArgs "$extra" -Script "map e1m1;wait10;god;notarget;vr_mock_hand main 0.15 1.3 -0.35 70 0 0;$SPIN" \
        -QuitOn "dwmodels-test: done" -Timeout 300 -Filter "widowmaker spin hand|  model:|rror|exit=" 2>&1 | tr -d '\r')
    spin=$(grep -E "widowmaker spin hand [01]: turn" <<<"$log")
    echo "$spin" | sed 's/^/  /' | cut -c1-110
    check $(grep -q "4 barrels" <<<"$log" && echo 1 || echo 0) "$mode: the Rotary's model has its four barrels' spin frames"
    check $(head -1 <<<"$spin" | grep -qE "speed [1-9][0-9]* deg/s.*trigger 1" && echo 1 || echo 0) "$mode: its barrels turn with the trigger held"
    check $(sed -n 2p <<<"$spin" | grep -qE "turn (0|90|180|270)\.0 deg, speed 0 deg/s" && echo 1 || echo 0) "$mode: at rest after, a barrel on top"
    check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "$mode spin: the run ended on its last line"
done
# Screenshots: each in the main hand (206 the Rotary Shotgun, 207 the Crystal Lance).
VRS=""
FLS=""
for t in 206 207; do
    VRS="$VRS vr_ad_test $t;vr_mock_hand main 0.15 1.3 -0.35 70 0 0;vr_mock_camera 0.7 1.4 -0.5 5 90;wait20;screenshot;vr_mock_camera;vr_mock_hand main 0.16 1.55 -0.38 70 0 0;wait10;screenshot;"
    FLS="$FLS vr_ad_test $t;wait30;screenshot;"
done
bash $KIT/run.sh $AGENT -Clean -Script "map e1m1;wait10;god;notarget;vr_eyeshot 1;$VRS echo dwmodels-test: done" -QuitOn "dwmodels-test: done" \
    -Out dwell_vrmodels_vr.png -Timeout 300 -Filter "composed|exit=" 2>&1 | tr -d '\r' | grep composed
bash $KIT/run.sh $AGENT -Clean -ExtraArgs "-flatscreenmode" -Script "map e1m1;wait10;god;notarget;$FLS +flatholsters;wait30;screenshot;-flatholsters;wait5;echo dwmodels-test: done" \
    -QuitOn "dwmodels-test: done" -Out dwell_vrmodels_flat.png -Timeout 300 -Filter "composed|exit=" 2>&1 | tr -d '\r' | grep composed
verdict
exit $fail
