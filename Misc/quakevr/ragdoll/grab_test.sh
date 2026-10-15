#!/bin/bash
# grab_test.sh <agent> [cases] -- falling monsters grabbing others (ROUND22.md, "Grabbing others"; vr_ragdoll_active_grab):
# in vrtesthall's open floor, monster B (MONSB, a grunt) 150 units ahead, monster A (MONS: vr_test_spawn's numbers) 110 ahead,
# vr_ragdoll_active_grab 1 (the chance aside). Cases:
#   death  A killed by a body shot (vr_test_kill 1): falling back towards B it grabs at him (PASS: it reached for him;
#          prints whether the hand got there and held: about 2 in 3)
#   rag    B knocked down first (its ragdoll lying behind A), then A killed: it grabs at B's ragdoll (PASS: it reached)
#   shove  A shoved two-handed, beaten (Knockdowns' Chance 100): it backpedals into B, gives way, grabs at him (prints
#          what it did: a fast fall often lands before the hand gets there)
#   off    the death case with vr_ragdoll_active_grab 0 (PASS: no grab)
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
export TB_NODRAW=${TB_NODRAW-1} # (batched frames not drawn: no check here reads the drawing; testbatch.sh)
# (The body: tb_main plays its runs as batches, testbatch.sh; TB_SLOW=1: each alone by run.sh, as before.)
tb_body() {
CASES=${*:-death rag shove off}
MONS=${MONS:-0 5 1 8 6} # (a zombie isn't killed by a body shot)
MONSB=${MONSB:-0}
name() { case $1 in 0) echo grunt;; 1) echo ogre;; 2) echo zombie;; 5) echo knight;; 6) echo hknight;; 8) echo enforcer;; *) echo m$1;; esac; }
FILTER="grabs at|ragenv: [0-9]+ (left|right) hand (holds|lets go \(grab)|rror|CRASH"
one() { # one <A> <grab> <before B is passed by> <then>
    tb_run_sh -Script "sv_random_seed ${SEED:-7};map vrtesthall;wait30;god;notarget;developer 1;vr_debug_ragdoll 1;vr_knockdown_chance 100;vr_ragdoll_active_grab $2;setpos -150 0 24 0 90 0;wait5;vr_test_spawn $MONSB;vr_test_spawn_dist ${DB:-150};impulse 241;wait5;$3;vr_test_spawn $1;vr_test_spawn_dist 110;impulse 241;wait40;$4;wait300;toggleconsole;quit" -Filter "$FILTER" -Timeout 300 2>&1 | tr -d '\r'
}
for c in $CASES; do
    pass=0
    all=0
    for m in $MONS; do
        case $c in
        death) out=$(one $m 1 "wait1" "vr_test_kill 1") ;;
        rag)   out=$(DB=170 one $m 1 "wait30;vr_knockdown_push 0.1;vr_knockdown_time_min 6;vr_knockdown_time_max 6;vr_knockdown_test 0;wait60" "vr_test_kill 1") ;;
        shove) out=$(DB=190 one $m 1 "wait1" "vr_knockdown_test 22") ;;
        off)   out=$(one $m 0 "wait1" "vr_test_kill 1") ;;
        esac
        grab=$(echo "$out" | grep -o "grabs at [a-z ]*[0-9]* falling at [0-9.]* m/s" | head -1)
        held=$(echo "$out" | grep -o "lets go (grab, [a-z ]*[^)]*) after [0-9.]* s, held [0-9.]* s" | head -1 | sed 's/lets go //')
        ok=FAIL
        case $c in
        death|rag) [ -n "$grab" ] && ok=PASS ;;
        shove) ok=INFO ;;
        off) [ -z "$grab" ] && ok=PASS ;;
        esac
        echo "$out" | grep -q "rror\|CRASH" && ok=FAIL
        [ $ok = PASS ] && pass=$((pass + 1))
        all=$((all + 1))
        echo "grab $c $(name $m): ${grab:-no grab}; ${held:-not held}: $ok"
    done
    echo "grab $c: $pass of $all PASS"
done
}
source "$(dirname "$0")/../testbatch/testbatch.sh"
tb_main "$AGENT" tb_body "$@"
