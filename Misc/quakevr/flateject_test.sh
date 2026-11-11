#!/bin/bash
# flateject_test.sh <agent> -- the super shotgun's and the revolver's spent shells thrown off away from their chambers
# (vr_shells_ssg_* / vr_shells_rv_*, vr_shells_face_deflect; Quake/vr/vr_shells.cpp), the flat super shotgun's shells
# out once it is drawn fully open (vr_flat_ssg_eject_delay; QVR_EJECT_AFTER_OPEN), and the revolver's open pose
# (vr_flat_rv_open_*; vr_flatreload.cpp). Headless, fast mode; each run ends on its own last line (-QuitOn), and
# flateject_check.py prints the checks and "flateject: N passed, M failed".
#   ssg       flat: the super shotgun fired empty breaks open; its two shells leave after it is drawn open (its barrels
#             down, its open pose eased in) and vr_flat_ssg_eject_delay, thrown sideways past the face and up
#   ssgold    the same with the old ejection (vr_shells_face_deflect 0, 1.6 m/s back and up): the control, dropped
#   rv        flat: the revolver fired empty (six pulls): its cylinder out, the gun in its open pose, its six shells straight
#             back out of their chambers (vr_shells_rv_stroke), then thrown off away from the frame (not across the gun)
#   rvold     the same, the old (0.5 m/s straight back, no deflection)
#   vr        the mock headset (VR): the same shared ejection, the revolver's casings by vr_shells_eject: thrown off too
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flateject
mkdir -p "$S"
HERE=$(cd "$(dirname "$0")" && pwd -W)
FILTER="^(shells (velocity|eject|rv)|shells: hand|flatreload: (main|off) hand: (open|the gun)|flatreload: main hand in its open|flateject_test)"
run() { local tag=$1 extra=$2 script=$3
        bash $KIT/run.sh $AGENT $extra -Script "$script; echo flateject_test: $tag done; toggleconsole; quit" \
            -QuitOn "^flateject_test: $tag done" -Timeout 150 -Filter "$FILTER" > "$S/$tag.log" 2>&1; }
FLAT="-ExtraArgs -flatscreenmode"
BASE="vr_flat_holsters 0; map vrtesthall; god; notarget; developer 1; vr_flat_reload_debug 1; vr_debug_weaponfx 1; wait5; setpos 0 300 25 0 90 0; wait5"
OLDSSG="vr_shells_face_deflect 0; vr_shells_face_clear 100; vr_shells_ssg_speed 1.6; vr_shells_ssg_out 0; vr_flat_ssg_eject_delay 0"
OLDRV="vr_shells_rv_stroke 0; vr_shells_face_deflect 0; vr_shells_face_clear 100; vr_shells_rv_speed 0.5; vr_shells_rv_up 0; vr_shells_rv_out 0"
# The super shotgun (wid 5): one pull fires both barrels; it breaks open vr_flat_ssg_break_delay (0.15 s) later, is drawn
# open over its open pose's 0.2 s, its shells out 0.05 s after: 60 frames cover it.
SSG="vr_flat_autoreload 0; vr_test_give_wid 5; impulse 149; give s 50; wait30; +attack; wait3; -attack; wait60; vr_flatreload_report; wait2"
run ssg "$FLAT" "$BASE; $SSG"
run ssgold "$FLAT" "$BASE; $OLDSSG; $SSG"
# The revolver (wid 31): six pulls (vr_revolver_cooldown 0.3 s = 22 frames apart), its cylinder out 0.15 s after the
# last, its shells out 0.14 + 0.1 s after that: 40 frames cover it.
PULL="+attack; wait3; -attack; wait30"
RV="vr_flat_autoreload 0; vr_test_give_wid 31; impulse 149; give s 50; wait30; $PULL; $PULL; $PULL; $PULL; $PULL; $PULL; wait40; vr_flatreload_report; wait2"
run rv "$FLAT" "$BASE; $RV"
run rvold "$FLAT" "$BASE; $OLDRV; $RV"
run vr "" "$BASE; +grabmain; vr_mock_button main grip 1; wait2; vr_test_give_wid 31; impulse 149; wait30; vr_shells_eject 1 2; wait10"
python "$HERE/flateject_check.py" "$S"
exit $?
