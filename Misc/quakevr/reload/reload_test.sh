#!/bin/bash
# reload_test.sh <agent> -- immersive reloading, phase 1 (docs/vr-port/RELOAD_PLAN.md; QC vr_reload.qc), headless:
#   1. the QC self-test (vr_reload_test 9; impulse 125): takes, loads until full, the full gun refusing, put back,
#      dropped (lying about, force grabbable), taken again, the taped pair (two in, one in with one place left), the empty
#      pouch, nothing for a gun that doesn't load by hand, and the modes (the holsters' reload in 2, none for the shotgun
#      in 3, the nailgun's as before in 3, no magazines in 0): "reload: N passed, 0 failed".
#   2. by the mock hands: the off hand holding the shotgun, the main hand to the ammo pouch (vr_mock_hand_to main
#      ammopouch), the grip (a shell taken: the reserve one less), the shell 6 units under the gun's loading port (not in:
#      Port Leniency 4), then at it (in: the magazine one more), a second shell taken and let go of away from the pouch
#      (dropped: lying about), the fist down onto it (taken again), let go of at the pouch (refunded).
#   3. Hip Holsters (vr_reload_mode 2): no ammo pouch (the hand at its place takes nothing), the hip holster reloads the
#      shotgun as before.
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }

log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait60;vr_reload_test 9;impulse 125;wait5;toggleconsole;quit" -Filter "^reload:" 2>&1)
echo "$log" | grep "FAIL"
total=$(echo "$log" | grep -m1 "passed,")
echo "self-test: ${total#reload: }"
check $(echo "$total" | grep -q " 0 failed" && echo 1 || echo 0) "self-test"

# (The first report is the set-up's: the shotgun emptied into the reserve, 38 shells.)
PRE="map e1m1;wait60;developer 1;vr_reload_debug 1;vr_weapon_grip_mode 1;impulse 9;wait2;impulse 154;wait3;vr_test_weaponinst 7;impulse 120;wait3;give s 30;vr_reload_test 5;impulse 125;wait2;vr_mock_hand off -0.15 1.25 -0.40 50 0 0;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait10"
POUCH="vr_mock_hand_to main ammopouch;wait5;vr_mock_hand_to main ammopouch;wait5"
log=$(bash $KIT/run.sh $AGENT -Script "$PRE;$POUCH;+grabmain;vr_mock_button main grip 1;wait10;vr_mock_hand_to main lport 6;wait5;vr_mock_hand_to main lport 6;wait10;vr_reload_test 0;impulse 125;wait3;vr_mock_hand_to main lport;wait5;vr_mock_hand_to main lport;wait10;vr_reload_test 0;impulse 125;wait3;vr_mock_button main grip 0;-grabmain;wait5;$POUCH;+grabmain;vr_mock_button main grip 1;wait10;vr_mock_hand main 0.02 0.9 -0.15 0 0 0;wait5;vr_mock_hand main 0.05 0.8 -0.25 0 0 0;wait5;vr_mock_hand main 0.08 0.7 -0.3 0 0 0;wait90;vr_mock_button main grip 0;-grabmain;wait120;vr_reload_test 0;impulse 125;wait3;vr_mock_hand_to main nearest vr_ammo_shell 12;wait2;vr_mock_hand_to main nearest vr_ammo_shell 12;wait5;+grabmain;vr_mock_button main grip 1;wait1;vr_mock_hand_to main nearest vr_ammo_shell 1;wait2;vr_mock_hand_to main nearest vr_ammo_shell 1;wait30;vr_reload_test 0;impulse 125;wait3;vr_mock_hand main 0.08 0.7 -0.3 0 0 0;wait5;$POUCH;vr_mock_button main grip 0;-grabmain;wait5;vr_reload_test 0;impulse 125;wait3;toggleconsole;quit" -Filter "^reload:" 2>&1)
echo "$log" > "${OUT:-/dev/null}"
reports=$(echo "$log" | grep "^reload: mode")
holds=$(echo "$log" | grep "^reload: off hand")
check $(echo "$log" | grep -q "a shell taken from the pouch by hand 1: 37 left" && echo 1 || echo 0) "pouch: a shell taken, 37 left of 38"
check $(echo "$holds" | sed -n 2p | grep -q "clip 0 holds nothing | main hand weapon 0 clip 0 holds a round of 1" && echo 1 || echo 0) "6 units under the port: not loaded, still held"
check $(echo "$holds" | sed -n 3p | grep -q "clip 1 holds nothing | main hand weapon 0 clip 0 holds nothing" && echo 1 || echo 0) "at the port: loaded (magazine 1), the hand empty"
check $(echo "$reports" | sed -n 4p | grep -q "shells 36, 1 lying about" && echo 1 || echo 0) "a second shell dropped: 36 in the reserve, 1 lying about"
check $(echo "$holds" | sed -n 5p | grep -q "holds a round of 1" && echo 1 || echo 0) "the lying shell taken again by the fist"
check $(echo "$reports" | sed -n 6p | grep -q "shells 37, 0 lying about" && echo 1 || echo 0) "let go of at the pouch: refunded (37)"

log=$(bash $KIT/run.sh $AGENT -Script "vr_reload_mode 2;$PRE;$POUCH;vr_dumpview;+grabmain;vr_mock_button main grip 1;wait10;vr_reload_test 0;impulse 125;wait3;vr_mock_button main grip 0;-grabmain;vr_mock_hand off 0.20 0.95 0.0 0 0 0;wait20;vr_reload_test 0;impulse 125;wait3;toggleconsole;quit" -Filter "^reload:|ammo pouch at|vr_mock_hand_to: no ammo" 2>&1)
check $(echo "$log" | grep -q "no ammo pouch" && ! echo "$log" | grep -q "^ammo pouch at" && echo 1 || echo 0) "hip holsters: no ammo pouch"
check $(echo "$log" | grep "^reload: off hand" | sed -n 2p | grep -q "clip 0 holds nothing | main hand weapon 0 clip 0 holds nothing" && echo 1 || echo 0) "hip holsters: nothing taken"
check $(echo "$log" | grep "^reload: off hand" | sed -n 3p | grep -q "weapon 4 clip 8" && echo 1 || echo 0) "hip holsters: the hip holster reloads the shotgun"
exit $fail
