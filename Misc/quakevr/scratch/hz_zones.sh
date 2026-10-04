#!/bin/bash
# hz_zones.sh <agent> -- the acceptance cases for the hitzone work, on one monster (the vrfiringrange training
# dummy) from one standing spot (219 -656.7 41, facing it):
#   A  a shotgun's pellets at the dummy's head (vr_decap_test 17)  -> the parts and multipliers the pellets report
#   B  a sword's slash at the same head point (vr_decap_test 1)    -> must report the same zone and multiplier
#   C  a real fist punch swept through the head                     -> the same zone, from a real model triangle
#   D  a diagonal punch whose probe meets the body while its way through the body crosses the head zone,
#      with vr_hit_head_priority 1 then 0                           -> head first, then the limb/body claim
AGENT=${1:-hitzones}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
punch() { # punch <z-start> <z-end> : the knuckles swept straight at the dummy along the player's facing
    local z0=$1 z1=$2 n
    P="$P vr_mock_hand_to main 214 -656.7 $z0;wait6;"
    for n in 208 202 196 192; do
        P="$P vr_mock_hand_to main $n -656.7 $(python -c "print(round($z0+($z1-$z0)*($n-214)/(-22),1))");wait2;"
    done
    P="$P wait40;"
}
P="map vrfiringrange;wait60;god;notarget;developer 1;vr_debug_shots 1;vr_hit_melee_scale 1;setpos 219 -656.7 41 0 180 0;wait40;"
P="$P echo CASE-A-shot-head;vr_decap_test 17;wait40;"
P="$P echo CASE-B-sword-same-point;vr_decap_test 1;wait40;"
P="$P +grabright;vr_mock_fingers main 1 1;vr_mock_button main grip 1;wait40;"
P="$P echo CASE-C-punch-through-head;" ; punch 64 64
P="$P echo CASE-D1-diagonal-head-priority-on;vr_hit_head_priority 1;"; punch 92 46
P="$P echo CASE-D2-diagonal-head-priority-off;vr_hit_head_priority 0;"; punch 92 46
P="$P echo CASE-E1-chest-low-head-priority-on;vr_hit_head_priority 1;"; punch 50 50
P="$P echo CASE-E2-chest-low-head-priority-off;vr_hit_head_priority 0;"; punch 50 50
P="$P vr_hit_head_priority 1;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Timeout 600 -Script "$P" -Filter "CASE-|damage:|Dummy: |decaptest: 17" 2>&1 | grep "CASE-\|damage:\|Dummy: \|decaptest: 17" > Misc/quakevr/scratch/hz_zones.txt
cat Misc/quakevr/scratch/hz_zones.txt
