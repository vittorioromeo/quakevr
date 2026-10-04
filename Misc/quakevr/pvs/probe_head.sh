#!/bin/bash
# STEP 1 probe: put the *head* at the reported spot and prove where the view really is.
# usage: probe_head.sh [setpos-z]   (default 24; the eye lands ~viewheight above the origin)
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-pvs-selfbit}
Z=${1:-24}
F='QVRVIS|setpos|ENGINE CRASH|Host_Error'
bash "$K/run.sh" "$A" -Timeout 400 -Script "map start;wait120;god;notarget;developer 1;
setpos 278 1728 $Z 7 -20 0; wait60; toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "$F" | head -20
