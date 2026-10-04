#!/bin/bash
# The episode gate in `start`, at the proven head position (eye 278 1729 59, leaf 595),
# head yaw -60..+60, with vr_pvs_selfleaf 0 and 1. Screenshots are named pvs_<flag>_<yaw>.
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-pvs-selfbit}
F='QVRVIS|QVRPVS|MARK|Wrote|ENGINE CRASH|Host_Error'
Y() { echo "echo MARK-$1-$2; vr_mock_look 0 $2; wait25; screenshot pvs_$1_$2; wait10;"; }
S() { local p; for p in -60 -30 0 30 60; do echo "$(Y "$1" "$p")"; done; }
bash "$K/run.sh" "$A" -Timeout 900 -Script "map start;wait120;god;notarget;developer 1;
setpos 278 1728 24 7 -20 0; wait60;
vr_pvs_selfleaf 0; $(S off)
vr_pvs_selfleaf 1; $(S on)
echo MARK-headmove; vr_mock_hand head 0.5 1.7 0 0 0 0; wait25; screenshot pvs_move; wait10;
toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "$F" | awk '/MARK/{print;next} $0!=prev{print} {prev=$0}' | head -50
