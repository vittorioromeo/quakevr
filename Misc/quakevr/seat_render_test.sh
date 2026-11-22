#!/bin/bash
# seat_render_test.sh <agent> [shots folder] -- splitscreen phase 7 (docs/vr-port/SPLITSCREEN.md section 15;
# Quake/vr/vr_seatrender.cpp): 2, 3 and 4 seats in a coop game on e1m1 (maxplayers 4, flatscreen, fast mode), each seat
# drawn in its tile: the tiles as laid out, apart and covering the window, each drawn from its own seat's player, each
# showing its seat's marker and a picture of its own; then one seat drawn in the whole window again. Each run's
# screenshot composed into <shots folder>/split7_<n>seats.png (default: the worktree's scratch folder).
# Prints "seat_render_test: N passed, M failed" per run, then "seat_render_test.sh: N passed, M failed"; exits 1 on a
# failure.
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
WT=C:/OHWorkspace/qvr-agents/$AGENT
S=$WT/scratch
SHOTS=${2:-$S}
mkdir -p "$S"
P=0; F=0
for N in 2 3 4; do
    LOG=$S/seat_render_test_$N.log
    bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -QuitOn "seat_render_test: [0-9]+ passed" -Timeout 240 -Clean \
        -Out "$SHOTS/split7_${N}seats.png" \
        -Filter "^seat_render_test|^seat [0-9]|seat_join|Host_Error|SV_Error|Sys_Error|ENGINE CRASH" \
        -Script "deathmatch 0; coop 1; maxplayers 4; vr_seat_render_test $N; map e1m1" > "$LOG" 2>&1
    echo "== $N seats"
    grep -a "seat_render_test\|Host_Error\|SV_Error\|Sys_Error\|ENGINE CRASH" "$LOG"
    V=$(grep -a "seat_render_test: [0-9]* passed" "$LOG" | tail -1)
    if [ -z "$V" ]; then F=$((F + 1)); echo "seat_render_test: FAIL $N seats: no verdict (see $LOG)"; continue; fi
    P=$((P + $(echo "$V" | sed 's/.*: \([0-9]*\) passed.*/\1/')))
    F=$((F + $(echo "$V" | sed 's/.* \([0-9]*\) failed.*/\1/')))
done
echo "seat_render_test.sh: $P passed, $F failed"
[ $F -eq 0 ]
