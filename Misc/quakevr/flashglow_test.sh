#!/bin/bash
# flashglow_test.sh <agent> [tag] (ROUND24.md, "Flat-screen: the hidden torch's lens glow"): the flashlight's lit lens follows its model. Flat-screen (the light on from the start), the
# torch hidden (vr_flat_flashlight_hide 1): at a wall and at open space, then the light off; vr_flat_flashlight_hide 0:
# the lens back. The VR mock, the torch on the head, lit: the lens drawn. Screenshots scratch/flashglow_<tag>_*.png;
# the verdict line "flashglow: N passed, M failed".
AGENT=${1:-embed5}
TAG=${2:-after}
KIT=C:/OHWorkspace/qvr-kit
W=C:/OHWorkspace/qvr-agents/$AGENT
LOG=$W/scratch/flashglow_${TAG}.log
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Out $W/scratch/flashglow_${TAG}_flat.png -Script "map e1m1; wait60; god; notarget; r_norefresh 0; setpos 480 -352 88 0 0 0; wait20; vr_flashlight_probe flatwall; screenshot; setpos 480 -352 88 0 90 0; wait20; vr_flashlight_probe flatopen; screenshot; vr_flashlight_toggle; wait20; screenshot; vr_flashlight_toggle; vr_flat_flashlight_hide 0; wait20; vr_flashlight_probe flatshown; screenshot; vr_flat_flashlight_hide 1; echo flashglow: flat done; toggleconsole; quit" -QuitOn "flashglow: flat done" -Filter "flashglow|torchprobe .* (lens|mode)" -Timeout 120 -Clean > $LOG 2>&1
bash $KIT/run.sh $AGENT -Out $W/scratch/flashglow_${TAG}_vr.png -Script "map e1m1; wait60; god; notarget; r_norefresh 0; vr_flashlight 1; wait20; vr_flashlight_clip_head right; wait10; vr_flashlight_toggle; wait20; vr_flashlight_probe vrhead; screenshot; echo flashglow: vr done; toggleconsole; quit" -QuitOn "flashglow: vr done" -Filter "flashglow|torchprobe .* (lens|mode)" -Timeout 120 -Clean >> $LOG 2>&1
grep -E "torchprobe|composed|exit=" $LOG
python $(dirname "$0")/flashglow_check.py $LOG
