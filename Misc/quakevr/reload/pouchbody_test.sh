#!/bin/bash
# pouchbody_test.sh <agent> -- the pouches with the body hidden (ROUND25.md, "pouchbody"; his note 2026-11-21: "with
# vr_body_mode 0 I was not able to interact with the ammo pouch"), headless, with each vr_body_mode (0 Only Hands, 2
# Torso and Arms, 3 Full Body):
#   1. vr_body_crouch_report's `gear ring:`: the hip and upper holsters, the ammo pouch and the grenade pouch all out of
#      the player's torso (its ring 1 or more: a real hand reaches them; the pouches were inside it with the body hidden);
#   2. the main hand at the ammo pouch takes a shell (the shotgun in the off hand);
#   3. a magazine from the ammo pouch seats in the nailgun;
#   4. the main hand at the grenade pouch takes a grenade (the grenade launcher in the off hand).
# Prints PASS/FAIL per check and `pouchbody: N passed, M failed`; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
export TB_NODRAW=${TB_NODRAW-1} # (batched frames not drawn: no check here reads the drawing; testbatch.sh)
tb_body() {
fail=0
source "$(dirname "$0")/../qvrtest.sh"
F="^reload: (a shell|a magazine)|^grenade: hand|^gear ring"
POUCH="vr_mock_hand_to main ammopouch;wait5;vr_mock_hand_to main ammopouch;wait5"
BACK="vr_mock_hand_to main grenadepouch;wait5;vr_mock_hand_to main grenadepouch;wait5"
GRIP="+grabmain;vr_mock_button main grip 1;wait10"
WELL="vr_mock_hand_to main lport;wait5;vr_mock_hand_to main lport;wait10"
BASE="map e1m1;wait60;developer 1;god;vr_reload_debug 1;vr_weapon_grip_mode 1;impulse 9;wait2"
MAIN="vr_test_weaponinst 7;impulse 120;wait3"
SG="vr_reload_shell_pairs 0;$BASE;impulse 154;wait3;$MAIN;give s 30;vr_reload_test 5;impulse 125;wait2;vr_mock_hand off -0.15 1.25 -0.40 50 0 0;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait10"
NG="$BASE;impulse 156;wait3;$MAIN;give n 100;vr_mock_hand off -0.15 1.25 -0.40 50 0 0;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait10;vr_mock_button off secondary 1;wait3;vr_mock_button off secondary 0;wait10"
GL="$BASE;impulse 158;wait3;$MAIN;give r 30;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;vr_mock_hand off -0.15 1.0 -0.40 140 0 0;wait10;vr_reload_test 5;impulse 125;wait2"
for m in 0 2 3; do
    log=$(tb_run_sh -Script "vr_body_mode $m;$SG;vr_body_crouch_report;$POUCH;$GRIP;toggleconsole;quit" -Filter "$F" 2>&1)
    ring=$(echo "$log" | grep -m1 "^gear ring: body $m " | tr -d '\r')
    low=$(echo "$ring" | awk '{ n = 0; for(i = 6; i <= NF; i += 2) if($i + 0 < 0.99) n++; print (NF >= 16) ? n : -1 }')
    check $([ "$low" = 0 ] && echo 1 || echo 0) "body $m: the holsters and pouches out of the player's torso (${ring#gear ring: })"
    check $(echo "$log" | grep -q "a shell taken from the pouch by hand 1: 37 left" && echo 1 || echo 0) "body $m: a shell taken from the ammo pouch"
    log=$(tb_run_sh -Script "vr_body_mode $m;$NG;$POUCH;$GRIP;$WELL;toggleconsole;quit" -Filter "$F" 2>&1)
    check $(echo "$log" | grep -q "a magazine of 24 seated (hand 0): the gun holds 24" && echo 1 || echo 0) "body $m: a magazine from the ammo pouch seated in the nailgun"
    log=$(tb_run_sh -Script "vr_body_mode $m;$GL;$BACK;$GRIP;toggleconsole;quit" -Filter "$F" 2>&1)
    check $(echo "$log" | grep -q "^grenade: hand grenade taken from the pouch" && echo 1 || echo 0) "body $m: a grenade from the grenade pouch"
done
verdict
exit $fail
}
source "$(dirname "$0")/../testbatch/testbatch.sh"
tb_main "$AGENT" tb_body "$@"
