#!/bin/bash
# vrtrailer_test.sh <agent> [extra commands] -- the trailer scene, headless (MAPPING.md, "vrtrailer"; QC vr_trailer.qc):
#   1. map vrtrailer; walk along the bridge (the stick), stop before the pedestal; the main hand's fist lowered onto the
#      Super Axe and closed: taken into the main hand (needs Dawn of the Machine's data);
#   2. the hands at the swing's start (the axe held low on the right), walk on (the stick) to the north islet, stop 30
#      units behind the grunt; the off hand onto the axe's handle and closed (two hands); the grunt stays idle and oblivious all the while
#      (vr_trailer_log 1: enemy worldspawn, think stand, stealth state 0, oblivious 1), stealth AI on (vr_ai_enhanced 1);
#   3. the two-handed swing across his neck (Misc/quakevr/vrtrailer_swing.mock): beheaded, dead;
#   4. vr_trailer_reset 1: a living, oblivious grunt where he stood, the axe on its pedestal, the player at the start.
# Prints PASS/FAIL per check; exits 1 on a failure. Extra commands (e.g. "vr_ai_enhanced 0") run after the map loads.
AGENT=$1; EXTRA=$2; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
MOCK=${MOCK:-$TREE/Misc/quakevr/vrtrailer_swing.mock}
OUT=$TREE/scratch/vrtrailer_test.log
mkdir -p $TREE/scratch
# the swing's first pose (the hands where the swing starts, before the off hand takes the handle)
M0=$(grep -m1 " main " $MOCK | cut -d' ' -f3-)
O0=$(grep -m1 " off " $MOCK | cut -d' ' -f3-)
S="developer 1;vr_ai_enhanced 1;map vrtrailer;wait60;$EXTRA;vr_trailer_log 1"
S="$S;vr_mock_stick off 0 1;wait70;vr_mock_stick off 0 0;wait30;setpos 0 -34 64 0 90 0;wait10"
S="$S;echo STEP pickup;vr_box3d_hand_props 0;vr_mock_hand_to main weapon 0.3 2;wait6;+grabmain;vr_mock_button main grip 1;wait30"
S="$S;echo STEP walk;vr_mock_hand main $M0;vr_mock_hand off $O0;wait20;vr_mock_stick off 0 1;wait140;vr_mock_stick off 0 0;wait40;setpos 0 730 64 0 90 0;wait30"
S="$S;echo STEP twohand;vr_mock_hand_to off held 0.6;wait6;+graboff;vr_mock_button off grip 1;wait30"
S="$S;echo STEP swing;vr_mock_play $MOCK;wait150;echo STEP after;wait60"
S="$S;echo STEP reset;vr_trailer_reset 1;vr_mock_play;-grabmain;-graboff;vr_mock_button main grip 0;vr_mock_button off grip 0;wait150;echo STEP end"
S="$S;vr_trailer_log 0;vr_startup_times;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "STEP|trailer|decap|taken into|weapon: |ENGINE|rror|map load|exit=${FILTER_EXTRA:+|$FILTER_EXTRA}" > $OUT 2>&1
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
# the log's lines between two STEP markers
between() { awk -v a="STEP $1" -v b="STEP $2" 'index($0,a){f=1;next} index($0,b){f=0} f' $OUT; }
check $(grep -q "Super Axe taken into the main hand" $OUT && echo 1 || echo 0) "the Super Axe taken from the pedestal into the main hand"
pre=$(awk 'index($0,"STEP swing"){exit} /trailer: t /' $OUT)
st=$(echo "$pre" | grep "grunt health")
n=$(echo "$st" | grep -c "grunt health")
bad=$(echo "$st" | grep -v "enemy worldspawn.*think stand stl 0 oblivious 1")
check $([ "$n" -gt 20 ] && [ -z "$bad" ] && echo 1 || echo 0) "the grunt idle and oblivious until the swing ($n log lines: enemy none, think stand, stealth 0, oblivious 1)"
near=$(echo "$pre" | grep -o "dist [0-9]*" | awk '{print $2}' | sort -n | head -1)
check $([ -n "$near" ] && [ "$near" -le 40 ] && echo 1 || echo 0) "the player came up right behind him (closest $near units)"
check $(echo "$pre" | grep -q "axe in hand 18" && echo 1 || echo 0) "the Super Axe in hand on the way"
cut=$(between swing reset | grep -m1 "beheaded by player")
check $([ -n "$cut" ] && echo 1 || echo 0) "the swing beheaded him (${cut:0:110})"
last=$(between swing reset | grep "grunt health" | tail -1)
check $(echo "$last" | grep -q "health -1 \|health 0 " && echo 1 || echo 0) "and he is dead ($(echo "$last" | grep -o 'health [-0-9.]*'))"
post=$(between reset end | grep "grunt health" | tail -1)
pos=$(between reset end | grep "dist" | tail -1)
check $(between reset end | grep -q "trailer: reset 1" && echo "$post" | grep -q "health 30 enemy worldspawn.*oblivious 1" && echo 1 || echo 0) "reset: a living, oblivious grunt again (${post:9:100})"
axez=$(echo "$pos" | sed -n "s/.*| axe '[^']* \([-0-9.]*\)' |.*/\1/p")
check $(echo "$axez" | awk '{print ($1 > 72 && $1 < 78) ? 1 : 0}') "reset: the axe on its pedestal (z $axez)"
check $(echo "$pos" | grep -q "player '  0.0 -444.0" && echo 1 || echo 0) "reset: the player at the start ($(echo "$pos" | grep -o "player '[^']*'"))"
grep -m1 -A0 "the last map load" $OUT | cut -c1-120
grep "ENGINE\|rror" $OUT | head -5
echo "(log: $OUT)"
exit $fail
