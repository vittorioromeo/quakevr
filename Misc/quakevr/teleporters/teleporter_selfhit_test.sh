#!/bin/bash
# teleporter_selfhit_test.sh <agent>: headless checks that you can shoot yourself through a teleporter (vrteleporters' loop:
# T's west gate comes out of T's east gate, so facing west you see your own back), with the agent kit
# (C:/OHWorkspace/qvr-kit). Each line prints your health before and after (start 100, no armour); "hit" is a drop.
#   pellet   vr_physics_fire 10 (a shotgun pellet from your eyes) west through the loop: hit
#   nail     vr_physics_fire 1 (a nail, carried through the gate as it flies, then meeting its owner): hit
#   rocket   vr_physics_fire 0: hit
#   shotgun, nailgun   the real guns (mock hand level, 0.4 m back): hit
#   lightning          the real gun, 23 units from the gate with the hand 0.4 m forward (the bolt's 600 units reach
#                      round the 640-wide room only from near the gate): hit
#   held through       the shotgun with the muzzle reached through the gate (its shot starts at your image): hit
#   god                the shotgun under god mode: no drop
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
START="developer 1;vr_fixed_frames 1;vr_fixed_frames_rate 90;map vrteleporters;wait60;notarget"
health() { bash $KIT/run.sh $AGENT -Filter "^health" -Script "$1" | grep -E "^health" | awk '{print $2}' | tr '\n' ' '; }
test_fire() { # test_fire <kind>
    health "$START;setpos -1560 560 24 0 180 0;wait5;noclip 0;wait10;edict 1;vr_physics_fire $1 -1700 560 40;wait90;edict 1;toggleconsole;quit"
}
gun() { # gun <impulse> <x> <hand forward (m, negative: ahead)> [extra]
    health "$START;$4;setpos $2 560 24 0 180 0;vr_weapon_grip_mode 1;impulse 9;wait5;impulse $1;wait10;vr_mock_hand main 0.15 1.40 $3 70 0 0;wait20;edict 1;+attack;vr_mock_button main trigger 1;wait3;-attack;vr_mock_button main trigger 0;wait60;edict 1;toggleconsole;quit"
}
echo "pellet:      $(test_fire 10)(100 then less)"
echo "nail:        $(test_fire 1)(100 then less)"
echo "rocket:      $(test_fire 0)(100 then less)"
echo "shotgun:     $(gun 154 -1560 0.40)(100 then less)"
echo "nailgun:     $(gun 156 -1560 0.40)(100 then less)"
echo "lightning:   $(gun 161 -1583 -0.40)(100 then less)"
echo "held through: $(gun 154 -1583 -0.80)(100 then less)"
echo "god:         $(gun 154 -1560 0.40 god)(100 then 100)"
