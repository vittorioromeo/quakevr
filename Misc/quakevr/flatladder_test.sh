#!/bin/bash
# flatladder_test.sh <agent> -- flatscreen play's ledge ladders (-flatscreenmode; vr_flat_ladder; Quake/vr/vr_climb.cpp
# flatLadder, flatLadderFrame, flatLadderMount; docs/vr-port/FLATPLAY.md, "Ladders"), headless. Prints PASS/FAIL lines
# and a verdict, exits 1 on a failure.
#   vr_flatladder_test in vrtutorial (its lesson 3 ladder: rungs every 20 up a 156 high block): walked into with forward
#   held, climbed hand over hand (the hold serials change) and mantled over the top; backed off its top (lowered onto
#   it, the mantle's way backwards) and climbed down to the ground; climbed up then down looking down; climbed up then
#   pushed off with jump. And no ladder: a plain wall, single ledges 48 and 96 high, the 100 high jump wall, the pit's
#   stairs (walked up). Then vr_flat_ladder 0: the ladder walked into, no climb.
AGENT=${1:-dagger}
QT_NAME=flatladder
source "$(dirname "$0")/qvrtest.sh"
# (its runs played as batches: many cases a game, copies in parallel; TESTING.md "Batched test cases")
source "$(dirname "$0")/testbatch/testbatch.sh"
# (the batch at run.sh's own clock: vr_fixed_frames_rate 72, a tick a frame, as each run alone; and the collect
# pass without its awk/grep checks)
export TB_RATE=${TB_RATE-72} TB_COLLECT_STUBS=${TB_COLLECT_STUBS-2}
# (the batch's frames undrawn, vr_mock_fast 2: its checks are QC-measured, none reads the drawing; the same verdicts
# drawn and undrawn, testbatch3. TB_NODRAW=0: drawn)
export TB_NODRAW=${TB_NODRAW-1}
F=(-ExtraArgs "-flatscreenmode")
H="map vrtutorial; wait60; god; notarget"

tb_body() {
out=$(tb_run_sh "${F[@]}" -QuitOn "^flatladder: [0-9]+ passed" -Timeout 180 -Filter "^flatladder" \
    -Script "$H; vr_flatladder_test; wait9000; toggleconsole; quit" 2>&1)
echo "$out" | grep -a "^flatladder: \(PASS\|FAIL\)"
expect_verdict "$out" "flatladder" "ledge ladder: up and over, down from the top, down looking down, push off; no ladder at a wall, single ledges, stairs"

out=$(tb_run_sh "${F[@]}" -QuitOn "^flatladder: [0-9]+ passed" -Timeout 120 -Filter "^flatladder" \
    -Script "$H; vr_flat_ladder 0; vr_flatladder_test 9; wait3000; toggleconsole; quit" 2>&1)
echo "$out" | grep -a "^flatladder: \(PASS\|FAIL\)"
expect_verdict "$out" "flatladder" "ledge ladder: vr_flat_ladder 0, walked into: no climb"
verdict
}
tb_main "$AGENT" tb_body
exit $fail
