#!/bin/bash
# menu_anchor_test.sh <agent> -- two VR menu panels on one page (Quake/vr/vr_menu.cpp items(), `menu_vr anchortest`;
# REVIEW_NEXT_2026-11-18.md, finding 12): a rebuild one panel asks for (Menu Detail raised, rows added above the other's
# selection, then lowered again) keeps the other panel's row, by its label, on the same line of its view, and the
# first's own. Headless, no map; the run ends on the test's verdict line "menu_anchor_test: N passed, M failed".
AGENT=${1:?agent}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
bash $KIT/run.sh $AGENT -Script "menu_vr anchortest; toggleconsole; quit" -QuitOn "^menu_anchor_test: [0-9]+ passed" \
    -Timeout 120 -Filter "^menu_anchor_test|rror|CRASH" 2>&1 | grep -v "^$" | tee /dev/stderr | grep -q "^menu_anchor_test: [1-9][0-9]* passed, 0 failed"
