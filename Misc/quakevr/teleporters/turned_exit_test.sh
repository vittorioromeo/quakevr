#!/bin/bash
# turned_exit_test.sh <agent>: the body walked (mock stick) through a gate that turns it 45 degrees (vrteleporters' room d45,
# make_vrteleporters_map.py's narrow_exit): the walls' planes are x - y = K +- half.
#   fits      a corridor 28.3 wide at the exit, the body's box 16: through it, 32+ units past the destination
#   blocked   a corridor 14.1 wide: the body stays on the gate's side
# 300 frames of walking each (counted: 64 units to the gate and on down the corridor at half speed). Ends on the verdict line.
AGENT=${1:?agent}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
F='vr_portals_turned_test|carried edict|ENGINE CRASH|Host_Error'
S='map vrteleporters;wait60;god;notarget;developer 1;vr_portals_turned_test start'
S="$S;setpos -2304 368 24 0 180 0;wait10;noclip 0;wait40;vr_mock_stick off 0 0.5;wait300;vr_mock_stick off 0 0;vr_portals_turned_test past fits -2080 400 45 32"
S="$S;setpos -2304 640 24 0 180 0;wait10;noclip 0;wait40;vr_mock_stick off 0 0.5;wait300;vr_mock_stick off 0 0;vr_portals_turned_test at blocked -2352 640 24"
S="$S;vr_portals_turned_test end;toggleconsole;quit"
bash "$KIT/run.sh" "$AGENT" -Timeout 300 -QuitOn 'vr_portals_turned_test: [0-9]+ passed' -Script "$S" -Filter "$F"
