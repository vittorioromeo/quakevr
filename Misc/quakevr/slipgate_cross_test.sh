#!/bin/bash
# Physical movement regression: setpos enables noclip, so explicitly restore collision and settle onto the floor.
# The low sill intentionally requires a jump. Torso entry is y=1384; edges x211 and x253 must not cross.
# Off-centre entries x220 and x244 must retain their offset at the destination (x532 and x556).
K=${KIT:-C:/OHWorkspace/qvr-kit}
A=${AGENT:-slipgate-cross}
F='CASE|carried edict|player at|torso |ENGINE CRASH|Host_Error'
P='map start;wait120;god;notarget;developer 1;'
for x in 211 253 220 244 232 544 864; do
    P="$P echo CASE-jump-x$x;setpos $x 1330 24 0 90 0;wait10;noclip 0;wait80;vr_mock_stick off 0 0.5;wait20;+jump;wait30;-jump;wait100;vr_mock_stick off 0 0;vr_portals_info;"
done
P="$P echo CASE-no-jump;setpos 232 1330 24 0 90 0;wait10;noclip 0;wait80;vr_mock_stick off 0 0.5;wait300;vr_mock_stick off 0 0;vr_portals_info;"
P="$P echo CASE-lean;setpos 232 1365 24 0 90 0;wait10;noclip 0;wait80;vr_mock_hand head 0 0 -0.5 45 0 0;wait40;vr_portals_info;"
bash "$K/run.sh" "$A" -Timeout 900 -Script "$P toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "$F"
