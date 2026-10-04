#!/bin/bash
# diagnostic (not a shipped test): the eye's own view from where the portal view of side 0 is drawn (the carried
# camera), against the destination itself. If the eye is dark there too, the carried camera is the problem.
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-slipgate-dark}
F='CASE|Wrote|ENGINE CRASH|Host_Error'

bash "$K/run.sh" "$A" -Timeout 500 -Script "map start;wait90;god;notarget;
echo CASE1-carried-far; setpos 544 552 63 0 90 0; wait60; vr_eyeshot 1; wait10;
echo CASE2-carried-near; setpos 544 1492 63 0 90 0; wait40; vr_eyeshot 1; wait10;
echo CASE3-destination-floor; setpos 544 1536 28 0 90 0; wait40; vr_eyeshot 1; wait10;
echo CASE4-just-inside; setpos 544 1420 40 0 90 0; wait40; vr_eyeshot 1; wait10;
toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "$F"
