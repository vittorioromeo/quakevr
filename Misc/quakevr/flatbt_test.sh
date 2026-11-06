#!/bin/bash
# flatbt_test.sh <agent> -- flatscreen play's bullet time look (-flatscreenmode): the window gets the headset's look
# (desaturated, tinted, vignetted; vr_bullettime_fx*, eased by the same meter state). Two frames of the same scene, bullet
# time off and on (looking at the same spot, the HUD off), compared by flatbt_check.py: the on frame has less colour, is
# darker at the corners than in the middle, and the middle is tinted. Prints PASS/FAIL, exits 1 on a failure.
AGENT=${1:-probes}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch; mkdir -p "$S"
HERE=$(cd "$(dirname "$0")" && pwd -W)
P="map e1m1; wait60; developer 1; god; notarget; hudstyle 1; viewsize 120; crosshair 0; vr_crosshair 0; vr_bullettime_enabled 1; vr_debug_bullettime 1; tune vr_flat_bob 0; tune vr_flat_sway 0"
run() { local tag=$1 script=$2; shift 2
        bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Low "$@" -Script "$script; wait3; tune_reset; toggleconsole; quit" -Filter "^bullet time" -Out "$S/$tag.png" > "$S/$tag.log" 2>&1; }
run btoff "$P; wait30; screenshot"
run bton "$P; vr_bullettime; wait90; screenshot"
out=$(python "$HERE/flatbt_check.py" "$S/btoff.png" "$S/bton.png")
echo "$out"
case "$out" in flatbt:*PASS*) echo "flatbt: PASS";; *) echo "flatbt: FAIL"; fail=1;; esac
exit $fail
