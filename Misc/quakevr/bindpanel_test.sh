#!/bin/bash
# bindpanel_test.sh <agent> -- flat-screen play's bindings reminder (wheelui, vr_bindpanel.cpp): its rows and their keys
# as bound now (the defaults: R reloads, C and CTRL crouch, LMB fires, 1 and 2 hold the wheel), shown in flat play; the
# main menu's panel (a screenshot: main.png) and its Setup Bindings (vr_bindpanel_setup: Options > Controls at the flat
# keys while no controller screen opens). Ends "bindpanel_test: N passed, M failed".
AGENT=${1:?agent}
QT_NAME=bindpanel_test
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/bindpanel
mkdir -p "$S"
SCRIPT="developer 1; map vrtesthall; wait30; vr_bindpanel_report; togglemenu; wait30; screenshot; vr_bindpanel_report; vr_bindpanel_setup; wait5; vr_bindpanel_report; echo bindpanel_test_end; toggleconsole; quit"
bash $KIT/run.sh $AGENT -ExtraArgs -flatscreenmode -Script "$SCRIPT" -QuitOn "^bindpanel_test_end" -Timeout 120 \
    -Filter "^(bindpanel)" -Out "$S/main.png" > "$S/run.log" 2>&1
L=$(cat "$S/run.log")
has() { grep -qE "$1" <<<"$L" && echo 1 || echo 0; }
check $(has "^bindpanel: 14 rows, shown 1, menu none") "fourteen rows, shown in flat play, no menu"
check $(has '^bindpanel: row 0 "Fire" keys "(LMB|CTRL)"') "fire on LMB"
check $(has '^bindpanel: row 2 "Reload" keys "R" ""') "reload on R"
check $(has '^bindpanel: row 3 "Wheel \(hold\)" keys "1" "2"') "the wheel on 1 and 2"
check $(has '^bindpanel: row 12 "Crouch / slide" keys "C" "CTRL"') "crouch on C and CTRL (two keys)"
check $(has '^bindpanel: row 1 "Aim / off hand" keys "RMB" ""') "no gamepad key listed"
check $(has "^bindpanel: 14 rows, shown 1, menu keys") "Setup Bindings opens Options > Controls"
check $(has "^bindpanel: Setup Bindings: Options > Controls") "Setup Bindings said where it went"
verdict
exit $fail
