#!/bin/bash
# flatzoom_test.sh <agent> -- flat-screen aim per weapon (ROUND25.md, "flat1115b"): a gun's own ironsight placing (weapons.txt
# fsight_fwd / _right / _up / _pitch, live as vr_wofs_fsight_*_NN) and its zoom-only aim (fsight_mode 1, or vr_flat_aim_zoom
# for every gun). Reads vr_flat_sight_check (the weapon as drawn against the crosshair's ray). Fast mode; one run, its
# script quits; the aim's travel is 18 frames (vr_flat_aim_time 0.25), each read 30 frames after a change.
#   a. the shotgun sighted: on the ray; Ironsight Up 2: both sights 2 units off it; Pitch 5: 5 degrees far along; back to 0;
#   b. vr_flat_aim_zoom 1 on the shotgun: the view narrows, the gun doesn't move, the crosshair stays;
#   c. per weapon (vr_flat_aim_zoom -1): the nailgun zooms (weapons.txt), the shotgun sights.
AGENT=${1:-pickup}
source "$(dirname "$0")/qvrtest.sh"
QT_NAME=flatzoom
export QVR_AUTHOR_WAIT_ALWAYS=1
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"
log=$HERE/scratch/flatzoom.log
P="map e1m1; god; notarget; developer 1; r_norefresh 0; setpos 480 -352 88 0 0 0; vr_flat_aim_zoom -1"
C="wait30; vr_flat_sight_check;"
S="$P; impulse 154; wait30; vr_flat_aim 1; $C vr_wofs_fsight_up_02 2; $C vr_wofs_fsight_up_02 0; vr_wofs_fsight_pitch_02 5; $C vr_wofs_fsight_pitch_02 0; $C"
S="$S vr_flat_aim_zoom 1; $C vr_flat_aim_zoom -1; vr_flat_aim 0; wait30; vr_test_give_wid 6; impulse 149; wait25; vr_flat_aim 1; $C"
S="$S vr_flat_aim 0; wait30; vr_test_give_wid 4; impulse 149; wait25; vr_flat_aim 1; $C vr_flat_aim 0; wait3; toggleconsole; quit"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$S" -Filter "^flatsightcheck: .*(rear off|zoom)" -Timeout 90 > "$log" 2>&1
grep -a "^flatsightcheck:" "$log" | cut -c1-170 | sed 's/^/  /'
line() { grep -a "^flatsightcheck:" "$log" | sed -n "${1}p"; }
field() { awk -v k="$2" -v k2="$3" '{ for (i = 1; i < NF; i++) if ($i == k && $(i+1) == k2) { print $(i+2) + 0; exit } }' <<<"$1"; }
farDeg() { awk '{ for (i = 1; i <= NF; i++) if ($i == "far") { d = $(i+3); gsub(/[(]/, "", d); print d + 0; exit } }' <<<"$1"; }
num() { awk -v k="$2" '{ for (i = 1; i < NF; i++) if ($i == k) { v = $(i+1); gsub(/[:,]/, "", v); print v + 0; exit } }' <<<"$1"; }
within() { awk -v v="${1:-999}" -v lo="$2" -v hi="$3" 'BEGIN { exit !(v >= lo && v <= hi) }' && echo 1 || echo 0; }
L1=$(line 1); L2=$(line 2); L3=$(line 3); L4=$(line 4); L5=$(line 5); L6=$(line 6); L7=$(line 7)
check $(within "$(field "$L1" rear off)" 0 0.02) "a. the shotgun sighted: its rear sight on the ray ($(field "$L1" rear off))"
check $(within "$(field "$L2" rear off)" 1.9 2.1) "a. Ironsight Up 2 (vr_wofs_fsight_up_02): the rear sight 2 units off the ray ($(field "$L2" rear off))"
check $(within "$(field "$L2" front off)" 1.9 2.1) "a. Ironsight Up 2: the front sight too, the gun parallel ($(field "$L2" front off))"
check $(within "$(farDeg "$L3")" 4.8 5.2) "a. Ironsight Pitch 5: the sight line 5 degrees off the ray far along ($(farDeg "$L3"))"
check $(within "$(field "$L4" rear off)" 0 0.02) "a. back to 0: on the ray again ($(field "$L4" rear off))"
zf=$(num "$L5" fov); zh=$(num "$L5" hip)
check $([[ $L5 == *v_shot.mdl\ zoom* ]] && awk -v f="${zf:-0}" -v h="${zh:-0}" 'BEGIN { exit !(f > 0 && h > 0 && f < h - 5) }' && echo 1 || echo 0) "b. vr_flat_aim_zoom 1: the shotgun zooms, the view narrowed ($zf from $zh)"
check $(within "$(num "$L5" shift)" 0 0.001) "b. zoom: the gun not moved onto the sights (shift $(num "$L5" shift))"
check $(within "$(num "$L5" xhair)" 0.99 1) "b. zoom: the crosshair stays (opacity $(num "$L5" xhair))"
check $([[ $L6 == *v_nail.mdl\ zoom* ]] && echo 1 || echo 0) "c. per weapon: the nailgun zooms (weapons.txt fsight_mode 1)"
check $([[ $L7 == *v_shot.mdl\ t* && $L7 == *rear\ off* ]] && within "$(field "$L7" rear off)" 0 0.02 | grep -q 1 && echo 1 || echo 0) "c. per weapon: the shotgun sights"
verdict; exit $fail
