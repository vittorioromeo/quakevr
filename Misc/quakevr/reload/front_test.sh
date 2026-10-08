#!/bin/bash
# front_test.sh <agent> -- the launchers loaded at the muzzle (QC vr_reload.qc, "Front-loaded launchers"; the author's
# note vrfiringrange_2026-10-07_22-18-57), headless, per launcher (the off hand's: the grenade launcher, the proximity
# launcher, the rocket launcher):
#   1. by the mock hands: the main hand to the ammo pouch, the grip (a round taken: the reserve one less), its butt to the
#      muzzle (vr_mock_hand_to main lport): in, drawn sliding in (vr_debug_collect_fx: into hotspot 240); the hand turned
#      upside down: the wrong way round, refused (a proximity grenade goes in any way round);
#   2. loose: tossed into the muzzle butt first (in), nose first (out; a proximity grenade in), sideways at it (out),
#      dropped from above into it turned up (in), lying on the floor and the launcher brought down onto it (in);
#   3. the other ammo mode (impulse 42, the gun's button): the pouch gives the multi-grenade or multi-rocket, from the
#      multi-rockets; a rocket doesn't go into the grenade launcher;
#   4. the Simple mode (vr_reload_mode 2): the pouch gives nothing for a launcher, which reloads at the hip holster;
#      vr_reload_front 0: as before in Immersive too.
# Test steps (impulse 125): vr_reload_test 1 take, 10 tossed, 11 sideways, 12 on the floor, 13 dropped from above, 14 the
# loose rounds' report, 16 tossed the wrong way round, 17/18 the held round at the load point the right/wrong way.
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
REP="vr_reload_test 0;impulse 125;wait2"
F="^reload: (off hand|rockets|[0-9] into|a .* (taken|the wrong|held at)|an? .*placed loose|loose |the pouch)|collect fx: .*(240|gone in)"
POUCH="vr_mock_hand_to main ammopouch;wait5;vr_mock_hand_to main ammopouch;wait5"
GRIP="+grabmain;vr_mock_button main grip 1;wait10"
# (A round from the pouch stands in the fist, nose up by the thumb: vr_reload_front_hold_pitch 90. The hand tipped 50
# degrees, it lies along the launcher's barrel as these poses hold it.)
TIP="vr_mock_hand main 0.25 1.1 -0.3 50 0 0;wait5"
LPORT="vr_mock_hand_to main lport 6;wait5;vr_mock_hand_to main lport 6;wait10;vr_mock_hand_to main lport;wait5;vr_mock_hand_to main lport;wait20"

# The off hand holding the launcher (impulse $1) at pose $2 (pitch yaw roll; 140 0 0: its muzzle up 70 degrees ahead),
# emptied into the reserve, the main hand empty.
setup() {
    echo "map e1m1;wait60;developer 1;vr_reload_debug 1;vr_debug_collect_fx 1;vr_weapon_grip_mode 1;impulse 9;wait2;impulse $1;wait3;vr_test_weaponinst 7;impulse 120;wait3;give r 30;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;vr_mock_hand off -0.15 1.0 ${OFFZ:--0.40} $2;wait10;vr_reload_test 5;impulse 125;wait2"
}
# The launcher brought to a round lying on the floor (as on a table) under its muzzle: the off hand at pose $1 (45 0 0: the
# muzzle 25 degrees down) from 0.2 m ahead drawn back 0.25 m and down to the floor, then pushed forward 0.5 m along it
# (the muzzle onto the round's butt; straight down, the barrel lands on the butt instead).
push() { awk -v pose="$1" 'BEGIN { for(i = 0; i <= 20; i++) printf "vr_mock_hand off -0.15 %.3f 0.05 %s;wait2;", 1.0 - i * 0.05, pose; for(i = 0; i <= 25; i++) printf "vr_mock_hand off -0.15 0 %.3f %s;wait2;", 0.05 - i * 0.02, pose }'; }
last() { echo "$1" | grep "^reload: off hand" | tail -1; }
clip() { last "$1" | sed -E 's/^reload: off hand weapon [0-9]+ clip ([0-9]+) .*/\1/'; }

for row in "158 grenade_launcher grenade" "159 proximity_launcher proximity" "160 rocket_launcher rocket"; do
    set -- $row; gun=$1; name=${2//_/ }; round=$3
    S="$(setup $gun "140 0 0")"
    # 1. The pouch to the muzzle, right way up; then upside down.
    log=$(bash $KIT/run.sh $AGENT -Script "$S;$REP;$POUCH;$GRIP;$TIP;$REP;$LPORT;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
    check $(echo "$log" | grep -q "^reload: rockets 33 " && echo "$log" | grep -q "taken from the pouch" && echo 1 || echo 0) "$name: the pouch gives a $round, the reserve one less"
    check $([ "$(clip "$log")" = 1 ] && echo "$log" | grep -q "into hotspot 240" && echo 1 || echo 0) "$name: its butt to the muzzle: in, sliding in ($(last "$log" | cut -c9-40))"
    log=$(bash $KIT/run.sh $AGENT -Script "$S;$POUCH;$GRIP;vr_mock_hand main 0.25 1.1 -0.3 50 0 180;wait5;$LPORT;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
    if [ $round = proximity ]; then
        check $([ "$(clip "$log")" = 1 ] && echo 1 || echo 0) "$name: held upside down it goes in all the same (a ball)"
    else
        check $([ "$(clip "$log")" = 0 ] && echo "$log" | grep -q "the wrong way round" && echo 1 || echo 0) "$name: held upside down (nose first) it is refused, a dull tap"
    fi
    # The held round at the load point by the test steps: the wrong way round, then the right way.
    log=$(bash $KIT/run.sh $AGENT -Script "$S;$GRIP;vr_reload_test 1;impulse 125;wait3;vr_reload_test 18;impulse 125;wait3;vr_reload_test 17;impulse 125;wait10;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
    want18=$([ $round = proximity ] && echo "loaded" || echo "not loaded")
    check $(echo "$log" | grep -q "held at the load point the wrong way round: $want18" && echo 1 || echo 0) "$name: test step 18 (the wrong way round): $want18"
    # 2. Loose: tossed butt first, nose first, sideways; dropped from above; on the floor, the launcher brought down.
    log=$(bash $KIT/run.sh $AGENT -Script "$S;vr_reload_test 10;impulse 125;wait40;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
    check $([ "$(clip "$log")" = 1 ] && echo 1 || echo 0) "$name: a $round tossed into the muzzle butt first goes in"
    log=$(bash $KIT/run.sh $AGENT -Script "$S;vr_reload_test 16;impulse 125;wait40;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
    want=$([ $round = proximity ] && echo 1 || echo 0)
    check $([ "$(clip "$log")" = $want ] && echo 1 || echo 0) "$name: tossed nose first: $([ $want = 1 ] && echo in || echo out)"
    log=$(bash $KIT/run.sh $AGENT -Script "$S;vr_reload_test 11;impulse 125;wait40;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
    if [ $round != proximity ]; then
        check $([ "$(clip "$log")" = 0 ] && echo 1 || echo 0) "$name: lying across the muzzle it stays out"
    fi
    log=$(bash $KIT/run.sh $AGENT -Script "$(setup $gun "160 0 0");vr_reload_test 13;impulse 125;wait60;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
    check $([ "$(clip "$log")" = 1 ] && echo 1 || echo 0) "$name: dropped from above into the muzzle turned up"
    SLAM="45 0 0"
    log=$(bash $KIT/run.sh $AGENT -Script "$(OFFZ=-0.2 setup $gun "$SLAM");vr_reload_test 12;impulse 125;wait90;vr_reload_test 14;impulse 125;$(push "$SLAM")wait10;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
    check $([ "$(clip "$log")" = 1 ] && echo 1 || echo 0) "$name: lying on the floor, the launcher brought to it ($(echo "$log" | grep -m1 "^reload: loose" | cut -c9-80))"
done

# A rocket lying about, shot (test step 19): it goes off (the launcher grenade's blast), no round left.
log=$(bash $KIT/run.sh $AGENT -Script "$(OFFZ=-0.2 setup 160 "45 0 0");vr_reload_test 12;impulse 125;wait60;vr_reload_test 19;impulse 125;wait30;$REP;toggleconsole;quit" -Filter "$F|set off" 2>&1)
check $(echo "$log" | grep -q "a rocket set off" && echo "$log" | grep -q "rockets [0-9]* .*, 0 rounds lying" && echo 1 || echo 0) "a rocket lying about, shot, goes off"

# 3. The other ammo mode: the multi-grenade and the multi-rocket from the multi-rockets; a rocket into the grenade launcher.
for row in "158 multi-grenade" "160 multi-rocket"; do
    set -- $row
    log=$(bash $KIT/run.sh $AGENT -Script "$(setup $1 "140 0 0");give r 30;impulse 42;wait5;$REP;$POUCH;$GRIP;$TIP;$REP;$LPORT;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
    m0=$(echo "$log" | grep "^reload: rockets" | head -1 | sed -E 's/.*multi-rockets ([0-9]+).*/\1/')
    m1=$(echo "$log" | grep "^reload: rockets" | tail -1 | sed -E 's/.*multi-rockets ([0-9]+).*/\1/')
    check $(echo "$log" | grep -q "a $2 taken from the pouch" && [ $((m0 - m1)) = 1 ] && [ "$(clip "$log")" = 1 ] && echo 1 || echo 0) "the other ammo mode: a $2 from the pouch (multi-rockets $m0 -> $m1), loaded"
done
log=$(bash $KIT/run.sh $AGENT -Script "$(setup 160 "140 0 0");$GRIP;vr_reload_test 1;impulse 125;wait3;impulse 178;wait10;vr_reload_test 5;impulse 125;wait2;$LPORT;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
check $(echo "$log" | grep -q "a rocket taken" && [ "$(clip "$log")" = 0 ] && echo 1 || echo 0) "a rocket brought to the grenade launcher's muzzle stays out"

# 4. Simple mode: the pouch gives nothing for a launcher; it reloads at the hip holster. vr_reload_front 0 likewise.
for mode in "vr_reload_mode 2" "vr_reload_front 0"; do
    log=$(bash $KIT/run.sh $AGENT -Script "$mode;$(setup 160 "140 0 0");$POUCH;$GRIP;$REP;vr_mock_button main grip 0;-grabmain;vr_mock_hand_to off holster 2;wait5;vr_mock_hand_to off holster 2;wait30;$REP;vr_reload_mode 3;vr_reload_front 1;toggleconsole;quit" -Filter "$F" 2>&1)
    check $(! echo "$log" | grep -q "a rocket taken from the pouch" && [ "$(clip "$log")" = 4 ] && echo 1 || echo 0) "$mode: the pouch gives no rocket; the launcher reloads at the hip holster (clip $(clip "$log"))"
done
# 5. (the author's note vrfiringrange_2026-10-08_22-18-19) Every launcher round from the ammo pouch sits the same way in
#    the hand, whatever the hand's turn as it reached in: its long axis (the multi-grenade's z, the others' x) nose up in
#    the hand (vr_reload_front_hold_pitch 90: hand frame forward, left, up), the proximity grenade placed the same way;
#    Round In Hand Pitch 0 lays it along the hand's forward.
# (The last "from the ammo pouch" line, the console's wrapped lines joined: its x and its z in the hand frame.)
hold() { echo "$1" | tr -d '\r\n' | grep -o "grip: from the ammo pouch.\{0,400\}(hand frame" | tail -1 | sed -nE 's/.*its x ([-0-9.]+) ([-0-9.]+) ([-0-9.]+), its z ([-0-9.]+) ([-0-9.]+) ([-0-9.]+) \(hand frame/\1 \2 \3 \4 \5 \6/p'; }
axes=""
ok=1
for row in "158 0 grenade" "158 1 multi-grenade" "159 0 proximity" "160 0 rocket" "160 1 multi-rocket"; do
    set -- $row
    multi=""; [ $2 = 1 ] && multi="give r 30;impulse 42;wait5;"
    for turn in "0 0 0" "30 40 -20" "-50 -30 60"; do
        log=$(bash $KIT/run.sh $AGENT -Script "$(setup $1 "140 0 0");${multi}vr_mock_hand main 0.25 1.1 -0.3 $turn;wait5;$POUCH;$GRIP;$REP;toggleconsole;quit" -Filter "^grip: from the ammo pouch|taken from the pouch" 2>&1)
        a=$(hold "$log")
        long=$(echo "$a" | awk -v m=$3 '{ if(m == "multi-grenade") print $4, $5, $6; else print $1, $2, $3 }')
        axes="$axes $3:[$long]"
        [ $(echo "$a" | wc -w) = 6 ] || ok=0
        [ $3 = proximity ] && continue
        echo "$long" | awk '{ exit !(NF == 3 && $3 > 0.99) }' || ok=0
    done
done
check $ok "every launcher round from the ammo pouch sits nose up in the hand, whatever the hand's turn ($axes)"
log=$(bash $KIT/run.sh $AGENT -Script "vr_reload_front_hold_pitch 0;$(setup 160 "140 0 0");vr_mock_hand main 0.25 1.1 -0.3 30 40 -20;wait5;$POUCH;$GRIP;$REP;vr_reload_front_hold_pitch 90;toggleconsole;quit" -Filter "^grip: from the ammo pouch" 2>&1)
check $(hold "$log" | awk '{ print (NF == 6 && $1 > 0.99) ? 1 : 0 }') "Round In Hand Pitch 0: the rocket along the hand's forward ($(hold "$log"))"
exit $fail
