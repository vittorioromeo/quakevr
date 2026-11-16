#!/usr/bin/env bash
# vrslide_test.sh -- the headset's slide (vr_slide; Quake/vr/vr_slide.cpp, ROUND25.md "Sliding in VR"), with the mock
# headset's sticks: vr_slide 0 never slides (the stick's down is +movedown as before); vr_slide 1: pulled 30 and 45
# degrees off down, short of the threshold, snap and smooth turns (swept round to down, or a little downwards) never
# slide; straight down from the middle slides as the flat slide does (its boost, length and end, a crouched box, its
# scrape, no +movedown), up again after it; let go mid-slide, it ends. The flat slide's own tests: flatslide_test.sh.
# Usage: bash Misc/quakevr/vrslide_test.sh <agent>
AGENT=${1:-selfshot}
QT_NAME=vrslidetest
source "$(dirname "$0")/qvrtest.sh"

out=$(bash $KIT/run.sh $AGENT -QuitOn "^vrslide: [0-9]+ passed" -Timeout 240 -Filter "^vrslide" \
    -Script "map vrtesthall; wait60; god; notarget; vr_stamina_speed 0; wait10; vr_slide_test; wait30000; toggleconsole; quit" 2>&1)
echo "$out" | grep -a "^vrslide.*\(FAIL\|passed\)"
expect_verdict "$out" "vrslide" "VR slide: the stick straight down slides as the flat slide; off, angled, short or turning never"
verdict; exit $fail
