#!/bin/bash
# seat_split21_test.sh <agent> -- split21 (docs/vr-port/SPLITSCREEN.md section 30; Quake/vr/vr_seatmenu.cpp
# vr_seat_split21_test): the features of 11-27/28 each seat's own, headless, fast mode, flatscreen, coop e1m1 with
# maxplayers 4: seat 2's visor boots on its own; both seats reload at once (each its own trip and rounds); seat 2 holds a
# grenade from its back pouch while seat 1's hands hold none.
# Prints the PASS/FAIL lines and "seat_split21_test: N passed, M failed"; exits 1 on a failure.
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch
mkdir -p "$S"
LOG=$S/seat_split21_test.log
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -QuitOn "seat_split21_test: [0-9]+ passed" -Timeout 300 \
    -Filter "^seat_split21_test|seat_join|Host_Error|SV_Error|Sys_Error|ENGINE CRASH" \
    -Script "volume 0; bgmvolume 0; deathmatch 0; coop 1; maxplayers 4; vr_seat_split21_test; map e1m1" > "$LOG" 2>&1
grep -a "seat_split21_test\|Host_Error\|SV_Error\|Sys_Error\|ENGINE CRASH" "$LOG"
V=$(grep -a "seat_split21_test: [0-9]* passed" "$LOG" | tail -1)
if [ -z "$V" ]; then echo "seat_split21_test: FAIL no verdict (see $LOG)"; exit 1; fi
echo "$V" | grep -q " 0 failed"
