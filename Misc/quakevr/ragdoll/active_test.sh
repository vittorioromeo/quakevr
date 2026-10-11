#!/bin/bash
# active_test.sh <agent> [cases] -- active ragdolls (ROUND22.md, "Active ragdolls: core and deaths"; vr_ragdoll_active):
# the first monster set (grunt, enforcer, knight, death knight, ogre; MONS="0 8" to pick: vr_test_spawn's numbers) on
# vrfiringrange, each standing 110 units ahead, unaware of you (notarget), killed by a shot of yours (vr_test_kill), the
# same scene each time (vr_ragdoll_active_seed). Each case prints, per monster, the frame it went limp in and a summary
# of its `ragtrace` (vr_ragdoll_active_trace 3; active_trace.py): the joints' error from the death animation per channel
# while strong (deg), when the pelvis reached the floor, how long it was driven, the most energy and spin after 1.5 s
# (no explosions, no jitter), its pelvis's travel; NaN or a crash fails.
#   deaths   a body shot (vr_test_kill 1)
#   head     a headshot (2): a collapse (strength ~0 from the start)
#   wound    a body (1), arm (3) and leg (4) shot with vr_ragdoll_active_wound 1: which hand holds it and how near
#   crawl    vr_ragdoll_active_crawl 1 (a body shot): whether he crawls and how far
#   stumble  he walks at you (no notarget, he sees you: grunts shoot, god mode) and dies on the move: carried, a step
#   off      vr_ragdoll_active 0: today's limp ragdoll (limp at Go Limp At's frame), the same summary
#   cost     8 grunts killed at once (vr_ragdoll_max 8): the drive's ms a frame (vr_ragdoll_active_list) and the
#            physics step's (vr_physics_steptime), with vr_ragdoll_active 1 and 0
#   zombie   a zombie's headshot (vr_decap_zombies): beheaded, a collapse
# EYES=1: contact sheets (the kit's scratch: active_<monster>.png): the active death's frames 0.15 .. 1.2 s and at rest
# (top row) over the limp one's (bottom row), cropped round the body.
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
CASES=${*:-deaths head wound crawl stumble off cost zombie}
MONS=${MONS:-0 8 5 6 1}
PY=${PY:-py -3.13}
SUM="$TREE/Misc/quakevr/ragdoll/active_trace.py"
name() { case $1 in 0) echo grunt;; 1) echo ogre;; 2) echo zombie;; 5) echo knight;; 6) echo hknight;; 8) echo enforcer;; *) echo m$1;; esac; }
PRE="wait30;god;vr_debug_ragdoll 1;vr_ragdoll_active_seed ${SEED:-0};vr_ragdoll_active_trace 3"
FILTER="^ragtrace|limp at frame|active death|test kill|rror|CRASH|exit=|TIMEOUT|vr_ragdoll_active_list|steptime"
run() { bash $KIT/run.sh $AGENT -Script "$1" -Filter "$FILTER" -Timeout 300 ${2:+-Clean} 2>&1 | tr -d '\r'; }
one() { # one <monster> <kill> <extra settings> [notarget]
    local SP="vr_test_spawn $1;vr_test_spawn_dist ${DIST:-110};impulse 241;wait40"
    run "sv_random_seed ${SEED:-7};map vrfiringrange;$PRE;${4-notarget};$3;$SP;vr_test_kill $2;wait400;toggleconsole;quit"
}
for c in $CASES; do
    echo "== $c"
    case $c in
    deaths|head|wound|crawl|off)
        for m in $MONS; do
            case $c in
            deaths) one $m 1 "" | $PY "$SUM" "$(name $m)" ;;
            head) one $m 2 "" | $PY "$SUM" "$(name $m) head" ;;
            off) one $m 1 "vr_ragdoll_active 0" | $PY "$SUM" "$(name $m) off" ;;
            crawl) one $m 1 "vr_ragdoll_active_crawl 1;vr_ragdoll_active_crawl_time 3" | $PY "$SUM" "$(name $m) crawl" ;;
            wound) for k in 1 3 4; do one $m $k "vr_ragdoll_active_wound 1" | $PY "$SUM" "$(name $m) wound$k"; done ;;
            esac
        done ;;
    stumble)
        # Far enough to walk a while at you; killed as he comes (2 s on).
        for m in $MONS; do
            bash $KIT/run.sh $AGENT -Script "sv_random_seed ${SEED:-7};map vrfiringrange;$PRE;vr_test_spawn $m;vr_test_spawn_dist 400;impulse 241;wait150;vr_test_kill 1;wait400;toggleconsole;quit" \
                -Filter "$FILTER" -Timeout 300 2>&1 | tr -d '\r' | $PY "$SUM" "$(name $m) stumble"
        done ;;
    zombie)
        one 2 2 "vr_decap_zombies 1" | $PY "$SUM" "zombie head" ;;
    cost)
        for a in 1 0; do
            S="sv_random_seed ${SEED:-7};map vrfiringrange;wait30;god;notarget;vr_ragdoll_max 8;vr_ragdoll_active $a;vr_test_spawn 0"
            for d in 80 120 160 200 240 280 320 360; do S="$S;vr_test_spawn_dist $d;impulse 241;wait2"; done
            S="$S;wait40;vr_ragdoll_active_list"
            for i in 1 2 3 4 5 6 7 8; do S="$S;vr_test_kill 1;wait1"; done
            S="$S;wait4;vr_physics_steptime;wait60;vr_physics_steptime;vr_ragdoll_active_list;wait300;vr_physics_steptime;toggleconsole;quit"
            echo "-- vr_ragdoll_active $a"
            bash $KIT/run.sh --exclusive $AGENT -Script "$S" -Filter "vr_ragdoll_active_list|steptime|rror|CRASH|exit=" -Timeout 300 2>&1 | tr -d '\r' | grep -v "^$"
        done ;;
    esac
done
if [ -n "$EYES" ]; then
    SHOTS="$KIT/bases/$AGENT/qbase/quakevr/screenshots"
    for m in $MONS; do
        for a in 1 0; do
            S="sv_random_seed ${SEED:-7};map vrfiringrange;wait30;god;notarget;vr_ragdoll_active $a;vr_kill_push ${KP:-1};vr_ragdoll_active_seed ${SEED:-0};vr_test_spawn $m;vr_test_spawn_dist ${DIST:-110}"
            S="$S;impulse 241;wait40;vr_mock_camera 5.2 0.9 -3.2 9 90;wait3;vr_test_kill 1;wait11;screenshot"
            for w in 11 11 11 11 11 11 21; do S="$S;wait$w;screenshot"; done
            S="$S;wait300;screenshot;toggleconsole;quit"
            bash $KIT/run.sh $AGENT -Clean -Script "$S" -Filter "exit=" -Timeout 300 >/dev/null 2>&1
            OUT="$KIT/scratch/active_$(name $m)$([ $a = 0 ] && echo _limp).png"
            $PY "$TREE/Misc/quakevr/ragdoll/active_sheet.py" "$SHOTS" "$OUT" 9 && echo "sheet: $OUT"
        done
    done
fi
