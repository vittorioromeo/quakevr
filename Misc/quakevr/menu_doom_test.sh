#!/bin/bash
# menu_doom_test.sh <agent> -- the menu's Doom page lists the WADs of the last search (Quake/vr/vr_menu.cpp items(),
# `menu_vr doomtest`; REVIEW_NEXT_2026-11-18.md, finding 19): a test IWAD dropped into quakevr/doom/wads and Search Again
# (vr_doom_wads) give a row more; removed and searched again, as before (the test removes its file). Headless, no map;
# the run ends on the verdict line "menu_doom_test: N passed, M failed".
AGENT=${1:?agent}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
bash $KIT/run.sh $AGENT -Script "menu_vr doomtest; toggleconsole; quit" -QuitOn "^menu_doom_test: [0-9]+ passed" \
    -Timeout 120 -Filter "^menu_doom_test|rror|CRASH" 2>&1 | grep -v "^$" | tee /dev/stderr | grep -q "^menu_doom_test: [1-9][0-9]* passed, 0 failed"
