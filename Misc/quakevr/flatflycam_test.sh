#!/bin/bash
# flatflycam_test.sh <agent> -- flat-screen play's fly camera (vr_flat_flycam; ROUND24.md, "Flat-screen fly camera";
# Quake/vr/vr_flycam.cpp). Headless, fast mode; each run ends on its own verdict line (-QuitOn), and the script prints
# "flatflycam: N passed, M failed".
#   flight   vr_flat_flycam_test on e1m1: on (paused), turned round with +right, flown 230 units back and up with +back and
#            +jump (+attack held), a screenshot; flown in to 80 units and looked down at him (+forward, +lookdown), a second
#            one; then vr_flat_flycam_pause 0 for a second with +forward, +jump, +attack held; then off. Checks: his
#            origin, angles and hands unchanged, no button or impulse reached the server, the game's time held (pause 1)
#            and ran (pause 0), the view back in his eye exactly. The two screenshots: scratch/flatflycam.png.
#   vr       in the mock headset the toggle does nothing but say so, and vr_flat_flycam 1 goes back to 0.
AGENT=${1:-flatwall}
QT_NAME=flatflycam
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch; mkdir -p "$S"

out=$(bash $KIT/run.sh $AGENT -Clean -ExtraArgs "-flatscreenmode" -QuitOn "^vr_flat_flycam_test: [0-9]+ passed" -Timeout 120 \
    -Filter "^flycam_test: (FAIL|paused|running)|^vr_flat_flycam_test" -Out flatflycam.png \
    -Script "map e1m1; god; notarget; r_norefresh 0; setpos 480 -352 88 0 0 0; impulse 154; vr_flat_flycam_test; wait3000; toggleconsole; quit" 2>&1)
echo "$out" | grep -a "^flycam_test: \(FAIL\|paused\|running\)"
expect_verdict "$out" "vr_flat_flycam_test" "flight: he stands still, the game held then running on, the view back in his eye"
check $(grep -aq "composed 2 screenshots" <<<"$out" && echo 1 || echo 0) "flight: two screenshots (scratch/flatflycam.png)"

out=$(bash $KIT/run.sh $AGENT -QuitOn "^flatflycam: vr done" -Timeout 120 -Filter "vr_flat_flycam|^flycam|^flatflycam" \
    -Script "map e1m1; wait10; vr_flat_flycam_toggle; vr_flat_flycam 1; wait5; vr_flat_flycam; echo flatflycam: vr done; wait3000; toggleconsole; quit" 2>&1)
check $(grep -aq "^vr_flat_flycam_toggle: flat-screen play only" <<<"$out" && grep -aq '"vr_flat_flycam" is "0"' <<<"$out" && ! grep -aq "^flycam: on" <<<"$out" && echo 1 || echo 0) \
    "vr: the toggle only says it is flat-screen play's, vr_flat_flycam 1 goes back to 0"

verdict; exit $fail
