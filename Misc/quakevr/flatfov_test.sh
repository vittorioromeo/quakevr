#!/bin/bash
# flatfov_test.sh <agent> [label] [wids]: flatscreen play's model FOV (vr_flat_model_fov; ROUND24.md, "Flatscreen model
# FOV, second pass"), real path, pixels. fov 70, the world hidden (r_drawworld 0: only the player's models and the clear
# colour), vr_flat_model_fov_near 200 / _far 201 so every part of his models is scaled alike: the frame at model FOV M is
# then the model-FOV-70 frame shrunk round the screen's middle by tan(35)/tan(M/2), lit the same (his models keep their
# true place for the light). Each weapon (vr_test_give_wid; impulse 149) at model FOV 70, 90, 110, each with the chest
# flashlight (a dynamic light) on and off. flatfov_check.py judges:
#   size:  the drawn models' pixels against the 70 frame shrunk (IoU); the heavy revolver's parts were drawn at 70 still.
#   light: the flashlight's share (lit / unlit) on the parts whose true place is off the screen against those just inside
#          it; it was none there (a dark band below a line).
# Ends on its own line (-QuitOn "flatfov: shots done"); prints `flatfov: N passed, M failed`.
AGENT=${1:-notesb}; LABEL=${2:-now}; WIDS=${3:-"1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 29 30 31"}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
HERE=$(dirname "$0"); mkdir -p "$HERE/../../scratch"
S="map e1m1; god; notarget; viewsize 120; crosshair 0; fov 70; vr_flat_bob 0; gl_clear 1; r_drawworld 0; vr_flashlight_give left"
S="$S; vr_flat_model_fov_near 200; vr_flat_model_fov_far 201"
for w in $WIDS; do
    S="$S; vr_test_give_wid $w; impulse 149; wait60"
    for m in 70 90 110; do
        S="$S; vr_flat_model_fov $m; wait10; screenshot; vr_flashlight_toggle; wait10; screenshot; vr_flashlight_toggle"
    done
done
S="$S; echo flatfov: shots done; toggleconsole; quit"
LOG="$HERE/../../scratch/vm_modelfov_$LABEL.log"
bash "$KIT/run.sh" "$AGENT" -ExtraArgs "-flatscreenmode" -Clean -Script "$S" -QuitOn "flatfov: shots done" \
    -Filter "flatfov|ENGINE|TIMEOUT|composed|exit=" -Timeout 600 -Out "vm_modelfov_$LABEL.png" > "$LOG" 2>&1
grep -a -E "ENGINE|TIMEOUT" "$LOG" | cut -c1-160
SHOTS=$(grep -a -h 'full size in' "$LOG" | sed 's/.*full size in //' | tr -d '\r' | tail -1)
python "$HERE/flatfov_check.py" "$SHOTS" "$WIDS" "$HERE/../../scratch/vm_modelfov_${LABEL}_sheet.png"
