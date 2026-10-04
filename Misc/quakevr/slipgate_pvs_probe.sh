#!/bin/bash
# diagnostic (not a shipped test): at the reported position in `start`, what the gate test says across a yaw sweep.
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-slipgate-pvs}
F='CASE|head|side [0-9]|looking through|last frame|not in the|ENGINE CRASH|Host_Error'

bash "$K/run.sh" "$A" -Timeout 420 -Script "map start;wait90;god;notarget;skill 2;
echo CASE1-seed; setpos 278 1728 24 7 -20 0; wait40; vr_portals_view;
echo CASE2-sweep; setpos 278 1728 24 7 -60 0; wait20; vr_portals_view;
setpos 278 1728 24 7 -40 0; wait20; vr_portals_view;
setpos 278 1728 24 7 -20 0; wait20; vr_portals_view;
setpos 278 1728 24 7 0 0;   wait20; vr_portals_view;
setpos 278 1728 24 7 20 0;  wait20; vr_portals_view;
setpos 278 1728 24 7 40 0;  wait20; vr_portals_view;
setpos 278 1728 24 7 60 0;  wait20; vr_portals_view;
toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "$F"
