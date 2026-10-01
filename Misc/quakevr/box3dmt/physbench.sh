#!/bin/bash
# Box3D on the pool (ROUND21.md, "Box3D on the pool"): a stress scene on vrfiringrange, <count> props (half small
# explosive boxes, half crates) spawned and stacked in toppling columns of 10, then tossed twice (vr_physics_fling); prints Box3D's step
# time (vr_physics_steptime: the collapse, then the tosses) and the props' hash (vr_physics_hash: determinism) after.
# Usage: bash Misc/quakevr/box3dmt/physbench.sh <agent name> <count> "<cvars>" [run.sh options, e.g. --exclusive]
#   e.g. physbench.sh box3dmt 100 "vr_box3d_threads 0"     (the main thread alone)
#        physbench.sh box3dmt 100 "vr_box3d_workers 0"     (every worker of the pool)
#   PB_CRATES=0: explosive boxes only (crates draw random numbers: no two runs alike)
name=${1:?agent name}; count=${2:-100}; cvars=${3:-}; shift 3
q="C:/OHWorkspace/qvr-agents/$name/quakevr"
{
  echo 'alias w5 "wait;wait;wait;wait;wait"'
  echo 'alias w30 "w5;w5;w5;w5;w5;w5"'
  echo 'alias w60 "w30;w30"'
  half=$((count / 2)); [ "${PB_CRATES:-1}" = 0 ] && half=0
  for ((i = 0; i < count - half; i++)); do echo "vr_physics_spawn misc_explobox2 100"; done
  for ((i = 0; i < half; i++)); do echo "vr_physics_spawn vr_crate 100"; done
  echo "vr_physics_pile misc_explobox2 10 -180 -456 40 40"
  echo "vr_physics_pile vr_crate 10 -180 -376 40 48"
} > "$q/physbench_scene.cfg"
S="$cvars;map vrfiringrange;wait60;exec physbench_scene.cfg;wait2;vr_physics_steptime;w60;w60;w60;w60;echo BENCH_COLLAPSE;vr_physics_steptime;vr_physics_fling props 150 90 350;w60;vr_physics_fling props 150 270 350;w60;w60;echo BENCH_TOSS;vr_physics_steptime;vr_physics_hash;toggleconsole;quit"
bash C:/OHWorkspace/qvr-kit/run.sh "$@" "$name" -Script "$S" -Filter "BENCH_|steptime:|physics_hash|physics_pile|rror|ENGINE" -Timeout 300
