#!/bin/bash
# gadget_lock_test.sh <agent> -- the gadget lock (vr_gadgetlock.cpp): double taps on the gadget's screen (the main hand,
# glided onto it and back: vr_mock_hand_glide; fixed frames) with
#   A two different weapons (main shotgun, off nailgun)          PASS: the double tap counts (on; then toggled off)
#   B one shotgun two-handed (the off hand on it)                PASS: locked, and no tap counts once locked
#   D one sword in both hands                                    PASS: the same
#   C one crate in both hands                                    PASS: locked, no tap once locked
#   E the crate's off hand lets go, at once a double tap (grace) PASS: no double tap; the first tap inside the grace
#                                                                       ignored, the second (after it) a first tap
#   F a calm double tap after                                    PASS: counts
#   H the side button pressed during a two-handed hold, then after letting go: PASS: only the second press counts
# Ends on its verdict line ("gk_test: N passed, M failed").
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
OUT=$TREE/scratch/gadget_lock_test_out.txt
mkdir -p "$TREE/scratch"
S="map vrfiringrange;wait60;god;notarget;developer 1;vr_debug_bullettime 1;vr_bullettime_cooldown 0;vr_bullettime_duration 600"
S="$S;vr_bullettime_trigger_cooldown 0.5;vr_bullettime_tap_gesture 1;vr_gadget_button 0;vr_weapon_grip_mode 0"
S="$S;vr_bullettime_tap_calm_speed 0;vr_bullettime_tap_blow_quiet 0"
# (Test aids: the holds kept while the main hand reaches the gadget's screen on the off wrist -- a tap's tracked hand
# moves 10 cm or more against the other, which pulls it off a grip as shipped.)
S="$S;vr_2h_sticky 30;vr_carry_two_hands_drift 60"
settle() { S="$S;vr_mock_hand_glide 1;vr_mock_hand_to main screen 10 0;wait80;vr_mock_hand_to main screen 10 0;wait80"; }
tap() { S="$S;vr_mock_hand_glide 0.06;vr_mock_hand_to main screen 0 0;wait6;vr_mock_hand_glide 0.08;vr_mock_hand_to main screen 10 0;wait8"; }
double() { S="$S;echo TAP"; tap; S="$S;echo TAP"; tap; }
home() { S="$S;vr_mock_hand_glide 0;vr_mock_hand main 0.15 1.2 -0.45 70 0 0;vr_mock_hand off -0.15 1.2 -0.45 70 0 0;wait10"; }
letgo() { S="$S;-grabright;vr_mock_button main grip 0;-graboff;-grableft;vr_mock_button off grip 0;wait30"; }
mainGun() { S="$S;+grabright;vr_mock_button main grip 1;impulse $1;wait30"; }
offOn() { S="$S;vr_mock_hand_to off held $1;wait5;vr_mock_hand_to off held $1;wait5;+grableft;vr_mock_button off grip 1;wait30"; }
locked_case() { settle; double; S="$S;wait40"; double; S="$S;wait40;echo === END"; }
# A
S="$S;echo === A two weapons"; home; mainGun 154; S="$S;vr_mock_hand off -0.15 1.2 -0.45 70 0 0;wait5;+grableft;vr_mock_button off grip 1;impulse 176;wait30"
settle; double; S="$S;wait40;echo === END;vr_bullettime;wait10"; letgo
# B
S="$S;echo === B shotgun two-handed"; home; mainGun 154; offOn 0.6; locked_case; letgo
# D
S="$S;echo === D sword in both hands"; home; mainGun 163; offOn 0.3; locked_case; letgo
# E (the shotgun two-handed, the main hand over the screen): the off hand lets go, at once a double tap
home; mainGun 154; offOn 0.6; settle
S="$S;echo === E let go then at once a double tap;-grableft;vr_mock_button off grip 0"; double; S="$S;wait70;echo === END"
S="$S;echo === F a calm double tap after"; settle; double; S="$S;wait40;echo === END;vr_bullettime;wait10"; letgo
# C
S="$S;echo === C crate in both hands"; home
S="$S;vr_mock_hand main 0.16 1.25 -0.55 0 0 0;+grabright;vr_mock_button main grip 1;wait5;vr_test_spawn 107;vr_test_spawn_dist 24;vr_test_spawn_hold 1;impulse 241;wait10"
S="$S;vr_carry2h_grip_test hand off 2 14 0;wait3;vr_carry2h_grip_test hand off 2 10.5 0;wait3;+graboff;vr_mock_button off grip 1;wait30"
locked_case; letgo
# H the side button: pressed during a two-handed hold (nothing), then let go of and pressed again (it counts)
home; mainGun 154; offOn 0.6; settle
S="$S;echo === H side button;vr_gadget_button 1;vr_debug_gadget_button 1;vr_mock_hand_glide 0.1;vr_mock_hand_to main button;wait20;vr_mock_hand_to main screen 10 0;wait20"
S="$S;-grableft;vr_mock_button off grip 0;wait40;vr_mock_hand_to main button;wait20;vr_mock_hand_to main screen 10 0;wait20;echo === END;vr_debug_gadget_button 0"; letgo
S="$S;echo === DONE;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -QuitOn "=== DONE" -Filter "===|screen tapped|first tap|no second|never stopped|ignored|dropped|bullet time: (on|off)|gadget lock|tap off|rror|no gadget|usage|carry: (both|one|kept)|TAP|gadget button: press" -Timeout 300 "$@" 2>&1 | tr -d '\r' > "$OUT"
case_of() { sed -n "/=== $1 /,/=== END/p" "$OUT"; }
after_lock() { case_of "$1" | sed -n '/gadget lock: on/,$p'; }
fail=0; pass=0
chk() { if [ "$1" = 1 ]; then pass=$((pass+1)); echo "PASS $2"; else fail=$((fail+1)); echo "FAIL $2"; fi; }
has() { case_of "$1" | grep -q "$2" && echo 1 || echo 0; }
hasnt() { case_of "$1" | grep -q "$2" && echo 0 || echo 1; }
none_locked() { after_lock "$1" | grep -q "first tap\|double tap:" && echo 0 || echo 1; }
chk $(has A "double tap: screen tapped") "A two weapons: the double tap counts"
chk $(hasnt A "gadget lock: on") "A ... never locked"
chk $(hasnt B "double tap: screen tapped") "B shotgun two-handed: no double tap"
chk $(has B "gadget lock: on (both hands hold one weapon") "B ... locked"
chk $(none_locked B) "B ... no tap counts once locked"
chk $(hasnt D "double tap: screen tapped") "D sword in both hands: no double tap"
chk $(has D "gadget lock: on (both hands hold one weapon") "D ... locked"
chk $(none_locked D) "D ... no tap counts once locked"
chk $(hasnt C "double tap: screen tapped") "C crate in both hands: no double tap"
chk $(has C "gadget lock: on (both hands hold the same prop") "C ... locked"
chk $(none_locked C) "C ... no tap counts once locked"
chk $(hasnt E "double tap: screen tapped") "E let go then at once: no double tap"
chk $(has E "gadget lock: let go") "E ... the grace"
chk $(case_of E | sed -n '/gadget lock: off/,$p' | grep -c "bullet time: first tap" | grep -q "^1$" && echo 1 || echo 0) "E ... the tap after the grace counts as a first"
chk $(has F "double tap: screen tapped") "F after the grace: counts"
chk $(case_of H | grep -c "gadget button: pressed" | grep -q "^1$" && echo 1 || echo 0) "H the side button: one press counts"
chk $(case_of H | sed -n '/gadget lock: off/,$p' | grep -q "gadget button: pressed" && echo 1 || echo 0) "H ... the one after letting go"
echo "gadget_lock_test: $pass passed, $fail failed"
exit $fail
