#!/bin/bash
# flat_ammo_key_test.sh <agent>: flat-screen play's Next Ammo Type key (vr_flat_key_ammo, B; impulse 43, QC
# ToggleSecondaryAmmo): the rocket launcher in the main hand, B pressed twice: its other ammo, then its own again; with the
# toolgun in hand, B (its adjust key there) switches nothing. Ends on "flatammo: N passed, M failed".
AGENT=${1:-posebake}
QT_NAME=flatammo
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch
KEY() { echo "vr_mock_key $1 down; wait3; vr_mock_key $1 up; wait3"; }
bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode -Script "developer 1; vr_flat_reload_debug 1; vr_scenario flat_ammo_key; wait30; $(KEY b); $(KEY b); $(KEY F7); wait10; $(KEY b); echo flatammo: done; toggleconsole; quit" \
  -QuitOn "^flatammo: done" -Timeout 180 -Filter "^ammo:|flatreload: b|flatammo|SCNERR" > "$S/flatammo.log" 2>&1
L="$S/flatammo.log"
check "$(grep -a '^ammo:' "$L" | head -1 | grep -qi 'rocket launcher: its other ammo' && echo 1 || echo 0)" "B: the rocket launcher to its other ammo"
check "$(grep -a '^ammo:' "$L" | sed -n 2p | grep -qi 'rocket launcher: its own ammo' && echo 1 || echo 0)" "B again: its own ammo"
check "$(test "$(grep -ac '^flatreload: b: next ammo type' "$L")" = 2 && echo 1 || echo 0)" "the toolgun in hand: B sent nothing (2 presses sent of 3)"
verdict; exit $fail
