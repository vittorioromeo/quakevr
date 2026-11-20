#!/bin/bash
# pouch_slider_test.sh <agent> -- VR Settings > Holster Calibration's Ammo Pouch Forward / Right / Up rows
# (vr_menu_holster_pouch_*, `menu_vr pouchtest` in Quake/vr/vr_menu.cpp; ROUND25.md "comfort"): each moves the standing
# pouch's setting (vr_ammo_pouch_x/y/z) and its reach point that way, the setting moved elsewhere (as Holster Setup's drag
# does) shows on its row, Reset Holsters puts the pouch back. The test waits for the map itself; the run ends on its
# verdict line "menu_pouch_test: N passed, M failed".
AGENT=${1:?agent}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
bash $KIT/run.sh $AGENT -Script "map e1m1; menu_vr pouchtest; toggleconsole; quit" -QuitOn "^menu_pouch_test: [0-9]+ passed" \
    -Timeout 180 -Filter "^menu_pouch_test|rror|CRASH" 2>&1 | grep -v "^$" | tee /dev/stderr | grep -q "^menu_pouch_test: [1-9][0-9]* passed, 0 failed"
