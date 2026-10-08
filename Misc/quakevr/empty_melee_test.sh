#!/bin/bash
# empty_melee_test.sh <agent> -- the trigger closed on an empty melee weapon is silent (weapons.qc W_AttackImpl), headless:
#   1. the Super Axe with no cells (impulse 168): no dry click (the author's note vrfiringrange_2026-10-08_14-14-01: as
#      Mjolnir, held by the whole hand the trigger closes round its haft);
#   2. the control: the empty shotgun still clicks.
# vr_debug_shots 1 prints each "empty click". Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait60;developer 1;vr_debug_shots 1;vr_weapon_grip_mode 1;impulse 9;wait2;impulse 168;wait5;give c 0;wait2;echo AXE;+attack;wait40;-attack;wait5;vr_reload_mode 0;impulse 154;wait3;give s 0;wait2;echo SHOTGUN;+attack;wait40;-attack;wait5;toggleconsole;quit" -Filter "empty click|^AXE|^SHOTGUN" 2>&1)
axe=$(echo "$log" | sed -n '/^AXE/,/^SHOTGUN/p' | grep -c "empty click")
gun=$(echo "$log" | sed -n '/^SHOTGUN/,$p' | grep -c "empty click: hand 1, weapon 4")
check $([ "$axe" = 0 ] && echo 1 || echo 0) "the Super Axe with no cells, the trigger held: silent ($axe clicks)"
check $([ "$gun" -ge 1 ] && echo 1 || echo 0) "the empty shotgun still clicks ($gun)"
exit $fail
