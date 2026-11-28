#!/bin/bash
# seat_bind_test.sh <agent> [shots folder] -- split20 (docs/vr-port/SPLITSCREEN.md section 29; Quake/vr/vr_seatmenu.cpp
# vr_seat_bind_test): each seat's own controller bindings, headless, fast mode, flatscreen, coop e1m1 with maxplayers 4.
#   edit   seat 2 joins by a mock pad, opens its seat menu, Controller Bindings: its own screen; X, d-pad, X swaps two
#          buttons: seat 2's swapped, seat 1's unchanged, seat 2's bindings panel shows it; writeconfig seatbind_test
#   check  a restart: exec seatbind_test.cfg (seat_bind lines), seat 2 joins again with the swap (against a fresh copy)
# A picture of seat 2's screen in its tile: <shots folder>/split20_bind.png (default: scratch).
# Prints the PASS/FAIL lines and "seat_bind_test: N passed, M failed" per run, then "seat_bind_test.sh: N passed,
# M failed" over both; exits 1 on a failure.
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
WT=C:/OHWorkspace/qvr-agents/$AGENT
S=$WT/scratch
SHOTS=${2:-$S}
mkdir -p "$S"
P=0; F=0
for MODE in edit check; do
    LOG=$S/seat_bind_test_$MODE.log
    if [ $MODE = edit ]; then PRE=""; ARG=""; OUT="-Out $SHOTS/split20_bind.png"; else PRE="exec seatbind_test.cfg;"; ARG=" check"; OUT=""; fi
    bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -QuitOn "seat_bind_test: [0-9]+ passed" -Timeout 240 -Clean $OUT \
        -Filter "^seat_bind_test|seat_join|Host_Error|SV_Error|Sys_Error" \
        -Script "$PRE deathmatch 0; coop 1; maxplayers 4; vr_seat_bind_test$ARG; map e1m1" > "$LOG" 2>&1
    echo "== $MODE"
    grep -a "seat_bind_test\|Host_Error\|SV_Error\|Sys_Error" "$LOG"
    V=$(grep -a "seat_bind_test: [0-9]* passed" "$LOG" | tail -1)
    if [ -z "$V" ]; then F=$((F + 1)); echo "seat_bind_test: FAIL $MODE: no verdict (see $LOG)"; continue; fi
    P=$((P + $(echo "$V" | sed 's/.*: \([0-9]*\) passed.*/\1/')))
    F=$((F + $(echo "$V" | sed 's/.* \([0-9]*\) failed.*/\1/')))
done
echo "seat_bind_test.sh: $P passed, $F failed"
[ $F -eq 0 ]
