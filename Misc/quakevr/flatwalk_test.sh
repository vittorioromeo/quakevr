#!/bin/bash
# flatwalk_test.sh <agent> [console commands] [shots every N frames]: the flatscreen hands, arms and body while he walks and runs
# (docs/vr-port/FLATPLAY.md, "Bob and sway"). Each run holds +forward on e1m1's start and ends on vr_flat_walk_probe's own line
# (-QuitOn `flatwalk: ... done`): the wrist on the arm against the drawn hand, in the camera's frame (off its rest place: the
# hand detached from the forearm; and its most in a frame: a jump), each hand's and shoulder's range on the screen. Fast mode:
# game logic as ever, frames uncapped. Verdict: `flatwalk: N passed, M failed`.
AGENT=${1:-flatwalk}; EXTRA=${2:-echo}; SHOTS=${3:-0}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"
PASS=0; FAIL=0
GAP_MOST=0.5   # units: the wrist off where it sat on the hand at rest
JUMP_MOST=0.15 # units a frame
run() { # label, speed, weapon id, ads, crouch
    log=$HERE/scratch/flatwalk_$1.log
    aim=""; [ "$4" = 1 ] && aim="vr_flat_aim 1; wait30;"
    crouch=""; [ "$5" = 1 ] && crouch="+flatcrouch; wait30;"
    bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map e1m1; god; notarget; r_norefresh 0; viewsize 120; cl_forwardspeed $2; vr_test_give_wid $3; impulse 149; wait30; $crouch $aim $EXTRA; vr_flat_walk_probe $1 72 $SHOTS" \
        -QuitOn "flatwalk: .* (done|FAIL)" -Filter "flatwalk: |ENGINE|TIMEOUT" -Timeout 120 -Out "fw_raw_$1.png" > "$log" 2>&1
    line=$(grep -a -h "flatwalk: $1 " "$log" | tail -1 | tr -d '\r')
    echo "$line" | cut -c1-230
    verdict=$(echo "$line" | python "$HERE/flatwalk_judge.py" $GAP_MOST $JUMP_MOST)
    echo "  $verdict"
    case "$verdict" in pass*) PASS=$((PASS + 1));; *) FAIL=$((FAIL + 1));; esac
}
run walk_hip 160 4 0 0
run run_hip 320 4 0 0
run run_pistol 320 31 0 0
run run_crouch 320 4 0 1
run walk_ads 160 4 1 0
run run_ads 320 4 1 0
echo "flatwalk: $PASS passed, $FAIL failed"
