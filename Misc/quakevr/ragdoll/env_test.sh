#!/bin/bash
# env_test.sh <agent> [cases] -- active ragdolls and what is round them (ROUND22.md, "Active ragdolls: environment";
# vr_ragdoll_active_env): scripted deaths on vrtesthall (sv_random_seed, vr_ragdoll_active_seed: the same scene each
# time), each monster (MONS: vr_test_spawn's numbers, grunt and knight by default) killed by a body shot of yours
# (vr_test_kill 1) where the hall's geometry is round him. Each case prints per monster a line summing up its `ragtrace`
# (active_trace.py: errors, energy and spin once the drive let go, NaN or a crash fail) and one summing up its
# `ragenv` lines (env_trace.py: which hands caught, leaned, held or grabbed, for how long, the most load, how far the
# hold slid, why it let go; the pelvis's drop).
#   wall     shot towards the north wall (his back to it, GAP units from it): he leans on it and slides down
#   along    shot along the north wall (it at his side, 8 units off): a hand braces on it
#   ledge    killed at the platform's west lip (192 units up), his back to the drop, no kill push (vr_kill_push 0:
#            he topples over it): his hands grab the lip and he hangs, then drops; or they catch the floor
#   thrown   the same, the kill push on: thrown off past the lip's reach, the hands catch the floor (or he curls)
#   hit      the ledge case, a hand holding on struck (vr_ragdoll_env_hit) HITAT frames after the kill: it lets go
#   crate    shot towards a big crate (vr_test_spawn 108) behind him: he braces on it (no hold: a prop)
#   front    shot from behind in the open (facing away): falling forwards, the hands may catch the floor
#   open     shot in the open (no wall): nothing to hold (the control)
#   off      the wall case with vr_ragdoll_active_env 0 (today's stage 1 death)
#   cost     8 grunts in a row along the north wall killed at once: the drive's ms a frame with env 1 and 0
# EYES=1: a contact sheet per case and monster (the kit's scratch: env_<case>_<monster>.png), the frames 0.15 .. 2 s.
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
CASES=${*:-wall along ledge thrown hit crate front open off}
MONS=${MONS:-0 5}
GAP=${GAP:-24}
PY=${PY:-py -3.13}
SUM="$TREE/Misc/quakevr/ragdoll/active_trace.py"
ENV="$TREE/Misc/quakevr/ragdoll/env_trace.py"
name() { case $1 in 0) echo grunt;; 1) echo ogre;; 2) echo zombie;; 5) echo knight;; 6) echo hknight;; 8) echo enforcer;; *) echo m$1;; esac; }
PRE="wait30;god;notarget;vr_debug_ragdoll 1;vr_ragdoll_active_seed ${SEED:-0};vr_ragdoll_active_trace 3;${EXTRA:-}"
FILTER="^ragtrace|^ragenv|env_hit|limp at frame|active death|test kill|rror|CRASH|exit=|TIMEOUT|steptime|vr_ragdoll_active_list"
# Where the player stands (setpos x y z pitch yaw roll; he faces north unless turned: setpos's angles don't turn the
# headset's view) and how far ahead the monster is, per case.
place() {
    case $1 in
    wall|off) echo "setpos -150 $((640 - 16 - GAP - 110)) 24 0 90 0;vr_test_spawn_dist 110" ;;
    along) echo "setpos -270 616 24 0 0 0;vr_mock_turn_to 0;wait150;vr_test_spawn_dist 110" ;;
    ledge|hit) echo "vr_kill_push 0;setpos 430 430 216 0 180 0;vr_mock_turn_to 180;wait150;vr_test_spawn_dist ${LEDGE:-132}" ;;
    thrown) echo "setpos 430 430 216 0 180 0;vr_mock_turn_to 180;wait150;vr_test_spawn_dist ${LEDGE:-132}" ;;
    crate) echo "setpos -150 0 24 0 90 0;vr_test_spawn 108;vr_test_spawn_dist 150;impulse 241;wait5;vr_test_spawn_dist 105" ;;
    front) echo "setpos -150 0 24 0 90 0;vr_test_spawn_facing 2;vr_test_spawn_dist 110" ;;
    open) echo "setpos -150 0 24 0 90 0;vr_test_spawn_dist 110" ;;
    esac
}
one() { # one <case> <monster>
    local extra=""
    [ "$1" = off ] && extra="vr_ragdoll_active_env 0"
    local S="sv_random_seed ${SEED:-7};map vrtesthall;$PRE;$extra;$(place $1);vr_test_spawn $2;impulse 241;wait40"
    if [ "$1" = hit ]; then
        S="$S;vr_test_kill 1;wait${HITAT:-75};vr_ragdoll_env_hit;wait${LEN:-500}"
    else
        S="$S;vr_test_kill 1;wait${LEN:-500}"
    fi
    bash $KIT/run.sh $AGENT -Script "$S;toggleconsole;quit" -Filter "$FILTER" -Timeout 300 2>&1 | tr -d '\r'
}
for c in $CASES; do
    echo "== $c"
    case $c in
    cost)
        for a in 1 0; do
            S="sv_random_seed ${SEED:-7};map vrtesthall;wait30;god;notarget;vr_ragdoll_max 8;vr_ragdoll_active_env $a;vr_test_spawn 0;vr_test_spawn_dist 110"
            for x in -330 -270 -210 -150 -90 -30 30 80; do S="$S;setpos $x 474 24 0 90 0;wait2;impulse 241;wait2"; done
            S="$S;setpos -125 300 24 0 90 0;wait40;vr_ragdoll_active_list"
            for i in 1 2 3 4 5 6 7 8; do S="$S;vr_test_kill 1;wait1"; done
            S="$S;wait4;vr_physics_steptime;wait60;vr_physics_steptime;vr_ragdoll_active_list;wait300;vr_physics_steptime;toggleconsole;quit"
            echo "-- vr_ragdoll_active_env $a"
            bash $KIT/run.sh --exclusive $AGENT -Script "$S" -Filter "vr_ragdoll_active_list|steptime|rror|CRASH|exit=|^ragenv:" -Timeout 300 2>&1 | tr -d '\r' | grep -v "^$" | grep -v "^ragenv:"
        done ;;
    *)
        for m in $MONS; do
            OUT=$(one $c $m)
            echo "$OUT" | $PY "$SUM" "$(name $m) $c"
            echo "$OUT" | $PY "$ENV" "$(name $m) $c"
            [ -n "$VERBOSE" ] && echo "$OUT" | grep -v "^ragtrace\|^ragenv " | head -${VERBOSE}
            [ -n "$RAW" ] && echo "$OUT" >> "$RAW"
        done ;;
    esac
done
if [ -n "$EYES" ]; then
    # One sheet (the kit's scratch: env_sheet.png): a row per case and monster (EYECASES), its frames 0.1, 0.4, 0.8,
    # 1.4, 2.2 and 4 s after the kill, seen from his side (OVERLAY=1: vr_ragdoll_active_debug's lines over him).
    SHOTS="$KIT/bases/$AGENT/qbase/quakevr/screenshots"
    ROWS=""
    for c in ${EYECASES:-wall along ledge crate}; do
        for m in $MONS; do
            case $c in
            ledge|hit|thrown) CAM="vr_mock_camera 4.6 -1.2 -3.4 20 90" ;; # (to his side, the lip and the drop)
            *) CAM="vr_mock_camera 3.2 0.4 -2.9 4 90" ;;                 # (to his side: the wall or crate edge on)
            esac
            S="sv_random_seed ${SEED:-7};map vrtesthall;wait30;god;notarget;vr_ragdoll_active_seed ${SEED:-0};vr_ragdoll_active_debug ${OVERLAY:-0};${EXTRA:-};$(place $c);vr_test_spawn $m"
            S="$S;impulse 241;wait40;$CAM;wait3;vr_test_kill 1;wait7;screenshot"
            for w in 22 29 43 58 130; do S="$S;wait$w;screenshot"; done
            bash $KIT/run.sh $AGENT -Clean -Script "$S;toggleconsole;quit" -Filter "exit=" -Timeout 300 >/dev/null 2>&1
            D="$TREE/scratch/eyes_${c}_$(name $m)"
            mkdir -p "$D"
            cp "$SHOTS"/*.png "$D"/
            ROWS="$ROWS $D"
        done
    done
    $PY "$TREE/Misc/quakevr/ragdoll/env_sheet.py" "$KIT/scratch/env_sheet.png" 6 $ROWS && echo "sheet: $KIT/scratch/env_sheet.png"
fi
