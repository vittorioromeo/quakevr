#!/bin/bash
# neck_loose_test.sh <agent> <loose 0|1> [kd] -- a grunt 64 units ahead (vrtesthall) has its neck snapped (vr_foegrab_necksnap_test);
# its ragdoll's head-to-chest swing and twist sampled every 6 frames for 3.3 s (NECKANGLE). kd: knocked down (pushed hard)
# first, snapped 10 frames into its fall.
AGENT=${1:?}
LOOSE=${2:-1}
MODE=${3:-stand}
KIT=C:/OHWorkspace/qvr-kit
SAMPLES=""
for i in $(seq 1 40); do SAMPLES="$SAMPLES;wait6;vr_foegrab_neck_angle"; done
PRE="wait30;god;notarget;developer 1;vr_knockdown_debug 1;vr_ragdoll_active_seed 0;sv_random_seed 7;vr_foegrab_necksnap_loose $LOOSE"
S="map vrtesthall;$PRE;vr_mock_walk_to 0 -200 16;wait120;vr_mock_walk_to off;wait5;vr_test_spawn 0;vr_test_spawn_dist 64;impulse 241;wait60"
if [ "$MODE" = kd ]; then S="$S;vr_knockdown_push 2;vr_knockdown_test 0;wait10"; fi
S="$S;vr_foegrab_necksnap_test${SAMPLES}"
S="$S;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "NECKANGLE|necksnap|vr_foegrab_necksnap_test|rror|CRASH" -Timeout 300
