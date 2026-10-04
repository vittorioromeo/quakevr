#!/bin/bash
# slipgate_off_profile.sh -- the see-through view is not drawn with vr_slipgates 0: the profiler's "portal" scope
# (stereo::renderPortal, vr_stereo.cpp) appears while looking at a gate with the feature on, and not with it off.
# Same map, same position, facing the first gate of start (232 1372, yaw 90).
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-slipgate-off}
F='PHASE|portal|ENGINE CRASH|Host_Error'

bash "$K/run.sh" "$A" -Timeout 420 -Script "map start;wait90;god;notarget;setpos 232 1372 24 0 90 0;wait30;
echo PHASE1-on; vr_profile 1; vr_profile_gpu 1; wait20; vr_profile_dump;
echo PHASE2-off; vr_slipgates 0; wait20; vr_profile_dump;
vr_profile 0; toggleconsole;quit" -Filter "$F" 2>&1 | grep -Ei "$F" | grep -v "^\s*$" | head -40
