#!/bin/bash
# spentshake_test.sh <agent> -- a spent enemy rifle shakes and buzzes in the hand while it crackles (the author's note
# map1_2026-10-08_13-56-46; vr_shock.cpp gunShake, vr_view.cpp setupWeapon), headless: the enforcer's laser rifle in
# the off hand (impulse 186; impulse 214 leaves it one shot), its last shot fired:
#   1. vr_shock_info while it crackles: its shake (0..1, fading) and the buzzes sent to the hand; its drawn muzzle moves
#      against the hand from frame to frame (vr_dumpview, three frames), still with Spent Shake 0;
#   2. after the crackle (2.5 s): no shake; Spent Haptics 0: no buzz.
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
EG="map e1m1;wait60;developer 1;vr_debug_shots 1;vr_weapon_grip_mode 1;impulse 186;wait3;vr_mock_hand off -0.15 1.25 -0.40 50 0 0;vr_mock_hand main 0.25 1.1 -0.3 50 0 0;wait10;impulse 214;wait3"
FIRE="+offhandattack;wait3;-offhandattack;wait30"
DUMPS="vr_dumpview;wait1;vr_dumpview;wait1;vr_dumpview"
F="^bodyshock: (gun|active)|^off weapon muzzle"
# The muzzle's distances from the hand in the three dumps, how many different.
apart() { echo "$1" | grep "^off weapon muzzle" | sed -E 's/.*\), ([0-9.]+) units from the hand.*/\1/' | sort -u | wc -l; }
log=$(bash $KIT/run.sh $AGENT -Script "$EG;$FIRE;vr_shock_info;wait2;$DUMPS;wait300;vr_shock_info;toggleconsole;quit" -Filter "$F" 2>&1)
echo "$log" > "${OUT:-/dev/null}"
first=$(echo "$log" | grep -m1 "^bodyshock: gun in hand 0")
sh=$(echo "$first" | sed -E 's/.*shake=([0-9.]+).*/\1/'); bz=$(echo "$first" | sed -E 's/.*buzzes=([0-9]+).*/\1/')
check $(awk -v s="$sh" -v b="$bz" 'BEGIN { print (s > 0.5 && s <= 1 && b >= 3) ? 1 : 0 }') "crackling in the off hand: it shakes ($sh) and buzzes ($bz pulses)"
moving=$(apart "$log")
check $([ "$moving" -ge 2 ] && echo 1 || echo 0) "the drawn gun shakes: its muzzle $moving different distances from the hand in 3 frames"
check $(echo "$log" | grep "^bodyshock: active" | tail -1 | grep -q "active=0" && echo 1 || echo 0) "the crackle over: no shake"
log=$(bash $KIT/run.sh $AGENT -Script "vr_enemygun_spent_haptics 0;vr_enemygun_spent_shake 0;$EG;$FIRE;vr_shock_info;wait2;$DUMPS;toggleconsole;quit" -Filter "$F" 2>&1)
still=$(apart "$log")
check $(echo "$log" | grep -q "buzzes=0" && [ "$still" = 1 ] && echo 1 || echo 0) "Spent Haptics 0, Spent Shake 0: no buzz ($(echo "$log" | grep -o 'buzzes=[0-9]*')), the gun still ($still distance)"
exit $fail
