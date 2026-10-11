#!/bin/bash
# stagger_test.sh <agent> [cases] -- active staggers, falls and get-ups (ROUND22.md, "Active ragdolls: stagger, judo,
# get-up"), on vrfiringrange (MONS="0 8" to pick: vr_test_spawn's numbers; N trials, 8).
#   rates   fresh monsters shoved one-handed and two-handed (vr_knockdown_test 21, 22): how many recovered and fell,
#           each one's balance (R recovered, F fell)
#   steady  the same, one hand, with Knockdowns' Chance 0.01 (all should recover) and 100 (all should fall)
#   getup   knocked down (vr_knockdown_test 0) and up (1): the rise's end and the hand-back's gap (units: the ragdoll's
#           vertices against the animated frame), with vr_ragdoll_active_getup 1 and 0
#   judo    a two-hand throw (vr_foegrab_throw_test 0): the throw trace's summary and when it landed
#   cost    6 grunts shoved at once (--exclusive): the drive's ms a frame and the physics step's, stagger 1 and 0
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
CASES=${*:-rates steady getup judo cost}
MONS=${MONS:-0 8 5 6 1 2}
N=${N:-8}
PRE="sv_random_seed ${SEED:-7};map vrfiringrange;wait30;god;notarget;developer 1;vr_knockdown_debug 1"
run() { bash $KIT/run.sh $AGENT -Script "$1" -Filter "$2" -Timeout 600 2>&1 | tr -d '\r'; }
shoves() { # shoves <monster> <mode> <extra>
    local S="$PRE;$3"
    for i in $(seq 1 $N); do
        S="$S;vr_test_spawn $1;vr_test_spawn_dist 60;impulse 241;wait40;vr_knockdown_test $2;wait200;vr_knockdown_test 20"
    done
    run "$S;toggleconsole;quit" "stagger, balance|fell at|recovered at|rror|CRASH" \
        | grep -o "balance [0-9.]*\|fell at\|recovered at\|rror.*\|CRASH.*" | sed 's/balance //; s/fell at/F/; s/recovered at/R/' \
        | awk -v m=$1 -v k=$2 '/^F/ { f++ } /^R/ { r++ } /rror|CRASH/ { e++ } { s = s " " $0 } END { printf "monster %s mode %s: fell %d, recovered %d, errors %d |%s\n", m, k, f, r, e, s }'
}
for c in $CASES; do
    echo "== $c"
    case $c in
    rates) for m in $MONS; do shoves $m 21 ""; shoves $m 22 ""; done ;;
    steady) for m in $MONS; do shoves $m 21 "vr_knockdown_chance 0.01"; shoves $m 21 "vr_knockdown_chance 100"; done ;;
    getup)
        for m in $MONS; do
            for g in 1 0; do
                run "$PRE;vr_ragdoll_active_getup $g;vr_test_spawn $m;vr_test_spawn_dist 80;impulse 241;wait40;vr_knockdown_test 0;wait200;vr_knockdown_test 1;wait900;toggleconsole;quit" \
                    "risen at|handback|lapsed|lost|rror|CRASH" | grep -v "^$\|^exit" | tr '\n' ' ' | sed 's/getup:/\n  getup:/g; s/handback:/\n  handback:/g'
                echo
            done
        done ;;
    judo)
        for m in $MONS; do
            run "$PRE;vr_debug_ragdoll 1;vr_test_spawn $m;vr_test_spawn_dist 60;impulse 241;wait40;vr_foegrab_throw_test 0 0;wait500;toggleconsole;quit" \
                "throw trace done|landed|rror|CRASH" | grep -v "^$\|^exit" | tr '\n' ' '
            echo
        done ;;
    cost)
        for a in 1 0; do
            S="sv_random_seed 7;map vrfiringrange;wait30;god;notarget;vr_ragdoll_max 8;vr_ragdoll_active_stagger $a;vr_test_spawn 0"
            for d in 60 100 140 180 220 260; do S="$S;vr_test_spawn_dist $d;impulse 241;wait2"; done
            S="$S;wait40;vr_ragdoll_active_list"
            for i in 1 2 3 4 5 6; do S="$S;vr_knockdown_test 22;wait1"; done
            S="$S;wait4;vr_physics_steptime;wait60;vr_physics_steptime;vr_ragdoll_active_list;wait200;vr_physics_steptime;toggleconsole;quit"
            echo "-- vr_ragdoll_active_stagger $a"
            bash $KIT/run.sh --exclusive $AGENT -Script "$S" -Filter "vr_ragdoll_active_list|steptime|rror|CRASH" -Timeout 300 2>&1 | tr -d '\r' | grep -v "^$"
        done ;;
    esac
done
