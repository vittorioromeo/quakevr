#!/bin/bash
# flatkeybind_test.sh <agent> -- the flat keys in Options > Controls (menu.c: the "Quake VR (flat screen)" group of the keys
# menu; vr_flatmove.cpp VR_FlatKeyGet/Set), flat-screen mode, headless, fast mode. One run, ending on its own
# "flatkeybind: done" line (-QuitOn):
#   link     VR menu > Locomotion > "Flat Keys (Options > Controls)" opens Controls on the group's first row (Lean left)
#   rebind   Enter on the Drop row waits for a key (the Keys menu's "press a key"), a simulated key press (vr_mock_key k) sets
#            vr_flat_key_drop to "k"; Escape on a waiting row leaves it as it was
#   clash    k also bound (impulse 10): both rows name the clash and the rule ("in flat play: Drop, not Next weapon")
#   fires    with the new key pressed in flat play, the flat key wins: it drops what the hand holds ("flatplay: k: drop");
#            the old key (h) does nothing
#   clear    Backspace on the Swap hands row empties vr_flat_key_swap
#   reset    VR menu > Locomotion > Reset Flat Keys puts every flat key back (vr_flat_key_drop h again)
#   gear     Controls' Gear lights row (vr_flat_key_gearlights, 9, no clash); 9 pressed twice dims and lights the gear
#   numkeys  Controls' Cycle left hand row (vr_flat_key_cycle_left, 1: his 2026-11-22 holster keys) names its clash with
#            the Axe's bind; 1 pressed cycles the left hand; with vr_flat_holsters 0 it is not the flat key's (Quake's bind)
# Prints "flatkeybind: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch/flatkeybind; mkdir -p "$S"
N=19 # Locomotion (vr_menu_table.inc)
FILTER='^("vr_flat_key|"vr_gear_lights"|flatplay: |flatkeybind|menu_keys)'
PRE="map e1m1; wait40; developer 1; vr_flatplay_debug 1; god; notarget; impulse 152; wait10"
LINK="menu_vr $N Flat Keys; wait3; vr_mock_key enter; wait2"
RB="menu_keys vr_flat_key_drop; wait2; vr_mock_key enter; wait2; vr_mock_key k; wait2; vr_flat_key_drop"
ESC="menu_keys vr_flat_key_reload; wait2; vr_mock_key enter; wait2; vr_mock_key escape; wait2; vr_flat_key_reload"
CL="bind k \"impulse 10\"; menu_keys vr_flat_key_drop; wait2; menu_keys \"impulse 10\"; wait2; unbind k"
FIRE="togglemenu; togglemenu; wait5; vr_mock_key k down; wait4; vr_mock_key k up; wait4; echo flatkeybind: after k; impulse 152; wait10; vr_mock_key h down; wait4; vr_mock_key h up; wait4; echo flatkeybind: after h"
CLR="menu_keys vr_flat_key_swap; wait2; vr_mock_key backspace; wait2; vr_flat_key_swap"
RST="menu_vr $N Reset Flat Keys; wait3; vr_mock_key enter; wait2; vr_flat_key_drop"
GEAR="menu_keys vr_flat_key_gearlights; wait2; togglemenu; togglemenu; wait5; vr_gear_lights 1; vr_mock_key 9 down; wait4; vr_mock_key 9 up; wait4; echo flatkeybind: gear1; vr_gear_lights; vr_mock_key 9 down; wait4; vr_mock_key 9 up; wait4; echo flatkeybind: gear2; vr_gear_lights"
NUM="menu_keys vr_flat_key_cycle_left; wait2; togglemenu; togglemenu; wait5; vr_mock_key 1 down; wait4; vr_mock_key 1 up; wait4; echo flatkeybind: num1; vr_flat_holsters 0; vr_mock_key 1 down; wait4; vr_mock_key 1 up; wait4; echo flatkeybind: num0; vr_flat_holsters 1"
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "$PRE; $LINK; $RB; $ESC; $CL; $FIRE; $CLR; $RST; $GEAR; $NUM; echo flatkeybind: done; toggleconsole; quit" -QuitOn "^flatkeybind: done" -Timeout 150 -Filter "$FILTER" > "$S/run.log" 2>&1
L=$S/run.log
check $(grep -aq 'menu_keys: at "Lean left" (vr_flat_key_lean_left)' "$L" && echo 1 || echo 0) "link: the VR menu's Flat Keys button opens Controls on Lean left"
check $(grep -aq '"vr_flat_key_drop" is "k"' "$L" && echo 1 || echo 0) "rebind: Enter then k on Controls' Drop row sets vr_flat_key_drop to k"
check $(grep -aq '"vr_flat_key_reload" is "r"' "$L" && echo 1 || echo 0) "rebind: Escape on a waiting row leaves vr_flat_key_reload as it was"
check $(grep -aq 'menu_keys: at "Drop" (vr_flat_key_drop) ! k in flat play: Drop, not Next weapon' "$L" && echo 1 || echo 0) "clash: the flat key's row names the bind it beats"
check $(grep -aq 'menu_keys: at "Next weapon" (impulse 10) ! k in flat play: Drop, not Next weapon' "$L" && echo 1 || echo 0) "clash: the bind's row names the flat key that beats it"
check $(awk '/flatkeybind: after k/{exit} /flatplay: k: drop/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "fires: k drops what the hand holds"
check $(awk '/flatkeybind: after k/{a=1} a && /flatplay: h: drop/{f=1} END{exit f}' "$L" && echo 1 || echo 0) "fires: the old key h does nothing"
check $(grep -aq '"vr_flat_key_swap" is ""' "$L" && echo 1 || echo 0) "clear: Backspace empties vr_flat_key_swap"
check $(awk '/is "k"/{k=1} k && /"vr_flat_key_drop" is "h"/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "reset: Reset Flat Keys puts drop back to h"
check $(grep -aq 'menu_keys: at "Gear lights" (vr_flat_key_gearlights)\s*$' "$L" && echo 1 || echo 0) "gear: Controls has the Gear lights row, its key 9 clashing with no bind"
check $(awk '/flatkeybind: gear1/{exit} /flatplay: 9: gear lights/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "gear: 9 in flat play is the gear lights' key"
check $(awk '/flatkeybind: gear1/{a=1; next} /flatkeybind: gear2/{exit} a && /"vr_gear_lights" is "0"/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "gear: the first press dims the gear (vr_gear_lights 0)"
check $(awk '/flatkeybind: gear2/{a=1; next} a && /"vr_gear_lights" is "1"/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "gear: the second press lights it again (vr_gear_lights 1)"
check $(grep -aq 'menu_keys: at "Cycle left hand" (vr_flat_key_cycle_left) !.*Cycle left hand, not Axe' "$L" && echo 1 || echo 0) "numkeys: Controls' Cycle left hand row on 1, its clash with the Axe's bind named"
check $(awk '/flatkeybind: num1/{exit} /flatplay: 1: cycle the left hand/{f=1} END{exit !f}' "$L" && echo 1 || echo 0) "numkeys: 1 in flat play cycles the left hand"
check $(awk '/flatkeybind: num1/{a=1; next} /flatkeybind: num0/{exit} a && /flatplay: 1: cycle/{f=1} END{exit f}' "$L" && echo 1 || echo 0) "numkeys: with vr_flat_holsters 0, 1 is not the flat key's"
verdict; exit $fail
