#!/bin/bash
# stickclick_test.sh <agent> -- one stick click is one press (vr_stick_click_debounce; vr_input.cpp debounceStickClicks;
# ROUND25.md "comfort2"): the presses the mock off hand's stick click passes on to its key (LTHUMB, +speed as shipped:
# each press a switch of the speed toggle) in e1m1. 1. A clean click (0.14 s): one press. 2. His controllers' bounce
# (NOTES.md vrfiringrange_2026-11-21_00-23-29: let go, a press a frame later for 2 frames): still one press, one bounce
# swallowed. 3. Debounce off, the same bouncy click: two presses (the cause: the toggle switched twice, back where it
# began). 4. Two deliberate clicks 0.3 s apart: both count. (The keys' commands would run only after the script, which
# holds the command buffer, so the presses are counted where they leave the debounce.) The waits are counted frames at
# 72 a second (the window 0.08 s = 5.8 frames). Ends "stickclick: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
DN="vr_mock_button off stickclick 1"; UP="vr_mock_button off stickclick 0"
CLEAN="$DN;wait10;$UP;wait20"
BOUNCY="$DN;wait10;$UP;wait;$DN;wait2;$UP;wait20"
S="map e1m1;vr_doubletap_test ready;god;notarget;vr_stick_click_debounce 0.08;vr_doubletap_test state start;"
S="${S}$CLEAN;vr_doubletap_test state clean;"
S="${S}$BOUNCY;vr_doubletap_test state bouncy;"
S="${S}vr_stick_click_debounce 0;$BOUNCY;vr_doubletap_test state off;vr_stick_click_debounce 0.08;"
S="${S}$DN;wait10;$UP;wait12;$DN;wait10;$UP;wait20;vr_doubletap_test state two;"
OUT=$(bash $KIT/run.sh $AGENT -Script "${S}toggleconsole;quit" -Timeout 240 -Filter "^doubletap state|rror|CRASH" 2>&1)
grep -E "CRASH|rror|state ready" <<<"$OUT"
st() { grep "^doubletap state $1:" <<<"$OUT" | head -1; }
field() { st "$1" | awk -v n="$2" '{print $n}'; }
P0=$(field start 13); B0=$(field start 15)
d() { echo $(( $(field "$1" "$2") - ${3:-0} )); }
check $([ "$(d clean 13 "$P0")" = 1 ] && echo 1 || echo 0) "a clean click: one press ($(st clean))"
check $([ "$(d bouncy 13 "$P0")" = 2 ] && [ "$(d bouncy 15 "$B0")" = 1 ] && echo 1 || echo 0) "a bouncing click: one press, the bounce ignored ($(st bouncy))"
check $([ "$(d off 13 "$P0")" = 4 ] && [ "$(d off 15 "$B0")" = 1 ] && echo 1 || echo 0) "debounce off: the bounce is a second press ($(st off))"
check $([ "$(d two 13 "$P0")" = 6 ] && echo 1 || echo 0) "two clicks 0.3 s apart: both count ($(st two))"
verdict; exit $fail
