#!/bin/bash
# reload_test.sh <agent> -- immersive reloading, phases 1, 2 and 2b (docs/vr-port/RELOAD_PLAN.md; QC vr_reload.qc), headless:
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
#   6. The author's rounds 2 and 3: the full shotgun's "can't" click once per approach; a magazine seats by its top; on
#      the nailgun, super nailgun and thunderbolt the magazine is the two-handed grip (a gentle pull keeps it, a hard one
#      takes it out), a wrist snap and the hands moved apart take it out, a punch knocks it out; a closed fist moved onto
#      a grip takes no hold; the ammo button pressed from its front only.
#   8. The shells slide into the gun: loaded at once, drawn sliding in, carried by the gun; the pair into the chambers.
#   7. The super shotgun broken open (phase 2b): fired, its shells stay in; the flick breaks it open (spent ones out),
#      fired open it clicks, the pouch's pair at its breech loads both, the flick shuts it; shut, a pair is refused; both
#      hands on it, gentle moves keep it shut, the pry opens it, the lift shuts it; Close When Loaded; Break Open off.
#   5. The author's notes on phase 1: the pouch riding the legs as he walks, a load point moved by its offset, Collision
#      Leniency letting a held shell reach the port, an ammo box let go of at the pouch going in, the pouch's frame by
#      what it gives and how much (the self-test checks the pouch by ammo and its last kind).
#   8. The author's magazine notes, the gun in the main hand, grip mode Hold: the gun's own two-handed grip and its
#      magazine both hold it (each gun); the pull's four ways (gentle stays; hard, snap, apart out); Pull Reach 0 on the
#      magazine's box; a hit at its far end; a gun carried by the off hand, its magazine out and in again.
#   9. The ammo button: front, behind, side at cone 50, behind at 180; 20 approaches from the front (95% pressed) and 20
#      from behind (none).
#  11. The night notes of 10-07/08: the super nailgun's well flush, the super shotgun's firing animation's speed, a held
#      prop hitting its barrels and the nailgun's magazine, spent lava nail magazines smoking, spent magazines not
#      pouched, spent enemy guns' smoke and crackle, the pouch's shells.
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }

log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait60;developer 1;vr_reload_debug 1;vr_reload_test 9;impulse 125;wait5;toggleconsole;quit" -Filter "^reload:" 2>&1)
echo "$log" | grep "FAIL"
total=$(echo "$log" | grep -m1 "passed,")
echo "self-test: ${total#reload: }"
check $(echo "$total" | grep -q " 0 failed" && echo 1 || echo 0) "self-test"
check $(echo "$log" | grep -q "the gun is full now" && echo "$log" | grep -q "the gate won't open" && echo 1 || echo 0) "the shotgun's last shell: the full cue; a shell at the full gun: the 'can't' cue"

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
log=$(bash $KIT/run.sh $AGENT -Script "$MPRE;$REP;vr_mock_button off secondary 1;wait3;vr_mock_button off secondary 0;wait10;$REP;$POUCH;$GRIP;$REP;$WELL;$REP;$LETGO;+offhandattack;wait15;-offhandattack;wait10;$WELL;$GRIP;$GENTLE wait5;$REP;vr_mock_hand_to main lport;wait3;vr_mock_hand main 0.45 0.85 -0.1 0 0 70;wait1;vr_mock_hand main 0.5 0.8 -0.05 0 0 90;wait10;$REP;$POUCH;$LETGO;$REP;$AWAY;$POUCH;$GRIP;$WELL;$REP;$LETGO;$AWAY;$POUCH;$GRIP;vr_mock_hand_to main lport 8;wait5;vr_mock_hand_to main lport 8;wait10;$SLOW wait5;$REP;vr_mock_hand_to main lport 10;wait10;vr_mock_hand_to main lport 10;wait10;vr_mock_hand_to main lport;wait10;$REP;vr_mock_hand_to main lport 8;wait10;vr_mock_hand_to main lport;wait10;$REP;toggleconsole;quit" -Filter "^reload:" 2>&1 | grep -v "held round\|pulling the\|full well")
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
check $(h 9 | grep -q "clip 24 mag 1 holds nothing | main hand weapon 0 clip 0 holds a round of 24" && [ $(echo "$log" | grep -c "knocked out by a hit") = 1 ] && echo 1 || echo 0) "a slow meeting with the full gun: nothing happens"
check $(echo "$log" | grep -q "knocked out by a hit" && h 10 | grep -q "clip 0 mag 0 holds nothing | main hand weapon 0 clip 0 holds a round of 24" && echo 1 || echo 0) "a hit: the old one knocked out, the new one not seated by the same touch"
check $(h 11 | grep -q "clip 24 mag 1 holds nothing | main hand weapon 0 clip 0 holds nothing" && echo 1 || echo 0) "then away and back: the new one seated"

# 5. The author's phase 1 notes (ROUND21.md, "Immersive reloading: the author's first notes").
# The pouch rides the legs: walking, it is somewhere else with Follow Legs 1 than with 0.
p0=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait60;vr_ammo_pouch_leg_follow 0;+forward;wait25;vr_dumpview;-forward;wait3;toggleconsole;quit" -Filter "^ammo pouch at" 2>&1 | grep -m1 "ammo pouch at" | sed 's/.*at (\([^)]*\)).*/\1/')
p1=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait60;vr_ammo_pouch_leg_follow 1;+forward;wait25;vr_dumpview;-forward;wait3;toggleconsole;quit" -Filter "^ammo pouch at" 2>&1 | grep -m1 "ammo pouch at" | sed 's/.*at (\([^)]*\)).*/\1/')
moved=$(awk -v a="$p0" -v b="$p1" 'BEGIN { split(a, x, " "); split(b, y, " "); d = 0; for(i = 1; i <= 3; i++) d += (x[i] - y[i]) ^ 2; print (sqrt(d) > 0.1) ? 1 : 0 }')
check $moved "the pouch rides the legs walking (follow 0: $p0; 1: $p1)"
# A load point moved by its offset; Collision Leniency lets a held shell reach the port; an ammo box at the pouch goes in
# (with vr_carry_take 1, where a holster wouldn't take it); the pouch's frame by what it gives and how much.
SG="map e1m1;wait60;developer 1;vr_reload_debug 1;vr_weapon_grip_mode 1;impulse 9;wait2;impulse 154;wait3;vr_test_weaponinst 7;impulse 120;wait3;vr_mock_hand off -0.15 1.25 -0.40 50 0 0;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait10"
log=$(bash $KIT/run.sh $AGENT -Script "$SG;vr_mock_hand_to main lport;wait10;vr_reload_port_shot_z -3;wait10;vr_mock_hand_to main lport;wait5;vr_reload_port_shot_z 0;$POUCH;$GRIP;vr_reload_port_shot_radius 0.1;vr_reload_collide_leniency 0;vr_mock_hand_to main lport 1;wait5;vr_mock_hand_to main lport 1;wait10;echo PHASE_A;vr_debug_carry 1;wait3;vr_debug_carry 0;vr_reload_collide_leniency 12;wait10;echo PHASE_B;vr_debug_carry 1;wait3;vr_debug_carry 0;echo PHASE_C;vr_mock_hand_to main ammopouch;wait5;$LETGO;vr_carry_take 1;give n 0;vr_rigid_place item_spikes main 0 3 0;+grabright;vr_mock_button main grip 1;wait20;$POUCH;vr_mock_button main grip 0;-grabright;wait10;$REP;impulse 156;wait10;vr_dumpview;toggleconsole;quit" -Filter "^held:.*meet .*deep|PHASE|loading port|^reload: (an ammo|mode)|vrpouch_ammo.mdl" 2>&1)
z0=$(echo "$log" | grep -m1 "loading port" | awk '{print $NF}'); z1=$(echo "$log" | grep "loading port" | sed -n 2p | awk '{print $NF}')
check $(awk -v a="$z0" -v b="$z1" 'BEGIN { print (a - b > 0.5) ? 1 : 0 }') "the shotgun's port moved down by Load Points Z (z $z0 to $z1)"
pushedA=$(echo "$log" | sed -n '/PHASE_A/,/PHASE_B/p' | grep -c "^held:")
pushedB=$(echo "$log" | sed -n '/PHASE_B/,/PHASE_C/p' | grep -c "^held:")
check $([ "$pushedA" -gt 0 ] && [ "$pushedB" = 0 ] && echo 1 || echo 0) "Collision Leniency: a shell at the port kept off the gun with 0 ($pushedA frames), not with 12 ($pushedB)"
check $(echo "$log" | grep -q "an ammo box into the pouch" && echo "$log" | grep "^reload: mode" | tail -1 | grep -q "nails 50 " && echo 1 || echo 0) "an ammo box let go of at the pouch goes in (50 nails)"
check $(echo "$log" | grep "vrpouch_ammo.mdl" | tail -1 | grep -q "frame 8 " && echo 1 || echo 0) "the pouch shows 3 nailgun magazines for 50 nails, the third part-filled (frame 8)"

# 6. The author's rounds 2 and 3 (ROUND21.md, "Immersive reloading: rounds 2 and 3").
# The full shotgun's "can't" click: once as a shell comes within the port's range, again only after it left and came back.
log=$(bash $KIT/run.sh $AGENT -Script "$SG;$POUCH;$GRIP;vr_mock_hand_to main lport 8;wait5;vr_mock_hand_to main lport 8;wait10;vr_mock_hand_to main lport;wait5;vr_mock_hand_to main lport;wait40;vr_mock_hand_to main lport 8;wait10;vr_mock_hand_to main lport;wait10;toggleconsole;quit" -Filter "^reload:" 2>&1)
check $([ $(echo "$log" | grep -c "the gate won't open") = 2 ] && echo 1 || echo 0) "the full shotgun: one 'can't' click per approach (2 approaches: $(echo "$log" | grep -c "the gate won't open"))"
# A magazine seats by its top at the gun's point (its middle there: not seated), the empty nailgun (Well Radius 1: its
# middle is about 1.8 units from its top).
log=$(bash $KIT/run.sh $AGENT -Script "$MPRE;vr_reload_port_nail_radius 1;vr_mock_button off secondary 1;wait3;vr_mock_button off secondary 0;wait10;$POUCH;$GRIP;vr_mock_hand_to main lportmid;wait5;vr_mock_hand_to main lportmid;wait10;$REP;vr_mock_hand_to main lport 6;wait10;vr_mock_hand_to main lport;wait5;vr_mock_hand_to main lport;wait10;$REP;toggleconsole;quit" -Filter "^reload:" 2>&1)
holds=$(echo "$log" | grep "^reload: off hand")
check $(h 1 | grep -q "mag 0 holds nothing | main hand weapon 0 clip 0 holds a round of 24" && h 2 | grep -q "clip 24 mag 1" && echo 1 || echo 0) "the magazine's top is its point: its middle at the well doesn't seat, its top does"
# The magazine is the two-handed grip (nailgun, super nailgun, thunderbolt); a gentle pull keeps it in; a hard pull, a
# wrist snap, the hands moved apart each take it out; a punch knocks it out; a fist already closed takes no grip.
GP="vr_debug_2h_grip 1;vr_reload_bump_speed 100;vr_mock_hand_to main mag 0;wait3;vr_mock_hand_to main mag 0;wait10;$GRIP;vr_reload_bump_speed 2"
GENTLE8=$(for i in $(seq 8); do printf "vr_mock_hand_to main by 0 0 -0.3;wait2;"; done)
APART=$(for i in $(seq 40); do printf "vr_mock_hand_to main by 0.4 0 -0.2;wait2;"; done)
F2="^reload:|2h grip|hand: two-handed"
for gun in 156 157 161; do
    log=$(bash $KIT/run.sh $AGENT -Script "${MPRE/impulse 156/impulse $gun};give c 100;$GP;vr_dumpview;$GENTLE8 wait5;$REP;vr_mock_hand_to main mag -6 0;wait10;$REP;toggleconsole;quit" -Filter "$F2" 2>&1)
    check $(echo "$log" | grep -q "hand 1 holds the magazine of the gun in hand 0" && echo "$log" | grep -q "^main hand:.* helping 1" && echo "$log" | grep "^reload: off hand" | sed -n 1p | grep -q "mag 1 holds nothing" && echo "$log" | grep -q "pulled off at" && echo 1 || echo 0) "impulse $gun's gun: its magazine is the two-handed grip, a gentle pull keeps it in, a hard one takes it out"
done
log=$(bash $KIT/run.sh $AGENT -Script "$MPRE;$GP;wait5;vr_mock_hand_turn main 0 0 80;wait10;$REP;toggleconsole;quit" -Filter "$F2" 2>&1)
check $(echo "$log" | grep -q "snapped off at" && echo "$log" | grep "^reload: off hand" | tail -1 | grep -q "mag 0 holds nothing | main hand weapon 0 clip 0 holds a round of 24" && echo 1 || echo 0) "a wrist snap takes the magazine out into the hand"
log=$(bash $KIT/run.sh $AGENT -Script "$MPRE;$GP;wait5;$APART wait5;$REP;toggleconsole;quit" -Filter "$F2" 2>&1)
check $(echo "$log" | grep -q "units further apart)" && echo "$log" | grep "^reload: off hand" | tail -1 | grep -q "holds a round of 24" && echo 1 || echo 0) "the hands moved apart take the magazine out"
log=$(bash $KIT/run.sh $AGENT -Script "$MPRE;$GRIP;vr_mock_hand_to main lport 10;wait10;vr_mock_hand_to main lport;wait10;$REP;toggleconsole;quit" -Filter "$F2" 2>&1)
check $(echo "$log" | grep -q "knocked out by a hit" && echo "$log" | grep "^reload: off hand" | tail -1 | grep -q "clip 0 mag 0 holds nothing | main hand weapon 0 clip 0 holds nothing" && echo 1 || echo 0) "a punch knocks the magazine out"
log=$(bash $KIT/run.sh $AGENT -Script "${SG/vr_reload_debug 1/vr_debug_2h_grip 1};$GRIP;vr_mock_hand_to main heldspot 0;wait3;vr_mock_hand_to main heldspot 0;wait10;vr_dumpview;vr_mock_button main grip 0;-grabmain;wait10;$GRIP;vr_dumpview;toggleconsole;quit" -Filter "$F2" 2>&1)
check $(echo "$log" | grep -q "already closed: no hold" && echo "$log" | grep "^off hand:" | sed -n 1p | grep -q "two-handed 0.00" && echo "$log" | grep "^off hand:" | sed -n 2p | grep -q "two-handed 0.70" && echo 1 || echo 0) "a fist moved onto the shotgun's grip takes no hold; closing there does"
# 7. The super shotgun broken open (phase 2b and the author's notes on it; ROUND21.md): in the off hand, both barrels
# fired (their shells stay in), the flick (+flickreloadleft) breaks it open (every shell out), fired open it only clicks,
# a taped pair from the pouch at its breech loads both, the flick shuts it and it fires again; a pair at the shut gun is
# refused; both hands on it, gentle moves and turns keep it shut, the pry opens it, the barrels lifted (held) shut it, a
# jolt doesn't, the hand on the open barrels turns with them; a hit from above opens it, from below shuts it; B/Y opens
# it; the flick's speeds; Close When Loaded; each way's switch disables only its own (the matrix); the rear sight's ring
# on the barrels; the sights' colour shut, open, fired and on the shotgun's pump; the author's defaults; Break Open off.
SSG="map e1m1;wait60;developer 1;vr_reload_debug 1;vr_debug_2h_grip 1;vr_weapon_grip_mode 1;vr_reload_ssg_open_flick 1;vr_reload_ssg_flick_close_speed 650;impulse 9;wait2;impulse 155;wait3;vr_test_weaponinst 7;impulse 120;wait3;give s 30;vr_mock_hand off -0.15 1.25 -0.40 50 0 0;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait10"
REST="vr_mock_hand main 0.25 1.1 -0.3 0 0 0"
FIRE="+offhandattack;wait10;-offhandattack;wait70" # (its firing animation over: it opens only then)
FLICK="+flickreloadleft;wait3;-flickreloadleft;wait40"
BY="vr_mock_button off secondary 1;wait3;vr_mock_button off secondary 0;wait40"
AT="vr_mock_hand_to main lport 8;wait5;vr_mock_hand_to main lport 8;wait10;vr_mock_hand_to main lport;wait5;vr_mock_hand_to main lport;wait10"
TO2H="vr_mock_hand_to main heldspot 0;wait3;vr_mock_hand_to main heldspot 0;wait10"
GENTLE=$(for i in $(seq 10); do printf "vr_mock_hand_to main by 0 0 -0.4;wait3;"; done)
TURN=$(for i in $(seq 4); do printf "vr_mock_hand_turn off 0 5 0;wait3;"; done)
DOWN=$(for i in $(seq 6); do printf "vr_mock_hand_to main by 0 0 -4;wait1;"; done)
UP=$(for i in $(seq 9); do printf "vr_mock_hand_to main by 0 0 4;wait1;"; done)
HITDOWN="vr_mock_hand_to main held 0.85 30;wait5;vr_mock_hand_to main held 0.85 30;wait10;vr_mock_hand_to main held 0.85 0;wait10;$REST;wait40"
HITUP="vr_mock_hand_to main held 0.85 -40;wait5;vr_mock_hand_to main held 0.85 -40;wait10;vr_mock_hand_to main held 0.85 -15;wait10;$REST;wait40"
PRY="$TO2H;$GRIP;$DOWN wait30;$LETGO;$REST;wait40"
LIFT="$TO2H;$GRIP;$DOWN wait10;$UP wait40;$LETGO;$REST;wait40"
AUTO="$POUCH;$GRIP;$AT;wait20;$LETGO;$REST;wait40"
EASY="vr_reload_ssg_pry_angle 40;vr_reload_ssg_lift_angle 40" # (the mock hands' reach: the author's 60 needs a longer arm)
F7="^reload:|^ssg: (both|pried|the barrels|the hand on)|2h grip"
opens() { echo "$1" | grep "^reload: the super shotgun open" | sed 's/.*open \([01]\).*/\1/' | tr -d '\n'; }
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait30;vr_reload_ssg_pry_angle;vr_reload_ssg_pry_speed;vr_reload_ssg_open_angle;toggleconsole;quit" -Filter "vr_reload_ssg" 2>&1)
check $(echo "$log" | grep -q '"vr_reload_ssg_pry_angle" is "60"' && echo "$log" | grep -q '"vr_reload_ssg_pry_speed" is "250"' && echo "$log" | grep -q '"vr_reload_ssg_open_angle" is "45"' && echo 1 || echo 0) "the author's pry and open angle as defaults (60 deg, 250 deg/s, 45 deg)"
log=$(bash $KIT/run.sh $AGENT -Script "$SSG;$FIRE;$REP;$FLICK;$REP;$FIRE;$POUCH;$GRIP;$AT;$REP;$LETGO;$FLICK;$REP;$FIRE;$REP;toggleconsole;quit" -Filter "$F7" 2>&1)
st=$(echo "$log" | grep "^reload: the super shotgun open")
check $(echo "$st" | sed -n 1p | grep -q "open 0 spent 2" && echo "$log" | grep -q "broken open by a flick: 2 spent and 0 live thrown out" && echo "$st" | sed -n 2p | grep -q "open 1 spent 0" && echo 1 || echo 0) "both barrels fired: their shells stay in; the flick breaks it open, the two spent ones out"
check $(echo "$log" | grep -q "a dry click: the super shotgun (hand 0) is open" && echo 1 || echo 0) "fired open: a dry click"
check $(echo "$log" | grep -q "a taped pair taken from the pouch by hand 1: 28 left" && echo "$log" | grep -q "2 into the gun (hand 0): its magazine 2 of 2" && echo 1 || echo 0) "the pouch gives it a taped pair; at the open breech both go in"
holds=$(echo "$log" | grep "^reload: off hand")
check $(echo "$log" | grep -q "closed by a flick: 2 loaded" && h 4 | grep -q "weapon 5 clip 2 " && h 5 | grep -q "weapon 5 clip 0 " && echo 1 || echo 0) "the flick shuts it; it fires again (both barrels)"
log=$(bash $KIT/run.sh $AGENT -Script "$SSG;$FIRE;$POUCH;$GRIP;$AT;$REP;toggleconsole;quit" -Filter "$F7" 2>&1)
check $(echo "$log" | grep -q "the super shotgun is shut: break it open first" && echo "$log" | grep "^reload: off hand" | tail -1 | grep -q "clip 0 .*holds a round of 2" && echo 1 || echo 0) "shut, a pair at its breech is refused (the can't click)"
log=$(bash $KIT/run.sh $AGENT -Script "$SSG;$EASY;$FIRE;$TO2H;$GRIP;$GENTLE wait10;$TURN wait10;$REP;$DOWN wait10;$REP;wait20;$UP wait40;$REP;toggleconsole;quit" -Filter "$F7" 2>&1)
check $(echo "$log" | grep -q "ssg: both hands on it" && [ "$(opens "$log")" = 010 ] && echo "$log" | grep -q "broken open by the pry: 2 spent" && echo "$log" | grep -q "closed by the barrels lifted" && echo 1 || echo 0) "both hands on it: gentle moves and turns keep it shut, the pry opens it, the barrels lifted shut it ($(opens "$log"))"
JOLT="$TO2H;$GRIP;$DOWN wait10;vr_mock_hand_to main by 0 0 36;wait3;vr_mock_hand_to main by 0 0 -36;wait40"
log=$(bash $KIT/run.sh $AGENT -Script "$SSG;$EASY;$FIRE;$BY;$REP;$JOLT;$REP;$LETGO;$REST;wait20;$TO2H;$GRIP;vr_reload_debug 2;wait2;vr_reload_debug 1;toggleconsole;quit" -Filter "$F7" 2>&1)
check $(echo "$log" | grep -q "broken open by the button" && [ "$(opens "$log")" = 11 ] && echo 1 || echo 0) "B/Y breaks it open; a jolt up and down again (under Lift Hold) doesn't shut it ($(opens "$log"))"
check $(echo "$log" | grep "ssg: the hand on the open barrels turned" | head -1 | grep -q "turned 45.0 deg" && echo 1 || echo 0) "the hand on the open barrels turns down with them (45 deg)"
log=$(bash $KIT/run.sh $AGENT -Script "$SSG;$FIRE;$HITDOWN;$REP;$HITUP;$REP;toggleconsole;quit" -Filter "$F7" 2>&1)
check $(echo "$log" | grep -q "broken open by a hit from above" && echo "$log" | grep -q "closed by a hit from below" && [ "$(opens "$log")" = 10 ] && echo 1 || echo 0) "a hit from above on the barrels breaks it open, one from below shuts it ($(opens "$log"))"
T25=$(for i in $(seq 4); do printf "vr_mock_hand_turn off 25 0 0;wait1;"; done)
log=$(bash $KIT/run.sh $AGENT -Script "$SSG;vr_mock_turn_velocity 1;$FIRE;vr_reload_ssg_flick_open_speed 3000;$T25 wait30;$REP;vr_mock_hand off -0.15 1.25 -0.40 50 0 0;wait30;vr_reload_ssg_flick_open_speed 650;$T25 wait30;$REP;vr_mock_hand off -0.15 1.25 -0.40 50 0 0;wait30;vr_reload_ssg_flick_close_speed 3000;$T25 wait30;$REP;vr_mock_hand off -0.15 1.25 -0.40 50 0 0;wait30;vr_reload_ssg_flick_close_speed 650;$T25 wait30;$REP;toggleconsole;quit" -Filter "$F7" 2>&1)
check $(echo "$log" | grep -q "broken open by a flick" && echo "$log" | grep -q "closed by a flick" && [ "$(opens "$log")" = 0110 ] && echo 1 || echo 0) "the flick's speeds: a flick under Flick Open Speed doesn't open it, over it does; the same for Flick Close Speed ($(opens "$log"))"
log=$(bash $KIT/run.sh $AGENT -Script "$SSG;vr_reload_ssg_close_auto 1;$FIRE;$FLICK;$AUTO;$REP;$BY;$REP;toggleconsole;quit" -Filter "$F7" 2>&1)
check $(echo "$log" | grep -q "closed by itself (both loaded): 2 loaded" && echo "$log" | grep -q "broken open by the button: 0 spent and 2 live thrown out" && [ "$(opens "$log")" = 01 ] && echo 1 || echo 0) "Close When Loaded; opened again, both live shells are thrown out ($(opens "$log"))"
# Each way's switch disables only its own: with one off, it does nothing, the others still work.
declare -A OPEN_STEP=([vr_reload_ssg_open_flick]="$FLICK" [vr_reload_ssg_pry]="$PRY" [vr_reload_ssg_open_hit]="$HITDOWN" [vr_reload_ssg_open_button]="$BY")
declare -A CLOSE_STEP=([vr_reload_ssg_close_flick]="$FLICK" [vr_reload_ssg_close_pry]="$LIFT" [vr_reload_ssg_close_hit]="$HITUP" [vr_reload_ssg_close_auto]="$AUTO")
for off in "${!OPEN_STEP[@]}"; do
    s="$SSG;$EASY;vr_reload_ssg_close_auto 1;$off 0;$FIRE;${OPEN_STEP[$off]};$REP"
    for other in "${!OPEN_STEP[@]}"; do
        [ "$other" = "$off" ] || s="$s;${OPEN_STEP[$other]};$REP;$FLICK;$REP"
    done
    log=$(bash $KIT/run.sh $AGENT -Script "$s;toggleconsole;quit" -Filter "$F7" 2>&1)
    check $([ "$(opens "$log")" = 0101010 ] && echo 1 || echo 0) "$off 0: that way doesn't open it, the other three do ($(opens "$log"), want 0101010)"
done
for off in "${!CLOSE_STEP[@]}"; do
    s="$SSG;$EASY;vr_reload_ssg_close_auto 1;$off 0;$FIRE;$BY;$REP;${CLOSE_STEP[$off]};$REP"
    for other in "${!CLOSE_STEP[@]}"; do
        [ "$other" = "$off" ] || s="$s;${CLOSE_STEP[$other]};$REP;$BY;$REP"
    done
    log=$(bash $KIT/run.sh $AGENT -Script "$s;toggleconsole;quit" -Filter "$F7" 2>&1)
    check $([ "$(opens "$log")" = 11010101 ] && echo 1 || echo 0) "$off 0: that way doesn't shut it, the other three do ($(opens "$log"), want 11010101)"
done
PY=${PY:-py -3.13}
check $($PY Misc/quakevr/reload/ssg_checks.py ring | grep -q "ring barrels [1-9][0-9]* frame 0" && echo 1 || echo 0) "the rear sight's ring is on the barrels part (it swings open with them), none on the frame"
# The sights' colour: recoloured (the player's hue) on the super shotgun shut, fired, open and shut again, and on the
# shotgun's auto pump's parts (its fore-end held back), as on the gun itself.
SHOTS=C:/OHWorkspace/qvr-kit/bases/$AGENT/qbase/quakevr/screenshots
SEE="vr_weapon_screen 0;vr_shells 0;r_particles 0;vr_muzzle_flash 0;vr_mock_hand off -0.1 1.3 -0.5 0 0 0;vr_mock_hand main 0.45 0.9 -0.1 0 0 0;vr_shot_hide 5;vr_mock_camera 0.35 1.38 -0.62 8 90;wait20"
bash $KIT/run.sh $AGENT -Clean -Script "${SSG/vr_reload_debug 1/vr_reload_debug 0};$SEE;screenshot;$FIRE;screenshot;$FLICK;screenshot;$FLICK;screenshot;toggleconsole;quit" -Filter "^x" > /dev/null 2>&1
s1=$($PY Misc/quakevr/reload/ssg_checks.py sights $(ls -tr $SHOTS/*.png | tail -4))
check $(echo "$s1" | awk '{ok = 1; for(i = 2; i <= NF; i++) { split($i, a, "/"); if(a[1] < 50 || a[2] > a[1] / 2) ok = 0 } print ok}') "the super shotgun's sights keep their colour shut, fired, open, shut again ($s1: recoloured/red pixels)"
bash $KIT/run.sh $AGENT -Clean -Script "${SSG/impulse 155/impulse 154};$SEE;screenshot;vr_autopump_hold 0.4;wait20;screenshot;vr_autopump_hold -1;wait20;screenshot;toggleconsole;quit" -Filter "^x" > /dev/null 2>&1
s2=$($PY Misc/quakevr/reload/ssg_checks.py sights $(ls -tr $SHOTS/*.png | tail -3))
check $(echo "$s2" | awk '{ok = 1; for(i = 2; i <= NF; i++) { split($i, a, "/"); if(a[1] < 50 || a[2] > a[1] / 2) ok = 0 } print ok}') "the shotgun's sights keep their colour as it pumps (its parts drawn) ($s2)"
log=$(bash $KIT/run.sh $AGENT -Script "$SSG;vr_reload_ssg_break 0;$FIRE;$REP;$FLICK;$REP;toggleconsole;quit" -Filter "$F7" 2>&1)
holds=$(echo "$log" | grep "^reload: off hand")
check $(h 1 | grep -q "weapon 5 clip 0 " && h 2 | grep -q "weapon 5 clip 2 " && ! echo "$log" | grep -q "broken open" && echo 1 || echo 0) "Break Open off: the flick reloads it as before"

# 8. The author's magazine notes (ROUND21.md, "Immersive reloading: magazines, both grips, the pull"): the gun in the MAIN
#    hand, grip mode Hold (vr_weapon_grip_mode 0, the default: its grip held all the while), the off hand on it.
M0PRE="map e1m1;wait60;developer 1;vr_reload_debug 2;vr_debug_2h_grip 1;vr_weapon_grip_mode 0;give n 100;give c 100;vr_mock_hand main 0.15 1.2 -0.45 70 0 0;vr_mock_hand off -0.1 1.0 -0.3;wait10;+grabright;vr_mock_button main grip 1;impulse 156;wait30"
OGRIP="+grableft;vr_mock_button off grip 1;wait15"
OLETGO="-grableft;vr_mock_button off grip 0;wait15"
# (The mock's hand arrives in one frame, far faster than a hand: no knock as it comes.)
OTO() { echo "vr_reload_bump_speed 100;vr_mock_hand_to off $1;wait3;vr_mock_hand_to off $1;wait10"; }
MTO() { echo "vr_reload_bump_speed 100;vr_mock_hand_to main $1;wait3;vr_mock_hand_to main $1;wait10"; }
F7="^reload:|2h grip|hand: two-handed|^weapon:"
# Both grips (the author: the front grip was lost to the magazine): the gun's own two-handed grip holds it two-handed and
# leaves the magazine alone; the magazine, gripped, holds it two-handed too and holds the magazine.
for gun in 156 157 161; do
    log=$(bash $KIT/run.sh $AGENT -Script "${M0PRE/impulse 156/impulse $gun};$(OTO "heldspot 0");$OGRIP;reset vr_reload_bump_speed;vr_dumpview;$OLETGO;$(OTO "mag 0");$OGRIP;reset vr_reload_bump_speed;vr_dumpview;$REP;toggleconsole;quit" -Filter "$F7" 2>&1)
    d1=$(echo "$log" | grep "^off hand:" | sed -n 1p); d2=$(echo "$log" | grep "^off hand:" | sed -n 2p)
    held=$(echo "$log" | grep -c "hand 0 holds the magazine of the gun in hand 1")
    check $(echo "$d1" | grep -q "helping 1" && echo "$d2" | grep -q "helping 1" && [ "$held" = 1 ] && echo "$log" | grep "^reload: off hand" | tail -1 | grep -q "mag 1 holds nothing" && echo 1 || echo 0) "impulse $gun's gun: its front grip holds it two-handed (the magazine untouched), its magazine too (held: $held)"
done
# The pull's four ways, at the defaults: a gentle pull keeps it; a hard pull along its way out, a wrist snap, the hands
# moved apart each take it out into the off hand (24 nails in it).
GENTLE=$(for i in $(seq 12); do printf "vr_mock_hand_to off by 0 0 -0.3;wait2;"; done)
APART=$(for i in $(seq 40); do printf "vr_mock_hand_to off by -0.4 0 -0.2;wait2;"; done)
for way in "gentle|$GENTLE|" "hard|vr_mock_hand_to off mag -6 0;wait10|pulled off at" "snap|vr_mock_hand_turn off 0 0 80;wait10|snapped off at" "apart|$APART|units further apart"; do
    name=${way%%|*}; rest=${way#*|}; moves=${rest%%|*}; why=${rest#*|}
    log=$(bash $KIT/run.sh $AGENT -Script "$M0PRE;$(OTO "mag 0");$OGRIP;reset vr_reload_bump_speed;wait5;$moves;wait5;$REP;toggleconsole;quit" -Filter "$F7" 2>&1)
    last=$(echo "$log" | grep "^reload: off hand" | tail -1)
    if [ -z "$why" ]; then
        check $(echo "$log" | grep -q "holds the magazine of the gun in hand 1" && ! echo "$log" | grep -q "out of the gun" && echo "$last" | grep -q "main hand weapon 6 clip 24 holds nothing, main mag 1" && echo 1 || echo 0) "the pull, $name: held, it stays in"
    else
        check $(echo "$log" | grep -q "$why" && echo "$last" | grep -q "holds a round of 24 | main hand weapon 6 clip 0 holds nothing, main mag 0" && echo 1 || echo 0) "the pull, $name: out into the off hand ($(echo "$log" | grep -o "out of the gun ([^)]*" | head -1))"
    fi
done
# The magazine's shape: Pull Reach 0 holds it only gripped on it (a unit off its side: not held; inside its box: held);
# a hit at its far end (0.3 units off it, 90% of the way down; Knock Out Speed 1: the mock hand is slow) knocks it out.
log=$(bash $KIT/run.sh $AGENT -Script "$M0PRE;vr_reload_pull_reach 0;$(OTO "mag 0 1");$OGRIP;$OLETGO;$(OTO "mag 0 -0.6");$OGRIP;$REP;toggleconsole;quit" -Filter "$F7" 2>&1)
check $([ $(echo "$log" | grep -c "holds the magazine of the gun") = 1 ] && echo "$log" | grep "^reload: off hand" | tail -1 | grep -q "main mag 1" && echo 1 || echo 0) "Pull Reach 0: off its side not held, on it held"
log=$(bash $KIT/run.sh $AGENT -Script "$M0PRE;vr_reload_hit_reach 1;$(OTO "mag -0.9 4");vr_reload_bump_speed 1;wait5;vr_mock_hand_to off mag -0.9 0.3;wait10;$REP;toggleconsole;quit" -Filter "$F7" 2>&1)
check $(echo "$log" | grep -q "knocked out by a hit at [0-9.]* m/s, 0\.[0-9] units from it" && echo "$log" | grep "^reload: off hand" | tail -1 | grep -q "main hand weapon 6 clip 0 holds nothing, main mag 0" && echo 1 || echo 0) "a hit at the magazine's far end knocks it out ($(echo "$log" | grep -o "knocked out by a hit[^)]*" | head -1))"
# However it is held: carried by the off hand (handed off: the main hand let go while the off hand held its front grip),
# the main hand pulls its magazine out, takes it away and seats it again.
log=$(bash $KIT/run.sh $AGENT -Script "$M0PRE;$(OTO "heldspot 0");$OGRIP;-grabright;vr_mock_button main grip 0;wait30;$REP;$(MTO "mag 0");+grabright;vr_mock_button main grip 1;wait15;reset vr_reload_bump_speed;vr_mock_hand_to main mag -6 0;wait10;$REP;$(MTO "lport 10");$(MTO "lport");reset vr_reload_bump_speed;wait10;$REP;toggleconsole;quit" -Filter "$F7" 2>&1)
holds=$(echo "$log" | grep "^reload: off hand")
check $(echo "$log" | grep -q "handed off to the off hand" && h 1 | grep -q "off hand weapon 6 clip 24 mag 1" && echo "$log" | grep -q "hand 1 holds the magazine of the gun in hand 0" && h 2 | grep -q "off hand weapon 6 clip 0 mag 0 holds nothing | main hand weapon 0 clip 0 holds a round of 24" && h 3 | grep -q "off hand weapon 6 clip 24 mag 1 holds nothing | main hand weapon 0 clip 0 holds nothing" && echo 1 || echo 0) "a gun carried by the off hand: its magazine pulled out by the main hand, then seated again"

# 9. The ammo button: from its front pressed, from behind never, at cone 80 (the default was; 50 now) and depth 0.5;
#    from the side refused at cone 50 depth 0, from behind pressed at 180. Then approaches as a finger makes them (a
#    straight line in to the button, a quarter unit a frame), 0-60 degrees off its face and 120-180 (behind), four ways round.
BTN() { echo "vr_mock_hand_to main wbutton $1 6;wait10;vr_mock_hand_to main wbutton $1 6;wait10;vr_mock_hand_to main wbutton $1 1;wait10;vr_mock_hand_to main wbutton $1 1;wait15"; }
log=$(bash $KIT/run.sh $AGENT -Script "$MPRE;vr_weapon_button_cone 80;$(BTN front);$(BTN back);vr_weapon_button_cone 50;vr_weapon_button_depth 0;$(BTN side);vr_weapon_button_cone 180;$(BTN back);toggleconsole;quit" -Filter "^weapon button" 2>&1 | grep "^weapon button")
check $(echo "$log" | sed -n 1p | grep -q "pressed, the fingertip came 0 deg" && [ $(echo "$log" | grep -c "^weapon button 0: pressed") = 2 ] && echo "$log" | grep -q "not pressed, the fingertip came 180 deg off its face (cone 80)" && echo "$log" | grep -q "not pressed, the fingertip came 90 deg off its face (cone 50)" && echo "$log" | tail -1 | grep -q "^weapon button 0: pressed, the fingertip came 180" && echo 1 || echo 0) "the ammo button: from its front pressed, from behind not; from the side not at cone 50; at 180 from behind too"
approach() { local s="vr_mock_hand_to main wbutton $1 8 $2;wait5;"; for u in $(seq 8 -0.25 0.25); do s+="vr_mock_hand_to main wbutton $1 $u $2;wait1;"; done; echo "$s vr_mock_hand_to main wbutton $1 9 $2;wait10;"; }
for set in "front|0 15 30 45 60" "behind|120 135 150 165 180"; do
    S=""; n=0
    for a in ${set#*|}; do for z in 0 90 180 270; do S+="$(approach $a $z)"; n=$((n + 1)); done; done
    p=$(bash $KIT/run.sh $AGENT -Script "$MPRE;vr_weapon_button_cone 80;$S;toggleconsole;quit" -Filter "^weapon button" -Timeout 300 2>&1 | grep -c "^weapon button 0: pressed")
    if [ "${set%%|*}" = front ]; then
        check $([ $((p * 100)) -ge $((n * 95)) ] && echo 1 || echo 0) "the ammo button from the front: $p of $n approaches pressed (95% or more)"
    else
        check $([ "$p" = 0 ] && echo 1 || echo 0) "the ammo button from behind: $p of $n approaches pressed (none)"
    fi
done
# 8. The shells slide into the gun (vr_reload_insert_time; vr_collectfx.cpp's "into the gun" variant): a shell let go of
# at the shotgun's load point is loaded at once (the count, the sound) and drawn sliding up its port into the tube, carried
# by the gun as it moves; the super shotgun's pair slides into its chambers.
INS="vr_debug_collect_fx 1;vr_mock_hand_to main lport 6;wait5;vr_mock_hand_to main lport 6;wait10;vr_mock_hand_to main lport;wait3"
MOVEGUN="vr_mock_hand off -0.05 1.35 -0.35 30 20 0;wait1;vr_mock_hand off 0.05 1.40 -0.30 10 40 0;wait20"
log=$(bash $KIT/run.sh $AGENT -Script "$PRE;$POUCH;$GRIP;$INS;$MOVEGUN;toggleconsole;quit" -Filter "^reload: [0-9]|collect fx" 2>&1)
first=$(echo "$log" | grep -n "^reload: 1 into the gun" | head -1 | cut -d: -f1)
fx=$(echo "$log" | grep -n "collect fx: progs/vr_shell_live.mdl .* into hotspot 240" | head -1 | cut -d: -f1)
check $([ -n "$first" ] && [ -n "$fx" ] && [ "$first" -lt "$fx" ] && echo 1 || echo 0) "a shell at the shotgun's load point: loaded at once (the count first), then drawn sliding in"
path=$(echo "$log" | grep "collect fx: in gun 0")
w0=$(echo "$path" | head -1 | sed 's/.*world //'); w1=$(echo "$path" | tail -1 | sed 's/.*world //')
l1=$(echo "$path" | tail -1 | sed 's/.*port: \([-0-9.]*\) .*/\1/')
moved=$(awk -v a="$w0" -v b="$w1" 'BEGIN { split(a, x, " "); split(b, y, " "); d = 0; for(i = 1; i <= 3; i++) d += (x[i] - y[i]) ^ 2; print sqrt(d) }')
check $(awk -v l="$l1" -v m="$moved" 'BEGIN { print (l > 15.5 && m > 3) ? 1 : 0 }') "it goes up the port and forward into the tube (x $l1 in the gun) while the gun moves ($moved units), carried by it"
log=$(bash $KIT/run.sh $AGENT -Script "${SSG/vr_reload_debug 1/vr_reload_debug 1;vr_debug_collect_fx 1};$FIRE;$BY;$POUCH;$GRIP;$AT;wait20;toggleconsole;quit" -Filter "^reload: [0-9]|collect fx" 2>&1)
check $(echo "$log" | grep -q "^reload: 2 into the gun" && echo "$log" | grep -q "collect fx: progs/vr_shell_pair.mdl .* into hotspot 240" && echo "$log" | grep "collect fx: in gun 0" | tail -1 | grep -q " t 0.9" && echo 1 || echo 0) "the super shotgun's pair slides into its chambers"
# 10. The author's notes of vrfiringrange_2026-10-07 (ROUND21.md, "Reloading: the firing range notes of 10-07").
# The super shotgun isn't broken open while it fires: B/Y pressed during its firing animation does nothing, after it opens it.
BYNOW="vr_mock_button off secondary 1;wait3;vr_mock_button off secondary 0;wait3"
log=$(bash $KIT/run.sh $AGENT -Script "$SSG;+offhandattack;wait2;-offhandattack;$BYNOW;$REP;wait80;$BY;$REP;toggleconsole;quit" -Filter "$F7" 2>&1)
check $(echo "$log" | grep -q "is still firing (frame [1-6]): not opened" && echo "$log" | grep -q "broken open by the button: 2 spent" && [ "$(opens "$log")" = 01 ] && echo 1 || echo 0) "B/Y during the super shotgun's firing animation: not opened; after it: opened ($(opens "$log"), want 01)"
# A super shotgun dropped broken open lies drawn open (its prop's U_QVR_SSGOPEN: drawn in its parts); dropped shut, whole.
DROP="vr_weapon_grip_mode 0;+graboff;vr_mock_button off grip 1;wait5;vr_mock_button off grip 0;-graboff;wait60"
log=$(bash $KIT/run.sh $AGENT -Script "$SSG;$BY;$DROP;toggleconsole;quit" -Filter "^ssg: a super|broken open" 2>&1)
log2=$(bash $KIT/run.sh $AGENT -Script "$SSG;$DROP;toggleconsole;quit" -Filter "^ssg: a super" 2>&1)
check $(echo "$log" | grep -q "^ssg: a super shotgun lying open (entity [0-9]*), 0 loaded, drawn in its parts" && ! echo "$log2" | grep -q "^ssg: a super shotgun lying open" && echo 1 || echo 0) "dropped open, the super shotgun lies drawn open (its parts); dropped shut, whole"
# The super nailgun's magazine on its left, its ammo button on its right (22-10-39): in the off hand (drawn mirrored) at
# x -0.15, both inward of it the magazine, outward the button.
log=$(bash $KIT/run.sh $AGENT -Script "${MPRE/impulse 156/impulse 157};vr_reload_bump_speed 100;vr_mock_hand_to main mag 0;wait5;vr_mock_hand_to main mag 0;wait5;vr_mock_hand_to main wbutton front;wait5;vr_mock_hand_to main wbutton front;wait5;toggleconsole;quit" -Filter "vr_mock_hand_to: main hand at" 2>&1)
mx=$(echo "$log" | grep "main hand at" | sed -n 2p | awk '{print $5}'); bx=$(echo "$log" | grep "main hand at" | sed -n 4p | awk '{print $5}')
check $(awk -v m="$mx" -v b="$bx" 'BEGIN { print (m != "" && b != "" && m > -0.1 && b < -0.2) ? 1 : 0 }') "the super nailgun in the off hand (x -0.15): its magazine inward (x $mx), its ammo button outward (x $bx)"
# The thunderbolt's cell sparks as it is taken out and seated (22-14-04); taken out spent, it smokes for
# vr_reload_battery_smoke_time (7 s; 22-14-33), then stops; a part-used one doesn't smoke.
SEAT="vr_mock_hand_to main lport 6;wait10;vr_mock_hand_to main lport;wait5;vr_mock_hand_to main lport;wait10"
log=$(bash $KIT/run.sh $AGENT -Script "${MPRE/impulse 156/impulse 161};give c 100;$BY;$POUCH;$GRIP;$SEAT;$REP;+offhandattack;wait400;-offhandattack;$REP;$BY;$REP;wait700;$REP;toggleconsole;quit" -Filter "^reload: (the cell|a spent|the spent|a magazine)" 2>&1)
check $(echo "$log" | grep -q "a magazine of 36 out of the gun" && [ "$(echo "$log" | grep -c "the cell's contact sparks (taken out): 14")" = 2 ] && echo "$log" | grep -q "the cell's contact sparks (seated): 14" && [ "$(echo "$log" | grep -c "a spent cell smoking")" = 1 ] && echo 1 || echo 0) "the thunderbolt's cell: sparks taken out (twice) and seated; only the spent one smokes"
st=$(echo "$log" | grep "the spent cell stopped smoking after" | sed 's/.*after \([0-9.]*\) s: \([0-9]*\) wisps.*/\1 \2/')
check $(echo "$st" | awk '{print ($1 >= 6.9 && $1 <= 7.3 && $2 >= 30) ? 1 : 0}') "the spent cell smokes for 7 s, then stops (after, wisps: ${st:-none})"
log=$(bash $KIT/run.sh $AGENT -Script "${MPRE/impulse 156/impulse 161};give c 100;vr_reload_battery_sparks 0;vr_reload_battery_smoke_time 2;+offhandattack;wait400;-offhandattack;$BY;wait300;$REP;toggleconsole;quit" -Filter "^reload: (the cell|a spent|the spent)" 2>&1)
st=$(echo "$log" | grep "the spent cell stopped smoking after" | sed 's/.*after \([0-9.]*\) s.*/\1/')
check $(! echo "$log" | grep -q "contact sparks" && awk -v t="$st" 'BEGIN { print (t != "" && t >= 1.9 && t <= 2.3) ? 1 : 0 }') "Contact Sparks 0: none; Spent Cell Smoke 2: it smokes for 2 s ($st)"
# 11. The author's night notes of 10-07/08 (ROUND21.md, "Reloading and spent guns: the night notes of 10-07/08").
# The super nailgun's well flush on the flat band of its face (23-58-30): its rim at most 0.2 units off it, inside its edges.
sw=$($PY Misc/quakevr/reload/ssg_checks.py snailwell)
check $(echo "$sw" | awk '{ok = NF >= 12; for(i = 1; i <= NF; i += 6) { if($(i + 3) > 0.2 || $(i + 5) > 0) ok = 0 } print ok ? 1 : 0}') "the super nailgun's well flush on its face ($sw)"
# The super shotgun's firing animation paced by vr_ssg_fire_anim_speed (23-54-46): B/Y pressed every other frame from the
# shot, it opens as the animation ends: about 0.6 s after the shot at 1 (id's), 0.43 s at 1.4 (the default), 0.3 s at 2.
POLL=$(for i in $(seq 60); do printf "vr_mock_button off secondary 1;wait1;vr_mock_button off secondary 0;wait1;"; done)
ta=""
for k in 1 1.4 2; do
    t=$(bash $KIT/run.sh $AGENT -Script "$SSG;vr_ssg_fire_anim_speed $k;+offhandattack;wait2;-offhandattack;$POLL;toggleconsole;quit" -Filter "broken open by" 2>&1 | grep -o "[0-9.]* s after its last shot" | head -1 | cut -d' ' -f1)
    ta="$ta ${t:-none}"
done
check $(echo "$ta" | awk '{print ($1 >= 0.55 && $1 <= 0.65 && $2 >= 0.39 && $2 <= 0.47 && $3 >= 0.27 && $3 <= 0.34) ? 1 : 0}') "Firing Animation Speed 1, 1.4, 2: it opens 0.6, 0.43, 0.3 s after the shot (s:$ta)"
# A held prop hits the super shotgun's barrels with its surface (00-05-36): a head in the main hand (impulse 252,
# vr_test_held_hand 1) brought down to 20 cm over the barrels breaks it open, up to 20 cm under them shuts it; the fist
# alone stopping as far off does neither (its middle out of reach).
PHIT() { echo "vr_mock_hand_to main held 0.85 $1;wait5;vr_mock_hand_to main held 0.85 $1;wait10;vr_mock_hand_to main held 0.85 $2;wait10;$REST;wait40"; }
PROP="$GRIP;vr_test_held_hand 1;vr_test_held_pick 4;impulse 252;wait5"
log=$(bash $KIT/run.sh $AGENT -Script "$SSG;$FIRE;$PROP;$(PHIT 45 20);$REP;$(PHIT -45 -20);$REP;toggleconsole;quit" -Filter "$F7|^test:" 2>&1)
log2=$(bash $KIT/run.sh $AGENT -Script "$SSG;$FIRE;$(PHIT 45 20);$REP;toggleconsole;quit" -Filter "$F7" 2>&1)
check $(echo "$log" | grep -q "^test: progs/h_guard.mdl in the main hand" && echo "$log" | grep -q "broken open by a hit from above" && echo "$log" | grep -q "closed by a hit from below" && [ "$(opens "$log")" = 10 ] && [ "$(opens "$log2")" = 0 ] && echo 1 || echo 0) "a prop held in the other hand hits the barrels open and shut by its surface (prop $(opens "$log"), want 10; the fist as far off: $(opens "$log2"), want 0)"
# The same for the nailgun's magazine (VR_Reload_HitFrame: the prop's box, not its middle): the head brought fast from 30
# units off the magazine's side to 6 off (the hand and the prop's middle 5.5 off it, past Hit Reach 4; its box 1.2 off)
# knocks it out; the fist stopping as far off doesn't.
MHIT="vr_mock_hand_to main mag 0 30;wait5;vr_mock_hand_to main mag 0 30;wait10;vr_mock_hand_to main mag 0 6;wait10"
log=$(bash $KIT/run.sh $AGENT -Script "$MPRE;$PROP;$MHIT;$REP;toggleconsole;quit" -Filter "^reload: (a mag|off hand)|^test:" 2>&1)
log2=$(bash $KIT/run.sh $AGENT -Script "$MPRE;$GRIP;$MHIT;$REP;toggleconsole;quit" -Filter "^reload: (a mag|off hand)" 2>&1)
check $(echo "$log" | grep -q "^test: progs/h_guard.mdl in the main hand" && echo "$log" | grep -q "knocked out by a hit" && echo "$log" | grep "^reload: off hand" | tail -1 | grep -q "clip 0 mag 0" && ! echo "$log2" | grep -q "knocked out by a hit" && echo "$log2" | grep "^reload: off hand" | tail -1 | grep -q "clip 24 mag 1" && echo 1 || echo 0) "a prop held in the other hand knocks the nailgun's magazine out by its surface; the fist as far off doesn't ($(echo "$log" | grep -o "knocked out by a hit[^)]*" | head -1))"
# Spent lava nail magazines smoke as the spent cells do (00-00-42): the super nailgun and the nailgun on lava nails, fired
# dry (vr_reload_test 16), their magazine out: smoking for Spent Cell Smoke (2 s here); a plain nail magazine emptied
# (vr_reload_test 5) doesn't.
for g in 157 156; do
    log=$(bash $KIT/run.sh $AGENT -Script "${MPRE/impulse 156/impulse $g};vr_reload_battery_smoke_time 2;vr_reload_test 16;impulse 125;wait3;$BY;wait300;toggleconsole;quit" -Filter "^reload: (a spent|the spent)" 2>&1)
    st=$(echo "$log" | grep "the spent lava nail magazine stopped smoking after" | sed 's/.*after \([0-9.]*\) s: \([0-9]*\) wisps.*/\1 \2/')
    check $(echo "$log" | grep -q "a spent lava nail magazine smoking for 2 s" && echo "$st" | awk '{print ($1 >= 1.9 && $1 <= 2.3 && $2 >= 8) ? 1 : 0}') "impulse $g's gun on lava nails, its spent magazine out: it smokes for 2 s (after, wisps: ${st:-none})"
done
log=$(bash $KIT/run.sh $AGENT -Script "${MPRE/impulse 156/impulse 157};vr_reload_test 5;impulse 125;wait3;$BY;wait60;toggleconsole;quit" -Filter "^reload: (a spent|a magazine of)" 2>&1)
check $(echo "$log" | grep -q "a magazine of 0 out of the gun" && ! echo "$log" | grep -q "a spent" && echo 1 || echo 0) "a plain nail magazine taken out empty doesn't smoke"
# A spent magazine doesn't go back into the pouch (00-01-10): the nailgun's emptied (vr_reload_test 5), pulled off into
# the hand and let go of at the pouch, it falls: the hand empty, the reserve as it was.
SNAP="$WELL;$GRIP;vr_mock_hand_to main lport;wait3;vr_mock_hand main 0.45 0.85 -0.1 0 0 70;wait1;vr_mock_hand main 0.5 0.8 -0.05 0 0 90;wait10"
log=$(bash $KIT/run.sh $AGENT -Script "$MPRE;vr_reload_bump_speed 100;vr_reload_test 5;impulse 125;wait3;$SNAP;$REP;$POUCH;$LETGO;wait30;$REP;toggleconsole;quit" -Filter "^reload: (a mag|off hand|mode)" 2>&1)
holds=$(echo "$log" | grep "^reload: off hand")
check $(h 2 | grep -q "holds a round of 0" && echo "$log" | grep -q "a magazine of 0 is spent: not back in the pouch, it falls (hand 1)" && h 3 | grep -q "main hand weapon 0 clip 0 holds nothing" && ! echo "$log" | grep -q "back in the pouch (hand" && [ "$(echo "$log" | grep "^reload: mode" | sed -n 2p | grep -o "nails [0-9]*")" = "$(echo "$log" | grep "^reload: mode" | sed -n 3p | grep -o "nails [0-9]*")" ] && echo 1 || echo 0) "a spent magazine let go of at the pouch falls: not put back"
# A spent enemy gun's cues (grenedin 00-07-22): the enforcer's rifle in the off hand, its last shot fired (impulse 214
# leaves one): it crackles over the gun in the hand (vr_shock_info: the gun's arcs drawn), dropped it crackles on as it
# lies (a body's lasting shock on it), and it smokes for vr_enemygun_spent_smoke (5 s); the grunt's burst rifle in the
# main hand the same; Spent Smoke 2 and Spent Crackle 1: 2 s.
DROPOFF="vr_weapon_grip_mode 0;+graboff;vr_mock_button off grip 1;wait5;vr_mock_button off grip 0;-graboff"
EG="map e1m1;wait60;developer 1;vr_debug_shots 1;vr_weapon_grip_mode 1"
HANDS="vr_mock_hand off -0.15 1.25 -0.40 50 0 0;vr_mock_hand main 0.25 1.1 -0.3 50 0 0;wait10"
FE="^spent gun|^bodyshock: (gun|entity)"
log=$(bash $KIT/run.sh $AGENT -Script "$EG;impulse 186;wait3;$HANDS;impulse 214;wait3;+offhandattack;wait3;-offhandattack;wait30;vr_shock_info;wait2;$DROPOFF;wait60;vr_shock_info;wait2;wait400;toggleconsole;quit" -Filter "$FE" 2>&1)
st=$(echo "$log" | grep -o "cues over after [0-9.]* s: [0-9]* wisps" | awk '{print $4, $6}')
check $(echo "$log" | grep -q "spent gun: enforcer's rifle (hand 0) empty: smoking 5 s, crackling 2.5 s" && echo "$log" | grep -q "bodyshock: gun in hand 0 arcs=[1-9]" && echo "$log" | grep -q "spent gun: lying about" && echo "$log" | grep -q "bodyshock: entity=[0-9]* kind=4 .* arcs=[1-9]" && echo "$st" | awk '{print ($1 >= 4.9 && $1 <= 5.3 && $2 >= 15) ? 1 : 0}') "the enforcer's rifle spent: it crackles in the hand and dropped, and smokes 5 s (after, wisps: ${st:-none})"
log=$(bash $KIT/run.sh $AGENT -Script "$EG;vr_enemygun_spent_smoke 2;vr_enemygun_spent_crackle 1;impulse 165;wait3;$HANDS;impulse 214;wait3;+attack;wait3;-attack;wait20;vr_shock_info;wait300;toggleconsole;quit" -Filter "$FE" 2>&1)
st=$(echo "$log" | grep -o "cues over after [0-9.]* s" | awk '{print $4}')
check $(echo "$log" | grep -q "spent gun: grunt's burst rifle (hand 1) empty: smoking 2 s, crackling 1 s" && echo "$log" | grep -q "bodyshock: gun in hand 1 arcs=[1-9]" && awk -v t="$st" 'BEGIN { print (t != "" && t >= 1.9 && t <= 2.3) ? 1 : 0 }') "the grunt's burst rifle spent in the main hand: crackling; Spent Smoke 2, Spent Crackle 1: over after 2 s ($st)"
# The ammo pouch's shells (the author's typed note: overly big, not symmetric): a held shell's width (0.66-0.8 units
# across the middle one's rim; they were 1.07) and the row its own mirror image (they leaned and stood out at random).
ps=$($PY Misc/quakevr/reload/ssg_checks.py pouchshells)
check $(echo "$ps" | awk '{print ($3 >= 0.6 && $3 <= 0.8 && $5 <= 0.02) ? 1 : 0}') "the pouch's shells a held shell's size, the row symmetric ($ps)"
exit $fail
