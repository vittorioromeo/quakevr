#!/bin/bash
# guard_test.sh <agent> [cases] -- monsters guarding their heads in melee (ROUND22.md, "Guarding the head";
# vr_ragdoll_active_guard 1 here: the chance aside). Each case prints PASS or FAIL and what it saw.
#   swing   vrfiringrange: your main hand swinging (vr_mock_swing 0.5) at a grunt and a knight 45 units ahead: it guards
#           (standing: a hit reaction's guard, its arms turned: `hittrace`'s most, deg; PASS: a guard, arms past 30 deg)
#   shot    the same monsters shot in the head (vr_test_hit 2), no swing: no guard (PASS)
#   stagger vrfiringrange: a grunt shoved (steady: it recovers), vr_test_guard as it staggers: its ragdoll's arms (PASS:
#           guarded, and it still recovers)
#   down    vrtesthall: a grunt knocked down, vr_test_guard: its ragdoll's arms (PASS: guarded, and it gets up after)
#   off     the swing case with vr_ragdoll_active_guard 0 (PASS: no guard)
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
# (The body: tb_main plays its runs as batches, testbatch.sh; TB_SLOW=1: each alone by run.sh, as before.)
tb_body() {
CASES=${*:-swing shot stagger down off}
run() { tb_run_sh -Script "sv_random_seed ${SEED:-7};$1;toggleconsole;quit" -Filter "guard:|hittrace|recovers|rises through|gets up after|rror|CRASH" -Timeout 300 2>&1 | tr -d '\r'; }
FR="map vrfiringrange;wait30;god;notarget;developer 1;vr_hit_react_debug 2;vr_knockdown_debug 1;vr_ragdoll_active_guard ${GUARD:-1}"
verdict() { # verdict <name> <condition result 0/1> <what>
    [ "$2" = 0 ] && echo "guard $1: $3: PASS" || echo "guard $1: $3: FAIL"
}
for c in $CASES; do
    case $c in
    swing|off)
        for m in 0 5; do
            g=1; [ $c = off ] && g=0
            out=$(GUARD=$g run "$FR;vr_ragdoll_active_guard $g;vr_test_spawn $m;vr_test_spawn_dist 45;impulse 241;wait60;vr_mock_swing 0.5;wait150;vr_mock_swing 0;wait10")
            n=$(echo "$out" | grep -c "standing guards")
            most=$(echo "$out" | grep -o "most [0-9.]* deg [a-z_]*" | awk '{if($2>m){m=$2;b=$4}} END{printf "%.0f %s", m, b}')
            if [ $c = swing ]; then
                echo "$most" | awk '{exit !($1 > 30)}'; ok=$?
                [ "$n" -ge 1 ] || ok=1
            else
                [ "$n" = 0 ]; ok=$?
            fi
            verdict "$c m$m" $ok "$n guards, arms at most $most deg"
        done ;;
    shot)
        for m in 0 5; do
            out=$(run "$FR;vr_test_hit_calm 1;vr_test_spawn $m;vr_test_spawn_dist 90;impulse 241;wait60;vr_test_hit 2;wait60;vr_test_hit 2;wait60")
            n=$(echo "$out" | grep -c "guard:")
            [ "$n" = 0 ]; verdict "shot m$m" $? "$n guards from two head shots"
        done ;;
    stagger)
        out=$(run "$FR;vr_knockdown_chance 0.01;vr_test_spawn 0;vr_test_spawn_dist 100;impulse 241;wait60;vr_knockdown_test 21;wait10;vr_test_guard 1;wait200")
        echo "$out" | grep -q "guards its head (staggering)" && echo "$out" | grep -q "recovers from its stagger"
        verdict stagger $? "$(echo "$out" | grep -o "guards its head ([a-z ]*)" | head -1); $(echo "$out" | grep -o "recovers from its stagger after [0-9.]* s")" ;;
    down)
        out=$(run "map vrtesthall;wait30;god;notarget;developer 1;vr_knockdown_debug 1;vr_ragdoll_active_guard 1;setpos -150 0 24 0 90 0;wait5;vr_test_spawn 0;vr_test_spawn_dist 100;impulse 241;wait60;vr_knockdown_push 0.3;vr_knockdown_test 0;wait60;vr_test_guard 1;wait400")
        echo "$out" | grep -q "guards its head (down)" && echo "$out" | grep -q "rises through its get-up\|gets up after"
        verdict down $? "$(echo "$out" | grep -o "guards its head ([a-z ]*)" | head -1); $(echo "$out" | grep -o "\(rises through its get-up\|gets up\) after [0-9.]* s" | head -1)" ;;
    esac
done
}
source "$(dirname "$0")/../testbatch/testbatch.sh"
tb_main "$AGENT" tb_body "$@"
