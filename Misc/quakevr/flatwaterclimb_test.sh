#!/bin/bash
# flatwaterclimb_test.sh <agent> -- targeted verification for items 9, 11, 12, 13, 14:
#   #9  vertical pole & rope climbing IK elbows flare outwards (lateral > 0, never inward)
#   #11 zero player footstep sounds while walking on the bottom underwater (flatscreen, VR, legs on & off)
#   #12 crouching underwater swims down ONLY with crouch pose = 0 (regular swimming pose)
#   #13 prone underwater is auto-exited on entering water and ignored while underwater (flatscreen & VR)
#   #14 pressing prone at running speed does not initiate a slide, while crouching at running speed still slides
AGENT=${1:?agent}
QT_NAME=flatwaterclimb
source "$(dirname "$0")/qvrtest.sh"

# 1) Underwater footsteps = 0 on bottom, underwater crouch swim-down (no crouch pose), underwater prone exited/blocked (flat)
FLAT_SCRIPT="map vrtesthall; wait60; god; notarget; cl_alwaysrun 1; \
echo FWC_CASE steps_bottom_flat; setpos 256 -256 -104 0 0 0; noclip; wait30; vr_prone_status; vr_body_step_report reset; +forward; wait180; -forward; vr_prone_status; vr_body_step_report bottom_flat silent; \
echo FWC_CASE steps_bottom_old; vr_body_walk 0; setpos 256 -256 -104 0 0 0; noclip; wait20; vr_body_step_report reset; +forward; wait180; -forward; vr_body_step_report bottom_old silent; vr_body_walk 1; \
echo FWC_CASE crouch_swim_before; setpos 384 -256 -32 0 90 0; noclip; wait20; vr_prone_status; vr_flatplay_status; vr_flatswim_report; \
echo FWC_CASE crouch_swim_during; +flatcrouch; wait50; vr_prone_status; vr_flatplay_status; vr_flatswim_report; -flatcrouch; \
echo FWC_CASE prone_in_water; setpos 384 -256 -64 0 90 0; noclip; wait20; vr_prone_toggle; wait50; vr_prone_status; vr_body_pitch; vr_flatswim_report; \
echo FWC_CASE prone_land; setpos 0 300 25 0 90 0; noclip; wait20; vr_prone_toggle; wait60; vr_prone_status; \
echo FWC_CASE prone_enter_water; setpos 384 -256 -64 0 90 0; noclip; wait60; vr_prone_status; vr_body_pitch; vr_flatswim_report; \
vr_body_step_report done; echo FWC_FLAT_DONE; toggleconsole; quit"

out=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Clean -QuitOn "^FWC_FLAT_DONE" -Timeout 180 \
    -Filter "^(FWC_|prone:|playersteps|body_pitch:|flatswim:)|sounds 0 |old played |Crouched |units down" -Script "$FLAT_SCRIPT" 2>&1)

section() { printf '%s\n' "$1" | tr -d '\r' | awk -v c="$2" '$1 == "FWC_CASE" && $2 == c { on = 1; next } /^FWC_/ { on = 0 } on'; }

# Check #11 flatscreen footsteps on bottom underwater (drawn feet + QC old trigger)
s=$(section "$out" steps_bottom_flat)
moved=$(echo "$s" | grep -a "^prone: on " | awk 'NR==1{x0=$NF} NR==2{printf "%.0f", $NF-x0}')
ground=$(echo "$s" | grep -a "^prone: on " | head -1 | sed -n 's/.*ground \([0-9]*\).*/\1/p')
water=$(echo "$s" | grep -a "^prone: y " | head -1 | sed -n 's/.*water \([0-9]*\).*/\1/p')
ok_flat_steps=$(echo "$s" | grep -aq "sounds 0 " && echo "$s" | grep -aq "old played 0 " && [ "${water:-0}" -ge 2 ] && [ "${ground:-0}" = 1 ] && [ "${moved:-0}" -gt 100 ] && echo 1 || echo 0)
check "$ok_flat_steps" "#11 flat underwater bottom footsteps = 0 (water=${water:-?} ground=${ground:-?} moved=${moved:-?}u)"

s=$(section "$out" steps_bottom_old)
ok_old_steps=$(echo "$s" | grep -aq "sounds 0 " && echo "$s" | grep -aq "old played 0 " && echo 1 || echo 0)
check "$ok_old_steps" "#11 flat underwater bottom QC old trigger footsteps = 0 (vr_body_walk 0)"

# Check #12 crouch underwater swims down with crouch pose = 0 (regular swim pose)
sb=$(section "$out" crouch_swim_before)
sd=$(section "$out" crouch_swim_during)
z0=$(echo "$sb" | grep -a "^prone: on " | head -1 | sed -n 's/.* z \([-0-9.]*\).*/\1/p')
z1=$(echo "$sd" | grep -a "^prone: on " | head -1 | sed -n 's/.* z \([-0-9.]*\).*/\1/p')
box1=$(echo "$sd" | grep -a "^prone: on " | head -1 | sed -n 's/.* box \([0-9]*\).*/\1/p')
sw1=$(echo "$sd" | grep -a "^flatswim: swimming " | head -1 | awk '{print $3}')
crouch_zero=$(echo "$sd" | tr '\n' ' ' | grep -q "Crouched 0 (held 1): his head 0\.0 units down" && echo 1 || echo 0)
swam_down=$(awk -v a="${z0:-0}" -v b="${z1:-0}" 'BEGIN{exit !(a - b > 20.0)}' && echo 1 || echo 0)
check $([ "$swam_down" = 1 ] && [ "$crouch_zero" = 1 ] && [ "${box1:-1}" = 0 ] && [ "${sw1:-0}" = 1 ] && echo 1 || echo 0) \
    "#12 flat crouch underwater swims down (z ${z0:-?} -> ${z1:-?}) with crouch=0, drop=0.0, box=${box1:-?}, swimming=${sw1:-?}"

# Check #13 flatscreen prone blocked underwater & auto-exited entering water
sp=$(section "$out" prone_in_water)
ly_in=$(echo "$sp" | grep -a "^prone: on " | head -1 | sed -n 's/.* lying \([0-9]*\).*/\1/p')
bx_in=$(echo "$sp" | grep -a "^prone: on " | head -1 | sed -n 's/.* box \([0-9]*\).*/\1/p')
src_in=$(echo "$sp" | grep -a "^body_pitch:" | head -1 | sed -n 's/.*source \([a-z]*\).*/\1/p')
check $([ "${ly_in:-1}" = 0 ] && [ "${bx_in:-1}" = 0 ] && [ "$src_in" != "prone" ] && echo 1 || echo 0) \
    "#13 flat prone toggle underwater ignored (lying=${ly_in:-?} box=${bx_in:-?} pitch_source=${src_in:-?})"

sl=$(section "$out" prone_land)
se=$(section "$out" prone_enter_water)
ly_land=$(echo "$sl" | grep -a "^prone: on " | head -1 | sed -n 's/.* lying \([0-9]*\).*/\1/p')
ly_exit=$(echo "$se" | grep -a "^prone: on " | head -1 | sed -n 's/.* lying \([0-9]*\).*/\1/p')
bx_exit=$(echo "$se" | grep -a "^prone: on " | head -1 | sed -n 's/.* box \([0-9]*\).*/\1/p')
src_exit=$(echo "$se" | grep -a "^body_pitch:" | head -1 | sed -n 's/.*source \([a-z]*\).*/\1/p')
check $([ "${ly_land:-0}" = 1 ] && [ "${ly_exit:-1}" = 0 ] && [ "${bx_exit:-1}" = 0 ] && [ "$src_exit" != "prone" ] && echo 1 || echo 0) \
    "#13 flat prone auto-exits entering water (land lying=${ly_land:-?} -> water lying=${ly_exit:-?} box=${bx_exit:-?} pitch_source=${src_exit:-?})"

# 2) VR checks: #11 underwater bottom footsteps = 0, #13 VR prone auto-exits / blocked underwater
VR_SCRIPT="map vrtesthall; wait60; god; notarget; cl_alwaysrun 1; \
echo FWC_CASE steps_bottom_vr; setpos 256 -256 -104 0 0 0; noclip; wait30; vr_prone_status; vr_body_step_report reset; vr_mock_stick off 0 1; wait180; vr_mock_stick off 0 0; vr_prone_status; vr_body_step_report bottom_vr silent; \
echo FWC_CASE vr_prone_land; setpos 0 300 25 0 90 0; noclip; wait10; vr_mock_hand head 0 0.4 0; wait30; vr_prone_status; \
echo FWC_CASE vr_prone_water; setpos 384 -256 -64 0 90 0; noclip; wait40; vr_prone_status; vr_body_pitch; vr_mock_hand head; \
vr_body_step_report done; echo FWC_VR_DONE; toggleconsole; quit"

vout=$(bash $KIT/run.sh $AGENT -Clean -QuitOn "^FWC_VR_DONE" -Timeout 180 \
    -Filter "^(FWC_|prone:|playersteps|body_pitch:)|sounds 0 |old played " -Script "$VR_SCRIPT" 2>&1)

s=$(section "$vout" steps_bottom_vr)
moved=$(echo "$s" | grep -a "^prone: on " | awk 'NR==1{x0=$NF} NR==2{printf "%.0f", $NF-x0}')
ground=$(echo "$s" | grep -a "^prone: on " | head -1 | sed -n 's/.*ground \([0-9]*\).*/\1/p')
water=$(echo "$s" | grep -a "^prone: y " | head -1 | sed -n 's/.*water \([0-9]*\).*/\1/p')
ok_vr_steps=$(echo "$s" | grep -aq "sounds 0 " && echo "$s" | grep -aq "old played 0 " && [ "${water:-0}" -ge 2 ] && [ "${ground:-0}" = 1 ] && [ "${moved:-0}" -gt 80 ] && echo 1 || echo 0)
check "$ok_vr_steps" "#11 VR underwater bottom footsteps = 0 (water=${water:-?} ground=${ground:-?} moved=${moved:-?}u)"

sl=$(section "$vout" vr_prone_land)
se=$(section "$vout" vr_prone_water)
ly_land=$(echo "$sl" | grep -a "^prone: on " | head -1 | sed -n 's/.* lying \([0-9]*\).*/\1/p')
ly_exit=$(echo "$se" | grep -a "^prone: on " | head -1 | sed -n 's/.* lying \([0-9]*\).*/\1/p')
src_exit=$(echo "$se" | grep -a "^body_pitch:" | head -1 | sed -n 's/.*source \([a-z]*\).*/\1/p')
check $([ "${ly_land:-0}" = 1 ] && [ "${ly_exit:-1}" = 0 ] && [ "$src_exit" != "prone" ] && echo 1 || echo 0) \
    "#13 VR prone auto-exits & stays blocked underwater (land lying=${ly_land:-?} -> water lying=${ly_exit:-?} pitch_source=${src_exit:-?})"

# 3) #14 prone at running speed does not slide while crouch still slides
sout=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -NoDraw -Clean -QuitOn "^flatslide: [0-9]+ passed" -Timeout 120 \
    -Filter "^flatslide" -Script "map vrtesthall; wait60; god; notarget; cl_alwaysrun 1; vr_stamina_speed 0; wait10; +forward; vr_flatslide_test; wait3000; toggleconsole; quit" 2>&1)
expect_verdict "$sout" "flatslide" "#14 crouch slides at speed; prone at running speed never slides"

# 4) #9 vertical rope & pole climbing IK elbows flared outwards (lateral > 0, never inward)
for M in rope pole; do
    rout=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Clean -QuitOn "^vr_rope_flat_test: [0-9]+ passed" -Timeout 180 \
        -Filter "^vr_rope_flat_test" -Script "map vrropes; wait60; god; notarget; vr_climb_stamina 0; vr_rope_flat_test $M" 2>&1)
    el=$(echo "$rout" | grep -a "elbows while climbing" | head -1)
    expect_verdict "$rout" "vr_rope_flat_test" "#9 vertical $M climbing elbows flare outwards (${el#*: })"
done

verdict
exit $fail
