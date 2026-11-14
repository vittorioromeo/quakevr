#!/bin/bash
# Random walks with the shipped hitbox defaults (ROUND21.md, "Player hitbox defaults"): vr_hull_walktest on each map.
# Usage: bash walk_test.sh <worktree> [seconds] [seed] [maps]
# Each run ends at its walk's summary line (-QuitOn; the log flushed every frame). The summary is matched unanchored:
# it can follow a console bar left open on the same log line, so "^hullwalk" missed it and the run waited out its
# wait. The last line counts the maps whose walk printed none.
wt=${1:?worktree name}; secs=${2:-90}; seed=${3:-7}; maps=${4:-"e1m1 e1m3 e2m2 e3m3 e4m1"}
n=0; missing=0
for m in $maps; do
    out=$(bash C:/OHWorkspace/qvr-kit/run.sh "$wt" -Timeout 600 -QuitOn "hullwalk .*: width" \
        -Filter "hullwalk|Host_Error|SV_Error|^\"vr_hull_width\"" -Script \
        "map $m;wait60;vr_hull_width;god;notarget;vr_hull_walktest $secs $seed;wait$((secs * 160));toggleconsole;quit")
    echo "$out" | grep -v "^$\|exit=0"
    n=$((n + 1))
    echo "$out" | grep -q "hullwalk .*: width" || { missing=$((missing + 1)); echo "walk_test: FAIL $m: no summary"; }
done
echo "walk_test: $n maps, $missing without a summary"
