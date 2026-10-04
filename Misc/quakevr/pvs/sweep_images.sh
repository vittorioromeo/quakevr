#!/bin/bash
# Diagnostic: the hidden staircase cover is func_bossgate (*38), not func_episodegate (*41).
# Compare gates on/off and r_novis at the same player positions. setpos enables noclip.
# Each sample takes eyeshots; comparing different yaw angles alone cannot isolate visibility.
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-slipgate-pvs}
F='PHASE|S[0-9]-|Wrote|looking through|ENGINE CRASH|Host_Error'

S() { echo "setpos 278 1728 24 7 $1 0; wait30; echo $2; vr_eyeshot 1; wait10; vr_eyeshot 0; wait5;"; }

bash "$K/run.sh" "$A" -Timeout 600 -Script "skill 2;wait120;map start;wait120;god;notarget;
setpos 278 1728 24 7 -20 0; wait60;
echo PHASE1-gates-on; $(S -20 S1-on-y20) $(S 40 S2-on-p40)
echo PHASE2-gates-off; vr_slipgates 0; $(S -20 S3-off-y20) $(S 40 S4-off-p40)
echo PHASE3-novis; vr_slipgates 1; r_novis 1; $(S -20 S5-nv-y20) $(S 40 S6-nv-p40)
toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "$F" | head -40