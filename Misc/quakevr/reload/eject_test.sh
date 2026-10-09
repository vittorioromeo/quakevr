#!/bin/bash
# eject_test.sh <agent> -- a magazine ejected by B/Y (QC vr_reload.qc VR_Reload_MagFrame, VR_Reload_Eject), headless:
#   1. it stays out whatever the other hand holds (the author's note vrfiringrange_2026-10-09_12-33-54: the off hand's
#      nailgun's went straight back in with the super nailgun in the main hand; .vr_ammo_fresh until it has left both
#      guns' load points, vr_reload_eject_cooldown): each pair of the nailgun, super nailgun and thunderbolt, either hand
#      ejecting, the other hand empty too;
#   2. it leaves as it sat (the author's note 12-34-55: the super nailgun's fell out flat): its feed end along the seated
#      magazine's (1.00), pushed out the way its well faces (the nailgun's and the thunderbolt's down, the super nailgun's
#      out of its side and a little down, never up: within 50 degrees of its well's way, which tilts up 21 degrees), each
#      gun in each hand, the hand pitched two ways.
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
REP="vr_reload_test 0;impulse 125;wait3"
BASE="map e1m1;wait60;developer 1;vr_reload_debug 1;vr_weapon_grip_mode 1;impulse 9;wait2"

# 1. Each pair (the off hand's gun, the main hand's: 0 none), the hand that ejects.
for row in "156 157 off" "156 156 off" "157 156 off" "157 157 off" "161 156 off" "156 161 off" "161 161 off" "157 161 off" "161 157 off" "156 157 main" "157 156 main" "157 157 main" "156 156 main" "161 156 main" "161 157 main" "156 0 off"; do
    set -- $row; off=$1; main=$2; hand=$3
    S="$BASE;impulse $off;wait3;vr_test_weaponinst 7;impulse 120;wait3;give n 200;give c 200"
    if [ $main != 0 ]; then S="$S;impulse $main;wait3"; fi
    S="$S;vr_mock_hand off -0.15 1.25 -0.40 50 0 0;vr_mock_hand main 0.25 1.25 -0.40 50 0 0;wait10;vr_mock_button $hand secondary 1;wait3;vr_mock_button $hand secondary 0;wait90;$REP;toggleconsole;quit"
    log=$(bash $KIT/run.sh $AGENT -Script "$S" -Filter "^reload: (off hand|.*out of the gun|.*seated \(hand)" 2>&1)
    outs=$(echo "$log" | grep -c "out of the gun")
    seats=$(echo "$log" | grep -c "seated (hand")
    st=$(echo "$log" | grep "^reload: off hand" | tail -1)
    if [ $hand = off ]; then pat="off hand weapon [0-9]+ clip 0 mag 0"; else pat="main mag 0"; fi
    check $([ "$outs" = 1 ] && [ "$seats" = 0 ] && echo "$st" | grep -qE "$pat" && echo 1 || echo 0) "off hand $off, main hand $main, the $hand hand's B/Y: out once, never back in (outs $outs, seats $seats)"
done

# 2. As it sat: per gun, hand and pitch.
for gun in 156 157 161; do for hand in off main; do for pitch in 0 30; do
    S="$BASE;impulse $gun;wait3"
    if [ $hand = off ]; then S="$S;vr_test_weaponinst 7;impulse 120;wait3"; fi
    S="$S;vr_mock_hand off -0.15 1.25 -0.40 $pitch 30 10;vr_mock_hand main 0.25 1.25 -0.40 $pitch -30 -10;wait10;vr_mock_button $hand secondary 1;wait3;vr_mock_button $hand secondary 0;wait5;toggleconsole;quit"
    line=$(bash $KIT/run.sh $AGENT -Script "$S" -Filter "out of the gun|u/s" 2>&1 | grep -A1 "out of the gun" | tr "\n" " ")
    along=$(echo "$line" | sed -n "s/.*seated one's \([-0-9.]*\).*/\1/p")
    way=$(echo "$line" | sed -n "s/.*up), \([-0-9.]*\) along.*/\1/p")
    up=$(echo "$line" | sed -n "s/.*u\/s (\([-0-9.]*\) up).*/\1/p")
    check $(awk -v a="$along" -v w="$way" -v u="$up" 'BEGIN { print (a != "" && a >= 0.98 && w != "" && w >= 0.65 && u != "" && u < 0) ? 1 : 0 }') "gun $gun, the $hand hand pitched $pitch: as it sat (feed end along the seated one's $along), pushed out of its well ($way along its way out, $up u/s up)"
done; done; done
exit $fail
