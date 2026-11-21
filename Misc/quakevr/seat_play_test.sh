#!/bin/bash
# seat_play_test.sh <agent> -- splitscreen phase 6 (docs/vr-port/SPLITSCREEN.md; Quake/vr/vr_seatplay.cpp): seats
# joining a running local game, headless, in fast mode, flatscreen. Two runs of e1m1 with maxplayers 4:
#   coop  seat 2 joins by a mock pad (its own slot, name, player; a save refused, the coop autosave allowed), walks by
#         its stick (seat 1 stays), is seen by seat 1 where the server has him (and sees seat 1), leaves (his slot
#         freed), joins again: his old slot, health 70 and 50 nails as he left
#   dm    the same in deathmatch: spawned apart, walks, seen, leaves, spawns again
# Prints the PASS/FAIL lines and "seat_play_test: N passed, M failed" per run, then "seat_play_test.sh: N passed,
# M failed" over both; exits 1 on a failure.
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
WT=C:/OHWorkspace/qvr-agents/$AGENT
S=$WT/scratch
mkdir -p "$S"
P=0; F=0
for MODE in coop dm; do
    LOG=$S/seat_play_test_$MODE.log
    if [ $MODE = coop ]; then RULES="deathmatch 0; coop 1"; else RULES="coop 0; deathmatch 1"; fi
    bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -QuitOn "seat_play_test: [0-9]+ passed" -Timeout 240 \
        -Filter "^seat_play_test|^seat |seat_join|Host_Error|SV_Error|Sys_Error" \
        -Script "$RULES; maxplayers 4; vr_seat_play_test; map e1m1; toggleconsole; quit" > "$LOG" 2>&1
    echo "== $MODE"
    grep -a "seat_play_test\|Host_Error\|SV_Error\|Sys_Error" "$LOG"
    V=$(grep -a "seat_play_test: [0-9]* passed" "$LOG" | tail -1)
    if [ -z "$V" ]; then F=$((F + 1)); echo "seat_play_test: FAIL $MODE: no verdict (see $LOG)"; continue; fi
    P=$((P + $(echo "$V" | sed 's/.*: \([0-9]*\) passed.*/\1/')))
    F=$((F + $(echo "$V" | sed 's/.* \([0-9]*\) failed.*/\1/')))
done
echo "seat_play_test.sh: $P passed, $F failed"
[ $F -eq 0 ]
