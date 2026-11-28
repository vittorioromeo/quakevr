#!/bin/bash
# seat_padlabels_test.sh <agent> [out.png] -- split21 labels (docs/vr-port/SPLITSCREEN.md section 30; Quake/vr/vr_seatmenu.cpp
# vr_seat_padlabels_test): the controller layout's labels (the join screen's, a seat's Controls page) show what each
# button really does (its flat key, else its bind): padflat's layout for a fresh seat; seat 2's own flat-key button on its
# Controls page after it moves one, seat 1's unchanged. Headless, fast mode, flatscreen, coop e1m1 with maxplayers 4.
# Prints the PASS/FAIL lines and "seat_padlabels_test: N passed, M failed"; exits 1 on a failure. The pictures (the join
# screen's layout, seat 2's Controls page before and after) composed into scratch/<out.png> when given.
AGENT=${1:?agent}
OUT=${2:-}
KIT=C:/OHWorkspace/qvr-kit
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch
mkdir -p "$S"
LOG=$S/seat_padlabels_test.log
OUTARG=()
if [ -n "$OUT" ]; then OUTARG=(-Out "$OUT" -Clean); fi
bash $KIT/run.sh $AGENT "${OUTARG[@]}" -ExtraArgs "-flatscreenmode" -QuitOn "seat_padlabels_test: [0-9]+ passed" -Timeout 300 \
    -Filter "^seat_padlabels_test|seat_join|Host_Error|SV_Error|Sys_Error|ENGINE CRASH" \
    -Script "volume 0; bgmvolume 0; deathmatch 0; coop 1; maxplayers 4; vr_seat_padlabels_test; map e1m1" > "$LOG" 2>&1
grep -a "seat_padlabels_test\|Host_Error\|SV_Error\|Sys_Error\|ENGINE CRASH" "$LOG"
V=$(grep -a "seat_padlabels_test: [0-9]* passed" "$LOG" | tail -1)
if [ -z "$V" ]; then echo "seat_padlabels_test: FAIL no verdict (see $LOG)"; exit 1; fi
echo "$V" | grep -q " 0 failed"
