#!/bin/bash
# The enforcers' laser rifle (ROUND21.md, "The enforcers' rifle: low zeroed sights, recoil, muzzle flashes"): held in the
# main hand at e1m1's start, its sights checked against its laser (vr_sight_check: the laser's angle to the sight line
# and how far off it it is at 2..50 m), one shot fired (vr_debug_weaponfx 2: the kick, the flash; an eyeshot from the
# side at the flash), then an enforcer 250 units ahead firing at you (vr_debug_weaponfx 3: the view at its first flash).
# Usage: bash Misc/quakevr/enfrifle/rifle_test.sh <agent name> [extra commands before the shot]
name=${1:?agent name}; extra=${2:-}
S="map e1m1;wait60;god;developer 1;vr_debug_shots 1;vr_debug_weaponfx 2;vr_weapon_grip_mode 1;impulse 166;wait60;vr_mock_look 0 0;vr_mock_hand main 0.10 1.40 -0.32 70 0 0;$extra;wait30;vr_sight_check;echo FIRE;+attack;wait2;-attack;wait2;vr_mock_camera 0.55 1.45 -0.75 10 150;wait1;vr_eyeshot 1;wait40;vr_mock_camera;vr_debug_weaponfx 3;vr_test_spawn 8;vr_test_spawn_dist 250;impulse 241;wait100;wait100;wait100;echo DONE;toggleconsole;quit"
bash C:/OHWorkspace/qvr-kit/run.sh "$name" -Script "$S" -Filter "weaponfx (fired|monster)|sightcheck|FIRE|DONE|rror|laser|eyeshots" -Timeout 120 "${@:3}"
