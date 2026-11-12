#!/bin/bash
# ssgtorch_test.sh <agent> -- the super shotgun broken open (his words 2026-11-12), headless:
#   1. the flashlight clipped on it (vr_flashlight_clip_gun left) rides its barrels down about the hinge (vr_view.cpp
#      ssgBarrelsCarry; vr_flashlight.cpp gunPose): the probe's lens-to-breech and lens-to-barrel distances (metres, to
#      two points of the barrels as drawn) the same shut and open, its beam turned down by the open angle (45 deg);
#   2. the pouch's pair loaded into the open chambers: the barrels drawn with frame 2 and skin 2 (setSsgPart: the frame
#      is the skin's), and the model's frames (make_ssg_open.py loaded_frames; ssg_checks.py heads): a loaded chamber's
#      shell head flush with the breech face (0.000), an empty bore's bottom still in (0.454).
# Prints PASS/FAIL per check and the verdict `ssgtorch: N passed, M failed`; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
export TB_NODRAW=${TB_NODRAW-1} # (batched frames not drawn: no check here reads the drawing; testbatch.sh)
# (The body: tb_main plays its runs as batches, testbatch.sh; TB_SLOW=1: each alone by run.sh, as before.)
tb_body() {
fail=0
source "$(dirname "$0")/../qvrtest.sh"
PY=${PY:-py -3.13}
SSG="map e1m1;wait60;developer 1;vr_reload_debug 1;vr_weapon_grip_mode 1;vr_reload_ssg_open_flick 1;vr_reload_ssg_close_auto 0;impulse 9;wait2;impulse 155;wait3;vr_test_weaponinst 7;impulse 120;wait3;give s 30;vr_mock_hand off -0.15 1.25 -0.40 50 0 0;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait10"
FLICK="+flickreloadleft;wait3;-flickreloadleft;wait40"
POUCH="vr_mock_hand_to main ammopouch;wait5;vr_mock_hand_to main ammopouch;wait5"
GRIP="+grabmain;vr_mock_button main grip 1;wait10"
LETGO="vr_mock_button main grip 0;-grabmain;wait5"
AT="vr_mock_hand_to main lport 8;wait5;vr_mock_hand_to main lport 8;wait10;vr_mock_hand_to main lport;wait5;vr_mock_hand_to main lport;wait10"
REST="vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait20"
DBG="vr_reload_debug 2;wait1;vr_reload_debug 1"
# (The waits: the torch's clip-on ease, vr_flashlight_clip_time; the barrels dropping open at 450 deg/s; the pair's slide.)
log=$(tb_run_sh -Script "$SSG;vr_flashlight 1;wait2;vr_flashlight_clip_gun left;wait40;vr_flashlight_probe shut;$FLICK;$POUCH;$GRIP;$AT;$LETGO;$REST;$DBG;vr_flashlight_probe open;toggleconsole;quit" -Filter "^torchprobe .* (gunmount|ssg)|^ssg: hand|^reload: 2 into" 2>&1)
d() { echo "$log" | grep "^torchprobe $1 ssg" | tail -1 | sed 's/.*breech \([-0-9.]*\) to barrel \([-0-9.]*\)/\1 \2/'; }
pitch() { echo "$log" | grep "^torchprobe $1 gunmount" | tail -1 | sed 's/.* turn \([-0-9.]*\) .*/\1/'; }
shut=$(d shut); open=$(d open); ps=$(pitch shut); po=$(pitch open)
deg=$(echo "$log" | grep "^torchprobe open ssg" | tail -1 | sed 's/.* ssg open \([0-9.]*\) .*/\1/')
check $(echo "$shut $open $deg" | awk 'NF == 5 && $5 >= 44.9 && $1 > 0 && $2 > 0 && ($1 - $3)^2 < 1e-6 && ($2 - $4)^2 < 1e-6 { ok = 1 } END { print ok ? 1 : 0 }') "the torch rides the open barrels: lens to breech, to barrel (m) shut $shut, open $open (${deg:-?} deg open)"
check $(echo "$ps $po $deg" | awk 'NF == 3 && ($1 - $2 - $3)^2 < 0.04 { ok = 1 } END { print ok ? 1 : 0 }') "its beam turned down with them: pitch shut ${ps:-?}, open ${po:-?}"
check $(echo "$log" | grep -q "^reload: 2 into the gun (hand 0)" && echo "$log" | grep "^ssg: hand 0 open" | tail -1 | grep -q "barrels frame 2 skin 2" && echo 1 || echo 0) "the pair loaded open: the barrels drawn with frame 2, skin 2 ($(echo "$log" | grep "^ssg: hand 0 open" | tail -1 | sed 's/.*, barrels //'))"
heads=$($PY "$(dirname "$0")/ssg_checks.py" heads 2>&1)
check $(echo "$heads" | grep -q "frame 0 left 0.454 right 0.454, frame 1 left 0.000 right 0.454, frame 2 left 0.000 right 0.000" && echo 1 || echo 0) "the loaded shells' heads flush with the breech face, the empty bores open ($heads)"
verdict
exit $fail
}
source "$(dirname "$0")/../testbatch/testbatch.sh"
tb_main "$AGENT" tb_body "$@"
