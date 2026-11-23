#!/bin/bash
# modladder_test.sh <agent> -- the mods' ladders (Quoth's and AD's trigger_ladder brushes) on our climbing: the ladder
# volume's own rungs (Quake/vr/vr_climb.cpp ladderHold, ladderRungStep), climbed by hand and by flatscreen play's ladder
# (docs/vr-port/ARCANE_DIMENSIONS.md 16.5, FLATPLAY.md "Ladders"), headless. Needs Quoth 2.2 in the test base
# (quoth_setup.py) and AD 1.80. Prints PASS/FAIL lines and a verdict, exits 1 on a failure.
#   Quoth's kellbase1 (free-standing non-solid ladders, their trigger_ladder 2 units thick): flat, on the drawn rungs as
#   the hands (vr_climb.cpp findRungs' drawnBelow): the catwalk's ladder (bars 24 apart) and a pillar's (32 apart, 256
#   up to 640) each up and over the top, down from its top (lowered onto it off the edge), down looking down
#   (vr_flatladder_test); vr_flat_ladder 0: Quoth's own jump climb lifts him. AD's ad_e1m1 (a wall ladder): the same three, and AD's own jump climb. VR hands (the
#   mock headset): vr_climb_try along kellbase1's ladders, exact holds on their drawn rungs (the func_illusionary's bars:
#   24 apart on the catwalk's, 32 on the pillars'), none past its top or with Hand-Climbed Ladders off.
AGENT=${1:-dagger}
QT_NAME=modladder
source "$(dirname "$0")/qvrtest.sh"
F=(-ExtraArgs "-flatscreenmode")
Q="vr_campaign_native quoth;wait30;map kellbase1;wait60;god;notarget"
AD=(-Games "-game ad -game quakevr" -ExtraArgs "-flatscreenmode -nomapindex -noaddons")
A="map ad_e1m1;wait60;god;notarget"

out=$(bash $KIT/run.sh $AGENT "${F[@]}" -QuitOn "^flatladder: [0-9]+ passed" -Timeout 360 -Filter "^flatladder" \
    -Script "$Q;vr_flatladder_test;wait40000;toggleconsole;quit" 2>&1)
echo "$out" | grep -a "^flatladder: \(PASS\|FAIL\)"
expect_verdict "$out" "flatladder" "Quoth's ladders (catwalk, pillar), flat on their drawn rungs: up and over, down from the top, down looking down"

out=$(bash $KIT/run.sh $AGENT "${F[@]}" -QuitOn "^flatladder: [0-9]+ passed" -Timeout 120 -Filter "^flatladder" \
    -Script "$Q;vr_flat_ladder 0;vr_flatladder_test 6;wait3000;toggleconsole;quit" 2>&1)
echo "$out" | grep -a "^flatladder: \(PASS\|FAIL\)"
expect_verdict "$out" "flatladder" "Quoth's ladder, vr_flat_ladder 0: its own jump climb"

out=$(bash $KIT/run.sh $AGENT "${AD[@]}" -QuitOn "^flatladder: [0-9]+ passed" -Timeout 240 -Filter "^flatladder" \
    -Script "$A;vr_flatladder_test;wait20000;toggleconsole;quit" 2>&1)
echo "$out" | grep -a "^flatladder: \(PASS\|FAIL\)"
expect_verdict "$out" "flatladder" "AD's ladder, flat: up and over, down from the top, down looking down"

out=$(bash $KIT/run.sh $AGENT "${AD[@]}" -QuitOn "^flatladder: [0-9]+ passed" -Timeout 120 -Filter "^flatladder" \
    -Script "$A;vr_flat_ladder 0;vr_flatladder_test 3;wait3000;toggleconsole;quit" 2>&1)
echo "$out" | grep -a "^flatladder: \(PASS\|FAIL\)"
expect_verdict "$out" "flatladder" "AD's ladder, vr_flat_ladder 0: its own jump climb"

# VR hands (no -flatscreenmode): along kellbase1's ladder (x 32, the trigger's face at y -657), the hand 4 units into
# it from the body's side; its drawn rungs (a func_illusionary's bars, their ridge on y -664, tops 704 + 24 k: his note
# kellbase1 2026-11-22 16:06): exact holds on the bar nearest the hand, 1 over its top; over its top none; Hand-Climbed
# Ladders off, none. The pillar ladders (x -64): their bars 32 apart (tops 508 + 32 k, their ridge on y -1354).
T=""
for z in 700 726 760 790 885; do T="$T;vr_climb_try 32 -655 $z main"; done
out=$(bash $KIT/run.sh $AGENT -Filter "^climbtry" -Timeout 120 \
    -Script "$Q;noclip;setpos 32 -664 664 0 90 0;wait5;vr_climb_leniency 0;vr_climb_leniency_air 0$T;vr_climb_try 32 -655 930 main;vr_climb_ladders 0;vr_climb_try 32 -655 726 main;vr_climb_ladders 1;setpos 17 -619 920 0 270 0;wait5;vr_climb_try 32 -656 878 main;setpos -64 -1300 480 0 270 0;wait5;vr_climb_try -64 -1336 500 main;vr_climb_try -64 -1336 530 main;toggleconsole;quit" 2>&1)
echo "$out" | grep -a "^climbtry" | cut -c1-120
n=0
for want in "700.00 main: exact hold 32.00 -664.00 705.00" "726.00 main: exact hold 32.00 -664.00 729.00" \
    "760.00 main: exact hold 32.00 -664.00 753.00" "790.00 main: exact hold 32.00 -664.00 801.00" \
    "885.00 main: exact hold 32.00 -660.00 897.00"; do
    if echo "$out" | grep -q "$want"; then n=$((n+1)); fi
done
check "$([ "$n" = 5 ] && echo 1 || echo 0)" "VR hands on Quoth's ladder: 5 exact holds on its drawn rungs ($n of 5)"
nn=$(echo "$out" | grep -c ': none')
check "$([ "$nn" = 2 ] && echo 1 || echo 0)" "VR hands: none over its top, none with Hand-Climbed Ladders off ($nn of 2)"
# (From its top: standing on the catwalk, the hand reaching down past its edge to the ladder's top rung.)
check "$(echo "$out" | grep -q '878.00 main: exact hold 32.00 -664.00 873.00' && echo 1 || echo 0)" "VR hands from its top: the drawn rung under the catwalk's edge"
check "$(echo "$out" | grep -q '500.00 main: exact hold -64.00 -1354.00 509.00' && echo "$out" | grep -q '530.00 main: exact hold -64.00 -1354.00 541.00' && echo 1 || echo 0)" "VR hands on the pillar ladder: its drawn rungs 32 apart"

# kellbase1's corridor fail-safe (QC vr_quoth.qc VR_Quoth_KellThink): shut in between its top doors and its south exit
# doors (both shut, no monster), the top doors open again after 10 s; standing in the arena, nothing.
CS="vr_campaign_native quoth;wait30;map kellbase1;wait60;developer 1;god;notarget;noclip"
out=$(bash $KIT/run.sh $AGENT "${F[@]}" -QuitOn "^quoth: kellbase1's corridor|^modladder: none" -Timeout 180 -Filter "^quoth: kell|^modladder:" \
    -Script "$CS;setpos 32 -960 664 0 90 0;wait1500;echo modladder: none;toggleconsole;quit" 2>&1)
check "$(echo "$out" | grep -q "the top doors opened again" && echo 1 || echo 0)" "kellbase1: shut in its corridor, the top doors open again"
out=$(bash $KIT/run.sh $AGENT "${F[@]}" -QuitOn "^quoth: kellbase1's corridor|^modladder: none" -Timeout 180 -Filter "^quoth: kell|^modladder:" \
    -Script "$CS;setpos 32 -560 664 0 90 0;wait1000;echo modladder: none;toggleconsole;quit" 2>&1)
check "$(echo "$out" | grep -q "^modladder: none" && echo 1 || echo 0)" "kellbase1: in the arena, no fail-safe"
verdict; exit $fail
