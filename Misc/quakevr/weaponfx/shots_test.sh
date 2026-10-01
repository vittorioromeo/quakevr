#!/bin/bash
# Weapon effects' pictures (ROUND21.md, "Weapon effects: recoil, muzzle flash, tracers"): the burst rifle's flash and 6
# tracers (vr_weaponfx_test, slowed to 15 m/s to be seen in flight) from the side, then a grunt 250 units ahead firing at you (its flash
# at its gun, its tracer: vr_debug_weaponfx 3 takes the view at its first round), each taken with vr_eyeshot 1 (quakevr/eyeshots).
# Usage: bash Misc/quakevr/weaponfx/shots_test.sh <agent name> [camera yaw]
name=${1:?agent name}; yaw=${2:-90}
S="map e1m1;wait60;god;developer 1;vr_debug_weaponfx 1;vr_weapon_grip_mode 1;impulse 165;wait60;vr_mock_look 0 0;vr_mock_hand main 0.10 1.40 -0.32 70 0 0;wait30;vr_tracer_speed 15;vr_mock_camera 0.70 1.50 -0.45 15 $yaw;wait2;vr_weaponfx_test 1 6;wait3;vr_eyeshot 1;wait3;vr_tracer_speed 150;vr_mock_camera;vr_tracer_chance 1;vr_test_spawn 0;vr_test_spawn_dist 250;vr_debug_weaponfx 3;impulse 241;wait100;wait100;wait100;echo DONE;toggleconsole;quit"
bash C:/OHWorkspace/qvr-kit/run.sh "$name" -Script "$S" -Filter "weaponfx|DONE|rror|eyeshots" -Timeout 120 "${@:3}"
