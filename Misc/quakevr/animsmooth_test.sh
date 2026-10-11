#!/bin/bash
# animsmooth_test.sh <agent> -- Model Animation Smooth (r_lerpmodels 3; vr_animsmooth.cpp, ROUND22.md "Smoother animations"),
# headless on e1m1 with a grunt 96 units ahead (vr_fixed_frames 1):
#   1. vr_anim_smooth_check: the spline's weights exact at the keys; every rigged model's shape 0.001 off a key within
#      0.1 units of it (no pop entering or leaving a frame; 0.03-0.08 when written)
#   2. the grunt drawn smoothed: splines with limbs swung (vr_anim_smooth_info), and vr_anim_smooth_trace reports
#   3. vr_anim_smooth_pin on it, then a map change: the pin is off (another map's entity has its number)
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=${1:?worktree name}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
S="vr_fixed_frames 1;r_lerpmodels 3;map e1m1;wait30;god;notarget;vr_test_spawn 0;vr_test_spawn_dist 96;impulse 241;wait30"
S="$S;vr_anim_smooth_check;vr_anim_smooth_info;vr_anim_smooth_trace 60;wait90;vr_anim_smooth_info"
S="$S;vr_anim_smooth_pin 83 84 0.5 -1 -1 soldier;wait5;vr_anim_smooth_info;map e1m2;wait30;vr_anim_smooth_info;toggleconsole;quit"
OUT=$(bash $KIT/run.sh $AGENT -Script "$S" -Filter "^animsmooth: (check|trace|pinned|[0-9]+ splines)|ERROR|Host_Error|Sys_Error|CRASH" -Timeout 600 2>&1 |
    grep -v "^$" | grep -v "^exit=0")
echo "$OUT"
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
check $(echo "$OUT" | grep -q "weights at the keys exact" && echo 1 || echo 0) "the spline's weights exact at the keys"
gaps=$(echo "$OUT" | grep -o "0.001 off a key: at most [0-9.]*" | grep -o "[0-9.]*$")
worst=$(echo "$gaps" | sort -g | tail -1)
check $([ -n "$gaps" ] && awk "BEGIN{exit !($worst < 0.1)}" && echo 1 || echo 0) "0.001 off a key within 0.1 units (worst ${worst:-none})"
swung=$(echo "$OUT" | grep -o "info, [0-9]* with limbs swung" | grep -o "[0-9]* with limbs" | sort -n | tail -1 | grep -o "^[0-9]*")
check $([ "${swung:-0}" -gt 0 ] && echo 1 || echo 0) "the grunt drawn smoothed, limbs swung (${swung:-0} splines)"
check $(echo "$OUT" | grep -q "^animsmooth: trace ent [0-9]* (progs/soldier.mdl" && echo 1 || echo 0) "the trace reports"
pins=$(echo "$OUT" | grep -o "pinned entity -\?[0-9]*" | grep -o "\-\?[0-9]*$" | tr '\n' ' ')
check $(echo "$pins" | grep -q "^-1 -1 [0-9][0-9]* -1 $" && echo 1 || echo 0) "the pin set, then off after the map change ($pins)"
check $(echo "$OUT" | grep -q "ERROR\|Host_Error\|Sys_Error\|CRASH" && echo 0 || echo 1) "no error"
exit $fail
