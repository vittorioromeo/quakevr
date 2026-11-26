#!/bin/bash
# flatgrabthrow_test.sh <agent> -- end-to-end automated tests for flatscreen unified Throw / Drop on B
# (vr_flat_key_throw, tap < vr_flat_throw_tap_time drops / releases stuck blade, hold throws) and Grab / Pull on H
# (vr_flat_key_pull, uses every available empty hand to grab crates, active/shoved ragdolls and gibs; Toggle default
# vr_flat_grab_hold 0 and Hold mode vr_flat_grab_hold 1).
# Prints "flatgrabthrow: N passed, M failed".
AGENT=${1:?agent}
QT_NAME=flatgrabthrow
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch/flatgrabthrow
mkdir -p "$S"
FILTER='^(flatgrabthrow:|flat ?(grab|throw|drop)|flatkeys:|ragdoll:|vr_physics_spawn:|force grab:|flatplay:)'

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
THROW_V=$(tr '\r\n' ' ' < "$L" | sed -n 's/.*flat throw: thrown_weapon thrown[^)]*), it leaves at \([0-9]*\) *u\/s.*/\1/p')
check $([ -n "$DROP_V" ] && [ "$DROP_V" -lt 120 ] && echo 1 || echo 0) "weapon tap B: drops axe gently (${DROP_V:-none} u/s)"
check $(grep -aq 'flatkeys: held main - .* wpn 0 0' "$L" && echo 1 || echo 0) "weapon tap B: hand is empty after tap drop"
check $([ -n "$THROW_V" ] && [ "$THROW_V" -gt 300 ] && echo 1 || echo 0) "weapon hold B: throws axe at full speed (${THROW_V:-none} u/s)"

# 2. H grabs a crate with 2 empty hands, moves/pulls it while walking, releases in Toggle mode, re-grabs & throws with Hold B;
#    then grabs with 1 empty hand while main hand holds an axe; then Hold mode (vr_flat_grab_hold 1) grab & release.
run crate "$BASE; vr_flat_grab_hold 0; setpos 0 -200 25 14 90 0; wait5; vr_physics_spawn vr_crate 56; wait20; \
    echo flatgrabthrow: t2h_grab; $HD; wait3; $HU; wait20; vr_flatkeys_test 4; vr_flatgrenade_report; wait5; \
    echo flatgrabthrow: t2h_walk; +forward; wait35; -forward; wait10; vr_flatkeys_test 4; wait5; \
    echo flatgrabthrow: t2h_rel; $HD; wait3; $HU; wait15; vr_flatkeys_test 4; vr_flatgrenade_report; wait5; \
    echo flatgrabthrow: t2h_throw; setpos 0 -80 25 14 90 0; wait5; vr_physics_spawn vr_crate 56; wait20; $HD; wait3; $HU; wait20; setpos 0 -80 25 -6 90 0; wait5; $BD; wait85; $BU; wait60; vr_flatkeys_test 4; wait5; \
    echo flatgrabthrow: onehand; setpos 160 -200 25 14 90 0; wait5; impulse 152; wait15; vr_physics_spawn vr_crate 56; wait20; $HD; wait3; $HU; wait25; vr_flatkeys_test 4; wait5; $HD; wait3; $HU; wait15; vr_flatkeys_test 4; wait5; impulse 47; wait10; \
    echo flatgrabthrow: holdmode; vr_flat_grab_hold 1; setpos -160 -200 25 14 90 0; wait5; vr_physics_spawn vr_crate 56; wait20; $HD; wait25; vr_flatkeys_test 4; vr_flatgrenade_report; wait5; $HU; wait20; vr_flatkeys_test 4; vr_flatgrenade_report; wait5; vr_flat_grab_hold 0"
L=$S/crate.log
check $(grep -aq 'flatgrabthrow: crate done' "$L" && echo 1 || echo 0) "crate: run completed"
check $(grep -aq 'flat grab: took vr_crate with both hands' "$L" && echo 1 || echo 0) "crate 2H: H grabs crate with both empty hands"
check $(awk '/flatgrabthrow: t2h_grab/{a=1} /flatgrabthrow: t2h_walk/{exit} a && /flatgrab: mode toggle, held off 1 main 1 \(holdPose 1\)/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "crate 2H: client reports both hands holding with hold pose"
Y0=$(awk '/flatgrabthrow: t2h_grab/{a=1} /flatgrabthrow: t2h_walk/{exit} a && /flatkeys: held main vr_crate/{print $6; exit}' "$L")
Y1=$(awk '/flatgrabthrow: t2h_walk/{a=1} /flatgrabthrow: t2h_rel/{exit} a && /flatkeys: held main vr_crate/{print $6; exit}' "$L")
check $(awk -v a="$Y0" -v b="$Y1" 'BEGIN{exit !(a != "" && b != "" && (b - a) > 30)}' && echo 1 || echo 0) "crate 2H: walking forward moves/pulls crate with player (y: ${Y0:-?} -> ${Y1:-?})"
check $(awk '/flatgrabthrow: t2h_rel/{a=1} /flatgrabthrow: t2h_throw/{exit} a && /flatkeys: held main - .* off - .* grab 0/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "crate Toggle: pressing H again releases crate cleanly"
check $(awk '/flatgrabthrow: t2h_throw/{a=1} /flatgrabthrow: onehand/{exit} a && /flat throw: vr_crate thrown/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "crate Hold B: throws H-grabbed crate"
check $(awk '/flatgrabthrow: onehand/{a=1} /flatgrabthrow: holdmode/{exit} a && /flat grab: took vr_crate with off hand/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "crate 1H: with weapon in main hand, H grabs crate with empty off hand"
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
run gib "$BASE; vr_flat_grab_hold 0; vr_grab_gibs 1; setpos 0 -200 25 22 90 0; wait5; vr_flatkeys_test 3; wait35; \
    echo flatgrabthrow: fg_try; impulse 46; wait20; vr_flatkeys_test 4; wait5; \
    echo flatgrabthrow: gib_grab; $HD; wait3; $HU; wait20; vr_flatkeys_test 4; wait5; \
    echo flatgrabthrow: gib_throw; setpos 0 -200 25 -5 90 0; wait5; $BD; wait75; $BU; wait40; vr_flatkeys_test 4; wait5"
L=$S/gib.log
check $(grep -aq 'flatgrabthrow: gib done' "$L" && echo 1 || echo 0) "gib: run completed"
check $(awk '/flatgrabthrow: fg_try/{a=1} /flatgrabthrow: gib_grab/{exit} a && /flatkeys: held main - .* off -/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "gib: default vr_grab_gibs 1 is NOT force-grabbable (control)"
check $(awk '/flatgrabthrow: gib_grab/{a=1} /flatgrabthrow: gib_throw/{exit} a && /flat grab: took vr_test_gib/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "gib: H grabs non-force-grabbable gib off the ground"
check $(awk '/flatgrabthrow: gib_throw/{a=1} a && /flat throw: vr_test_gib thrown/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "gib Hold B: throws H-grabbed gib"

verdict
exit $fail
