#!/bin/bash
# reload_test.sh <agent> -- immersive reloading, phases 1 and 2 (docs/vr-port/RELOAD_PLAN.md; QC vr_reload.qc), headless:
#   1. the QC self-test (vr_reload_test 9; impulse 125): takes, loads until full, the full gun refusing, put back,
#      dropped (lying about, force grabbable), taken again, the taped pair (two in, one in with one place left), the empty
#      pouch, nothing for a gun that doesn't load by hand, the magazines (out, taken, seated, part-used refunded, a short
#      reserve, a slow meeting doing nothing and a bump, the super nailgun's and the thunderbolt's) and the modes (the
#      holsters' reload in 2, none for the shotgun or the nailgun in 3, the super shotgun's as before in 3, no magazines in
#      0): "reload: N passed, 0 failed".
#   2. by the mock hands: the off hand holding the shotgun, the main hand to the ammo pouch (vr_mock_hand_to main
#      ammopouch), the grip (a shell taken: the reserve one less), the shell 6 units under the gun's loading port (not in:
#      Port Leniency 4), then at it (in: the magazine one more), a second shell taken and let go of away from the pouch
#      (dropped: lying about), the fist down onto it (taken again), let go of at the pouch (refunded).
#   3. Hip Holsters (vr_reload_mode 2): no ammo pouch (the hand at its place takes nothing), the hip holster reloads the
#      shotgun as before.
#   4. Magazines by the mock hands, the nailgun in the off hand: B/Y drops its magazine; a magazine from the pouch seated
#      at the well; a few nails fired; the main hand gripping the magazine and pulling it gently (it stays in), then
#      snapping it off (out into the hand, its count kept); put back in the pouch (the part-used count refunded); another
#      seated; a third brought up to the full gun slowly (nothing) and then fast (the bump: the old one out, the new in).
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
check $(echo "$holds" | sed -n 2p | grep -qE "clip 0 mag [01] holds nothing \| main hand weapon 0 clip 0 holds a round of 1" && echo 1 || echo 0) "6 units under the port: not loaded, still held"
check $(echo "$holds" | sed -n 3p | grep -qE "clip 1 mag [01] holds nothing \| main hand weapon 0 clip 0 holds nothing" && echo 1 || echo 0) "at the port: loaded (magazine 1), the hand empty"
check $(echo "$reports" | sed -n 4p | grep -qE "shells 36 nails [0-9]+ cells [0-9]+, 1 lying about" && echo 1 || echo 0) "a second shell dropped: 36 in the reserve, 1 lying about"
check $(echo "$holds" | sed -n 5p | grep -q "holds a round of 1" && echo 1 || echo 0) "the lying shell taken again by the fist"
check $(echo "$reports" | sed -n 6p | grep -qE "shells 37 nails [0-9]+ cells [0-9]+, 0 lying about" && echo 1 || echo 0) "let go of at the pouch: refunded (37)"

log=$(bash $KIT/run.sh $AGENT -Script "vr_reload_mode 2;$PRE;$POUCH;vr_dumpview;+grabmain;vr_mock_button main grip 1;wait10;vr_reload_test 0;impulse 125;wait3;vr_mock_button main grip 0;-grabmain;vr_mock_hand off 0.20 0.95 0.0 0 0 0;wait20;vr_reload_test 0;impulse 125;wait3;toggleconsole;quit" -Filter "^reload:|ammo pouch at|vr_mock_hand_to: no ammo" 2>&1)
check $(echo "$log" | grep -q "no ammo pouch" && ! echo "$log" | grep -q "^ammo pouch at" && echo 1 || echo 0) "hip holsters: no ammo pouch"
check $(echo "$log" | grep "^reload: off hand" | sed -n 2p | grep -qE "clip 0 mag [01] holds nothing \| main hand weapon 0 clip 0 holds nothing" && echo 1 || echo 0) "hip holsters: nothing taken"
check $(echo "$log" | grep "^reload: off hand" | sed -n 3p | grep -q "weapon 4 clip 8" && echo 1 || echo 0) "hip holsters: the hip holster reloads the shotgun"

# 4. Magazines (the nailgun in the off hand, 100 nails).
SLOW=$(for i in $(seq 30); do printf "vr_mock_hand_to main by 0 0 0.25;wait2;"; done)
GENTLE=$(for i in $(seq 12); do printf "vr_mock_hand_to main by 0 0 -0.3;wait2;"; done)
REP="vr_reload_test 0;impulse 125;wait3"
GRIP="+grabmain;vr_mock_button main grip 1;wait10"
LETGO="vr_mock_button main grip 0;-grabmain;wait5"
WELL="vr_mock_hand_to main lport;wait5;vr_mock_hand_to main lport;wait10"
MPRE="map e1m1;wait60;developer 1;vr_reload_debug 1;vr_weapon_grip_mode 1;impulse 9;wait2;impulse 156;wait3;vr_test_weaponinst 7;impulse 120;wait3;give n 100;vr_mock_hand off -0.15 1.25 -0.40 50 0 0;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait10"
AWAY="vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait5"
log=$(bash $KIT/run.sh $AGENT -Script "$MPRE;$REP;vr_mock_button off secondary 1;wait3;vr_mock_button off secondary 0;wait10;$REP;$POUCH;$GRIP;$REP;$WELL;$REP;$LETGO;+offhandattack;wait15;-offhandattack;wait10;$WELL;$GRIP;$GENTLE wait5;$REP;vr_mock_hand_to main lport;wait3;vr_mock_hand main 0.45 0.85 -0.1 0 0 70;wait1;vr_mock_hand main 0.5 0.8 -0.05 0 0 90;wait10;$REP;$POUCH;$LETGO;$REP;$AWAY;$POUCH;$GRIP;$WELL;$REP;$LETGO;$AWAY;$POUCH;$GRIP;vr_mock_hand_to main lport 8;wait5;vr_mock_hand_to main lport 8;wait10;$SLOW wait5;$REP;vr_mock_hand_to main lport 10;wait10;vr_mock_hand_to main lport 10;wait10;vr_mock_hand_to main lport;wait10;$REP;toggleconsole;quit" -Filter "^reload:" 2>&1 | grep -v "held round\|pulling the\|full well")
echo "$log" > "${OUT2:-/dev/null}"
holds=$(echo "$log" | grep "^reload: off hand")
h() { echo "$holds" | sed -n "$1p"; }
check $(echo "$log" | grep -q "a magazine of 24 out of the gun (hand 0, the button)" && h 2 | grep -q "weapon 6 clip 0 mag 0" && echo 1 || echo 0) "B/Y: the magazine (24) drops, the gun empty with none in"
check $(echo "$log" | grep -q "a magazine of 24 taken from the pouch by hand 1: 76 left" && h 3 | grep -q "holds a round of 24" && echo 1 || echo 0) "pouch: a magazine of 24 taken, 76 nails left"
check $(echo "$log" | grep -q "a magazine of 24 seated (hand 0): the gun holds 24" && h 4 | grep -q "clip 24 mag 1 holds nothing | main hand weapon 0 clip 0 holds nothing" && echo 1 || echo 0) "at the well: seated (24), the hand empty"
fired=$(h 5 | sed 's/.*weapon 6 clip \([0-9]*\) .*/\1/')
check $(h 5 | grep -q "mag 1 holds nothing | main hand weapon 0 clip 0 holds nothing" && [ "$fired" -lt 24 ] && echo "$log" | grep -q "holds the magazine of the gun" && echo 1 || echo 0) "a gentle pull: held, it stays in (the gun holds $fired)"
check $(echo "$log" | grep -q "a magazine of $fired out of the gun (hand 0, pulled off" && h 6 | grep -q "mag 0 holds nothing | main hand weapon 0 clip 0 holds a round of $fired" && echo 1 || echo 0) "a hard pull with a wrist snap: out into the hand, its $fired kept"
check $(echo "$log" | grep -q "a magazine of $fired back in the pouch (hand 1): $((76 + fired)) left" && echo 1 || echo 0) "the part-used magazine put back: $((76 + fired)) nails"
check $(h 8 | grep -q "clip 24 mag 1 holds nothing | main hand weapon 0 clip 0 holds nothing" && echo 1 || echo 0) "another seated in the empty gun"
check $(h 9 | grep -q "clip 24 mag 1 holds nothing | main hand weapon 0 clip 0 holds a round of 24" && [ $(echo "$log" | grep -c "knocked out by a bump") = 1 ] && echo 1 || echo 0) "a slow meeting with the full gun: nothing happens"
check $(echo "$log" | grep -q "knocked out by a bump" && h 10 | grep -q "clip 24 mag 1 holds nothing | main hand weapon 0 clip 0 holds nothing" && echo 1 || echo 0) "a bump: the old one knocked out, the new one seated"
exit $fail
