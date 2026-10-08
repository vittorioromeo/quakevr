#!/bin/bash
# cellcord_test.sh <agent> -- the cell cords (vr_cellcord.cpp; the author's notes vrfiringrange_2026-10-08_14-33-07,
# 14-33-36, 14-34-47), headless on e1m1 with vr_cellcord_info:
#   1. the laser cannon in the main hand: the pouch shows cells (QC VR_Reload_Corded), the cord plugged into cell 0, its
#      line's ends on the weapon's end and on the cell's contact;
#   2. no cells (give c 0): loose, its plug hanging under the weapon's end;
#   3. cells again (give c 50): plugging in, then plugged (onto the contact again);
#   4. a laser cannon in each hand: two cords, the main hand's into cell 0, the off hand's into the last (2);
#   5. Laser Cannon Cord off: no cord;
#   6. Mjolnir and the Super Axe with their cords on: plugged in.
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
F="^cellcord|^cell cords|^STEP"
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait60;god;notarget;vr_weapon_grip_mode 1;impulse 9;wait2;impulse 162;wait40;echo STEP1;vr_cellcord_info;give c 0;wait150;echo STEP2;vr_cellcord_info;give c 50;wait3;echo STEP3a;vr_cellcord_info;wait40;echo STEP3b;vr_cellcord_info;give c 100;impulse 182;wait40;echo STEP4;vr_cellcord_info;vr_cellcord_laser 0;wait5;echo STEP5;vr_cellcord_info;vr_cellcord_hammer 1;vr_cellcord_superaxe 1;impulse 153;wait40;echo STEP6a;vr_cellcord_info;impulse 168;wait40;echo STEP6b;vr_cellcord_info;toggleconsole;quit" -Filter "$F" 2>&1)
echo "$log" > "${OUT:-/dev/null}"
step() { echo "$log" | sed -n "/^STEP$1/,/^STEP/p"; }
near() { awk -v a="$1" 'BEGIN { print (a != "" && a + 0 <= 0.05) ? 1 : 0 }'; }
val() { echo "$1" | grep "^cellcord $2: [a-z]* [a-z]* cell=" | sed -E "s/.* $3=([-0-9.]+).*/\\1/"; }
s=$(step 1)
check $(echo "$s" | grep -q "the pouch shows 3 cells" && echo 1 || echo 0) "held: the pouch shows cells ($(echo "$s" | grep -o 'shows [0-9]* cells'))"
check $(echo "$s" | grep -q "^cellcord main: lasercannon plugged cell=0" && [ "$(near "$(val "$s" main gunend)")" = 1 ] && [ "$(near "$(val "$s" main plug)")" = 1 ] && echo 1 || echo 0) "held: plugged into cell 0, ends on the weapon ($(val "$s" main gunend)) and the contact ($(val "$s" main plug))"
check $(echo "$s" | grep -q "^cellcord off: none" && echo 1 || echo 0) "held: one cord (the off hand none)"
s=$(step 2)
below=$(val "$s" main below)
check $(echo "$s" | grep -q "^cellcord main: lasercannon loose cell=-1" && awk -v b="$below" 'BEGIN { exit !(b > 4) }' && echo 1 || echo 0) "no cells: loose, the plug $below units under the weapon's end"
s=$(step 3a)
check $(echo "$s" | grep -q "^cellcord main: lasercannon plugging cell=0" && echo 1 || echo 0) "cells again: plugging in ($(echo "$s" | grep -o 'plug=[0-9.]*') off the contact)"
s=$(step 3b)
check $(echo "$s" | grep -q "^cellcord main: lasercannon plugged cell=0" && [ "$(near "$(val "$s" main plug)")" = 1 ] && echo 1 || echo 0) "then plugged (plug $(val "$s" main plug) off the contact)"
s=$(step 4)
check $(echo "$s" | grep -q "^cellcord main: lasercannon plugged cell=0" && echo "$s" | grep -q "^cellcord off: lasercannon plugged cell=2" && [ "$(near "$(val "$s" off plug)")" = 1 ] && [ "$(near "$(val "$s" off gunend)")" = 1 ] && echo 1 || echo 0) "two laser cannons: two cords, cells 0 and 2"
s=$(step 5)
check $(echo "$s" | grep -q "^cellcord main: none" && echo "$s" | grep -q "^cellcord off: none" && echo 1 || echo 0) "Laser Cannon Cord off: none"
s=$(step 6a)
check $(echo "$s" | grep -q "^cellcord main: mjolnir plugged cell=0" && echo 1 || echo 0) "Mjolnir Cord on: plugged"
s=$(step 6b)
check $(echo "$s" | grep -q "^cellcord main: superaxe plugged cell=0" && echo 1 || echo 0) "Super Axe Cord on: plugged"
exit $fail
