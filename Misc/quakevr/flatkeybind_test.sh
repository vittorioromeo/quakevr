#!/bin/bash
# flatkeybind_test.sh <agent> -- the Flat Keys menu page (Locomotion > Desktop Screen Play > Flat Keys; vr_menu.cpp Item::Key),
# flat-screen mode, headless, fast mode. One run, ending on its own "flatkeybind: done" line (-QuitOn):
#   rebind   Enter on the Drop row waits for a key (the Keys menu's "press a key"), a simulated key press (vr_mock_key k) sets
#            vr_flat_key_drop to "k"; Escape on a waiting row leaves it as it was
#   fires    with the new key held, flat play drops what the hand holds ("flatplay: k: drop"); the old key (h) does nothing
#   clear    Backspace on the Swap Hands row empties vr_flat_key_swap
#   reset    Reset Flat Keys puts every flat key back (vr_flat_key_drop h again)
#   flash    the flashlight key is a cvar now (vr_flat_key_flashlight, F by default): set to j, J switches it
# Prints "flatkeybind: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch/flatkeybind; mkdir -p "$S"
N=$(bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "map e1m1; wait10; menu_vr list; toggleconsole; quit" -Filter "Flat Keys" -Timeout 90 2>&1 | grep -a "Flat Keys" | head -1 | awk '{print $1}')
check $([ -n "$N" ] && echo 1 || echo 0) "the Flat Keys page is in the menu tree (page $N)"
FILTER='^("vr_flat_key|flatplay: |flatkeybind|menu_vr pos)'
PRE="map e1m1; wait40; developer 1; vr_flatplay_debug 1; god; notarget; impulse 152; wait10"
RB="menu_vr $N Drop; wait3; vr_mock_key enter; wait2; vr_mock_key k; wait2; vr_flat_key_drop"
ESC="menu_vr $N Reload; wait3; vr_mock_key enter; wait2; vr_mock_key escape; wait2; vr_flat_key_reload"
FIRE="vr_mock_key escape; wait5; vr_mock_key k down; wait4; vr_mock_key k up; wait4; echo flatkeybind: after k; impulse 152; wait10; vr_mock_key h down; wait4; vr_mock_key h up; wait4; echo flatkeybind: after h"
CLR="menu_vr $N Swap; wait3; vr_mock_key backspace; wait2; vr_flat_key_swap"
RST="menu_vr $N Reset; wait3; vr_mock_key enter; wait2; vr_flat_key_drop"
FL="vr_mock_key escape; wait5; vr_flat_key_flashlight j; wait2; vr_mock_key j down; wait4; vr_mock_key j up; wait4; echo flatkeybind: after j"
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "$PRE; $RB; $ESC; $FIRE; $CLR; $RST; $FL; echo flatkeybind: done; toggleconsole; quit" -QuitOn "^flatkeybind: done" -Timeout 150 -Filter "$FILTER" > "$S/run.log" 2>&1
L=$S/run.log
check $(grep -aq '"vr_flat_key_drop" is "k"' "$L" && echo 1 || echo 0) "rebind: Enter then k sets vr_flat_key_drop to k"
check $(grep -aq '"vr_flat_key_reload" is "r"' "$L" && echo 1 || echo 0) "rebind: Escape on a waiting row leaves vr_flat_key_reload as it was"
check $(awk '/flatkeybind: after k/{exit} /flatplay: k: drop/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "fires: k drops what the hand holds"
check $(awk '/flatkeybind: after k/{a=1} a && /flatplay: h: drop/{f=1} END{exit f}' "$L" && echo 1 || echo 0) "fires: the old key h does nothing"
check $(grep -aq '"vr_flat_key_swap" is ""' "$L" && echo 1 || echo 0) "clear: Backspace empties vr_flat_key_swap"
check $(awk '/is "k"/{k=1} k && /"vr_flat_key_drop" is "h"/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "reset: Reset Flat Keys puts drop back to h"
check $(awk '/flatkeybind: after j/{exit} /flatplay: j: the flashlight/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "flash: j switches the flashlight once vr_flat_key_flashlight is j"
verdict; exit $fail
