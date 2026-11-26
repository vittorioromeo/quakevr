#!/bin/bash
# darkflash_test.sh <agent> [tag] (ROUND26.md, "questrender"): the flashlight lights the faces without baked light in
# the Standalone preset. vrtutorial's dark course (lesson 9), the Standalone preset's dynamic lights in the lightmap
# (vr_gl_world_dlmap; on the PC through vr_gl_pc_forms' unlit world form), a head torch on, looking down the first
# corridor: the light the build added on the block's face ~310 units ahead and on the far wall ~680 ahead (both faces no
# map light reaches: before the fix they shared one black block and stayed pitch black). Picture
# scratch/darkflash_<tag>.png; the verdict line "dlmap_probe: N passed, M failed".
AGENT=${1:-questi}
TAG=${2:-after}
KIT=C:/OHWorkspace/qvr-kit
W=C:/OHWorkspace/qvr-agents/$AGENT
LOG=$W/scratch/darkflash_${TAG}.log
bash $KIT/run.sh $AGENT -Out $W/scratch/darkflash_${TAG}.png -Script "vr_gl_pc_forms 15; vr_standalone_preset 0; map vrtutorial; wait30; god; notarget; setpos 4390 330 -100 12 270 0; wait10; vr_flashlight 1; vr_flashlight_clip_head right; wait5; vr_flashlight_toggle; wait5; vr_gl_world_dlmap_info; vr_gl_world_dlmap_probe 4390 16 -124 0.05; vr_gl_world_dlmap_probe 4390 -352 -110 0.01; screenshot; vr_gl_world_dlmap_probe end; toggleconsole; quit" -QuitOn "dlmap_probe: [0-9]+ passed" -Filter "dlmap" -Timeout 150 > $LOG 2>&1
grep -E "dlmap|composed|exit=" $LOG
