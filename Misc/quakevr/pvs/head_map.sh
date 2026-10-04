#!/bin/bash
# Where the head is, in tracking space, and what moving it does to the eye (leaf) in `start`.
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-pvs-selfbit}
F='QVRVIS|MARK|ENGINE CRASH|Host_Error'
S() { echo "echo MARK-$1; vr_mock_hand head $2 1.7 $3 0 $4 0; wait25;"; }
bash "$K/run.sh" "$A" -Timeout 500 -Script "map start;wait120;god;notarget;developer 1;
setpos 278 1728 24 7 -20 0; wait60;
$(S base 0 0 0) $(S x+ .3 0 0) $(S x- -.3 0 0) $(S z+ 0 0 .3) $(S z- 0 0 -.3) $(S yaw40 0 0 0 40) $(S yaw-40 0 0 0 -40)
toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "$F" | awk '!/MARK/ || 1' | head -60
