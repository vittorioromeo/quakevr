#!/bin/bash
# An ogre's grenades at the player (notarget: it only throws when told, impulse 240), 10 throws from 250 units on e1m1's
# start: the health lost and the direct hits (developer 1's "grenade: ... hits ..."), for each vr_hull_hit_width.
wt=${1:?worktree name}; widths=${2:-"0 24"}
for w in $widths; do
    s="skill 0;map e1m1;wait60;impulse 9;wait5;notarget;vr_hull_hit_width $w;give h 5000;vr_test_spawn 1;vr_test_spawn_dist 250;impulse 241;wait120;developer 1"
    for i in 1 2 3 4 5 6 7 8 9 10; do s="$s;impulse 240;wait200"; done
    out=$(bash C:/OHWorkspace/qvr-kit/run.sh "$wt" -Filter "^health|grenade: .* hits|Host_Error|SV_Error" -Script "$s;developer 0;edict 1;toggleconsole;quit")
    h=$(echo "$out" | grep -m1 -o "health *[0-9-]*" | grep -o "[0-9-]*$")
    echo "ogre grenades, width $w: lost $((5000 - ${h:-5000})), direct hits $(echo "$out" | grep -c "hits player")"
    echo "$out" | grep "hits" | sort | uniq -c | head -5
done
