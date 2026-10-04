#!/bin/bash
# STEP 3: the episode gate in `start` at the proven head position (eye 278 1729 59, leaf 595),
# head yaw -60..+60, vr_pvs_selfleaf 0 then 1. Screenshots pvs_<flag>_<yaw>.png.
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-pvs-selfbit}
F='MARK|Wrote|screenshot|ENGINE CRASH|Host_Error|full size'
Y() { echo "echo MARK-$1-$2; vr_mock_look 0 $2; wait20; screenshot; wait10;"; }
S() { local p; for p in -60 -30 0 30 60; do echo "$(Y "$1" "$p")"; done; }
bash "$K/run.sh" "$A" -Clean -Timeout 900 -Script "map start;wait120;god;notarget;developer 1;
setpos 278 1728 24 7 -20 0; wait60;
vr_pvs_selfleaf 0; $(S off)
vr_pvs_selfleaf 1; $(S on)
toggleconsole;quit" -Out pvs_pair.png -Filter "$F" 2>&1 | grep -Ei "$F" | head -30
