#!/bin/bash
# diagnostic (not a shipped test): why a gate is or is not looked through, and what its pixels show, from far from
# it, from near it, and from the same far distance again after having been near (the author's report).
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-slipgate-see}
F='CASE|side [0-9]|head |last frame|looking through|box on|Wrote|ENGINE CRASH|Host_Error'

bash "$K/run.sh" "$A" -Timeout 420 -Script "map start;wait90;god;notarget;developer 1;
echo CASE1-far-first; setpos 232 400 24 0 90 0; wait60; vr_portals_view; vr_eyeshot 1; wait5;
echo CASE2-mid; setpos 232 900 24 0 90 0; wait40; vr_portals_view; vr_eyeshot 1; wait5;
echo CASE3-near; setpos 232 1372 24 0 90 0; wait40; vr_portals_view; vr_eyeshot 1; wait5;
echo CASE4-far-again; setpos 232 400 24 0 90 0; wait40; vr_portals_view; vr_eyeshot 1; wait5;
toggleconsole;quit" -Filter "$F" 2>&1 | grep -Ei "$F" | head -80
