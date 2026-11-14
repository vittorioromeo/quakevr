#!/bin/bash
# menu_pagereset_test.sh <agent> -- Reset This Page and change tracking on the weapon pages (Quake/vr/vr_menu.cpp
# pageResetTest, `menu_vr pageresettest`; ROUND25.md, "Reset This Page and changes on the weapon and prop pages"): the
# shotgun's hotspot 1 X changed -> its row marked (*) and on Changed Settings under the shotgun's name -> Reset This
# Page on Weapon Offsets - Two-Handed (pressed twice) puts back its weapons.txt default, the shotgun's hotspot 2 X, its
# Shot Pitch (another page) and the nailgun's hotspot kept. The weapon pages pinned to the shotgun's slot: no map needed.
# Headless, fast mode; the run ends on the test's verdict line.
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/menu_pagereset
mkdir -p "$S"
SCRIPT="menu_vr pageresettest; toggleconsole; quit"
bash $KIT/run.sh $AGENT -Script "$SCRIPT" -QuitOn "^pagereset_test: [0-9]+ passed" -Timeout 120 \
    -Filter "^pagereset_test|^menu_vr" > "$S/run.log" 2>&1
grep -E "^pagereset_test|^menu_vr" "$S/run.log" | tail -20
