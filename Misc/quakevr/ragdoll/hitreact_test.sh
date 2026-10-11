#!/bin/bash
# hitreact_test.sh <agent> [cases] -- living hit reactions (ROUND22.md, "Active ragdolls: living hit reactions";
# vr_hit_react). A grunt (MON=0; vr_test_spawn's numbers) on vrtesthall, 110 units ahead, struck by vr_test_hit.
#   trace  a calm leg hit (vr_test_hit_calm: no pain frames), its bones' turns frame by frame (hittrace: the decay curve)
#   eyes   body, head, arm and leg hits seen from his side, reaction on and off, the same frames (the kit's scratch:
#          hitreact_sheet.png; the differing pixels per frame printed); ON=2: the whole-model tilt
#   ai     he sees you (god mode) and is struck three times (body, leg, head; real hits, pain and all), watched each
#          server frame (vr_test_hit_watch): his animation frames' sequence with the reactions off (twice: the runs'
#          own spread), on, and on without the slow-down; the runs are not bit-exact (a few outcomes for one setting)
#   slow   a knight running at you, a calm leg hit: his steps, each scaled (hitslow:), the hitch
#   cost   12 grunts struck at once (--exclusive): the module's ms a frame before and during, Limbs and Whole Body
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
CASES=${*:-trace eyes ai slow cost}
MON=${MON:-0}
PY=${PY:-py -3.13}
HERE=$TREE/Misc/quakevr/ragdoll
SHOTS="$KIT/bases/$AGENT/qbase/quakevr/screenshots"
SCR=$TREE/scratch/hitreact
mkdir -p $SCR
PRE="sv_random_seed ${SEED:-7};map vrtesthall;wait30;god;developer 1;setpos -150 0 24 0 90 0"
run() { bash $KIT/run.sh $3 $AGENT -Script "$1" -Filter "$2" -Timeout 300 2>&1 | tr -d '\r' | grep -v "^$"; }
aiRun() { # aiRun <tag> <settings>
    run "$PRE;$2;vr_test_spawn $MON;vr_test_spawn_dist 300;impulse 241;wait60;vr_test_hit_watch 5;vr_test_hit_damage 6;vr_test_hit 1;wait120;vr_test_hit 4;wait120;vr_test_hit 2;wait400;toggleconsole;quit" "hitwatch|exit=" > $SCR/ai_$1.txt
}
for c in $CASES; do
    echo "== $c"
    case $c in
    trace)
        run "$PRE;notarget;vr_hit_react_debug 2;vr_test_spawn $MON;vr_test_spawn_dist 110;impulse 241;wait40;vr_test_hit_calm 1;vr_test_hit_damage 12;vr_test_hit 4;wait160;toggleconsole;quit" "struck on|hittrace" \
            | awk '/struck on/ { print; next } { n++; if(n % 8 == 1) print }' | cut -c1-140 ;;
    eyes)
        ROWS=""
        for k in 1 2 3 4; do
            for mode in on off; do
                R="vr_hit_react ${ON:-1}"
                if [ $mode = off ]; then R="vr_hit_react 0"; fi
                S="$PRE;notarget;$R;vr_test_spawn $MON;vr_test_spawn_dist 110;impulse 241;wait40;vr_mock_camera 4.6 1.3 -4.1 8 90;wait3;vr_test_hit_damage 12;vr_test_hit_calm 1;vr_test_hit $k;wait2;screenshot;wait8;screenshot;wait8;screenshot;wait8;screenshot;wait15;screenshot;wait40;screenshot"
                bash $KIT/run.sh $AGENT -Clean -Script "$S;toggleconsole;quit" -Filter "exit=" -Timeout 300 >/dev/null 2>&1
                D="$SCR/eyes_${MON}_$k$mode"
                mkdir -p "$D"
                cp "$SHOTS"/*.png "$D"/
                ROWS="$ROWS $D"
            done
        done
        $PY $HERE/hitreact_sheet.py "$KIT/scratch/hitreact_sheet.png" 6 0.30,0.30,0.50,0.80 $ROWS && echo "sheet: $KIT/scratch/hitreact_sheet.png" ;;
    ai)
        aiRun off1 "vr_hit_react 0"
        aiRun off2 "vr_hit_react 0"
        aiRun on "vr_hit_react 1"
        aiRun noslow "vr_hit_react 1;vr_hit_react_slow 0"
        $PY $HERE/hitreact_ai.py $SCR/ai_off1.txt $SCR/ai_off2.txt $SCR/ai_on.txt $SCR/ai_noslow.txt
        $PY $HERE/hitreact_ai.py $SCR/ai_off2.txt $SCR/ai_off1.txt $SCR/ai_on.txt $SCR/ai_noslow.txt ;;
    slow)
        run "$PRE;vr_hit_react_debug 1;vr_test_spawn 5;vr_test_spawn_dist 500;impulse 241;wait150;vr_test_hit_calm 1;vr_test_hit_damage 20;vr_test_hit 4;wait200;toggleconsole;quit" "hitslow|exit=" ;;
    cost)
        S="sv_random_seed ${SEED:-7};map vrfiringrange;wait30;god;notarget;developer 1"
        for d in 80 130 180 230 280 330 380 430 480 530 580 630; do S="$S;vr_test_spawn 0;vr_test_spawn_dist $d;impulse 241;wait5"; done
        for mode in 1 2; do
            echo "vr_hit_react $mode"
            run "$S;vr_hit_react $mode;wait60;vr_hit_react_info;wait60;vr_hit_react_info;vr_test_hit_calm 1;vr_test_hit_damage 6;vr_test_hit_all 1;vr_test_hit 1;wait3;vr_hit_react_info;wait60;vr_hit_react_info;toggleconsole;quit" "cost|monsters struck|exit=" --exclusive
        done ;;
    esac
done
