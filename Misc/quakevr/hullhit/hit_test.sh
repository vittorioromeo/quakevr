#!/bin/bash
# Monsters' attacks against the player's hit box (vr_hull_hit_width; ROUND21.md, "Player hitbox defaults").
# Usage: bash hit_test.sh <worktree> [frames] [kinds] [widths]
#   kinds: vr_test_spawn's numbers "kind:dist" (0 grunt, 1 ogre, 5 knight, 7 dog); widths: vr_hull_hit_width values
#   (0: Quake's 32). On e1m1's start, skill 1: the monster is put ahead of the player (5000 health, standing still) and
#   left to attack; prints the health the player lost after the frames.
wt=${1:?worktree name}; frames=${2:-1500}
kinds=${3:-"0:150 1:250 5:150 7:200"}
widths=${4:-"0 24 16"}
for kd in $kinds; do
    kind=${kd%%:*}; dist=${kd##*:}
    for w in $widths; do
        h=$(bash C:/OHWorkspace/qvr-kit/run.sh "$wt" -Filter "^health|Host_Error|SV_Error" -Script \
            "skill 1;map e1m1;wait60;impulse 9;wait5;vr_hull_hit_width $w;give h 5000;vr_test_spawn $kind;vr_test_spawn_dist $dist;impulse 241;wait$frames;edict 1;toggleconsole;quit" \
            | grep -m1 -o "health *[0-9-]*" | grep -o "[0-9-]*$")
        echo "kind $kind dist $dist width $w: lost $((5000 - ${h:-5000}))"
    done
done
