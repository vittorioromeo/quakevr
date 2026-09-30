#!/bin/bash
# An ogre's grenade splash when the grenade misses the player's box: the ogre (250 units ahead on e1m1's start, notarget)
# throws at the player (impulse 240), who then steps <offset> units aside (setpos), so the grenade passes that far from
# the box's centre: 14 is inside Quake's 32 box and outside a 24 one. Prints the health lost and the direct hits.
wt=${1:?worktree name}; off=${2:-14}; widths=${3:-"0 24"}
for w in $widths; do
    s="skill 0;map e1m1;wait60;impulse 9;wait5;notarget;vr_hull_hit_width $w;give h 5000;vr_test_spawn 1;vr_test_spawn_dist 250;impulse 241;wait120;developer 1"
    for i in 1 2 3 4 5 6 7 8 9 10; do s="$s;setpos 480 -352 88;wait5;impulse 240;wait3;setpos $((480 + off)) -352 88;wait200"; done
    out=$(bash C:/OHWorkspace/qvr-kit/run.sh "$wt" -Filter "^health|grenade: .* hits|Host_Error|SV_Error" -Script "$s;developer 0;edict 1;toggleconsole;quit")
    h=$(echo "$out" | grep -m1 -o "health *[0-9-]*" | grep -o "[0-9-]*$")
    echo "ogre splash, aside $off, width $w: lost $((5000 - ${h:-5000})), direct hits $(echo "$out" | grep -c "hits player")"
done
