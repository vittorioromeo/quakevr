#!/bin/bash
# diagnostic (not a shipped test): the view through a slipgate from far (vr_portals_shot) against the same view seen
# directly from just past the gate (vr_eyeshot), plus r_lightmap 1 (the baked light on its own).
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-slipgate-dark}
F='CASE|VR portal shot|whole|box|Wrote|ENGINE CRASH|Host_Error'

bash "$K/run.sh" "$A" -Timeout 420 -Script "map start;wait90;god;notarget;
echo CASE1-far; setpos 232 400 24 0 90 0; wait60; vr_portals_view; vr_portals_shot; wait10;
echo CASE2-far-lightmap; r_lightmap 1; wait40; vr_portals_shot; wait10; r_lightmap 0; wait20;
echo CASE3-just-past-gate; setpos 232 1400 24 0 90 0; wait40; vr_eyeshot 1; wait10;
echo CASE4-at-destination; setpos 544 1536 28 0 90 0; wait40; vr_eyeshot 1; wait10;
toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "$F"
