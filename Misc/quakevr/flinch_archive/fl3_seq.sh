#!/bin/bash
# flinch3: the shotgun -> super shotgun -> blow sequence (fl_classes.sh), RUNS times per class (0 grunt, 8 enforcer).
# Usage: bash fl3_seq.sh <outfile> [extra commands before the tests]
OUT=$1
EXTRA=$2
: > $OUT
for n in ${CLASSES:-0 8}; do
  for k in $(seq 1 ${RUNS:-10}); do
    S="map vrtesthall; wait30; god; notarget; developer 1; vr_flinch_debug 3; $EXTRA vr_knockdown_time_min 0.5; vr_knockdown_time_max 0.5; vr_test_spawn $n; vr_test_spawn_dist 140; impulse 241; wait90; ${SEQ:-vr_flinch_test 1; wait300; vr_flinch_test 2; wait500; vr_flinch_test 5; wait400;} vr_flinch_status; toggleconsole; quit"
    echo "== spawn $n run $k" >> $OUT
    bash C:/OHWorkspace/qvr-kit/run.sh fastship -Script "$S" -Filter "flinch: [0-9]+ (rec|topp|monster_[a-z_]* struck)|flinch: [0-9]* *struck (in|with)|flinch test|ENGINE|xception" -Timeout 150 | grep -v "^exit=0" | cut -c1-220 >> $OUT
  done
done
