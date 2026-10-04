#!/bin/bash
# diagnostic (not a shipped test): at the reported position in `start`, how many brush polys the view draws across a
# yaw sweep, with the slipgates on / off / with r_novis. Summarised per sample so the numbers stay small.
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-slipgate-pvs}
F='PHASE|CASE|wpoly|looking through|side [0-9]|not in the|ENGINE CRASH|Host_Error'

SWEEP='setpos 278 1728 24 7 -60 0; wait12; echo CASE-a60; vr_portals_view; wait8;
setpos 278 1728 24 7 -40 0; wait12; echo CASE-b40; vr_portals_view; wait8;
setpos 278 1728 24 7 -20 0; wait12; echo CASE-c20; vr_portals_view; wait8;
setpos 278 1728 24 7 0 0;   wait12; echo CASE-d00; vr_portals_view; wait8;
setpos 278 1728 24 7 20 0;  wait12; echo CASE-e20; vr_portals_view; wait8;
setpos 278 1728 24 7 40 0;  wait12; echo CASE-f40; vr_portals_view; wait8;
setpos 278 1728 24 7 60 0;  wait12; echo CASE-g60; vr_portals_view; wait8;'

bash "$K/run.sh" "$A" -Timeout 600 -Script "map start;wait90;god;notarget;skill 2;
setpos 278 1728 24 7 -20 0; wait60; r_speeds 2;
echo PHASE1-gates-on; $SWEEP
echo PHASE2-gates-off; vr_slipgates 0; $SWEEP
echo PHASE3-novis; vr_slipgates 1; r_novis 1; $SWEEP
toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "$F" \
| awk '
function flush(){ if(name!="") printf "%s %s frames %d wpoly min %d max %d | gate: %s\n", phase, name, n, lo, hi, gate; name=""; n=0; lo=""; hi=0; gate="" }
/PHASE[0-9]/{flush(); phase=substr($0,index($0,"PHASE"),7); next}
/CASE-/{flush(); name=substr($0,index($0,"CASE"),7); next}
/wpoly/{for(i=1;i<=NF;i++) if($i=="wpoly"){v=$(i-1)+0; n++; if(lo==""||v<lo)lo=v; if(v>hi)hi=v}}
/looking through side/{if(gate=="") gate=$0}
END{flush()}'
