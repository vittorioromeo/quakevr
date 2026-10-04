#!/bin/bash
# diagnostic (not a shipped test): the hidden staircase in `start` (the episode gate at x 289..303, y 1681..1775),
# at the reported position, sweeping yaw. Three phases: as shipped, with vr_slipgates 0, and with r_novis 1.
# Each yaw sample takes a pair of eye images (vr_eyeshot 1) and prints the gate test's answer for that head.
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-slipgate-pvs}
F='PHASE|CASE|looking through|side 3|head |Wrote|ENGINE CRASH|Host_Error'

SWEEP='setpos 278 1728 24 7 -60 0; wait30; echo CASE-a60; vr_eyeshot 1; vr_portals_view; wait10;
setpos 278 1728 24 7 -40 0; wait30; echo CASE-b40; vr_eyeshot 1; vr_portals_view; wait10;
setpos 278 1728 24 7 -20 0; wait30; echo CASE-c20; vr_eyeshot 1; vr_portals_view; wait10;
setpos 278 1728 24 7 0 0;   wait30; echo CASE-d00; vr_eyeshot 1; vr_portals_view; wait10;
setpos 278 1728 24 7 20 0;  wait30; echo CASE-e20; vr_eyeshot 1; vr_portals_view; wait10;
setpos 278 1728 24 7 40 0;  wait30; echo CASE-f40; vr_eyeshot 1; vr_portals_view; wait10;
setpos 278 1728 24 7 60 0;  wait30; echo CASE-g60; vr_eyeshot 1; vr_portals_view; wait10;'

bash "$K/run.sh" "$A" -Timeout 600 -Script "map start;wait90;god;notarget;skill 2;
setpos 278 1728 24 7 -20 0; wait60;
echo PHASE1-shipped; $SWEEP
echo PHASE2-slipgates-off; vr_slipgates 0; $SWEEP
echo PHASE3-novis; vr_slipgates 1; r_novis 1; $SWEEP
toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "$F"
