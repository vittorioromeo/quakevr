#!/bin/bash
# voice_20261012_test.sh <agent>: collection destinations, real save/load grip grace and reinsertion hysteresis.
# No headset, no timings. Saved games and logs remain in the agent's own quakevr/scratch folders.
set -eu
AGENT=${1:?worktree name required}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
FAIL=0
check() { if [ "$1" = 1 ]; then echo "PASS $2"; else echo "FAIL $2 ($LOG)"; FAIL=1; fi; }
run() {
    LOG="$TREE/scratch/voice_reload2_$1.log"
    bash "$KIT/run.sh" "$AGENT" -Script "$2;wait2;toggleconsole;quit" -ExtraArgs "-nomapindex" -Filter "$3|Host_Error|ENGINE|ERROR|Fatal error" > "$LOG"
    check $(grep -Eq 'Host_Error|ENGINE|ERROR|Fatal error' "$LOG" && echo 0 || echo 1) "$1 no engine errors"
}
BASE="vr_holster_mode 0;vr_reload_mode 3;map e1m1;wait60;god;notarget;developer 1"
run boundaries "vr_weapon_grip_mode 1;$BASE;vr_reload_test 28;impulse 125;wait3;vr_menu_path_check maps/vrcalibration.map" "reload reinsertion|reload: FAIL|menu paths"
check $(grep -q '21 passed, 0 failed' "$LOG" && echo 1 || echo 0) "21 reinsertion checks (18 boundaries, 3 Reinsertion Delay)"
check $(grep -q '0 missing' "$LOG" && echo 1 || echo 0) "calibration paths"

GET="+grabmain;+graboff;vr_mock_button main grip 1;vr_mock_button off grip 1;impulse 156;wait4;vr_test_weaponinst 7;impulse 120;wait4;impulse 154;wait4"
RELEASE="-grabmain;-graboff;vr_mock_button main grip 0;vr_mock_button off grip 0"
REP="vr_test_weaponinst 0;impulse 120"
for grace in 0 0.75; do
    S="vr_weapon_grip_mode 0;vr_weapon_load_grace $grace;$BASE;$GET;save voice_reload2_grace;wait3;$RELEASE;load voice_reload2_grace;wait10;$REP;wait20;$REP;wait20;$REP;wait4;$REP;wait7;$REP"
    run "grace_$grace" "$S" "weapon load grace|weaponinst:"
    if [ "$grace" = 0.75 ]; then
        check $(test "$(grep -c 'main weapon 4 .*off weapon 6' "$LOG")" = 4 && echo 1 || echo 0) "both loaded weapons held through 0.728 s with no grip"
        check $(grep -q 'weaponinst: main - | off -' "$LOG" && echo 1 || echo 0) "both released after grace (by 0.826 s)"
    else
        check $(grep -q 'weaponinst: main - | off -' "$LOG" && ! grep -q 'main weapon 4 .*off weapon 6' "$LOG" && echo 1 || echo 0) "zero grace keeps ordinary release behavior"
    fi
done
run normal "vr_weapon_grip_mode 0;vr_weapon_load_grace 0.75;$BASE;$GET;$RELEASE;wait4;$REP" "weaponinst:"
check $(grep -q 'weaponinst: main - | off -' "$LOG" && echo 1 || echo 0) "ordinary drops after a fresh map are immediate"
run firing "vr_weapon_grip_mode 0;vr_weapon_load_grace 0.75;$BASE;$GET;save voice_reload2_firing;wait3;$RELEASE;load voice_reload2_firing;wait10;+attack;vr_mock_button main trigger 1;wait10;-attack;vr_mock_button main trigger 0;$REP;wait50;$REP" "weapon load grace|weaponinst:"
check $(grep -q 'main weapon 4 .*clip 7 | off weapon 6' "$LOG" && echo 1 || echo 0) "restored gun can fire during grip grace (8 to 7 shells)"

HOLD="$BASE;vr_weapon_grip_mode 1;vr_debug_collect_fx 2;vr_collect_fx_time 0.4;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait5;+grabmain;vr_mock_button main grip 1;wait2"
for pose in standing crouched offsets; do
    POSE=""
    if [ "$pose" = crouched ]; then POSE="vr_mock_hand head 0 1.1 0;wait80;"; fi
    if [ "$pose" = offsets ]; then POSE="vr_ammo_pouch_x 6;vr_ammo_pouch_y -4;vr_ammo_pouch_z 3;vr_ammo_pouch_pitch 22;vr_ammo_pouch_yaw 37;vr_ammo_pouch_roll 18;vr_ammo_pouch_scale 1.75;wait15;"; fi
    for item in box shell magazine; do
        TAKE="vr_test_spawn_dist 48;vr_test_spawn_hold 1;vr_test_spawn 101;impulse 241;wait10"
        if [ "$item" != box ]; then
            GUN=154; AMMO="give s 30"
            if [ "$item" = magazine ]; then GUN=156; AMMO="give n 100"; fi
            TAKE="impulse $GUN;wait3;vr_test_weaponinst 7;impulse 120;wait3;$AMMO;vr_reload_test 1;impulse 125;wait5"
        fi
        S="$HOLD;$POSE$TAKE;vr_dumpview;vr_mock_hand_to main ammopouch;wait3;vr_mock_hand_to main ammopouch;wait3;vr_mock_button main grip 0;-grabmain;wait60"
        run "pouch_${pose}_$item" "$S" "ammo pouch collection|collect fx: .*into hotspot|ammo pouch endpoint|collect fx: .*gone in"
        check $(grep -q 'into hotspot 12' "$LOG" && grep -q 'gone in' "$LOG" && grep -q 'endpoint error 0.0000' "$LOG" && ! grep 'endpoint error' "$LOG" | grep -qv 'error 0.0000' && echo 1 || echo 0) "$pose $item routes to the pouch centre point (0.0000 units: routing only; ammoPouchCentre itself is not measured)"
    done
done
exit "$FAIL"
