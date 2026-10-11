#!/bin/bash
# liquid_active_test.sh <agent> [cases] -- active ragdolls in liquids (ROUND22.md, "Active ragdolls: liquids";
# vr_ragdoll_active_liquids): a live monster dropped (the player hovering in noclip, 150 units back) into a liquid, then
# killed or left to die there; each case prints one line per death (liquid_trace.py: what it did, how long, the limbs'
# speed early and late, the head under early and late, the pelvis's sink, energy and spin after the drive; NaN, a crash,
# an explosion or a ragdoll still spinning fail).
#   water    vrtesthall's pool (120 deep): a grunt standing on its bottom killed by a body shot: paddles, then limp
#   knight   the same, a knight (armour: sinks once limp)
#   ogre     the same, an ogre
#   enforcer the same, an enforcer
#   hknight  the same, a death knight
#   drown    vrtesthall: a grunt left on the pool's bottom drowns (vr_liquids.qc: 12 s of breath, then 2, 4, 6 ...)
#   fallin   vrtesthall: a grunt killed in the air over the pool (just spawned): his body falls in during his death
#   head     vrtesthall: a headshot in the pool: a collapse, no swim
#   lava     e1m7's lava: a grunt falls in standing and dies of it: flails, burns (the QC's), gibbed after vr_burn_lava_gib
#   slime    e3m1's slime: the same in slime (vr_enemy_liquid_damage): flails longer
#   off      vrtesthall, vr_ragdoll_active_liquids 0: the death animation as on land
#   actoff   vrtesthall: vr_ragdoll_active 0 while a grunt killed over the pool falls: no swim (PASS/FAIL)
#   kdswim   vrtesthall: a grunt knocked down alive in the pool paddles (full strength, head up) towards where it can
#            stand, then gets up (PASS/FAIL)
#   kddie    the same, killed as it swims: its strokes weaken as a death's (PASS/FAIL)
#   land     vrfiringrange: a body shot on land: no swim (unchanged)
#   cost     4 grunts killed together in the pool (--exclusive): vr_ragdoll_active_list's drive cost, the step's
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
CASES=${*:-water knight ogre enforcer hknight drown fallin head lava slime actoff off land}
PY=${PY:-py -3.13}
SUM="$TREE/Misc/quakevr/ragdoll/liquid_trace.py"
PRE="wait30;god;notarget;developer 1;vr_debug_ragdoll 1;vr_ragdoll_active_seed ${SEED:-0};vr_ragdoll_active_trace 3"
FILTER="^ragtrace|^ragswim|down alive|gets up|limp at frame|active death|ragdoll: [0-9]+ (in |out of)|drowned|burnt|gibbed|rror|CRASH|exit=|TIMEOUT|steptime|vr_ragdoll_active_list|^  [0-9]+: "
run() { bash $KIT/run.sh $AGENT -Script "$1" -Filter "$FILTER" -Timeout 400 2>&1 | tr -d '\r'; }
# drop <map> <x y z> <monster> <then> [dist]
drop() {
    run "sv_random_seed ${SEED:-7};map $1;$PRE;vr_ragdoll_active_liquids ${LIQ:-1};noclip;setpos $2 0 ${YAW:-0} 0;wait5;vr_test_spawn $3;vr_test_spawn_dist ${5:-150};impulse 241;$4;toggleconsole;quit"
}
for c in $CASES; do
    case $c in
    water)    drop vrtesthall "180 -256 200" 0 "wait150;vr_test_kill 1;wait800" | $PY "$SUM" "grunt water" ;;
    knight)   drop vrtesthall "180 -256 200" 5 "wait150;vr_test_kill 1;wait800" | $PY "$SUM" "knight water" ;;
    ogre)     drop vrtesthall "180 -256 200" 1 "wait150;vr_test_kill 1;wait800" | $PY "$SUM" "ogre water" ;;
    enforcer) drop vrtesthall "180 -256 200" 8 "wait150;vr_test_kill 1;wait800" | $PY "$SUM" "enforcer water" ;;
    hknight)  drop vrtesthall "180 -256 200" 6 "wait150;vr_test_kill 1;wait800" | $PY "$SUM" "hknight water" ;;
    drown)    drop vrtesthall "180 -256 200" 0 "vr_enemy_liquid_damage 1;wait2200" | $PY "$SUM" "grunt drown" ;;
    fallin)   drop vrtesthall "180 -256 200" 0 "wait2;vr_test_kill 1;wait800" | $PY "$SUM" "grunt fallin" ;;
    head)     drop vrtesthall "180 -256 200" 0 "wait150;vr_test_kill 2;wait600" | $PY "$SUM" "grunt head" ;;
    actoff)   # vr_ragdoll_active 0 mid-death over the pool: the released body falls in limp, no swim wakes it
        n=$(drop vrtesthall "180 -256 200" 0 "wait2;vr_test_kill 1;wait30;vr_ragdoll_active 0;wait600;vr_ragdoll_active 1" | grep -cE "ragdoll: [0-9]+ in (water|slime|lava)")
        [ "$n" = 0 ] && echo "actoff: PASS (no swim after Active Deaths off)" || echo "actoff: FAIL ($n swims after Active Deaths off)" ;;
    off)      LIQ=0 drop vrtesthall "180 -256 200" 0 "wait150;vr_test_kill 1;wait600" | $PY "$SUM" "grunt off" ;;
    lava)     drop e1m7 "300 160 150" 0 "vr_enemy_liquid_damage 1;wait700" 100 | $PY "$SUM" "grunt lava" ;;
    slime)    drop e3m1 "960 16 0" 0 "vr_enemy_liquid_damage 1;wait700" 100 | $PY "$SUM" "grunt slime" ;;
    kdswim)   # a grunt knocked down alive in the pool: paddles at full strength towards where it can stand, then gets up
        out=$(drop vrtesthall "180 -256 200" 0 "vr_knockdown_debug 1;wait150;vr_knockdown_test 0;wait900")
        first=$(echo "$out" | grep "^ragswim" | head -1 | grep -o "at [-0-9]* [-0-9]*$")
        last=$(echo "$out" | grep "^ragswim" | tail -1 | grep -o "at [-0-9]* [-0-9]*$")
        thrust=$(echo "$out" | grep "^ragswim" | grep -c "thrust [1-9]")
        echo "kdswim: from ($first) to ($last), $thrust traced frames pushing; $(echo "$out" | grep -o "gets up from frame.*lay" | head -1)"
        echo "$out" | grep -q "down alive in water" && echo "$out" | grep -q "gets up after" && [ "$thrust" -gt 0 ] &&
            ! echo "$out" | grep -q "rror\|CRASH" && echo "kdswim: PASS" || echo "kdswim: FAIL" ;;
    kddie)    # the same, killed as it swims: its strokes weaken as a death's (its vigour under 0.2 at the end)
        out=$(drop vrtesthall "180 -256 200" 0 "vr_knockdown_debug 1;wait150;vr_knockdown_test 0;wait100;vr_knockdown_test 3;wait700")
        v=$(echo "$out" | grep "^ragswim" | tail -1 | grep -o " v [0-9.]*")
        echo "kddie: the last traced vigour$v"
        echo "$out" | grep -q "down alive in water" && echo "$v" | grep -q " v 0\.[01]" && ! echo "$out" | grep -q "rror\|CRASH" &&
            echo "kddie: PASS" || echo "kddie: FAIL" ;;
    land)     run "sv_random_seed ${SEED:-7};map vrfiringrange;$PRE;vr_test_spawn 0;vr_test_spawn_dist 110;impulse 241;wait40;vr_test_kill 1;wait600;toggleconsole;quit" | $PY "$SUM" "grunt land" ;;
    cost)
        for a in 1 0; do
            S="sv_random_seed ${SEED:-7};map vrtesthall;$PRE;vr_ragdoll_active_liquids $a;noclip;setpos 180 -256 200 0 0 0;wait5;vr_test_spawn 0"
            S="$S;vr_test_spawn_dist 130;impulse 241;wait2;vr_test_spawn_dist 170;impulse 241;wait2;vr_test_spawn_dist 210;impulse 241;wait2;vr_test_spawn_dist 250;impulse 241;wait150"
            S="$S;vr_ragdoll_active_list;vr_test_kill 1;wait1;vr_test_kill 1;wait1;vr_test_kill 1;wait1;vr_test_kill 1;wait4;vr_physics_steptime;wait150;vr_physics_steptime;vr_ragdoll_active_list;toggleconsole;quit"
            echo "-- vr_ragdoll_active_liquids $a"
            bash $KIT/run.sh --exclusive $AGENT -Script "$S" -Filter "vr_ragdoll_active_list|steptime|rror|CRASH|exit=" -Timeout 300 2>&1 | tr -d '\r' | grep -v "^$"
        done ;;
    esac
done
