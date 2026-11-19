#!/bin/bash
# menu_changed_test.sh <agent> -- the menu's "changed from its default" is the config writer's (sameSettingValue;
# Quake/vr/vr_menu.cpp changedSetting and resetAll, `menu_vr changedtest`; REVIEW_NEXT_2026-11-18.md, finding 13): a
# value the config doesn't save has no * and isn't on Changed Settings; Reset All counts "0.50" as "0.5" (kept) and puts
# a changed setting back. Headless, no map; the run ends on the verdict line "menu_changed_test: N passed, M failed".
AGENT=${1:?agent}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
bash $KIT/run.sh $AGENT -Script "menu_vr changedtest; toggleconsole; quit" -QuitOn "^menu_changed_test: [0-9]+ passed" \
    -Timeout 120 -Filter "^menu_changed_test|rror|CRASH" 2>&1 | grep -v "^$" | tee /dev/stderr | grep -q "^menu_changed_test: [1-9][0-9]* passed, 0 failed"
