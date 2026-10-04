#!/bin/bash
# diagnostic (not a shipped test): the hidden staircase in `start` (behind the func_episodegate at x 289..303,
# y 1681..1775, z 1..95), at the reported position, at two head yaw angles, three phases:
#   PHASE1 as shipped (a gate is "looked through" at -20, "not looked at" at +40)
#   PHASE2 vr_slipgates 0   PHASE3 r_novis 1
# Each sample takes the eyes' images (vr_eyeshot 1) to eyeshots/start_<n>_L.png.
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-slipgate-pvs}
F='PHASE|S[0-9]-|Wrote|looking through|ENGINE CRASH|Host_Error'

S() { echo "setpos 278 1728 24 7 $1 0; wait30; echo $2; vr_eyeshot 1; wait10; vr_eyeshot 0; wait5;"; }

bash "$K/run.sh" "$A" -Timeout 600 -Script "map start;wait90;god;notarget;skill 2;
setpos 278 1728 24 7 -20 0; wait60;
echo PHASE1-gates-on; $(S -20 S1-on-y20) $(S 40 S2-on-p40)
echo PHASE2-gates-off; vr_slipgates 0; $(S -20 S3-off-y20) $(S 40 S4-off-p40)
echo PHASE3-novis; vr_slipgates 1; r_novis 1; $(S -20 S5-nv-y20) $(S 40 S6-nv-p40)
toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "$F" | head -40