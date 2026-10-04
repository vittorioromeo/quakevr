#!/bin/bash
# diagnostic (not a shipped test): the portal scene (vr_portals_shot) and the eye's own image (vr_eyeshot 1) at the
# same position and the same gate box, so the two readbacks are on the same scale.
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-slipgate-dark}
F='CASE|VR portal shot|whole|box|gate:|drawn from|its box|Wrote|ENGINE CRASH|Host_Error'

bash "$K/run.sh" "$A" -Timeout 500 -Script "map start;wait90;god;notarget;
echo CASE1-far-984; setpos 232 400 24 0 90 0; wait60; vr_portals_view; vr_portals_shot; vr_eyeshot 1; wait10;
echo CASE2-near-44; setpos 232 1340 24 0 90 0; wait40; vr_portals_view; vr_portals_shot; vr_eyeshot 1; wait10;
toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "$F"
