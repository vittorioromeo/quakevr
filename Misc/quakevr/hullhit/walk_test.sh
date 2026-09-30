#!/bin/bash
# Random walks with the shipped hitbox defaults (ROUND21.md, "Player hitbox defaults"): vr_hull_walktest on each map.
# Usage: bash walk_test.sh <worktree> [seconds] [seed] [maps]
wt=${1:?worktree name}; secs=${2:-90}; seed=${3:-7}; maps=${4:-"e1m1 e1m3 e2m2 e3m3 e4m1"}
for m in $maps; do
    bash C:/OHWorkspace/qvr-kit/run.sh "$wt" -Timeout 600 -Filter "^hullwalk|Host_Error|SV_Error|^\"vr_hull_width\"" -Script \
        "map $m;wait60;vr_hull_width;god;notarget;vr_hull_walktest $secs $seed;wait$((secs * 160));toggleconsole;quit" | grep -v "^$\|exit=0"
done
