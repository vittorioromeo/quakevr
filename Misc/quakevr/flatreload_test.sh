#!/bin/bash
# flatreload_test.sh <agent> -- flat-screen play's reload (-flatscreenmode; QC vr_flatreload.qc, Quake/vr/vr_flatreload.cpp;
# docs/vr-port/FLATPLAY.md, "Reloading"). Headless, fast mode; each run ends on its own last line (-QuitOn), and
# flatreload_check.py prints the checks and "flatreload: N passed, M failed".
#   rkey      the shotgun, two shots (clip 6 of 8, 17 shells): R reloads it (8 of 8, 15: VR's hip-holster sums), the hand
#             at the hip by vr_flat_reload_down and back by the trip's end, muzzle down; R with the clip full, and with
#             no shells, does nothing; the trigger held through a second trip fires nothing until it is over
#   auto      vr_flat_autoreload 1: the shotgun fired dry reloads by itself; autooff: with 0 it stays empty
#   twohand   the nail gun held in the main hand, the free hand on its hotspot (the stance): during the trip it hangs, after
#             it goes back on
#   switch    a weapon switch (impulse 2) before the hip: the trip cancelled, nothing moved; after the hip: cancelled, the
#             rounds in; the gun back in the hand reloads again as ever, clip + shells the same throughout; Q (impulse 48,
#             the hands swapped) mid-trip cancels it too, the gun in the left hand with its clip
#   contact   vr_reload_mode 1 (any holster reloads in a headset): the gun at rest (its hand at the right upper holster:
#             hotspot 9), then held at the hip and half way there (vr_flat_reload_pose 1, 0.5), while the view turns, looks
#             up and down and walks: no reload (R and the empty clip only)
#   vrhip     the control: the mock headset, vr_reload_mode 1, the hand put at the right upper holster: it reloads (the
#             test sees a holster reload), with the same sums as the flat R
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flatreload
mkdir -p "$S"
HERE=$(cd "$(dirname "$0")" && pwd -W)
FILTER="^(flat ?reload|flathud: (left|right) hand|  stance|flatreload_test)"
run() { local tag=$1 extra=$2 script=$3
        bash $KIT/run.sh $AGENT $extra -Script "$script; echo flatreload_test: $tag done; toggleconsole; quit" \
            -QuitOn "^flatreload_test: $tag done" -Timeout 150 -Filter "$FILTER" > "$S/$tag.log" 2>&1; }
FLAT="-ExtraArgs -flatscreenmode"
BASE="map vrtesthall; god; notarget; developer 1; vr_flat_reload_debug 1; wait5; setpos 0 300 25 0 90 0; wait5"
REP="vr_flathud_report; vr_flatreload_report; wait2"
R="vr_mock_key r down; wait2; vr_mock_key r up"
SG="impulse 154; wait20"
TWO="+attack; wait40; -attack; wait10" # two shotgun shots (0.5 s apart)
# (A trip is vr_flat_reload_down + _hold + _up = 0.8 s, 58 frames: each wait after R counts that, plus the client's frame.)

run rkey "$FLAT" "$BASE; vr_flat_autoreload 0; $SG; $TWO; $REP; $R; wait62; $REP; $R; wait10; $TWO; $R; wait4; +attack; wait50; -attack; wait20; $REP; +attack; wait10; -attack; wait10; $REP; give s 0; wait2; $TWO; $R; wait10"
run auto "$FLAT" "$BASE; vr_flat_autoreload 1; $SG; +attack; wait300; -attack; wait62; $REP"
run autooff "$FLAT" "$BASE; vr_flat_autoreload 0; $SG; +attack; wait300; -attack; wait62; $REP"
run twohand "$FLAT" "$BASE; vr_flat_autoreload 0; vr_test_give_wid 6; impulse 149; give n 100; wait90; vr_flatplay_status; +attack; wait10; -attack; wait5; $R; wait30; vr_flatplay_status; wait90; vr_flatplay_status"
# (The main hand's cycle in vrtesthall: the shotgun, the fist, the axe, the shotgun: three presses bring it back.)
BACK="impulse 2; wait5; impulse 2; wait30"
run switch "$FLAT" "$BASE; vr_flat_autoreload 0; $SG; $TWO; $REP; $R; wait10; impulse 2; wait20; $REP; $BACK; $REP; $TWO; $REP; $R; wait30; impulse 2; wait20; $REP; $BACK; $REP; $TWO; $R; wait62; $REP; $TWO; $R; wait10; impulse 48; wait30; vr_flathud_report; vr_flatreload_report; wait2"
LOOK="cl_yawspeed 400; +right; wait36; -right; $REP; +left; wait54; -left; +lookdown; wait20; $REP; -lookdown; +lookup; wait30; -lookup; +forward; wait72; -forward; $REP; +moveleft; wait36; -moveleft; $REP"
run contact "$FLAT" "$BASE; vr_flat_autoreload 0; vr_reload_mode 1; $SG; $TWO; $REP; $LOOK; vr_flat_reload_pose 1; wait20; $REP; $LOOK; vr_flat_reload_pose 0.5; wait20; $REP; $LOOK; vr_flat_reload_pose -1; wait62; $REP"
# (The mock's grip held, so the hand keeps the gun; its trigger for the shots; impulse 154's gun comes loaded: two shots.)
VTWO="vr_mock_button main trigger 1; wait40; vr_mock_button main trigger 0; wait10"
run vrhip "" "$BASE; vr_reload_mode 1; +grabmain; vr_mock_button main grip 1; wait2; $SG; $VTWO; $TWO; $REP; vr_mock_hand_to main holster 5; wait20; $REP"

# The guns' own parts (his words 2026-11-10): the super shotgun (wid 5) fired empty breaks open after
# vr_flat_ssg_break_delay, dry-clicks open, and R shuts it; the nailgun (6) fired empty drops its magazine and R seats a
# new one; the revolver (31) fired twice: R swings its cylinder out, its 2 spent shells out, home with 6 live; and the
# visor crosshair's kick (vr_flat_xhair_kick): none before a shot, spread just after, back to none.
FILTER="^(flat ?reload|flathud: (left|right) hand|  stance|flatreload_test|reload: |a dry click|flatxhair: (main|off) kick)"
run ssg "$FLAT" "$BASE; vr_reload_debug 1; vr_flat_autoreload 0; vr_test_give_wid 5; impulse 149; give s 50; wait5; +attack; wait3; -attack; wait30; +attack; wait3; -attack; wait10; $R; wait62"
run mag "$FLAT" "$BASE; vr_reload_debug 1; vr_flat_autoreload 0; vr_test_give_wid 6; impulse 149; give n 200; wait5; +attack; wait500; -attack; wait20; $R; wait62"
run revolver "$FLAT" "$BASE; vr_reload_debug 1; vr_flat_autoreload 0; vr_test_give_wid 31; impulse 149; give s 50; wait5; +attack; wait5; -attack; wait60; +attack; wait5; -attack; wait60; $R; wait62"
run kick "$FLAT" "$BASE; vr_flat_reload 0; vr_flat_xhair_style 1; $SG; wait10; vr_flat_xhair_probe; +attack; wait4; -attack; vr_flat_xhair_probe; wait90; vr_flat_xhair_probe"

python "$HERE/flatreload_check.py" "$S"
exit $?
