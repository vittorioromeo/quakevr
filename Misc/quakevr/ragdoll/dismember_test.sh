#!/bin/bash
# dismember_test.sh <agent> [cases] -- active ragdolls' dismemberment (ROUND22.md, "Active ragdolls: dismemberment";
# vr_ragdoll_active_dismember): the grunt, knight and ogre (MONS="0 5 1": vr_test_spawn's numbers) on vrfiringrange,
# each standing 110 units ahead, unaware of you (notarget), a limb cut off by a sword's cut (vr_test_cut), the same scene
# each time (vr_ragdoll_active_seed). Each prints a summary of its `ragdis` / `ragtrace` lines (vr_ragdoll_active_trace 3;
# dismember_trace.py): what he does, the balance beat (until when, his lean), the hand's distance from the stump over
# time, the writhe's stir, how long driven, energy and spin once let go; NaN, a crash or a restless body fails.
#   arm      killed by his left arm cut off at the shoulder (vr_test_cut 1): his right hand to the stump, a writhe
#   forearm  his left forearm at the elbow (6)
#   leg      his left leg at the hip (3): a balance beat on the right one, then the fall, a hand to the stump
#   shin     his left shin at the knee (7)
#   legs     both legs (5): he crawls on his arms
#   head     beheaded (8): limp at once (no drive), as before
#   dying    shot in the body (vr_test_kill 1), 0.4 s on his left arm (11), then his left leg (13) cut off as he falls
#   early    shot so, his left arm cut off 0.1 s on (his ragdoll not made yet: the cut makes it, an active death)
#   off      vr_ragdoll_active_dismember 0, his leg cut: no drive (limp at once, as before this stage)
#   run      e1m1, 450 units off and aware of you (active_test's stumble), his left leg cut as he runs at you (RUNWAIT
#            frames on; MONS=5: a knight, 117 u/s): the balance beat holds at least 0.5 s (PASS/FAIL; his run's
#            stumble no longer drags the pin off the planted foot). Not in the default cases (a grunt stops to shoot).
#   cost     8 grunts each killed by a leg cut: the drive's ms a frame (vr_ragdoll_active_list) and the physics step's,
#            with vr_ragdoll_active_dismember 1 and 0 (--exclusive)
# EYES=1: a contact sheet (the kit's scratch: dismember_<monster>_<case>.png, EYESCASE: leg) of 0.1 .. 4 s.
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
CASES=${*:-arm forearm leg shin legs head dying early off cost}
MONS=${MONS:-0 5 1}
PY=${PY:-py -3.13}
SUM="$TREE/Misc/quakevr/ragdoll/dismember_trace.py"
name() { case $1 in 0) echo grunt;; 1) echo ogre;; 5) echo knight;; 6) echo hknight;; 8) echo enforcer;; *) echo m$1;; esac; }
PRE="wait30;god;vr_debug_ragdoll 1;vr_ragdoll_active_seed ${SEED:-0};vr_ragdoll_active_trace 3${EXTRA:+;$EXTRA}"
FILTER="^ragtrace|^ragdis|ragdoll: [0-9]+ (lost|loses|killed|beheaded|active)|test cut|test kill|rror|CRASH|exit=|TIMEOUT"
run() { bash $KIT/run.sh $AGENT -Script "$1" -Filter "$FILTER" -Timeout 300 2>&1 | tr -d '\r'; }
one() { # one <monster> <what> <extra settings>
    run "sv_random_seed ${SEED:-7};map vrfiringrange;$PRE;notarget;$3;vr_test_spawn $1;vr_test_spawn_dist ${DIST:-110};impulse 241;wait40;$2;wait650;toggleconsole;quit"
}
for c in $CASES; do
    echo "== $c"
    for m in $MONS; do
        case $c in
        arm) one $m "vr_test_cut 1" "" | $PY "$SUM" "$(name $m) arm" ;;
        forearm) one $m "vr_test_cut 6" "" | $PY "$SUM" "$(name $m) forearm" ;;
        leg) one $m "vr_test_cut 3" "" | $PY "$SUM" "$(name $m) leg" ;;
        shin) one $m "vr_test_cut 7" "" | $PY "$SUM" "$(name $m) shin" ;;
        legs) one $m "vr_test_cut 5" "" | $PY "$SUM" "$(name $m) legs" ;;
        head) one $m "vr_test_cut 8" "" | $PY "$SUM" "$(name $m) head" ;;
        dying) one $m "vr_test_kill 1;wait30;vr_test_cut 11;wait20;vr_test_cut 13" "" | $PY "$SUM" "$(name $m) dying" ;;
        early) one $m "vr_test_kill 1;wait8;vr_test_cut 11" "" | $PY "$SUM" "$(name $m) early" ;;
        off) one $m "vr_test_cut 3" "vr_ragdoll_active_dismember 0" | $PY "$SUM" "$(name $m) off" ;;
        run) # active_test's stumble scene: e1m1, 450 units off, he sees you and runs at you; his leg cut on the way
            run "sv_random_seed ${SEED:-7};map e1m1;$PRE;vr_test_spawn $m;vr_test_spawn_dist 450;impulse 241;wait${RUNWAIT:-150};vr_test_cut 3;wait650;toggleconsole;quit" \
                | $PY "$SUM" "$(name $m) run" | tee "$TREE/scratch/dismember_run.txt"
            b=$(grep -oE "beat until [0-9.]+" "$TREE/scratch/dismember_run.txt" | grep -oE "[0-9.]+$")
            awk -v b="${b:-0}" 'BEGIN { exit !(b >= 0.5) }' && echo "run: PASS (beat ${b} s)" || echo "run: FAIL (beat ${b:-none} s: under 0.5)" ;;
        esac
    done
    if [ "$c" = cost ]; then
        for a in 1 0; do
            S="sv_random_seed ${SEED:-7};map vrfiringrange;wait30;god;notarget;vr_ragdoll_max 8;vr_ragdoll_active_dismember $a;vr_test_spawn 0"
            for d in 80 120 160 200 240 280 320 360; do S="$S;vr_test_spawn_dist $d;impulse 241;wait2"; done
            S="$S;wait40;vr_ragdoll_active_list"
            for i in 1 2 3 4 5 6 7 8; do S="$S;vr_test_cut 3;wait1"; done
            S="$S;wait4;vr_physics_steptime;wait60;vr_physics_steptime;vr_ragdoll_active_list;wait200;vr_physics_steptime;vr_ragdoll_active_list;toggleconsole;quit"
            echo "-- vr_ragdoll_active_dismember $a"
            bash $KIT/run.sh --exclusive $AGENT -Script "$S" -Filter "the drive:|steptime|rror|CRASH|exit=" -Timeout 300 2>&1 | tr -d '\r' | grep -v "^$"
        done
    fi
done
if [ -n "$EYES" ]; then
    SHOTS="$KIT/bases/$AGENT/qbase/quakevr/screenshots"
    EC=${EYESCASE:-leg}
    case $EC in arm) N=1;; leg) N=3;; legs) N=5;; shin) N=7;; *) N=3;; esac
    for m in $MONS; do
        S="sv_random_seed ${SEED:-7};map vrfiringrange;wait30;god;notarget;vr_ragdoll_active_seed ${SEED:-0};vr_ragdoll_active_debug ${DEBUGDRAW:-0};vr_test_spawn $m;vr_test_spawn_dist ${DIST:-110}"
        S="$S;impulse 241;wait40;vr_mock_camera ${CAM:-5.2 0.9 -3.2 9 90};wait3;vr_test_cut $N;wait7;screenshot"
        for w in 14 14 14 22 29 43 58 72; do S="$S;wait$w;screenshot"; done
        S="$S;toggleconsole;quit"
        bash $KIT/run.sh $AGENT -Clean -Script "$S" -Filter "exit=" -Timeout 300 >/dev/null 2>&1
        OUT="$KIT/scratch/dismember_$(name $m)_$EC.png"
        $PY "$TREE/Misc/quakevr/ragdoll/active_sheet.py" "$SHOTS" "$OUT" 9 ${BOX:-} && echo "sheet: $OUT"
    done
fi
