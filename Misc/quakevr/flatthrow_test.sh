#!/bin/bash
# flatthrow_test.sh <agent> -- flat-screen play's throw key (J, vr_flat_key_throw; his ask 2026-11-23; QC vr_flatgrenade.qc
# "The throw key", Quake/vr/vr_flatgrenade.cpp; docs/vr-port/FLATPLAY.md, "The throw key"). Headless, fast mode, in the
# tutorial (vrtutorial); each run ends on its own last line (-QuitOn) and flatthrow_check.py prints the checks and
# "flatthrow: N passed, M failed".
#   axe    lesson 12: an axe in the main hand, J held 1.1 s at the high target: the throw presses it (r12b_open) and
#          the axe sticks
#   rock   lesson 10: a rock force-grabbed off the table, tapped east (a quick toss: it flies and lands); another
#          grabbed, J held 1.1 s at the button behind the bars: it presses it (r10_open, the gate); the charged throw
#          leaves much faster than the tap
#   gren   G, then J held 0.7 s with the quick grenade: thrown as its fire button's release throws it (the shared motion)
#   twoh   the nail gun held two-handed (the free hand on its hotspot), J: the free hand lets go (hanging) as it winds
#          up and the gun is thrown one-handed (his call 2026-11-23; no two-handed throw)
#   nofire the nail gun: the fire button held while J winds it up shoots nothing (his words 2026-11-23), held before J
#          it shoots (the control); the axe and the charged rock leave from their hand's side of the eye (flatthrow3)
#   wall   lesson 12's plain south wall: an axe charged from 280 units (flying past vr_throw_self_grace, once never stuck)
#          and a sword from 150: both stick (the flat throw's timed spin, bladespinto)
AGENT=${1:?agent}; ONLY=$2 # (optional: the runs to make, "axe rock twoh")
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flatthrow
mkdir -p "$S"
HERE=$(cd "$(dirname "$0")" && pwd -W)
FILTER="^(  (stance|two hands):|flat ?(throw|grenade|gren)|throw: a (weapon|prop) held in both|button: .*pressed|force grab: caught|flatthrow_test|axestick|carry: thrown|bladespinto)"
run() { local tag=$1 script=$2; [ -n "$ONLY" ] && [[ " $ONLY " != *" $tag "* ]] && return
        bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode -Script "$script; echo flatthrow_test: $tag done; toggleconsole; quit" \
            -QuitOn "^flatthrow_test: $tag done" -Timeout 150 -Filter "$FILTER" > "$S/$tag.log" 2>&1; }
BASE="map vrtutorial; god; notarget; developer 1; vr_debug_wallbuttons 1; vr_flat_grenade_debug 1; wait10"
J="vr_mock_key j down"; JU="vr_mock_key j up"
# (Room 12b's target: x 1920, high on the south wall (y -814, z 32); from 210 units north on the floor it is 32 deg up.
# Room 10's button: x 2928 behind the bars (y -414, z -80), from y -200 a little above the eye. Its table's rocks at
# y -296, z -104. 80 frames = 1.1 s: past vr_flat_throw_charge_time 1; the force grab's flight takes under 60.)
GRAB="setpos 2770 -240 -120 6 270 0; wait5; impulse 46; wait60"
GRAB2="setpos 2800 -240 -120 6 270 0; wait5; impulse 46; wait60" # (the next rock along the table)

run axe "$BASE; setpos 1990 -640 -120 -35 248 0; wait5; impulse 152; wait30; $J; wait80; $JU; wait300"
run rock "$BASE; $GRAB; setpos 3200 0 -120 -10 0 0; wait5; $J; wait2; $JU; wait300; $GRAB2; setpos 2928 -200 -120 -5 270 0; wait5; $J; wait80; $JU; wait300"
run gren "$BASE; give r 10; setpos 3200 0 -120 0 0 0; wait5; vr_mock_key g down; wait2; vr_mock_key g up; wait50; $J; wait50; $JU; wait60; vr_flatgrenade_report; wait2"
run twoh "$BASE; setpos 3200 0 -120 0 0 0; wait5; vr_test_give_wid 6; impulse 149; give n 200; wait90; echo flatthrow_test: before; vr_flatplay_status; $J; wait40; echo flatthrow_test: winding; vr_flatplay_status; $JU; wait150"
run nofire "$BASE; setpos 3200 0 -120 0 0 0; wait5; vr_test_give_wid 6; impulse 149; give n 200; wait60; $J; wait5; $JU; wait60; impulse 149; wait60; +attack; wait20; -attack; wait5; $J; wait10; +attack; wait40; -attack; wait2; $JU; wait60"
run wall "$BASE; vr_debug_axestick 1; setpos 2000 -520 -120 0 270 0; wait5; impulse 152; wait30; $J; wait80; $JU; wait200; setpos 2000 -650 -120 0 270 0; wait5; impulse 163; wait30; $J; wait80; $JU; wait200"

python "$HERE/flatthrow_check.py" "$S"
exit $?
