#!/bin/bash
# spacewarp_test.sh <agent>: SpaceWarp's motion vectors and depth (vr_spacewarp.cpp) made into the game's own images
# (vr_spacewarp_test 1: no runtime with XR_FB_space_warp on the PC), the left eye's read back (vr_spacewarp_check). start,
# facing the hall: still (the world and the models without motion, a depth range,
# no play-space delta); the gun brought up in front of the eyes (its own motion as the hand eases there, the world's none); walking forward (the world
# streams by, the play space's delta forward, the held gun still on the eyes); turning (the delta turns, the world moves
# on the eyes: the main stick's smooth turn); a jump (setpos: the frame marked skipped). Ends on
# `spacewarp: N passed, M failed`.
AGENT=${1:?agent}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
F='spacewarp|SpaceWarp|ENGINE CRASH|Host_Error'
S='vid_fsaa 0;vr_spacewarp_test 1;map start;wait60;god;notarget;setpos 544 420 24 0 270 0'
S="$S;wait12"
S="$S;vr_spacewarp_check begin;vr_spacewarp_check still;vr_mock_hand main 0.05 1.45 -0.35;wait6;vr_spacewarp_check wrist;wait40"
S="$S;+forward;wait12;vr_spacewarp_check moving;-forward;wait12"
S="$S;vr_mock_stick main 1 0;wait12;vr_spacewarp_check turning;vr_mock_stick main 0 0;wait12"
S="$S;setpos 544 300 24 0 270 0;wait2;vr_spacewarp_check jumped"
S="$S;vr_spacewarp_check verdict;toggleconsole;quit"
bash "$KIT/run.sh" "$AGENT" -Timeout 300 -QuitOn 'spacewarp: [0-9]+ passed' -Script "$S" -Filter "$F"
