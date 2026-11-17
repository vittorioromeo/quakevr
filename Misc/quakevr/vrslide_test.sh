#!/usr/bin/env bash
# vrslide_test.sh -- the headset's slide (vr_slide; Quake/vr/vr_slide.cpp, ROUND25.md "Sliding in VR" and "VR slide:
# the flat slide's body and eye height"), with the mock headset's sticks: vr_slide 0 never slides (the stick's down is
# +movedown as before); vr_slide 1: pulled 30 and 45 degrees off down, short of the threshold, snap and smooth turns
# (swept round to down, or a little downwards) never slide; straight down from the middle slides as the flat slide does
# (its boost, length and end, a crouched box, its scrape, no +movedown), up again after it; let go mid-slide, it ends.
# The view (vr_slide_camera 1) comes down to the flat slide's eyes within vr_slide_camera_time, no overshoot, and back up
# to the real head as quickly after it; with vr_slide_camera 0 it stays at the real head. The body 0.5 s into the slide
# matches the flat slide's (vr_slide_pose_test, -flatscreenmode: its head, hips, knees and ankles within 1.5 units).
# The flat slide's own tests: flatslide_test.sh.
# Usage: bash Misc/quakevr/vrslide_test.sh <agent>
AGENT=${1:-selfshot}
QT_NAME=vrslidetest
source "$(dirname "$0")/qvrtest.sh"

# (The headset's body sized as the flat one, vr_flat_body_height's 1.55 m: the same body to compare the pose of.)
out=$(bash $KIT/run.sh $AGENT -QuitOn "^vrslide: [0-9]+ passed" -Timeout 240 -Filter "^vrslide" \
    -Script "map vrtesthall; wait60; god; notarget; vr_stamina_speed 0; vr_height_calibration 1.55; vr_flat_body_height 1.55; wait10; vr_slide_test; wait30000; toggleconsole; quit" 2>&1)
echo "$out" | grep -a "^vrslide.*\(FAIL\|passed\)"
expect_verdict "$out" "vrslide" "VR slide: the stick straight down slides as the flat slide; off, angled, short or turning never; the view down to its eyes and back"
vrpose=$(echo "$out" | grep -a "^vrslide_pose: pose" | head -2 | tr '\n' ' ')

flat=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -QuitOn "^flatslide_pose: (done|timed out)" -Timeout 120 \
    -Filter "^flatslide_pose" \
    -Script "map vrtesthall; wait60; god; notarget; cl_alwaysrun 1; vr_stamina_speed 0; vr_flat_body_height 1.55; wait10; vr_slide_pose_test; +forward; wait3000; toggleconsole; quit" 2>&1)
flatpose=$(echo "$flat" | grep -a "^flatslide_pose: pose" | head -2 | tr '\n' ' ')
echo "$vrpose"
echo "$flatpose"
worst=$(printf '%s\n%s\n' "$vrpose" "$flatpose" | awk '
    NR == 1 { for(i = 1; i <= NF; i++) a[i] = $i; n = NF }
    NR == 2 { worst = 0; split("4 6 7 8 9 10 11 12 24 27", f, " ")
              for(k in f) { i = f[k] + 0; d = a[i] - $i; if(d < 0) d = -d; if(d > worst) worst = d }
              if(NF != n || n < 27) worst = 999; printf "%.1f", worst }')
# (The fields: the body's floor, the head, both hips (forward, left, up), both ankles' heights. The legs' reach is not
# compared: a headset's legs are fitted to the height he stands at, the mock's taller than the flat body's 1.55 m.)
ok=$([ -n "$vrpose" ] && [ -n "$flatpose" ] && awk -v w="${worst:-999}" 'BEGIN { exit !(w <= 1.5) }' && echo 1 || echo 0)
check $ok "VR slide: the body 0.5 s into it is the flat slide's (floor, head, hips, feet on its floor: worst ${worst:-?} units off, at most 1.5)"
verdict; exit $fail
