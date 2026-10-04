#!/bin/bash
# diagnostic (not a shipped test): the view through a slipgate measured at the same aperture from several distances
# (vr_portals_shot's "gate box" is the same world region at every distance; only the camera is further behind it).
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-slipgate-dark}
F='CASE|VR portal shot|whole|gate box|drawn from|its box|ENGINE CRASH|Host_Error'

bash "$K/run.sh" "$A" -Timeout 600 -Script "map start;wait90;god;notarget;
echo CASE1-984; setpos 232 400 24 0 90 0; wait60; vr_portals_shot; wait10;
echo CASE2-694; setpos 232 690 24 0 90 0; wait40; vr_portals_shot; wait10;
echo CASE3-394; setpos 232 990 24 0 90 0; wait40; vr_portals_shot; wait10;
echo CASE4-194; setpos 232 1190 24 0 90 0; wait40; vr_portals_shot; wait10;
echo CASE5-94;  setpos 232 1290 24 0 90 0; wait40; vr_portals_shot; wait10;
echo CASE6-44;  setpos 232 1340 24 0 90 0; wait40; vr_portals_shot; wait10;
toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "$F"
