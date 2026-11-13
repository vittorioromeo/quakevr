#!/bin/bash
# menu_wheel_test.sh <agent> [page title] -- the mouse wheel on a VR settings page with a scroll bar (vr_menu_wheel_scroll;
# Quake/vr/vr_menu.cpp wheelScroll, `menu_vr wheeltest`): a notch scrolls the page by that part of its rows shown, the
# cursor kept on it; notches stop at the page's end; a notch up scrolls back; with the setting at 0 a notch moves the
# cursor a row. Headless, fast mode; the run ends on the test's verdict line.
AGENT=${1:?agent}
PAGE=${2:-Locomotion}
KIT=C:/OHWorkspace/qvr-kit
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/menu_wheel
mkdir -p "$S"
SCRIPT="map vrtesthall; wait30; menu_vr title $PAGE; wait5; menu_vr wheeltest; toggleconsole; quit"
bash $KIT/run.sh $AGENT -Script "$SCRIPT" -QuitOn "^menu_wheel_test: [0-9]+ passed" -Timeout 120 \
    -Filter "^menu_wheel_test|^menu_vr" > "$S/run.log" 2>&1
grep -E "^menu_wheel_test|^menu_vr" "$S/run.log" | tail -12
