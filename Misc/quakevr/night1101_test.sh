#!/bin/bash
# night1101_test.sh <agent> [part]: the 2026-11-01 night round's gameplay changes (ROUND23.md, "Night 11-01: live foes,
# drag slowdown, the death knight's Cast, armed humanoids in AD"). Parts (all by default):
#   cq     the close-quarters slowdown's test (closequarters_test.sh's) then "live": the grunt knocked down (a ragdoll)
#          is no live foe with Only Live Foes on (vr_closequarters_live 1), is one with it off. Ends on
#          "vr_closequarters_test: N passed, M failed".
#   drag   a grunt held standing (foegrab) and a dead grunt's ragdoll held: one body of a grunt's mass, the walk cut by
#          Drag Slowdown (0.25); nothing held: none. Ends on "vr_carry_drag_test: N passed, M failed".
#   dk     test 95 with the death knight: his sword swing, then his volley cast from his off hand's palm with the sword
#          kept, the gun holder's spell share (0.6) over 2000 rolls. Ends on "agtest: test 95 over".
#   ad     e1m1 in the Arcane Dimensions campaign: it loads, its grunts replaced (vr_agrunt_repl_ad 1), a plain grunt and
#          death knight spawned armed, AD's point knight and rocket soldier not (vr_ad_test 198). Ends on "adrepl: N
#          passed, M failed".
KIT=C:/OHWorkspace/qvr-kit
AGENT=${1:-posepromo}
PART=${2:-all}
if [ "$PART" = all ] || [ "$PART" = cq ]; then
S="sv_random_seed 7;vr_ragdoll 1;map vrtesthall;wait30;god;notarget;vr_test_spawn 0;vr_test_spawn_dist 120;impulse 241;wait40"
S="$S;vr_closequarters_test start;+forward;wait250;-forward;wait2;vr_closequarters_test approach"
S="$S;+moveright;wait12;-moveright;vr_closequarters_test live;vr_closequarters_test end;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "vr_closequarters_test" -QuitOn "vr_closequarters_test: \d+ passed"
fi
if [ "$PART" = all ] || [ "$PART" = drag ]; then
S="sv_random_seed 7;vr_ragdoll 1;map vrtesthall;wait30;god;notarget;vr_test_spawn 0;vr_test_spawn_dist 36;impulse 241;wait40"
S="$S;vr_carry_drag_test start;vr_carry_drag_test none empty"
S="$S;vr_mock_hand_to main nearest monster_army 8;wait3;+grabmain;wait20;vr_carry_drag_test held standing;-grabmain;wait10"
S="$S;vr_carry_drag_test none released;vr_test_spawn_dist 28;vr_test_spawn_dead 1;impulse 241;wait3;vr_test_spawn_dead 0;wait60"
S="$S;vr_mock_hand_to main ragdoll near 1;wait2;vr_mock_hand_to main ragdoll near 1;+grabmain;vr_mock_button main grip 1;wait10"
S="$S;vr_carry_drag_test held ragdoll;-grabmain;vr_mock_button main grip 0;wait5;vr_carry_drag_test end;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "vr_carry_drag_test" -QuitOn "vr_carry_drag_test: \d+ passed"
fi
if [ "$PART" = all ] || [ "$PART" = dk ]; then
S="developer 1;vr_agrunt_test_kind 3;map vrtesthall;wait30;vr_agrunt_test 95"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "agtest: (PASS|FAIL|test 95)|agp: cast" -QuitOn "agtest: test 95 over" -Timeout 240
fi
if [ "$PART" = all ] || [ "$PART" = ad ]; then
S="developer 1;sv_random_seed 7;vr_campaign_native ad;wait30;map e1m1;wait60;god;notarget;vr_ad_test 198;wait2;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Games '-game ad -game quakevr' -ExtraArgs '-nomapindex -noaddons' -Script "$S" -Filter "adrepl|Host_Error|SV_Error|ENGINE ERROR|CRASH" -QuitOn "adrepl: \d+ passed" -Timeout 240
fi
