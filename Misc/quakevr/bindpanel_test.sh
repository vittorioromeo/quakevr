#!/bin/bash
# bindpanel_test.sh <agent> -- the bindings reminder panels (vr_bindpanel.cpp; ROUND26 "wheelui", "bindpanel2").
# Flat (-flatscreenmode): its rows and their keys as bound now (the defaults: R reloads, C and CTRL crouch, LMB fires, 1
# and 2 hold the wheel), shown in flat play; the main menu's panel (a screenshot) and its Setup Bindings
# (vr_bindpanel_setup: Options > Controls at the flat keys while no controller screen opens); with a pad the device in
# use (vr_bindpanel_device xboxone / playstation: as if one were connected) the rows are its bound buttons with that pad's
# glyphs (Kenney's Xbox ones; our drawn PlayStation ones), the main menu's too (a second screenshot).
# VR (the mock headset): the weapon wheel's panel for a Quest 3 and an Index: both hands' pictures, every bound button a
# callout with its glyph (quest_*, index_*) and its action (weaponwheel_test.sh checks it is drawn over the open wheel).
# panelfix: twenty rows (the hands' number keys, next ammo, attach, lie down: minor, in groups); the menus' panel on
# Options too, above the bottom corner's rows (OBS's: the mock), nothing else under Setup Bindings, the keyboard's and
# the pad's ending at the same place.
# Screenshots: scratch/bindpanel/main.png (keyboard, a pad, Options: composed). Ends "bindpanel_test: N passed, M failed".
AGENT=${1:?agent}
QT_NAME=bindpanel_test
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/bindpanel
mkdir -p "$S"
SCRIPT="developer 1; map vrtesthall; wait30; vr_bindpanel_report; togglemenu; wait30; screenshot; echo bindpanel_kb; vr_bindpanel_report; vr_bindpanel_setup; wait5; echo bindpanel_setup; vr_bindpanel_report; vr_bindpanel_device xboxone; menu_main; wait30; screenshot; echo bindpanel_pad; vr_bindpanel_report; vr_bindpanel_device playstation; echo bindpanel_ps; vr_bindpanel_report; vr_bindpanel_device auto; menu_options; wait30; screenshot; echo bindpanel_opts; vr_bindpanel_report; vr_obs_mock 1; menu_main; wait60; echo bindpanel_obs; vr_bindpanel_report; vr_obs_mock 0; echo bindpanel_test_end; toggleconsole; quit"
bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode -Script "$SCRIPT" -QuitOn "^bindpanel_test_end" -Timeout 120 \
    -Filter "^(bindpanel)" -Out "$S/main.png" > "$S/run.log" 2>&1
L=$(cat "$S/run.log")
has() { grep -qE "$1" <<<"$L" && echo 1 || echo 0; }
# A part of the log, from marker $1 to marker $2.
part() { sed -n "/^$1/,/^$2/p" "$S/run.log"; }
check $(has "^bindpanel: 20 rows, shown 1, menu none, device keyboard") "twenty rows, shown in flat play, no menu, the keyboard"
check $(has '^bindpanel: row 0 "Fire" keys "(LMB|CTRL)"') "fire on LMB"
check $(has '^bindpanel: row 2 "Reload" keys "R" ""$') "reload on R (a main row)"
check $(has '^bindpanel: row 3 "Wheel \(hold\)" keys "1" "2"$') "the wheel on 1 and 2"
check $(has '^bindpanel: row 8 "Crouch / slide" keys "C" "CTRL"$') "crouch on C and CTRL (two keys)"
check $(has '^bindpanel: row 1 "Aim / off hand" keys "RMB" ""$') "no gamepad key listed"
# panelfix: the hands' number keys, next ammo, attach / detach, lie down: minor rows in their groups.
check $(has '^bindpanel: row 9 "Cycle L / R" keys "1" "2" minor group Hands$') "cycle: 1 and 2, the Hands group's first (minor)"
check $(has '^bindpanel: row 10 "Holster L / R" keys "4" "5" minor$') "holster: 4 and 5"
check $(has '^bindpanel: row 11 "Drop L / R" keys "6" "7" minor$') "drop: 6 and 7"
check $(has '^bindpanel: row 13 "Next ammo" keys "N" "" minor$') "next ammo on N"
check $(has '^bindpanel: row 14 "Attach / detach" keys "J" "" minor$') "attach / detach on J"
check $(has '^bindpanel: row 15 "Force grab" keys "M4" "" minor group More$') "force grab: the More group's first"
check $(has '^bindpanel: row 18 "Lie down" keys "X" "" minor$') "lie down on X"
check $(has "^bindpanel: 20 rows, shown 1, menu keys") "Setup Bindings opens Options > Controls"
check $(has "^bindpanel: Setup Bindings: Options > Controls") "Setup Bindings said where it went"
# The menus' panel (panelfix): the main menu, Options too, clear of the bottom corner's rows (OBS's shown: the mock),
# nothing else under Setup Bindings, the keyboard's and the pad's bottom at the same place.
K=$(part bindpanel_kb bindpanel_setup)
O=$(part bindpanel_opts bindpanel_obs)
B=$(part bindpanel_obs bindpanel_test_end)
P=$(part bindpanel_pad bindpanel_ps)
kb() { grep -qE "$1" <<<"$K" && echo 1 || echo 0; }
check $(kb '^bindpanel: menu panel drawn: 20 rows, .* page main, corner hits 0, obs row 0, clear (-0|[0-9])') "main menu: the keyboard's panel drawn, nothing under Setup Bindings, above the corner's rows"
check $(grep -qE '^bindpanel: menu panel drawn: 20 rows, .* page options, corner hits 0' <<<"$O" && echo 1 || echo 0) "Options: the panel drawn too"
check $(grep -qE '^bindpanel: menu panel drawn: 20 rows, .* page main, corner hits 0, obs row 1, clear (-0|[0-9])' <<<"$B" && echo 1 || echo 0) "OBS's row shown: the panel above it, nothing under Setup Bindings"
kbBottom=$(grep -oE '^bindpanel: menu panel room .* button [-0-9]+ [-0-9]+ \.\. [-0-9]+ [-0-9]+' <<<"$K" | grep -oE '[-0-9]+$' | head -1)
padBottom=$(grep -oE '^bindpanel: menu panel room .* button [-0-9]+ [-0-9]+ \.\. [-0-9]+ [-0-9]+' <<<"$P" | grep -oE '[-0-9]+$' | head -1)
kbLeft=$(grep -oE '^bindpanel: menu panel room .* button [-0-9]+' <<<"$K" | grep -oE '[-0-9]+$' | head -1)
padLeft=$(grep -oE '^bindpanel: menu panel room .* button [-0-9]+' <<<"$P" | grep -oE '[-0-9]+$' | head -1)
check $([ -n "$kbBottom" ] && [ "$kbBottom" = "$padBottom" ] && [ "$kbLeft" = "$padLeft" ] && echo 1 || echo 0) "the keyboard's and the pad's panels in the same bottom left corner (Setup Bindings's left and bottom $kbLeft $kbBottom / $padLeft $padBottom)"
# A pad (bindpanel2): its part of the log, between bindpanel_pad and bindpanel_ps.
pad() { grep -qE "$1" <<<"$P" && echo 1 || echo 0; }
check $(pad '^bindpanel: row [0-9]+ "[^"]+" keys "RT" "" glyph "xbox_rt"') "an Xbox pad: RT's row with Kenney's Xbox glyph"
check $(pad '^bindpanel: ([4-9]|1[0-9]|2[0-4]) rows, shown [01], menu main, device xboxone, glyphs \1$') "an Xbox pad: its bound buttons as rows, each a glyph"
check $(pad '^bindpanel: menu panel drawn: ([4-9]|1[0-9]|2[0-4]) rows') "the main menu's panel drawn with the pad's rows"
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
