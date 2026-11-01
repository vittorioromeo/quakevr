#!/bin/bash
# closequarters_test.sh <agent>: the close-quarters slowdown (vr_closequarters; ROUND23.md, "Close-quarters slowdown").
# A grunt spawned 120 units ahead (notarget: it stands), walked straight into (250 frames: it is reached in about 100)
# then "approach": unslowed far off, slowed only within Start Radius, to Min Factor at its face; strafed past (12
# frames) then "end": the sideways move kept Strafe Factor of its speed, and the factors' rule on the settings. Ends on
# "vr_closequarters_test: N passed, M failed".
KIT=C:/OHWorkspace/qvr-kit
AGENT=${1:-posepromo}
S="sv_random_seed 7;map vrfiringrange;wait30;god;notarget;vr_test_spawn 0;vr_test_spawn_dist 120;impulse 241;wait40"
S="$S;vr_closequarters_test start;+forward;wait250;-forward;wait2;vr_closequarters_test approach"
S="$S;+moveright;wait12;-moveright;vr_closequarters_test end;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "vr_closequarters_test" -QuitOn "vr_closequarters_test: \d+ passed"
