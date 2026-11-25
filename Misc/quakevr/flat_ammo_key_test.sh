#!/bin/bash
# flat_ammo_key_test.sh <agent>: flat-screen play's Next Ammo Type key (vr_flat_key_ammo, N; impulse 43, QC
# VR_Impulse_WeaponButton / ToggleSecondaryAmmo): the rocket launcher in the main hand, N pressed twice: its other ammo,
# then its own again; with the toolgun in hand, N (its adjust key there) switches nothing. A weapon with no other ammo
# whose grip may be flipped (flipgrip, his words 2026-11-24): the dagger in the main hand, N flips its grip (reversed),
# N again back (upright), no ammo change; the shotgun (neither): N does nothing. Ends on "flatammo: N passed, M failed".
AGENT=${1:-posebake}
QT_NAME=flatammo
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch
KEY() { echo "vr_mock_key $1 down; wait3; vr_mock_key $1 up; wait3"; }
# (the dagger: impulse 149 with id 13 and flags 3072, its kind; 154: the shotgun. Counted waits: an impulse a frame.)
DAGGER="vr_test_give_wid 13; vr_test_give_flags 3072; impulse 149; wait10; echo flatammo: dagger"
bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode -Script "developer 1; vr_flat_reload_debug 1; vr_scenario flat_ammo_key; wait30; $(KEY n); $(KEY n); $(KEY F7); wait10; $(KEY n); $(KEY F7); wait10; $DAGGER; $(KEY n); wait5; $(KEY n); wait5; impulse 154; wait10; echo flatammo: shotgun; $(KEY n); wait5; echo flatammo: done; toggleconsole; quit" \
  -QuitOn "^flatammo: done" -Timeout 180 -Filter "^ammo|flatreload: n|flatammo|revgrip: hand|SCNERR" > "$S/flatammo.log" 2>&1
L="$S/flatammo.log"
check "$(grep -a '^ammo:' "$L" | head -1 | grep -qi 'rocket launcher: its other ammo' && echo 1 || echo 0)" "N: the rocket launcher to its other ammo"
check "$(grep -a '^ammo:' "$L" | sed -n 2p | grep -qi 'rocket launcher: its own ammo' && echo 1 || echo 0)" "N again: its own ammo"
# (N sent twice with the rocket launcher before the dagger: the toolgun's press swallowed)
check "$(test "$(awk '/^flatammo: dagger/{exit} {print}' "$L" | grep -ac '^flatreload: n: next ammo type')" = 2 && echo 1 || echo 0)" "the toolgun in hand: N sent nothing (2 presses sent of 3)"
D=$(awk '/^flatammo: dagger/{on=1;next} /^flatammo: shotgun/{on=0} on' "$L")
check "$(grep -a 'revgrip: hand' <<<"$D" | head -1 | grep -q 'flips its grip: reversed' && echo 1 || echo 0)" "the dagger: N flips its grip (reversed)"
check "$(grep -a 'revgrip: hand' <<<"$D" | sed -n 2p | grep -q 'flips its grip: upright' && echo 1 || echo 0)" "the dagger: N again flips it back (upright)"
check "$(grep -aq '^ammo:' <<<"$D" && echo 0 || echo 1)" "the dagger: no ammo change"
G=$(awk '/^flatammo: shotgun/{on=1;next} /^flatammo: done/{on=0} on' "$L")
check "$(grep -aq 'flatreload: n: next ammo type' <<<"$G" && ! grep -aq 'revgrip: hand\|^ammo:' <<<"$G" && echo 1 || echo 0)" "the shotgun (no other ammo, no flip): N sent, nothing changed"
verdict; exit $fail
