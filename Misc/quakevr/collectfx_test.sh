#!/bin/bash
# collectfx_test.sh <agent> -- the put-away transition (vr_collect_fx; ROUND21.md, "Put-away transition"), headless, by
# the mock hands (vr_test_spawn_hold puts a shells box into the main hand; vr_mock_hand_to main holster 3 / ammopouch):
#   1. a shells box let go of at the right hip holster: taken once ("You got the shells", the reserve 20 more), and its
#      client draws it going in (vr_debug_collect_fx 1: "collect fx: maps/b_shell0.bsp (... drawn pose) ... into hotspot 6",
#      then "gone in").
#   2. the same at the ammo pouch (Weapon Mode Immersive: vr_weapon_grip_mode 1): hotspot 12.
#   3. the silver key (vr_test_spawn 113) taken from the floor and let go of at the hip holster: "You got the silver
#      keycard" once, drawn going in (progs/b_s_key.mdl on e1m1).
#   4. vr_collect_fx 0: taken as before, nothing drawn.
#   5. a save and a load, and a map change, while it goes in: no error, the box counted once.
# Prints PASS/FAIL per check; exits 1 on a failure. Pictures: scratch/strip.sh-style runs with vr_mock_camera 0.9 1.4 -0.9 25 150.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
FILTER="collect fx|You got|reload: mode|rror|ERROR"
REP="vr_reload_test 0;impulse 125;wait3"
HOLD="map e1m1;wait60;developer 1;vr_reload_debug 1;vr_debug_collect_fx 1;vr_test_spawn_dist 48;vr_test_spawn_hold 1;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait5;+grabmain;vr_mock_button main grip 1;wait2;vr_test_spawn 101;impulse 241;wait10"
HIP="vr_mock_hand_to main holster 3;wait3;vr_mock_hand_to main holster 3;wait3"
POUCH="vr_mock_hand_to main ammopouch;wait3;vr_mock_hand_to main ammopouch;wait3"
REL="vr_mock_button main grip 0;-grabmain"

log=$(bash $KIT/run.sh $AGENT -Script "$HOLD;$HIP;$REP;$REL;wait60;$REP;toggleconsole;quit" -Filter "$FILTER" 2>&1)
shells=$(echo "$log" | grep "^reload: mode" | sed -E 's/.*shells ([0-9]+).*/\1/' | tr '\n' ' ')
check $(echo "$log" | grep -c "You got the shells" | grep -qx 1 && echo 1 || echo 0) "hip: the box taken once"
check $(set -- $shells; [ $(( $2 - $1 )) = 20 ] && echo 1 || echo 0) "hip: the reserve 20 more ($shells)"
check $(echo "$log" | grep -q "collect fx: maps/b_shell0.bsp (entity [0-9]*, drawn pose) by hand 1 into hotspot 6" && echo "$log" | grep -q "b_shell0.bsp gone in" && echo 1 || echo 0) "hip: drawn going into the right hip holster, then gone"
check $(echo "$log" | grep -q "rror" && echo 0 || echo 1) "hip: no error"

log=$(bash $KIT/run.sh $AGENT -Script "vr_weapon_grip_mode 1;$HOLD;$POUCH;$REL;wait60;toggleconsole;quit" -Filter "$FILTER" 2>&1)
check $(echo "$log" | grep -c "You got the shells" | grep -qx 1 && echo "$log" | grep -q "drawn pose) by hand 1 into hotspot 12" && echo 1 || echo 0) "pouch: taken once, drawn going into the ammo pouch"

KEY="map e1m1;wait60;developer 1;vr_debug_collect_fx 1;vr_test_spawn_dist 40;vr_test_spawn 113;impulse 241;wait90;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait5;vr_mock_hand_to main nearest item_key1 7;wait1;+grabmain;vr_mock_button main grip 1;wait1;vr_mock_hand_to main nearest item_key1 7;wait10"
log=$(bash $KIT/run.sh $AGENT -Script "$KEY;$HIP;$REL;wait60;toggleconsole;quit" -Filter "$FILTER" 2>&1)
check $(echo "$log" | grep -c "You got the silver" | grep -qx 1 && echo "$log" | grep -q "collect fx: progs/b_s_key.mdl (entity [0-9]*, drawn pose) by hand 1 into hotspot 6" && echo 1 || echo 0) "key: taken once at the hip holster, drawn going in"

log=$(bash $KIT/run.sh $AGENT -Script "vr_collect_fx 0;$HOLD;$HIP;$REL;wait60;toggleconsole;quit" -Filter "$FILTER" 2>&1)
check $(echo "$log" | grep -q "You got the shells" && ! echo "$log" | grep -q "collect fx:" && echo 1 || echo 0) "off: taken, nothing drawn"

log=$(bash $KIT/run.sh $AGENT -Script "$HOLD;$HIP;$REP;$REL;wait3;save cfx1;wait2;load cfx1;wait60;$REP;toggleconsole;quit" -Filter "$FILTER" 2>&1)
shells=$(echo "$log" | grep "^reload: mode" | sed -E 's/.*shells ([0-9]+).*/\1/' | tr '\n' ' ')
check $(set -- $shells; [ $(( $2 - $1 )) = 20 ] && ! echo "$log" | grep -q "rror" && echo 1 || echo 0) "save and load while it goes in: counted once ($shells), no error"
log=$(bash $KIT/run.sh $AGENT -Script "$HOLD;$HIP;$REL;wait3;map e1m2;wait60;$REP;toggleconsole;quit" -Filter "$FILTER" 2>&1)
check $(echo "$log" | grep -q "^reload: mode" && ! echo "$log" | grep -q "rror" && echo 1 || echo 0) "a map change while it goes in: no error"
exit $fail
