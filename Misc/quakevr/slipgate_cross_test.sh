#!/bin/bash
# slipgate_cross_test.sh -- crossing a slipgate is physical (Quake/vr/vr_portals.cpp): a gate's frame stops you like
# any wall, with no teleport and no jump; you are carried through when your torso's middle plane (the plane that halves
# your collision box) is through the gate's plane, or as near to it as your box can bring it. Leaning in, or reaching
# with the hands, does not cross.
#
# In the original `start` map. Its three gates are at y=1384, normal (0,-1,0), openings 48 wide and 96 tall
# (vr_portals_info prints them all, with where your body is against each):
#   side 0 x 208..256   side 1 x 520..568   side 2 x 840..888   (trigger brushes 18 units deep in front of the plane)
# The player's box is -16,-16,-24 .. 16,16,26, so standing on the floor in front of a gate his origin is at z=24 and
# his torso's middle plane at z=25.
#
# The positions are stepped with setpos: what is under test is the crossing rule, not the locomotion (input-driven
# walking is not exercised headless). CASE 1, 2, 3 and 4 must show no "VR portal: carried edict" (against the frame,
# in the opening but short of the plane, leaning with the head through it, against the frame at the opening's edge);
# CASE 5 must show one (his torso's middle plane is past the plane), and CASE 6 "through 1 slipgate(s)" (a shot).
# In CASE 1 and 4 vr_portals_info says "his box reaches <the distance he is short>", which is what says he is stopped
# by the frame rather than free to walk; in CASE 2 it says "his box reaches -1" (nothing stops him: he would go on).
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-slipgate-cross}
F='CASE[0-9]|carried edict|player at|torso |test fire:|ENGINE CRASH|Host_Error'

bash "$K/run.sh" "$A" -Timeout 300 -Script "map start;wait60;god;notarget;developer 1;
echo CASE1-frame; setpos 200 1372 24 0 90 0; wait40; vr_portals_info;
echo CASE2-nearly; setpos 232 1372 24 0 90 0; wait40; vr_portals_info;
echo CASE3-lean; setpos 232 1365 24 0 90 0; wait20; vr_mock_hand head 0 0 -0.5 45 0 0; wait40; vr_portals_info; vr_mock_hand head 0 0 0 0 0 0;
echo CASE4-edge; setpos 262 1372 24 0 90 0; wait40; vr_portals_info;
echo CASE5-through; setpos 232 1390 24 0 90 0; wait40; vr_portals_info;
echo CASE6-shot; setpos 232 1360 24 0 90 0; wait10; vr_physics_fire 10 232 1500 25;
toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "$F"
