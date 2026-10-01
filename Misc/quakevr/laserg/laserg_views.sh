#!/bin/bash
# The laser cannon (refine_laserg.py) in the engine: held level in the main hand at e1m1's start, shot from the right,
# the left, below and behind (mock camera), one screenshot each.
# Usage: bash Misc/quakevr/laserg/laserg_views.sh <agent name> [out.png]
name=${1:?agent name}; out=${2:-laserg_views.png}
H="vr_mock_hand main 0.10 1.40 -0.32 70 0 0"
S="map e1m1;wait60;god;notarget;vr_weapon_grip_mode 1;impulse 162;wait30;vr_mock_look 0 0;$H;wait30"
for cam in "0.95 1.40 -0.85 0 90" "-0.75 1.40 -0.85 0 -90" "0.10 0.75 -0.85 -85 0" "0.45 1.75 0.15 30 30"; do
  S="$S;vr_mock_camera $cam;wait10;screenshot"
done
S="$S;vr_mock_camera;echo DONE;toggleconsole;quit"
bash C:/OHWorkspace/qvr-kit/run.sh "$name" -Clean -Script "$S" -Filter "DONE|rror|laserg" -Out "$out" -Timeout 120
