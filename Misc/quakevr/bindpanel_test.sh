#!/bin/bash
# bindpanel_test.sh <agent> -- the bindings reminder panels (vr_bindpanel.cpp; ROUND26 "wheelui", "bindpanel2").
# Flat (-flatscreenmode): its rows and their keys as bound now (the defaults: R reloads, C and CTRL crouch, LMB fires, 1
# and 2 hold the wheel), shown in flat play; the main menu's panel (a screenshot) and its Setup Bindings
# (vr_bindpanel_setup: Options > Controls at the flat keys while no controller screen opens); with a pad the device in
# use (vr_bindpanel_device xboxone / playstation: as if one were connected) the rows are its bound buttons with that pad's
# glyphs (Kenney's Xbox ones; our drawn PlayStation ones), the main menu's too (a second screenshot).
# VR (the mock headset): the weapon wheel's panel for a Quest 3 and an Index: both hands' pictures, every bound button a
# callout with its glyph (quest_*, index_*) and its action (weaponwheel_test.sh checks it is drawn over the open wheel).
# Screenshots: scratch/bindpanel/main.png (keyboard, then a pad: composed). Ends "bindpanel_test: N passed, M failed".
AGENT=${1:?agent}
QT_NAME=bindpanel_test
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/bindpanel
mkdir -p "$S"
SCRIPT="developer 1; map vrtesthall; wait30; vr_bindpanel_report; togglemenu; wait30; screenshot; vr_bindpanel_report; vr_bindpanel_setup; wait5; vr_bindpanel_report; vr_bindpanel_device xboxone; menu_main; wait30; screenshot; echo bindpanel_pad; vr_bindpanel_report; vr_bindpanel_device playstation; echo bindpanel_ps; vr_bindpanel_report; vr_bindpanel_device auto; echo bindpanel_test_end; toggleconsole; quit"
bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode -Script "$SCRIPT" -QuitOn "^bindpanel_test_end" -Timeout 120 \
    -Filter "^(bindpanel)" -Out "$S/main.png" > "$S/run.log" 2>&1
L=$(cat "$S/run.log")
has() { grep -qE "$1" <<<"$L" && echo 1 || echo 0; }
check $(has "^bindpanel: 14 rows, shown 1, menu none, device keyboard") "fourteen rows, shown in flat play, no menu, the keyboard"
check $(has '^bindpanel: row 0 "Fire" keys "(LMB|CTRL)"') "fire on LMB"
check $(has '^bindpanel: row 2 "Reload" keys "R" ""') "reload on R"
check $(has '^bindpanel: row 3 "Wheel \(hold\)" keys "1" "2"') "the wheel on 1 and 2"
check $(has '^bindpanel: row 12 "Crouch / slide" keys "C" "CTRL"') "crouch on C and CTRL (two keys)"
check $(has '^bindpanel: row 1 "Aim / off hand" keys "RMB" ""') "no gamepad key listed"
check $(has "^bindpanel: 14 rows, shown 1, menu keys") "Setup Bindings opens Options > Controls"
check $(has "^bindpanel: Setup Bindings: Options > Controls") "Setup Bindings said where it went"
# A pad (bindpanel2): its part of the log, between bindpanel_pad and bindpanel_ps.
P=$(sed -n '/^bindpanel_pad/,/^bindpanel_ps/p' "$S/run.log")
pad() { grep -qE "$1" <<<"$P" && echo 1 || echo 0; }
check $(pad '^bindpanel: row [0-9]+ "[^"]+" keys "RT" "" glyph "xbox_rt"') "an Xbox pad: RT's row with Kenney's Xbox glyph"
check $(pad '^bindpanel: ([4-9]|1[0-6]) rows, shown [01], menu main, device xboxone, glyphs \1$') "an Xbox pad: its bound buttons as rows, each a glyph"
check $(pad '^bindpanel: main menu panel drawn: ([4-9]|1[0-6]) rows') "the main menu's panel drawn with the pad's rows"
check $(has '^bindpanel: row [0-9]+ "[^"]+" keys "R2" "" glyph "drawn_playstation_rt"') "a PlayStation pad: its own (drawn) glyphs"
# VR: the headset's panel for a Quest 3 and an Index.
SCRIPT="developer 1; map vrtesthall; wait30; vr_bindpanel_device quest3; echo bindpanel_q3; vr_bindpanel_report; vr_bindpanel_device index; echo bindpanel_index; vr_bindpanel_report; vr_bindpanel_device auto; echo bindpanel_test_end; toggleconsole; quit"
bash $KIT/run.sh $AGENT -Script "$SCRIPT" -QuitOn "^bindpanel_test_end" -Timeout 120 -Filter "^(bindpanel)" > "$S/run_vr.log" 2>&1
Q=$(sed -n '/^bindpanel_q3/,/^bindpanel_index/p' "$S/run_vr.log")
I=$(sed -n '/^bindpanel_index/,/^bindpanel_test_end/p' "$S/run_vr.log")
q3() { grep -qE "$1" <<<"$Q" && echo 1 || echo 0; }
ix() { grep -qE "$1" <<<"$I" && echo 1 || echo 0; }
check $(q3 '^bindpanel: headset panel on: quest3, pictures 2, callouts ([0-9]+), glyphs \1;') "Quest 3: both hands' pictures, every callout a glyph"
check $(q3 '^bindpanel: callout [0-9]+ hand 1 "Trigger" glyph "quest_trigger_right" "') "Quest 3: the right trigger's callout, Kenney's Quest glyph"
check $(q3 '^bindpanel: callout [0-9]+ hand 0 "X" glyph "quest_button_x" "') "Quest 3: the left hand's X"
check $(ix '^bindpanel: headset panel on: index, pictures 2, callouts ([0-9]+), glyphs \1;') "Index: both hands' pictures, every callout a glyph"
check $(ix '^bindpanel: callout [0-9]+ hand 1 "Trigger" glyph "index_rt" "') "Index: the right trigger, Kenney's Index glyph"
check $(grep -q "(missing)" "$S/run_vr.log" && echo 0 || echo 1) "no glyph missing"
verdict
exit $fail
