#!/bin/bash
# flatgrabthrow_test.sh <agent> -- end-to-end automated tests for flatscreen unified Throw / Drop on B
# (vr_flat_key_throw, tap < vr_flat_throw_tap_time drops / releases stuck blade, hold throws) and Grab / Pull on H
# (vr_flat_key_pull, uses every available empty hand to grab crates, active/shoved ragdolls and gibs; Toggle default
# vr_flat_grab_hold 0 and Hold mode vr_flat_grab_hold 1). flathold2: big props (crates, explosive boxes; vr_flat_grab_drag_mass)
# dragged by their surface with the hands drawn on it, small ones (a health box, a rock, a gib) in the hand as a force grab's,
# nothing beyond vr_flat_grab_reach (1.25 m).
# Prints "flatgrabthrow: N passed, M failed".
AGENT=${1:?agent}
QT_NAME=flatgrabthrow
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch/flatgrabthrow
mkdir -p "$S"
FILTER='^(flatgrabthrow:|flat ?(grab|throw|drop)|propdrag:|ragdoll hand|flatkeys:|ragdoll:|vr_physics_spawn:|force grab:|flatplay:)'

run() {
    local tag=$1 script=$2
    bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Timeout 180 \
        -Script "$script; echo flatgrabthrow: $tag done; toggleconsole; quit" \
        -QuitOn "^flatgrabthrow: $tag done" -Filter "$FILTER" > "$S/$tag.log" 2>&1
}

BASE="map vrtesthall; god; notarget; developer 1; vr_flat_grenade_debug 1; vr_debug_ragdoll 1; wait15"
BD="vr_mock_key b down"; BU="vr_mock_key b up"
HD="vr_mock_key h down"; HU="vr_mock_key h up"

# 1. Tap B weapon drop vs Hold B weapon throw.
run weapon "$BASE; setpos 0 -200 25 0 90 0; wait5; impulse 152; wait20; echo flatgrabthrow: tap_start; $BD; wait3; $BU; wait60; vr_flatkeys_test 4; wait5; echo flatgrabthrow: hold_start; impulse 152; wait20; $BD; wait85; $BU; wait120; vr_flatkeys_test 4; wait5"
L=$S/weapon.log
check $(grep -aq 'flatgrabthrow: weapon done' "$L" && echo 1 || echo 0) "weapon: run completed"
DROP_V=$(tr '\r\n' ' ' < "$L" | sed -n 's/.*flat drop: thrown_weapon dropped[^,]*, it leaves at \([0-9]*\) *u\/s.*/\1/p')
THROW_V=$(tr '\r\n' ' ' < "$L" | sed -n 's/.*flat throw: thrown_weapon thrown[^,]*, it leaves at \([0-9]*\) *u\/s.*/\1/p')
check $([ -n "$DROP_V" ] && [ "$DROP_V" -lt 120 ] && echo 1 || echo 0) "weapon tap B: drops axe gently (${DROP_V:-none} u/s)"
check $(grep -aq 'flatkeys: held main - .* wpn 0 0' "$L" && echo 1 || echo 0) "weapon tap B: hand is empty after tap drop"
check $([ -n "$THROW_V" ] && [ "$THROW_V" -gt 300 ] && echo 1 || echo 0) "weapon hold B: throws axe at full speed (${THROW_V:-none} u/s)"

# 2. H drags a crate by its surface with 2 empty hands, moves/pulls it while walking, releases in Toggle mode, re-grabs & throws with Hold B;
#    then grabs with 1 empty hand while main hand holds an axe; then Hold mode (vr_flat_grab_hold 1) grab & release.
run crate "$BASE; vr_flat_grab_hold 0; setpos 0 -200 25 20 90 0; wait5; vr_physics_spawn vr_crate 28; wait20; \
    echo flatgrabthrow: t2h_grab; $HD; wait3; $HU; wait20; vr_flatkeys_test 4; vr_flatgrenade_report; vr_ragdoll_hand_probe; wait5; \
    echo flatgrabthrow: t2h_walk; +back; wait35; -back; wait10; vr_flatkeys_test 4; wait5; \
    echo flatgrabthrow: t2h_rel; $HD; wait3; $HU; wait15; vr_flatkeys_test 4; vr_flatgrenade_report; wait5; \
    echo flatgrabthrow: t2h_throw; setpos 0 -80 25 20 90 0; wait5; vr_physics_spawn vr_crate 28; wait20; $HD; wait3; $HU; wait20; setpos 0 -80 25 -6 90 0; wait5; $BD; wait85; $BU; wait60; vr_flatkeys_test 4; wait5; \
    echo flatgrabthrow: onehand; setpos 160 -200 25 20 90 0; wait5; impulse 152; wait15; vr_physics_spawn vr_crate 28; wait20; $HD; wait3; $HU; wait25; vr_flatkeys_test 4; wait5; $HD; wait3; $HU; wait15; vr_flatkeys_test 4; wait5; impulse 47; wait10; \
    echo flatgrabthrow: holdmode; vr_flat_grab_hold 1; setpos -160 -200 25 20 90 0; wait5; vr_physics_spawn vr_crate 28; wait20; $HD; wait25; vr_flatkeys_test 4; vr_flatgrenade_report; wait5; $HU; wait20; vr_flatkeys_test 4; vr_flatgrenade_report; wait5; vr_flat_grab_hold 0"
L=$S/crate.log
check $(grep -aq 'flatgrabthrow: crate done' "$L" && echo 1 || echo 0) "crate: run completed"
check $(grep -aq 'flat grab: drags vr_crate (40 kg) by its surface with both hands' "$L" && echo 1 || echo 0) "crate 2H: H drags the crate by its surface with both empty hands"
check $(awk '/flatgrabthrow: t2h_grab/{a=1} /flatgrabthrow: t2h_walk/{exit} a && /ragdoll hand .* rest: the palm .* cm from the limb.s mesh as held, -?[0-4]\.[0-9] cm rested on it/{n++} END{exit !(n >= 2)}' "$L" && echo 1 || echo 0) "crate 2H: both hands drawn on its surface (palms within 5 cm of its mesh: vr_ragdoll_hand_probe)"
check $(awk '/flatgrabthrow: t2h_grab/{a=1} /flatgrabthrow: t2h_walk/{exit} a && /flatgrab: mode toggle, held off 1 main 1 \(holdPose 1\)/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "crate 2H: client reports both hands holding with hold pose"
Y0=$(awk '/flatgrabthrow: t2h_grab/{a=1} /flatgrabthrow: t2h_walk/{exit} a && /flatkeys: held main vr_crate/{print $6; exit}' "$L")
Y1=$(awk '/flatgrabthrow: t2h_walk/{a=1} /flatgrabthrow: t2h_rel/{exit} a && /flatkeys: held main vr_crate/{print $6; exit}' "$L")
check $(awk -v a="$Y0" -v b="$Y1" 'BEGIN{exit !(a != "" && b != "" && (a - b) > 30)}' && echo 1 || echo 0) "crate 2H: backing away drags the crate along (y: ${Y0:-?} -> ${Y1:-?})"
check $(awk '/flatgrabthrow: t2h_rel/{a=1} /flatgrabthrow: t2h_throw/{exit} a && /flatkeys: held main - .* off - .* grab 0/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "crate Toggle: pressing H again releases crate cleanly"
check $(awk '/flatgrabthrow: t2h_throw/{a=1} /flatgrabthrow: onehand/{exit} a && /flat throw: vr_crate thrown/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "crate Hold B: throws H-grabbed crate"
check $(awk '/flatgrabthrow: onehand/{a=1} /flatgrabthrow: holdmode/{exit} a && /flat grab: drags vr_crate .* by its surface with off hand/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "crate 1H: with weapon in main hand, H drags crate with empty off hand"
check $(awk '/flatgrabthrow: onehand/{a=1} /flatgrabthrow: holdmode/{exit} a && /flatkeys: held main - .* off vr_crate .* wpn 2 0/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "crate 1H: main hand keeps axe while off hand holds crate"
check $(awk '/flatgrabthrow: holdmode/{a=1} a && /flatgrab: mode hold, held off 1 main 1/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "crate Hold mode: holding H grabs and holds crate"
check $(awk '/flatgrabthrow: holdmode/{a=1} a && /flatgrab: mode hold, held off 0 main 0/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "crate Hold mode: releasing H drops crate cleanly"

# 3. Shove a grunt -> active ragdoll -> H grabs with both hands -> walk/pull -> Hold B throws it!
run ragdoll "$BASE; vr_flat_grab_hold 0; setpos 0 -200 25 0 90 0; wait5; vr_physics_spawn monster_army 48; wait20; \
    echo flatgrabthrow: shove; vr_mock_key mouse5 down; wait4; vr_mock_key mouse5 up; wait20; \
    echo flatgrabthrow: rag_grab; setpos 0 -175 25 22 90 0; wait5; $HD; wait3; $HU; wait25; vr_flatkeys_test 4; vr_flatgrenade_report; wait5; \
    echo flatgrabthrow: rag_throw; setpos 0 -175 25 -8 90 0; wait10; $BD; wait85; $BU; wait60; vr_flatkeys_test 4; wait5"
L=$S/ragdoll.log
check $(grep -aq 'flatgrabthrow: ragdoll done' "$L" && echo 1 || echo 0) "ragdoll: run completed"
check $(grep -aq 'flat grab: took ragdoll monster_army (active) with both hands' "$L" && echo 1 || echo 0) "ragdoll: after shove, H grabs active ragdoll with both hands"
check $(awk '/flatgrabthrow: rag_grab/{a=1} /flatgrabthrow: rag_throw/{exit} a && /flatkeys: held main monster_army .* off monster_army .* grab 3/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "ragdoll: both hands hold active ragdoll"
RAG_V=$(tr '\r\n' ' ' < "$L" | sed -n 's/.*ragdoll: [0-9]* whole-body thrown by player 1 at \([0-9]*\) u\/s.*/\1/p')
check $([ -n "$RAG_V" ] && [ "$RAG_V" -gt 180 ] && echo 1 || echo 0) "ragdoll Hold B: throws active ragdoll (${RAG_V:-none} u/s)"
check $(awk '/flatgrabthrow: rag_throw/{a=1} a && /flatkeys: held main - .* off - .* grab 0/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "ragdoll Hold B: hands empty after throw"

# 4. Gib on the ground (non-force-grabbable with default vr_grab_gibs 1): force grab fails, H grabs it, Hold B throws it!
run gib "$BASE; vr_flat_grab_hold 0; vr_grab_gibs 1; setpos 0 -200 25 22 90 0; wait5; vr_flatkeys_test 3; wait35; setpos 0 -174 25 45 90 0; wait5; \
    echo flatgrabthrow: fg_try; impulse 46; wait20; vr_flatkeys_test 4; wait5; \
    echo flatgrabthrow: gib_grab; $HD; wait3; $HU; wait20; vr_flatkeys_test 4; wait5; \
    echo flatgrabthrow: gib_throw; setpos 0 -174 25 -5 90 0; wait5; $BD; wait75; $BU; wait40; vr_flatkeys_test 4; wait5"
L=$S/gib.log
check $(grep -aq 'flatgrabthrow: gib done' "$L" && echo 1 || echo 0) "gib: run completed"
check $(awk '/flatgrabthrow: fg_try/{a=1} /flatgrabthrow: gib_grab/{exit} a && /flatkeys: held main - .* off -/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "gib: default vr_grab_gibs 1 is NOT force-grabbable (control)"
check $(awk '/flatgrabthrow: gib_grab/{a=1} /flatgrabthrow: gib_throw/{exit} a && /flat grab: took vr_test_gib/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "gib: H grabs non-force-grabbable gib off the ground"
check $(awk '/flatgrabthrow: gib_throw/{a=1} a && /flat throw: vr_test_gib thrown/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "gib Hold B: throws H-grabbed gib"

# 5. flathold2: an explosive box dragged by its surface (and along as you back away), a health box and a rock in the hand as a
#    force grab's (no hold pose), a crate beyond the reach not grabbed.
run kinds "$BASE; vr_flat_grab_hold 0; \
    echo flatgrabthrow: explo; setpos -200 -200 25 20 90 0; wait5; vr_physics_spawn misc_explobox 28; wait25; $HD; wait3; $HU; wait30; vr_flatkeys_test 4; vr_flatgrenade_report; vr_ragdoll_hand_probe; wait5; \
    echo flatgrabthrow: explo_back; +back; wait25; -back; wait15; vr_flatkeys_test 4; vr_flatgrenade_report; wait5; $HD; wait3; $HU; wait15; vr_flatkeys_test 4; wait5; \
    echo flatgrabthrow: health; setpos 200 -200 25 50 90 0; wait5; vr_physics_spawn item_health 10; wait40; $HD; wait3; $HU; wait25; vr_flatkeys_test 4; vr_flatgrenade_report; wait5; $BD; wait3; $BU; wait20; \
    echo flatgrabthrow: rock; setpos 100 -200 25 50 90 0; wait5; vr_physics_spawn vr_debris_piece 14; wait40; $HD; wait3; $HU; wait25; vr_flatkeys_test 4; vr_flatgrenade_report; wait5; $BD; wait3; $BU; wait20; \
    echo flatgrabthrow: far; setpos -300 -200 25 10 90 0; wait5; vr_physics_spawn vr_crate 64; wait30; $HD; wait3; $HU; wait20; vr_flatkeys_test 4; wait5"
L=$S/kinds.log
check $(grep -aq 'flatgrabthrow: kinds done' "$L" && echo 1 || echo 0) "kinds: run completed"
check $(awk '/flatgrabthrow: explo$|flatgrabthrow: explo /{a=1} /flatgrabthrow: explo_back/{exit} a && /flat grab: drags misc_explobox .* by its surface with both hands/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "explosive box: H drags it by its surface with both hands"
check $(awk '/flatgrabthrow: explo$|flatgrabthrow: explo /{a=1} /flatgrabthrow: explo_back/{exit} a && /ragdoll hand .* rest: the palm .* cm from the limb.s mesh as held, -?[0-4]\.[0-9] cm rested on it/{n++} END{exit !(n >= 2)}' "$L" && echo 1 || echo 0) "explosive box: both hands drawn on its surface (palms within 5 cm of its mesh)"
E0=$(awk '/flatgrabthrow: explo$|flatgrabthrow: explo /{a=1} /flatgrabthrow: explo_back/{exit} a && /flatkeys: held main misc_explobox/{print $6; exit}' "$L")
E1=$(awk '/flatgrabthrow: explo_back/{a=1} /flatgrabthrow: health/{exit} a && /flatkeys: held main misc_explobox/{print $6; exit}' "$L")
check $(awk -v a="$E0" -v b="$E1" 'BEGIN{exit !(a != "" && b != "" && (a - b) > 30)}' && echo 1 || echo 0) "explosive box: dragged along backing away (y: ${E0:-?} -> ${E1:-?})"
check $(awk '/flatgrabthrow: explo_back/{a=1} /flatgrabthrow: health/{exit} a && /flat grab: let go of dragged misc_explobox/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "explosive box: H again lets go of it"
check $(awk '/flatgrabthrow: health/{a=1} /flatgrabthrow: rock/{exit} a && /flat grab: took item_health with main hand/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "health box: in the hand as a force grab's catch"
check $(awk '/flatgrabthrow: health/{a=1} /flatgrabthrow: rock/{exit} a && /flatgrab: mode toggle, held off 0 main 1 \(holdPose 0\)/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "health box: no hold-out pose (held in the hand)"
check $(awk '/flatgrabthrow: rock/{a=1} /flatgrabthrow: far/{exit} a && /flat grab: took vr_rock with main hand/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "rock: in the hand as a force grab's catch"
check $(awk '/flatgrabthrow: far/{a=1} a && /flat grab: nothing grabbable in reach/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "reach: a crate 64 units ahead is beyond the reach"
check $(awk '/flatgrabthrow: far/{a=1} a && /flatkeys: held main - .* off - .* grab 0/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "reach: nothing taken"

verdict
exit $fail
