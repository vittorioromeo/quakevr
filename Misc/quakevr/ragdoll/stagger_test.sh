#!/bin/bash
# stagger_test.sh <agent> [cases] -- active staggers, falls and get-ups (ROUND22.md, "Active ragdolls: stagger, judo,
# get-up"), on vrfiringrange (MONS="0 8" to pick: vr_test_spawn's numbers; N trials, 8). Each case prints PASS or FAIL
# lines; the last line counts the FAILs.
#   rates   fresh monsters shoved one-handed and two-handed (vr_knockdown_test 21, 22): how many recovered and fell,
#           each one's balance (R recovered, F fell); PASS: no error
#   steady  the same, one hand, with Knockdowns' Chance 0.01 (PASS: all recover) and 100 (PASS: all fall)
#   getup   knocked down (vr_knockdown_test 0) and up (1): the rise's end and the hand-back's gap (units: the ragdoll's
#           vertices against the animated frame), with vr_ragdoll_active_getup 1 and 0; PASS: handed back, no error
#   taken   a grunt knocked down and up, the player stepping onto its rise spot as it rises (setpos TAKEN, its spot on
#           vrfiringrange): PASS: it stands beside him or slumps ("its spot taken"), never inside him
#   lost    a grunt knocked down and up, a blast (vr_ragdoll_blast_test 200) beside it as it rises: its get-up lost;
#           PASS: the lapse waits vr_knockdown_retry before the next rise ("lapsed (1 so far)"), it gets up in the end,
#           at most 3 rises begun
#   judo    a two-hand throw (vr_foegrab_throw_test 0): the throw trace's summary and when it landed; PASS: it landed
#   cost    6 grunts shoved at once (--exclusive): the drive's ms a frame and the physics step's, stagger 1 and 0
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
CASES=${*:-rates steady getup taken lost judo cost}
MONS=${MONS:-0 8 5 6 1 2}
N=${N:-8}
PRE="sv_random_seed ${SEED:-7};map vrfiringrange;wait30;god;notarget;developer 1;vr_knockdown_debug 1"
OUT="$TREE/scratch/stagger_test_out.txt"
mkdir -p "$TREE/scratch"
FAILS=0
run() { bash $KIT/run.sh $AGENT -Script "$1" -Filter "$2" -Timeout 600 2>&1 | tr -d '\r'; }
verdict() { # verdict <ok 0/1> <what>
    if [ "$1" = 1 ]; then echo "  PASS $2"; else echo "  FAIL $2"; FAILS=$((FAILS + 1)); fi
}
shoves() { # shoves <monster> <mode> <extra> <want: any|R|F>
    local S="$PRE;$3"
    for i in $(seq 1 $N); do
        S="$S;vr_test_spawn $1;vr_test_spawn_dist 60;impulse 241;wait40;vr_knockdown_test $2;wait200;vr_knockdown_test 20"
    done
    run "$S;toggleconsole;quit" "stagger, balance|fell at|recovered at|rror|CRASH" \
        | grep -o "balance [0-9.]*\|fell at\|recovered at\|rror.*\|CRASH.*" | sed 's/balance //; s/fell at/F/; s/recovered at/R/' \
        | awk -v m=$1 -v k=$2 '/^F/ { f++ } /^R/ { r++ } /rror|CRASH/ { e++ } { s = s " " $0 } END { printf "monster %s mode %s: fell %d, recovered %d, errors %d |%s\n", m, k, f, r, e, s }' \
        > "$OUT"
    cat "$OUT"
    local f r e
    f=$(grep -oE "fell [0-9]+" "$OUT" | grep -oE "[0-9]+")
    r=$(grep -oE "recovered [0-9]+" "$OUT" | grep -oE "[0-9]+")
    e=$(grep -oE "errors [0-9]+" "$OUT" | grep -oE "[0-9]+")
    case $4 in
    R) verdict $([ "$r" = "$N" ] && [ "$e" = 0 ] && echo 1 || echo 0) "monster $1: all $N recovered ($r)" ;;
    F) verdict $([ "$f" = "$N" ] && [ "$e" = 0 ] && echo 1 || echo 0) "monster $1: all $N fell ($f)" ;;
    *) verdict $([ "$e" = 0 ] && [ $((f + r)) -gt 0 ] && echo 1 || echo 0) "monster $1 mode $2: $((f + r)) shoves, no error" ;;
    esac
}
for c in $CASES; do
    echo "== $c"
    case $c in
    rates) for m in $MONS; do shoves $m 21 "" any; shoves $m 22 "" any; done ;;
    steady) for m in $MONS; do shoves $m 21 "vr_knockdown_chance 0.01" R; shoves $m 21 "vr_knockdown_chance 100" F; done ;;
    getup)
        for m in $MONS; do
            for g in 1 0; do
                run "$PRE;vr_ragdoll_active_getup $g;vr_test_spawn $m;vr_test_spawn_dist 80;impulse 241;wait40;vr_knockdown_test 0;wait200;vr_knockdown_test 1;wait900;toggleconsole;quit" \
                    "risen at|handback|lapsed|lost|rror|CRASH" | grep -v "^$\|^exit" > "$OUT"
                tr '\n' ' ' < "$OUT" | sed 's/getup:/\n  getup:/g; s/handback:/\n  handback:/g'
                echo
                verdict $(grep -q "handback:" "$OUT" && ! grep -q "rror\|CRASH" "$OUT" && echo 1 || echo 0) "monster $m getup $g: handed back"
            done
        done ;;
    taken)
        run "$PRE;vr_test_spawn 0;vr_test_spawn_dist 80;impulse 241;wait40;vr_knockdown_test 0;wait200;vr_knockdown_test 1;wait40;setpos ${TAKEN:-38 -571 41};wait300;setpos 206 -700 41;wait600;toggleconsole;quit" \
            "getup|handback|knockdown: monster|no room|rror|CRASH" | grep -v "^$\|^exit" > "$OUT"
        grep "spot taken\|rises through its\|no room" "$OUT" | cut -c1-140
        verdict $(grep -q "its spot taken" "$OUT" && ! grep -q "rror\|CRASH" "$OUT" && echo 1 || echo 0) "grunt: its spot taken (beside him or slumps)" ;;
    lost)
        run "$PRE;vr_test_spawn 0;vr_test_spawn_dist 80;impulse 241;wait40;vr_knockdown_test 0;wait200;vr_knockdown_test 1;wait50;vr_ragdoll_blast_test 200;wait900;toggleconsole;quit" \
            "getup|handback|knockdown: monster|rror|CRASH" | grep -v "^$\|^exit" > "$OUT"
        grep "lost its\|lapsed\|rises through its" "$OUT" | cut -c1-140
        n=$(grep -c "rises through frames" "$OUT")
        verdict $(grep -q "lost its get-up" "$OUT" && grep -q "lapsed (1 so far)" "$OUT" && grep -q "rises through its get-up\|gets up" "$OUT" && [ "$n" -le 3 ] && echo 1 || echo 0) \
            "grunt: its lost get-up waits, then it gets up ($n rises begun)" ;;
    judo)
        for m in $MONS; do
            run "$PRE;vr_debug_ragdoll 1;vr_test_spawn $m;vr_test_spawn_dist 60;impulse 241;wait40;vr_foegrab_throw_test 0 0;wait500;toggleconsole;quit" \
                "throw trace done|landed|rror|CRASH" | grep -v "^$\|^exit" > "$OUT"
            tr '\n' ' ' < "$OUT"
            echo
            verdict $(grep -q "landed" "$OUT" && ! grep -q "rror\|CRASH" "$OUT" && echo 1 || echo 0) "monster $m: thrown and landed"
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
echo "stagger_test: $FAILS FAIL"
