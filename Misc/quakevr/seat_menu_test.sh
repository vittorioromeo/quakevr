#!/bin/bash
# seat_menu_test.sh <agent> [shots folder] [prefix] -- splitscreen phase 9 (docs/vr-port/SPLITSCREEN.md;
# Quake/vr/vr_seatmenu.cpp): menus per seat, headless, in fast mode, flatscreen. Two runs of e1m1 with maxplayers 4:
#   coop  one seat: pad 1's Start opens the full menu as before; seat 2 joins by a mock pad: its Start opens its own seat
#         menu (not the full menu), its pad drives it (cursor, sensitivity, invert), its stick walks no one meanwhile;
#         seat 1's pad opens the full menu, each pad drives its own seat's menu; the menu pauses coop (server time held);
#         the console stays seat 1's; scr_splitpause 0 runs on; scr_splitmenu 1 gives seat 2 the full menu; Leave Seat
#   dm    the same in deathmatch: no pause by default; scr_splitpause 2 pauses
# A picture of seat 2's menu in its tile per run: <shots folder>/<prefix>_<mode>.png (default: scratch, split9).
# Prints the PASS/FAIL lines and "seat_menu_test: N passed, M failed" per run, then "seat_menu_test.sh: N passed,
# M failed" over both; exits 1 on a failure.
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
WT=C:/OHWorkspace/qvr-agents/$AGENT
S=$WT/scratch
SHOTS=${2:-$S}
PREFIX=${3:-split9}
mkdir -p "$S"
P=0; F=0
for MODE in coop dm; do
    LOG=$S/seat_menu_test_$MODE.log
    if [ $MODE = coop ]; then RULES="deathmatch 0; coop 1"; else RULES="coop 0; deathmatch 1"; fi
    bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -QuitOn "seat_menu_test: [0-9]+ passed" -Timeout 240 -Clean \
        -Out "$SHOTS/${PREFIX}_$MODE.png" \
        -Filter "^seat_menu_test|^seat |seat_join|Host_Error|SV_Error|Sys_Error" \
        -Script "$RULES; maxplayers 4; vr_seat_menu_test; map e1m1" > "$LOG" 2>&1
    echo "== $MODE"
    grep -a "seat_menu_test\|Host_Error\|SV_Error\|Sys_Error" "$LOG"
    V=$(grep -a "seat_menu_test: [0-9]* passed" "$LOG" | tail -1)
    if [ -z "$V" ]; then F=$((F + 1)); echo "seat_menu_test: FAIL $MODE: no verdict (see $LOG)"; continue; fi
    P=$((P + $(echo "$V" | sed 's/.*: \([0-9]*\) passed.*/\1/')))
    F=$((F + $(echo "$V" | sed 's/.* \([0-9]*\) failed.*/\1/')))
done
echo "seat_menu_test.sh: $P passed, $F failed"
[ $F -eq 0 ]
