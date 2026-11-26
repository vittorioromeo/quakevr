#!/bin/bash
# flatthrow_test.sh <agent> -- flat-screen play's throw key (B, vr_flat_key_throw; J until 2026-11-25; his ask 2026-11-23; QC vr_flatgrenade.qc
# "The throw key", Quake/vr/vr_flatgrenade.cpp; docs/vr-port/FLATPLAY.md, "The throw key"). Headless, fast mode, in the
# tutorial (vrtutorial); each run ends on its own last line (-QuitOn) and flatthrow_check.py prints the checks and
# "flatthrow: N passed, M failed".
#   axe    lesson 12: an axe in the main hand, B held 1.1 s at the high target: the throw presses it (r12b_open) and
#          the axe sticks
#   rock   lesson 10: a rock force-grabbed off the table, tapped east (a quick toss: it flies and lands); another
#          grabbed, B held 1.1 s at the button behind the bars: it presses it (r10_open, the gate); the charged throw
#          leaves much faster than the tap
#   gren   G, then B held 0.7 s with the quick grenade: thrown as its fire button's release throws it (the shared motion)
#   twoh   the nail gun held two-handed (the free hand on its hotspot), B: the free hand lets go (hanging) as it winds
#          up and the gun is thrown one-handed (his call 2026-11-23; no two-handed throw)
#   nofire the nail gun: the fire button held from before B, through the wind-up and the throw, shoots nothing then (his
#          words 2026-11-23; pressed during the wind-up it cancels: cancel), held before B it shoots (the control); the
#          axe and the charged rock leave from their hand's side of the eye (flatthrow3)
#   cancel (flatthrow2, his words 2026-11-26) the nail gun: B held, LMB pressed in the wind-up: cancelled, nothing thrown,
#          the gun still held, no nail shot (LMB held on past B's release, then let go); B again throws it. Then the quick
#          grenade (G): B held, LMB: cancelled, held again unarmed, never thrown
#   cancoff (flatthrow2) vrtesthall: the axe in the main hand, a health box force-grabbed into the off hand: B held, RMB
#          (the off hand's button): cancelled, the box still held, nothing thrown or dropped
#   guide  (flatthrow2) vrtesthall: QC vr_flatitems_test 12: an ogre's grenade into the guiding off hand's palm while B
#          winds up (the quick grenade, the axe) is not caught; without B it is (the control)
#   wall   lesson 12's plain south wall: an axe charged from 280 units (flying past vr_throw_self_grace, once never stuck)
#          and a sword from 150: both stick (the flat throw's timed spin, bladespinto)
#   move   (flatfb1124, his note 2026-11-24: a health box thrown running forward took a weird path) vrtesthall: a health
#          box tapped standing, running forward and strafing: each passes through his body as it leaves (box3d's throw
#          grace), flies clean until it comes down (no knock off his body), and keeps his motion (running: further)
#   release (flatthrow3, his words 2026-11-26: the hand travels forward with the thing still in it, then lets go) the nail
#          gun thrown with vr_flat_throw_release_time 0 (it leaves at once, at the wind-up) and with the default (it leaves
#          that long after B's release, from the hand further forward); the quick grenade's fire-button throw: the same delay
#   guide also (flatthrow3) no catch without H: a grenade at the palm caught only after an H press, an H press too early
#          catches nothing, a grenade thrown at him caught with H, not without (vr_flatitems_test 12)
AGENT=${1:?agent}; ONLY=$2 # (optional: the runs to make, "axe rock twoh")
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flatthrow
mkdir -p "$S"
HERE=$(cd "$(dirname "$0")" && pwd -W)
FILTER="^(  (stance|two hands):|flat ?(throw|drop|grenade|gren)|throw: a (weapon|prop) held in both|button: .*pressed|force grab: caught|flatthrow_test|axestick|carry: (thrown|with)|bladespinto|box3d: [0-9]+ item_health thrown)"
run() { local tag=$1 script=$2; [ -n "$ONLY" ] && [[ " $ONLY " != *" $tag "* ]] && return
        bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode -Script "$script; echo flatthrow_test: $tag done; toggleconsole; quit" \
            -QuitOn "^flatthrow_test: $tag done" -Timeout 150 -Filter "$FILTER" > "$S/$tag.log" 2>&1; }
BASE="map vrtutorial; god; notarget; developer 1; vr_debug_wallbuttons 1; vr_flat_grenade_debug 1; wait10"
J="vr_mock_key b down"; JU="vr_mock_key b up"
# (Room 12b's target: x 1920, high on the south wall (y -814, z 32); from 210 units north on the floor it is 32 deg up.
# Room 10's button: x 2928 behind the bars (y -414, z -80), from y -200 a little above the eye. Its table's rocks at
# y -296, z -104. 95 frames = 1.3 s: past tap time 0.18 s + vr_flat_throw_charge_time 1; the force grab's flight takes under 60.)
GRAB="setpos 2770 -240 -120 6 270 0; wait5; impulse 46; wait60"
GRAB2="setpos 2800 -240 -120 6 270 0; wait5; impulse 46; wait60" # (the next rock along the table)

run axe "$BASE; setpos 1990 -640 -120 -35 248 0; wait5; impulse 152; wait30; $J; wait95; $JU; wait300"
run rock "$BASE; $GRAB; setpos 3200 0 -120 -10 0 0; wait5; $J; wait2; $JU; wait180; $GRAB2; setpos 2928 -200 -120 -5 270 0; wait5; $J; wait95; $JU; wait300"
run gren "$BASE; give r 10; setpos 3200 0 -120 0 0 0; wait5; vr_mock_key g down; wait2; vr_mock_key g up; wait50; $J; wait65; $JU; wait60; vr_flatgrenade_report; wait2"
run twoh "$BASE; setpos 3200 0 -120 0 0 0; wait5; vr_test_give_wid 6; impulse 149; give n 200; wait90; echo flatthrow_test: before; vr_flatplay_status; $J; wait55; echo flatthrow_test: winding; vr_flatplay_status; $JU; wait150"
run nofire "$BASE; setpos 3200 0 -120 0 0 0; wait5; vr_test_give_wid 6; impulse 149; give n 200; wait60; $J; wait25; $JU; wait60; impulse 149; wait60; +attack; wait20; -attack; wait5; +attack; wait3; $J; wait65; $JU; wait2; -attack; wait60"
# (flatthrow2) cancel: the throwing hand's attack button pressed while B is held.
run cancel "$BASE; setpos 3200 0 -120 0 0 0; wait5; vr_test_give_wid 6; impulse 149; give n 200; wait60; echo flatthrow_test: cancel gun; $J; wait30; +attack; wait20; $JU; wait20; -attack; wait10; echo flatthrow_test: cancel gun again; $J; wait30; $JU; wait60; give r 10; vr_mock_key g down; wait2; vr_mock_key g up; wait50; echo flatthrow_test: cancel gren; $J; wait30; +attack; wait10; -attack; wait5; $JU; wait30; vr_flatgrenade_report; wait2"
# (flatthrow3) release: the throw leaves the hand vr_flat_throw_release_time after B's (or the fire button's) release.
G6="vr_test_give_wid 6; impulse 149; give n 200; wait60"
run release "$BASE; setpos 3200 0 -120 0 0 0; wait5; $G6; echo flatthrow_test: release 0; vr_flat_throw_release_time 0; $J; wait40; $JU; wait60; $G6; echo flatthrow_test: release default; vr_flat_throw_release_time 0.07; $J; wait40; $JU; wait60; echo flatthrow_test: release gren; give r 10; vr_mock_key g down; wait2; vr_mock_key g up; wait50; +attack; wait3; -attack; wait3; +attack; wait40; -attack; wait60"
MB0="map vrtesthall; god; notarget; developer 1; vr_flat_grenade_debug 1; wait60"
run cancoff "$MB0; impulse 152; wait30; setpos 0 -300 25 26 90 0; wait5; vr_physics_spawn item_health 64; wait30; impulse 46; wait60; echo flatthrow_test: cancoff; $J; wait30; +offhandattack; wait10; $JU; wait10; -offhandattack; wait30; vr_flatgrenade_report; wait2"
[ -z "$ONLY" ] || [[ " $ONLY " == *" guide "* ]] && bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode -Script "$MB0; vr_flatitems_test 12" \
    -QuitOn "^flatitems: [0-9]+ passed" -Timeout 150 -Filter "^(flatitems|FAIL|PASS|flat throw|grenade:)" > "$S/guide.log" 2>&1
# (flatfb1124) move: a health box (vrtesthall's open floor) thrown standing, running forward, then strafing right.
# (flatthrow3: standing and running from y -380, not -200: with the release's delay the running throw leaves some 35 units
# further on, and its first bounce reached what stands at y 350; the strafe stays at -200, clear at its side.)
MB="map vrtesthall; god; notarget; developer 1; vr_flat_grenade_debug 2; vr_debug_box3d 1; wait60"
HB="setpos 0 -300 25 26 90 0; wait5; vr_physics_spawn item_health 64; wait30; impulse 46; wait60"
MV() { echo "$HB; setpos $1 $5 25 0 90 0; wait5; echo flatthrow_test: move $2; $3; wait20; $J; wait18; $JU; wait30; $4; wait150"; }
run move "$MB; $(MV 0 stand wait1 wait1 -380); $(MV 0 run '+forward;+speed' '-forward;-speed' -380); $(MV -150 side '+moveright;+speed' '-moveright;-speed' -200); wait150"
run wall "$BASE; vr_debug_axestick 1; setpos 2000 -520 -120 0 270 0; wait5; impulse 152; wait30; $J; wait95; $JU; wait200; setpos 2000 -650 -120 0 270 0; wait5; impulse 163; wait30; $J; wait95; $JU; wait200"

python "$HERE/flatthrow_check.py" "$S"
exit $?
