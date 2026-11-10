#!/bin/bash
# startgun_test.sh <agent> [case...] -- a new game's shotgun (2026-11-10, his words: "Shotgun still is unloaded after a
# new game starts and I grab it from the hip ... make sure that the magazine of the shotgun itself is loaded"). QC
# vr_reload_test 45-48 (vr_reload_test.qc, VR_Reload_TestStartGun) checks each step: the new game's right hip holster
# holds the shotgun with its own magazine full (8 of 8, 17 shells in the reserve); drawn from the hip into the main hand
# it is 8 of 8; one shot leaves 7 and the reserve untouched; a level change keeps the 7.
#   flate1m1, flatstart   flat-screen play (-flatscreenmode, its holsters on: vr_flat_holsters 1), a new game on e1m1
#                         and on start (then e1m2, e1m1): drawn by the holster menu's impulse (71: holster 3, right hand)
#   vre1m1, vrstart       the mock headset: the main hand at the right hip holster (the mock's holster 3), its grip
# Headless, fast mode; each run ends on its own changelevel verdict line (-QuitOn).
AGENT=${1:?agent}
shift
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/startgun
mkdir -p "$S"
CASES=${*:-flate1m1 flatstart vre1m1 vrstart}
FILTER="^startgun: "
T="impulse 125; wait2"
STEP() { echo "vr_reload_test $1; $T"; }
FLATDRAW="impulse 71; wait10"
FLATFIRE="wait40; +attack; wait3; -attack; wait30"
VRDRAW="vr_mock_hand_to main holster 3; wait5; +grabright; vr_mock_button main grip 1; wait10"
VRFIRE="wait40; +attack; wait3; -attack; wait30"
for c in $CASES; do
    case $c in
        flat*) EXTRA="-ExtraArgs -flatscreenmode"; PRE="vr_flat_holsters 1;"; DRAW=$FLATDRAW; FIRE=$FLATFIRE ;;
        *) EXTRA=""; PRE=""; DRAW=$VRDRAW; FIRE=$VRFIRE ;;
    esac
    case $c in
        *e1m1) MAP=e1m1; NEXT=e1m2 ;;
        *) MAP=start; NEXT=e1m1 ;;
    esac
    SCRIPT="$PRE map $MAP; wait30; developer 1; god; notarget; $(STEP 45); $DRAW; $(STEP 46); $FIRE; $(STEP 47); changelevel $NEXT; wait30; $(STEP 48); wait5; toggleconsole; quit"
    bash $KIT/run.sh $AGENT $EXTRA -Script "$SCRIPT" -QuitOn "^startgun: changelevel: [0-9]+ passed" -Timeout 150 \
        -Filter "$FILTER" > "$S/$c.log" 2>&1
    log=$(cat "$S/$c.log")
    grep -a "^startgun: \(step\|FAIL\)" <<<"$log" | sed "s/^/  $c: /"
    for part in newgame drawn fired changelevel; do
        expect_verdict "$log" "startgun: $part" "$c: $part"
    done
done
verdict
exit $fail
