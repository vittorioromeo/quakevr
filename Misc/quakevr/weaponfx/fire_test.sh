#!/bin/bash
# Weapon effects (ROUND21.md, "Weapon effects: recoil, muzzle flash, tracers"): the grunts' burst rifle in the main hand
# fires one burst at e1m1's start; the eyes' view taken at the first round's flash and from the side at the second's
# (vr_eyeshot 1, quakevr/eyeshots); vr_debug_weaponfx 2
# prints each shot's effects and the recoil each frame, vr_debug_fatigue 2 the aim and the muzzle the game uses
# (fatigueaim: they must not move while the drawn weapon kicks).
# Usage: bash Misc/quakevr/weaponfx/fire_test.sh <agent name> [extra commands before the shot] [-Out name.png]
name=${1:?agent name}; extra=${2:-}
S="map e1m1;wait60;god;notarget;developer 1;vr_debug_shots 1;vr_debug_weaponfx 2;vr_debug_fatigue 2;vr_weapon_grip_mode 1;impulse 165;wait60;vr_mock_look 0 0;vr_mock_hand main 0.10 1.40 -0.32 70 0 0;$extra;wait30;echo FIRE;+attack;wait2;vr_eyeshot 1;wait3;-attack;wait12;vr_mock_camera 0.55 1.45 -0.75 10 150;wait4;vr_eyeshot 1;wait60;vr_mock_camera;echo DONE;toggleconsole;quit"
bash C:/OHWorkspace/qvr-kit/run.sh "$name" -Script "$S" -Filter "weaponfx|fatigueaim|FIRE|DONE|rror|burst|shot:|eyeshots" -Timeout 120 "${@:3}"
