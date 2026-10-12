#!/bin/bash
# climb_test.sh <agent> [cases] -- living ledge pull-ups (ROUND22.md, "Ledge pull-ups"; vr_ragdoll_active_pullup): a
# monster (MONS: vr_test_spawn's numbers, all six active classes by default) at vrtesthall's platform west lip (192
# units up), its back to the drop, knocked down by a gentle push (vr_knockdown_test 0, vr_knockdown_push 0.15): it
# topples over the lip, grabs it and (cases):
#   pull   pulls itself up (vr_ragdoll_active_pullup 1): PASS if it grabbed, pulled itself up and got up on the top
#   drop   hangs and drops (vr_ragdoll_active_pullup 0.01: it grabs, the chance says no): PASS if it dropped, up below
#   hit    pulls, a hand struck 0.9 s after the knockdown (vr_ragdoll_env_hit): that hand lets go (PASS if it did)
#   off    vr_ragdoll_active_pullup 0: it never grabs (PASS if no climb), falls and gets up below
# Each prints one line per monster and trial: the climb's lines (its grab, how it ended, its chest's height over the lip
# at most) and where it stood up at the end (z 216: the top; 24: the floor below); then the passes per case.
# N=n: n trials each (sv_random_seed SEED0, SEED0 + 1, ...).
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
CASES=${*:-pull drop hit off}
MONS=${MONS:-0 5 1 8 6 2}
N=${N:-1}
SEED0=${SEED0:-7}
name() { case $1 in 0) echo grunt;; 1) echo ogre;; 2) echo zombie;; 5) echo knight;; 6) echo hknight;; 8) echo enforcer;; *) echo m$1;; esac; }
FILTER="climb:|ragenv: [0-9]+ (left|right) hand (holds|lets)|monster_[a-z_]*: state|rror|CRASH|exit="
one() { # one <monster> <pullup> <then> <seed>
    bash $KIT/run.sh $AGENT -Script "sv_random_seed $4;map vrtesthall;wait30;god;notarget;developer 1;vr_debug_ragdoll 1;vr_knockdown_debug 1;vr_knockdown_push ${PUSH:-0.15};vr_ragdoll_active_pullup $2;${EXTRA:-};setpos 430 430 216 0 180 0;vr_mock_turn_to 180;wait150;vr_test_spawn_dist ${DIST:-132};vr_test_spawn $1;impulse 241;wait40;vr_knockdown_test 0;$3;wait900;vr_knockdown_test 5;toggleconsole;quit" -Filter "$FILTER" -Timeout 300 2>&1 | tr -d '\r'
}
for c in $CASES; do
    pass=0
    all=0
    for m in $MONS; do
        i=0
        while [ $i -lt $N ]; do
            seed=$((SEED0 + i))
            i=$((i + 1))
            case $c in
            pull) out=$(one $m 1 "" $seed) ;;
            drop) out=$(one $m 0.01 "" $seed) ;;
            hit)  out=$(one $m 1 "wait65;vr_ragdoll_env_hit" $seed) ;;
            off)  out=$(one $m 0 "" $seed) ;;
            esac
            z=$(echo "$out" | grep "monster_" | grep -v "health 1000" | tail -1 | grep -o "at [-0-9.]* [-0-9.]* [-0-9.]*" | awk '{printf "%.0f", $4}')
            grab=$(echo "$out" | grep -o "grabs a ledge's lip ([0-9] hands)" | head -1)
            end=$(echo "$out" | grep -o "climb: [0-9]* \(pulled itself up\|dropped\) ([a-z ]*) after [0-9.]* s: its chest [-0-9]* units over the lip at most" | head -1 | sed 's/climb: [0-9]* //')
            hits=$(echo "$out" | grep -c "lets go (ledge, hit)")
            ok=FAIL
            case $c in
            pull) echo "$end" | grep -q "pulled itself up" && [ "${z:-0}" -gt 150 ] && ok=PASS ;;
            drop) [ -n "$grab" ] && echo "$end" | grep -q "dropped" && [ "${z:-999}" -lt 150 ] && ok=PASS ;;
            hit)  [ "$hits" -ge 1 ] && ok=PASS ;;
            off)  [ -z "$grab" ] && ok=PASS ;;
            esac
            echo "$out" | grep -q "rror\|CRASH" && ok=FAIL
            [ $ok = PASS ] && pass=$((pass + 1))
            all=$((all + 1))
            echo "climb $c $(name $m) (seed $seed): ${grab:-no grab}; ${end:-no end}; $hits hands hit off; up at z ${z:-?}: $ok"
        done
    done
    echo "climb $c: $pass of $all PASS"
done
