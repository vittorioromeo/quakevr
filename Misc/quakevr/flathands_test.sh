#!/bin/bash
# flathands_test.sh <agent> -- flat-screen play's drop key, quick grenade and hand swap animation (-flatscreenmode; his
# notes 2026-11-10, items 12, 13, 14, 17; QC vr_wpnforcegrab.qc VR_Forcegrab_FlatDropHand, vr_flatgrenade.qc,
# Quake/vr/vr_flatgrenade.cpp, vr_flatswap.cpp; docs/vr-port/FLATPLAY.md, "The quick grenade", "The hand swap"). Headless,
# fast mode; each run ends on its own last line (-QuitOn), and flathands_check.py prints the checks and
# "flathands: N passed, M failed".
#   drop      the rocket launcher held in both hands (the free hand on its hotspot): G with no rockets drops nothing ("No
#             rockets"), H drops the gun and the off hand stays empty (never handed off to the grip hand)
#   gren2h    the rocket launcher in both hands, 10 rockets: G sends the off hand off the grip to the pouch, it comes back with an
#             unarmed grenade (9 rockets), RMB arms it, RMB held 0.7 s winds it up overhead, let go throws it (the speed
#             from the wind-up), the hand goes back onto the grip; the drawn hand at the pouch, held, overhead
#   grenempty both hands empty, one-press mode: G takes the main hand; LMB held arms and winds up, let go throws; G again
#             with one held unarmed puts it back (the rocket back)
#   grendual  a gun in each hand: G does nothing (no free hand)
#   grenoff   a shotgun in the main hand: G takes the off hand; RMB arms, RMB held winds up, let go throws
#   grenmain  a nailgun in the off hand: G takes the main hand; LMB arms, LMB held winds up, let go throws
#   (every run, vr_debug_carry 2: the hand slot's kind follows the grenade, a round's (4) unarmed, a grenade's (5) once
#   armed, at once: no "handslot: mismatch/clash/stale" line; the hand empty (kind 0) once it is thrown or put back)
#   swap1     a shotgun in the main hand: 3 brings the hands together, swaps at the meeting point, the shotgun in the off
#             hand after, the hands back
#   swap2h    the rocket launcher in both hands: 3 first takes the free hand off the grip to the empty hand's guard, then the gun
#             across to it (the swap there), then the other hand onto its grip: the gun in the off hand, the main on the grip
#   swapdual  a gun in each hand: they meet and swap
#   swapoff   vr_flat_swap_anim 0: the swap at once, no animation
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flathands
mkdir -p "$S"
HERE=$(cd "$(dirname "$0")" && pwd -W)
FILTER="^(flat ?(grenade|gren|swap|drop)|flathud: (left|right) hand|  stance|flathands_test|grenade: |weapon: .*handed off|No rockets|handslot: )"
run() { local tag=$1 script=$2 extra=$3
        bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode $extra -Script "$script; echo flathands_test: $tag done; toggleconsole; quit" \
            -QuitOn "^flathands_test: $tag done" -Timeout 150 -Filter "$FILTER" > "$S/$tag.log" 2>&1; }
BASE="map vrtesthall; god; notarget; developer 1; vr_debug_carry 2; vr_flat_grenade_debug 1; vr_flat_swap_debug 1; wait5; setpos 0 300 25 0 90 0; wait5"
REP="vr_flathud_report; wait2"
NAIL2H="vr_test_give_wid 10; impulse 149; wait90; vr_flatplay_status; wait2" # (the rocket launcher: its grip ahead, taken by the free hand: FLATPLAY.md, "The ironsight")
key() { echo "vr_mock_key $1 down; wait2; vr_mock_key $1 up"; }
G=$(key g); H=$(key h); SW=$(key 3)
# (The pouch trip is vr_flat_grenade_fetch_time 0.3 s = 22 frames, back up 0.3 s; the wind-up 0.25 s; the follow-through
# 0.15 s and the way back 0.3 s. The swap: 0.15 s to meet, its answer a frame or two, 0.2 s back: 30 frames covers it.)

run drop "$BASE; give r 0; $NAIL2H; $REP; $G; wait30; $REP; $H; wait30; $REP; vr_flatplay_status; wait2"
run gren2h "$BASE; give r 10; $NAIL2H; $REP; $G; wait50; vr_flatgrenade_report; wait2; +offhandattack; wait3; -offhandattack; wait10; vr_flatgrenade_report; +offhandattack; wait50; vr_flatgrenade_report; -offhandattack; wait40; vr_flatgrenade_report; wait90; vr_flatplay_status; $REP; wait200"
run grenempty "$BASE; give r 10; vr_flat_grenade_arm_mode 0; $G; wait50; $G; wait30; vr_flatgrenade_report; $G; wait50; +attack; wait30; -attack; wait40; vr_flatgrenade_report; wait200"
run grendual "$BASE; give r 10; impulse 154; wait5; impulse 176; wait20; $REP; $G; wait30; vr_flatgrenade_report; $REP"
run grenoff "$BASE; give r 10; impulse 154; wait20; $G; wait50; +offhandattack; wait3; -offhandattack; wait10; +offhandattack; wait50; -offhandattack; wait40; vr_flatgrenade_report"
run grenmain "$BASE; give r 10; impulse 176; wait20; $G; wait50; +attack; wait3; -attack; wait10; +attack; wait50; -attack; wait40; vr_flatgrenade_report"
run swap1 "$BASE; impulse 154; wait20; $REP; $SW; wait30; $REP; vr_flatswap_report"
run swap2h "$BASE; $NAIL2H; $REP; $SW; wait70; $REP; wait90; vr_flatplay_status; vr_flatswap_report"
run swapdual "$BASE; impulse 154; wait5; impulse 176; wait20; $REP; $SW; wait30; $REP; vr_flatswap_report"
run swapoff "$BASE; vr_flat_swap_anim 0; impulse 154; wait20; $REP; $SW; wait4; $REP; vr_flatswap_report"

python "$HERE/flathands_check.py" "$S"
exit $?
