#!/bin/bash
# seat_render_test.sh <agent> [shots folder] [prefix] -- splitscreen phase 7 (docs/vr-port/SPLITSCREEN.md section 15;
# Quake/vr/vr_seatrender.cpp): 2, 3 and 4 seats in a coop game on e1m1 (maxplayers 4, flatscreen, fast mode), each seat
# drawn in its tile: the tiles as laid out, apart and covering the window, each drawn from its own seat's player, each
# showing its seat's marker and a picture of its own; then one seat drawn in the whole window again. Each run's
# screenshot composed into <shots folder>/<prefix>_<n>seats.png (default: the worktree's scratch folder, split7).
# Sound on at volume 0 (the per-seat listeners checked: no sound out).
# Prints "seat_render_test: N passed, M failed" per run, then "seat_render_test.sh: N passed, M failed"; exits 1 on a
# failure.
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
WT=C:/OHWorkspace/qvr-agents/$AGENT
S=$WT/scratch
SHOTS=${2:-$S}
PREFIX=${3:-split7}
mkdir -p "$S"
P=0; F=0
for N in 2 3 4; do
    LOG=$S/seat_render_test_$N.log
    bash $KIT/run.sh $AGENT -Sound -ExtraArgs "-flatscreenmode" -QuitOn "seat_render_test: [0-9]+ passed" -Timeout 240 -Clean \
        -Out "$SHOTS/${PREFIX}_${N}seats.png" \
        -Filter "^seat_render_test|^seat [0-9]|seat_join|Host_Error|SV_Error|Sys_Error|ENGINE CRASH" \
        -Script "volume 0; bgmvolume 0; deathmatch 0; coop 1; maxplayers 4; vr_seat_render_test $N; map e1m1" > "$LOG" 2>&1
    echo "== $N seats"
    grep -a "seat_render_test\|Host_Error\|SV_Error\|Sys_Error\|ENGINE CRASH" "$LOG"
    V=$(grep -a "seat_render_test: [0-9]* passed" "$LOG" | tail -1)
    if [ -z "$V" ]; then F=$((F + 1)); echo "seat_render_test: FAIL $N seats: no verdict (see $LOG)"; continue; fi
    P=$((P + $(echo "$V" | sed 's/.*: \([0-9]*\) passed.*/\1/')))
    F=$((F + $(echo "$V" | sed 's/.* \([0-9]*\) failed.*/\1/')))
done
# Part 3: 2 seats on e1m6 (torches by every start): each seat's own torch lights, chosen by its own eye.
LOG=$S/seat_render_test_torches.log
bash $KIT/run.sh $AGENT -Sound -ExtraArgs "-flatscreenmode" -QuitOn "seat_render_test: [0-9]+ passed" -Timeout 240 -Clean \
    -Out "$SHOTS/${PREFIX}_torches.png" \
    -Filter "^seat_render_test|^seat [0-9]|seat_join|Host_Error|SV_Error|Sys_Error|ENGINE CRASH" \
    -Script "volume 0; bgmvolume 0; deathmatch 0; coop 1; maxplayers 4; vr_seat_render_test 2 torches; map e1m6" > "$LOG" 2>&1
echo "== 2 seats, torches (e1m6)"
grep -a "seat_render_test\|Host_Error\|SV_Error\|Sys_Error\|ENGINE CRASH" "$LOG"
V=$(grep -a "seat_render_test: [0-9]* passed" "$LOG" | tail -1)
if [ -z "$V" ]; then F=$((F + 1)); echo "seat_render_test: FAIL torches: no verdict (see $LOG)"; else
    P=$((P + $(echo "$V" | sed 's/.*: \([0-9]*\) passed.*/\1/')))
    F=$((F + $(echo "$V" | sed 's/.* \([0-9]*\) failed.*/\1/')))
fi
# split18: 2 seats at 144 frames a second (the test sets it): a grunt and its ragdoll drawn alike in both passes, no pose
# flicker (anim); the same nail pistol in both seats' main hands, then in different hands, seat 1 firing (guns).
for MODE in anim guns; do
    LOG=$S/seat_render_test_$MODE.log
    bash $KIT/run.sh $AGENT -Sound -ExtraArgs "-flatscreenmode" -QuitOn "seat_render_test: [0-9]+ passed" -Timeout 240 -Clean \
        -Out "$SHOTS/${PREFIX}_$MODE.png" \
        -Filter "^seat_render_test|^seat [0-9]|seat_join|Host_Error|SV_Error|Sys_Error|ENGINE CRASH" \
        -Script "volume 0; bgmvolume 0; deathmatch 0; coop 1; maxplayers 4; vr_seat_render_test 2 $MODE; map e1m1" > "$LOG" 2>&1
    echo "== 2 seats, $MODE"
    grep -a "seat_render_test\|Host_Error\|SV_Error\|Sys_Error\|ENGINE CRASH" "$LOG"
    V=$(grep -a "seat_render_test: [0-9]* passed" "$LOG" | tail -1)
    if [ -z "$V" ]; then F=$((F + 1)); echo "seat_render_test: FAIL $MODE: no verdict (see $LOG)"; continue; fi
    P=$((P + $(echo "$V" | sed 's/.*: \([0-9]*\) passed.*/\1/')))
    F=$((F + $(echo "$V" | sed 's/.* \([0-9]*\) failed.*/\1/')))
done
echo "seat_render_test.sh: $P passed, $F failed"
[ $F -eq 0 ]
