#!/bin/bash
# pouchgren_test.sh <agent> -- one grenade from either pouch (QC vr_grenade.qc VR_HandGrenade_Make; ROUND21.md, "One
# grenade from either pouch"), headless:
#   1. the ammo pouch shows the launchers' rounds as many as the reserve has (0, 1, 3, 9 rockets: up to 3 rockets, 4
#      grenades, 4 proximity grenades; vr_dumpview's vrpouch_ammo.mdl frame), the multi-grenades on its skin 1;
#   2. a grenade from the ammo pouch armed by its trigger (the pin) and let go of goes off as the back pouch's does;
#      armed, it is not loaded at the grenade launcher's muzzle;
#   3. a grenade from the back pouch (the grenade launcher in the other hand) goes into the grenade launcher; the
#      multi-grenade launcher's mode gives a multi-grenade, which goes in too; one from the ammo pouch goes back into the
#      back pouch (its rocket refunded);
#   4. a proximity grenade from the back pouch (the proximity launcher in the other hand) goes into the proximity
#      launcher; one from the ammo pouch armed and let go of is a mine, which goes off as the player stands by it;
#   5. a grenade lying about (unarmed), shot: it goes off.
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
REP="vr_reload_test 0;impulse 125;wait2"
F="^reload: (off hand|rockets|[0-9] into)|^grenade: (hand|proximity|the proximity|grenade|vr_ammo_front|an unarmed|let go)|vrpouch_ammo"
POUCH="vr_mock_hand_to main ammopouch;wait5;vr_mock_hand_to main ammopouch;wait5"
BACK="vr_mock_hand_to main grenadepouch;wait5;vr_mock_hand_to main grenadepouch;wait5"
GRIP="+grabmain;vr_mock_button main grip 1;wait10"
LETGO="vr_mock_button main grip 0;-grabmain;wait5"
PIN="+attack;wait3;-attack;wait3"
AWAY="vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait5"
# (The back pouch's grenade comes out turned to be thrown, along the hand: brought to the muzzle by the mock as the
# ammo pouch's is, it lies across the barrel; test step 17 holds it there the right way, as a hand turned to it would.)
HELD="vr_reload_test 17;impulse 125;wait10"
LPORT="vr_mock_hand_to main lport 6;wait5;vr_mock_hand_to main lport 6;wait10;vr_mock_hand_to main lport;wait5;vr_mock_hand_to main lport;wait20"

# The off hand holding the launcher (impulse $1), its muzzle up ahead, emptied into the reserve; the main hand empty.
setup() {
    echo "map e1m1;wait60;developer 1;god;vr_reload_debug 1;vr_weapon_grip_mode 1;impulse 9;wait2;impulse $1;wait3;vr_test_weaponinst 7;impulse 120;wait3;give r 30;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;vr_mock_hand off -0.15 1.0 ${OFFZ:--0.40} 140 0 0;wait10;vr_reload_test 5;impulse 125;wait2"
}
last() { echo "$1" | grep "^reload: off hand" | tail -1; }
clip() { last "$1" | sed -E 's/^reload: off hand weapon [0-9]+ clip ([0-9]+) .*/\1/'; }
frames() { echo "$1" | grep "vrpouch_ammo.mdl" | sed -E 's/.* frame ([0-9]+) skin ([0-9]+) .*/\1.\2/' | tr '\n' ' ' | sed 's/ $//'; }

# 1. The pouch's rounds by the reserve: 0, 1, 3, 9 (frames: the empty pouch 0; rockets 14-16, grenades 17-20, proximity
# grenades 21-24).
for row in "160 rocket_launcher 0.0_14.0_16.0_16.0" "158 grenade_launcher 0.0_17.0_19.0_20.0" "159 proximity_launcher 0.0_21.0_23.0_24.0"; do
    set -- $row; name=${2//_/ }; want=${3//_/ }
    S="$(setup $1)"
    log=$(bash $KIT/run.sh $AGENT -Script "$S;give r 0;wait5;vr_dumpview;give r 1;wait5;vr_dumpview;give r 3;wait5;vr_dumpview;give r 9;wait5;vr_dumpview;toggleconsole;quit" -Filter "$F" 2>&1)
    check $([ "$(frames "$log")" = "$want" ] && echo 1 || echo 0) "$name: the pouch shows 0, 1, 3, 9 rounds as frames.skins $(frames "$log") (want $want)"
done
log=$(bash $KIT/run.sh $AGENT -Script "$(setup 158);impulse 42;wait5;$REP;vr_dumpview;toggleconsole;quit" -Filter "$F" 2>&1)
m=$(echo "$log" | grep "^reload: rockets" | tail -1 | sed -E 's/.*multi-rockets ([0-9]+).*/\1/')
want="$((16 + (m < 4 ? m : 4))).1"; [ "$m" = 0 ] && want=0.1
check $([ "$(frames "$log")" = "$want" ] && echo 1 || echo 0) "the multi-grenade mode: $m multi-rockets shown as $(frames "$log") (want $want: skin 1)"

# 2. The ammo pouch's grenade armed by hand goes off; armed, it doesn't load.
log=$(bash $KIT/run.sh $AGENT -Script "$(setup 158);$POUCH;$GRIP;$PIN;$AWAY;$LETGO;wait400;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
check $(echo "$log" | grep -q "hand grenade armed (the pin pulled)" && echo "$log" | grep -q "^grenade: grenade goes off" && echo 1 || echo 0) "the ammo pouch's grenade: the pin pulled, let go of, it goes off ($(echo "$log" | grep -m1 "goes off" | cut -c1-60))"
blog=$(bash $KIT/run.sh $AGENT -Script "$(setup 158);$BACK;$GRIP;$PIN;$AWAY;$LETGO;wait400;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
check $(echo "$blog" | grep -q "hand grenade armed (the pin pulled)" && echo "$blog" | grep -q "^grenade: grenade goes off" && echo 1 || echo 0) "the back pouch's grenade: the same"
log=$(bash $KIT/run.sh $AGENT -Script "$(setup 158);$POUCH;$GRIP;$PIN;$LPORT;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
check $(echo "$log" | grep -q "hand grenade armed" && [ "$(clip "$log")" = 0 ] && ! echo "$log" | grep -q "^reload: 1 into" && echo 1 || echo 0) "an armed grenade at the muzzle is not loaded (clip $(clip "$log"))"

# 3. The back pouch's grenade into the grenade launcher; the multi-grenade; the ammo pouch's back into the back pouch.
log=$(bash $KIT/run.sh $AGENT -Script "$(setup 158);$REP;$BACK;$GRIP;$REP;$HELD;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
check $(echo "$log" | grep -q "^grenade: hand grenade taken from the pouch" && echo "$log" | grep -q "^reload: rockets 33 " && [ "$(clip "$log")" = 1 ] && echo 1 || echo 0) "the back pouch's grenade goes into the grenade launcher (rockets 34 -> 33, clip $(clip "$log"))"
log=$(bash $KIT/run.sh $AGENT -Script "$(setup 158);impulse 42;wait5;$REP;$BACK;$GRIP;$REP;$HELD;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
m0=$(echo "$log" | grep "^reload: rockets" | head -1 | sed -E 's/.*multi-rockets ([0-9]+).*/\1/')
m1=$(echo "$log" | grep "^reload: rockets" | tail -1 | sed -E 's/.*multi-rockets ([0-9]+).*/\1/')
check $(echo "$log" | grep -q "^grenade: hand multi-grenade taken" && [ $((m0 - m1)) = 1 ] && [ "$(clip "$log")" = 1 ] && echo 1 || echo 0) "the multi-grenade mode: the back pouch's multi-grenade goes in (multi-rockets $m0 -> $m1)"
log=$(bash $KIT/run.sh $AGENT -Script "$(setup 158);$POUCH;$GRIP;$REP;$BACK;$LETGO;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
check $(echo "$log" | grep -q "put back in the pouch" && echo "$log" | grep "^reload: rockets" | tail -1 | grep -q "^reload: rockets 34 " && echo 1 || echo 0) "the ammo pouch's grenade let go of at the back pouch goes in (rockets 34)"

# 4. Proximity grenades: the back pouch's into the proximity launcher; the ammo pouch's armed and let go of: a mine.
log=$(bash $KIT/run.sh $AGENT -Script "$(setup 159);$BACK;$GRIP;$REP;$HELD;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
check $(echo "$log" | grep -q "^grenade: hand proximity grenade taken" && [ "$(clip "$log")" = 1 ] && echo 1 || echo 0) "the back pouch's proximity grenade goes into the proximity launcher (clip $(clip "$log"))"
log=$(bash $KIT/run.sh $AGENT -Script "$(setup 159);$POUCH;$GRIP;$PIN;$AWAY;$LETGO;wait20;$REP;wait400;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
mines=$(echo "$log" | grep "^reload: rockets" | sed -E 's/.* ([0-9]+) mines.*/\1/' | tr '\n' ' ')
check $(echo "$log" | grep -q "proximity grenade armed" && echo "$log" | grep -q "a mine" && [ "$mines" = "0 1 0 " ] && echo 1 || echo 0) "the ammo pouch's proximity grenade armed, let go of: a mine, gone off by the player (mines $mines)"
log=$(bash $KIT/run.sh $AGENT -Script "$(setup 159);$POUCH;$GRIP;$PIN;$LPORT;$REP;toggleconsole;quit" -Filter "$F" 2>&1)
check $(echo "$log" | grep -q "proximity grenade armed" && [ "$(clip "$log")" = 0 ] && echo 1 || echo 0) "an armed proximity grenade is not loaded (clip $(clip "$log"))"

# 5. A grenade lying about, shot (test step 19): it goes off.
log=$(bash $KIT/run.sh $AGENT -Script "$(OFFZ=-0.2 setup 158);vr_reload_test 12;impulse 125;wait60;vr_reload_test 19;impulse 125;wait30;$REP;toggleconsole;quit" -Filter "$F|set off" 2>&1)
check $(echo "$log" | grep -q "^grenade: grenade goes off" && echo "$log" | grep -q "rockets [0-9]* .*, 0 rounds lying" && echo 1 || echo 0) "a grenade lying about, shot, goes off"
exit $fail
