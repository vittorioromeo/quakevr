#!/bin/bash
# diagnostic (not a shipped test): what a far gate's pixels show, against two controls - the same gate with the
# view turned off (its own texture), and the destination room seen from inside it.
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-slipgate-see}
F='CASE|box on|Wrote|looking through|last frame|ENGINE CRASH|Host_Error'

bash "$K/run.sh" "$A" -Timeout 420 -Script "map start;wait90;god;notarget;developer 1;
echo CASE1-far-view-on; setpos 232 400 24 0 90 0; wait60; vr_portals_view; vr_eyeshot 1; wait5;
echo CASE2-far-view-off; vr_portals 0; wait20; vr_eyeshot 1; wait5; vr_portals 1;
echo CASE3-at-destination; setpos 544 1536 28 0 90 0; wait40; vr_stuck_info; vr_eyeshot 1; wait5;
echo CASE4-at-gate; setpos 232 1390 24 0 90 0; wait40; vr_eyeshot 1; wait5;
toggleconsole;quit" -Filter "$F" 2>&1 | grep -Ei "$F" | head -40
