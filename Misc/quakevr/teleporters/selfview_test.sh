#!/bin/bash
# selfview_test.sh <agent>: start's one-way exit (the gate back from the hub, side 3: its exit plane y 280 in front of the
# start, normal +y). The body stands 8 units in front of that plane facing away from it, the head leans back across it
# (0.35 m back, tilted up) and both hands reach back onto the plane over the opening. Nothing may be drawn from the hub:
# no eye from the entrance (the player seeing himself in its gate), no hand cut by the exit plane (hands vanishing), and
# each side's exit checked with a body 6 units in front and an eye 6 behind (vr_portals_selfview_test, vr_portals.cpp).
# 60 frames of leaning (counted: the lean and the hands settle in a few). Ends on the verdict line.
AGENT=${1:?agent}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
F='vr_portals_selfview_test|ENGINE CRASH|Host_Error'
S='map start;wait60;god;notarget;setpos 544 288 24 0 90 0;wait10;noclip 0;wait40'
S="$S;vr_mock_hand head 0 1.5 0.35 -25 0 0;vr_mock_hand_to main 552 280 56;vr_mock_hand_to off 536 280 56;wait20"
S="$S;vr_portals_selfview_test start;wait60;vr_portals_selfview_test end;toggleconsole;quit"
bash "$KIT/run.sh" "$AGENT" -Timeout 300 -QuitOn 'vr_portals_selfview_test: [0-9]+ passed' -Script "$S" -Filter "$F"
