#!/bin/bash
# A/B at the proven head position: does the view's PVS name its own leaf, and is the
# episode gate (a static brush model in that leaf) culled?  developer 1 prints QVRVIS/QVRPVS.
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-pvs-selfbit}
F='QVRVIS|QVRPVS|MARK|ENGINE CRASH|Host_Error'
bash "$K/run.sh" "$A" -Timeout 600 -Script "map start;wait120;god;notarget;developer 1;
setpos 278 1728 24 7 -20 0; wait60;
echo MARK-off; vr_pvs_selfleaf 0; wait40;
echo MARK-on; vr_pvs_selfleaf 1; wait40;
echo MARK-yaw40; vr_mock_look 0 40; wait40;
echo MARK-yaw-60; vr_mock_look 0 -60; wait40;
echo MARK-yaw0; vr_mock_look 0 0; wait40;
echo MARK-head+0.5m; vr_mock_hand head 0.5 1.7 0 0 0 0; wait40;
toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "$F" | awk '/MARK/{print} !/MARK/{if($0!=prev){print} prev=$0}' | head -60
