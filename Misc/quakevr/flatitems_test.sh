#!/bin/bash
# flatitems_test.sh <agent> -- pickups in flat-screen play (docs/vr-port/FLATPLAY.md, "Pickups"), headless, through the real
# paths: the map's spawn functions, E (impulse 46), +attack, +forward. QC side: vr_flatitems_test.qc.
#   hold    what stays in the off hand with the shotgun fired through it: no assertion, the shot goes, the hand keeps it
#   give    armour in the left hand, the shotgun from 2, `give n 40`, the shot: the ammo id stays an id, no assertion
#   headset the same give and shot in a headset game with no magazine (vr_reload_mode 0): the engine's bug is shared
#   walk    armour, keys, the suit, a rune, powerups and a weapon walked over: taken; armour puts the plates on (ammo and
#           health boxes: printed, not checked: FLATPLAY.md, "Pickups")
#   better  E on armour better than his: on him at once, nothing left in a hand, the plates on
#   vrwalk  headset: walking over armour still leaves it lying (his rule unchanged)
# Prints PASS/FAIL, exits 1 on a failure; ends with `flatitems_sh: N passed, M failed`.
AGENT=${1:?agent}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch; mkdir -p "$S"
QT_NAME=flatitems_sh
F="^flatitems:|^flatplates:|Assertion|ENGINE CRASH|Host_Error|^ff_done"
P="map e1m1; wait60; developer 1; god; notarget"
run() # run <tag> <flat|vr> <script> <quit-on>
{
    local extra=()
    [ "$2" = flat ] && extra=(-ExtraArgs "-flatscreenmode")
    bash "$KIT/run.sh" "$AGENT" -Low "${extra[@]}" -Timeout 180 -QuitOn "$4" -Script "$3" -Filter "$F" > "$S/$1.log" 2>&1
}
V="flatitems: [0-9]+ passed"
noassert() { grep -aq "Assertion failure" "$S/$1.log" && echo 0 || echo 1; }

run fi_hold flat "$P; vr_flatitems_test 1" "$V"
expect_verdict "$(cat "$S/fi_hold.log")" "flatitems" "held in the off hand, the shotgun fired"
check $(noassert fi_hold) "... and no assertion ($(grep -ac 'Assertion failure' "$S/fi_hold.log"))"

run fi_give flat "$P; vr_flatitems_test 5" "$V"
expect_verdict "$(cat "$S/fi_give.log")" "flatitems" "armour in the left hand, give, the shot"
check $(noassert fi_give) "... and no assertion ($(grep -ac 'Assertion failure' "$S/fi_give.log"))"

run fi_headset vr "$P; vr_reload_mode 0; impulse 154; wait20; give n 40; wait5; +attack; wait40; -attack; wait5; echo ff_done" "^ff_done"
check $(noassert fi_headset) "headset, no magazine: give and a shot: no assertion ($(grep -ac 'Assertion failure' "$S/fi_headset.log"))"

run fi_walk flat "$P; vr_flatitems_test 2" "$V"
grep -a "FAIL" "$S/fi_walk.log" | cut -c1-140
expect_verdict "$(cat "$S/fi_walk.log")" "flatitems" "each kind of pickup walked over"
grep -a "(a box)" "$S/fi_walk.log" | cut -c1-90

run fi_better flat "$P; vr_flatitems_test 3" "$V"
grep -a "FAIL" "$S/fi_better.log" | cut -c1-140
expect_verdict "$(cat "$S/fi_better.log")" "flatitems" "E on better armour"

# The headset's rule stands: armour stays until a hand takes it (the same walk: test 2 in a headset game; every armour and
# key FAILs there by design, what is checked is that they are left).
run fi_vrwalk vr "$P; vr_body_interactions 0; vr_flatitems_test 2" "$V"
left=$(grep -ac "walked over the .*armour: taken 0\|walked over the .*key: taken 0\|walked over the biosuit: taken 0" "$S/fi_vrwalk.log")
check $([ "$left" = 6 ] && echo 1 || echo 0) "headset: armour, keys and the suit walked over are left lying, as they were (left: $left of 6)"

verdict; exit $fail
