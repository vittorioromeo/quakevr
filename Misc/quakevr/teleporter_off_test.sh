#!/bin/bash
# teleporter_off_test.sh -- vr_teleporters 0 turns the whole teleporter feature off (Quake/vr/vr_portals.cpp): no gate is
# built, none is looked through, nothing is carried or traced through one, and Quake's trigger_teleport moves the
# player as it did before the feature. The flip takes effect at once, no map reload (that is what PHASE 2 tests: the
# map is loaded once, the switch is flipped in the middle of it).
#
# In the original `start` map, the first gate: plane y=1384 normal (0,-1,0), opening x 208..256, its trigger brush
# 18 units deep in front of it (y 1366..1384). Its destination is printed by vr_portals_info as "from ... to ...".
#
# PHASE 1 (feature on, as shipped): CASE-cross must print "VR portal: carried edict 1 through side 0" and CASE-shot
# must print "through 1 teleporter(s)". PHASE 2 (vr_teleporters 0, same map, same positions): neither may print, and
# vr_portals_info must say the feature is off. The player's position differs between the two: carried by the engine to
# the near side of the gate, or teleported by the trigger to its destination (vr_stuck_info prints where he is).
# PHASE 3 turns it back on, without reloading: the crossing works again (the gates are built anew).
K=C:/OHWorkspace/qvr-kit
A=${AGENT:-teleporter-off}
F='PHASE|CASE|carried edict|vr_stuck_info:|teleporter|VR portals|ENGINE CRASH|Host_Error|Error'

bash "$K/run.sh" "$A" -Timeout 420 -Script "map start;wait90;god;notarget;developer 1;
echo PHASE1-on;
echo CASE1-info; vr_portals_info;
echo CASE2-cross; setpos 232 1390 24 0 90 0; wait40; vr_stuck_info;
echo CASE3-shot; setpos 232 1360 24 0 90 0; wait10; vr_physics_fire 10 232 1500 25;
echo PHASE2-off; vr_teleporters 0;
echo CASE4-info; vr_portals_info;
echo CASE5-cross; setpos 232 1390 24 0 90 0; wait60; vr_stuck_info;
echo CASE6-shot; setpos 232 1360 24 0 90 0; wait10; vr_physics_fire 10 232 1500 25;
echo PHASE3-on-again; vr_teleporters 1;
echo CASE7-info; vr_portals_info;
echo CASE8-cross; setpos 232 1390 24 0 90 0; wait40; vr_stuck_info;
toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "$F"
