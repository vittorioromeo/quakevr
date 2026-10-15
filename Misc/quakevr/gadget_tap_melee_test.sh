#!/bin/bash
# gadget_tap_melee_test.sh <agent> -- bullet time's double tap on the gadget screen in a melee's thick (his note
# 2026-10-15: slow motion turned on by accident; ROUND22.md, "Slow motion by accident: the screen tap in melee"),
# headless, the shipped tap settings (Double Tap). Taps are glided (vr_mock_hand_glide) from 10 cm over the screen onto it
# and back (0.06 s down: 1.7 m/s).
#   A  a calm double tap: on (and B again: off)                      PASS: "double tap" twice, on then off
#   C  a double tap 0.2 s after a shove landed (vr_knockdown_test 21)  PASS: no double tap ("after a blow")
#   D  the same 1 s after it                                         PASS: a double tap, on (E: off)
#   F  a swing across the screen (4 m/s, 3 cm over) and at once a double tap: PASS: no double tap ("tapping hand")
#   G  the gadget's hand swung (3.75 m/s) and at once a double tap      PASS: no double tap ("gadget's hand")
#   H  F again with vr_bullettime_tap_calm_speed 0 and _blow_quiet 0 (the guards off): the swing's own taps aside, a
#      double tap goes through as before                              PASS: on
# Prints PASS/FAIL per case; exits 1 on a failure. -RealTime: the glides and the windows are realtime.
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
OUT=$TREE/scratch/gadget_tap_melee_out.txt
mkdir -p "$TREE/scratch"
S="map vrfiringrange;wait60;god;notarget;developer 1;vr_debug_bullettime 1;vr_bullettime_cooldown 0;vr_bullettime_duration 600"
S="$S;vr_bullettime_trigger_cooldown 0.5;vr_bullettime_tap_gesture 1;vr_gadget_button 0;vr_knockdown_chance 0.01"
settle() { # <point>: the hand settled 10 cm over the screen's middle (a slow glide)
    S="$S;vr_mock_hand_glide 1;vr_mock_hand_to main $1 10 0;wait120;vr_mock_hand_to main $1 10 0;wait100"
}
tap() { # <point> <seconds down>: down onto the screen, then back up
    S="$S;vr_mock_hand_glide $2;vr_mock_hand_to main $1 0 0;wait6;vr_mock_hand_glide 0.08;vr_mock_hand_to main $1 10 0;wait8"
}
double() { tap screen 0.06; tap screen 0.06; }
S="$S;echo === A calm double tap"; settle screen; double; S="$S;wait60;echo === END"
S="$S;echo === B again: off"; settle screen; double; S="$S;wait60;echo === END"
S="$S;vr_test_spawn 0;vr_test_spawn_dist 60;wait40"
S="$S;echo === C double tap just after a shove"; settle screen; S="$S;vr_knockdown_test 21;wait14"; double; S="$S;wait60;echo === END"
S="$S;echo === D double tap 1 s after a shove"; settle screen; S="$S;vr_knockdown_test 21;wait72"; double; S="$S;wait60;echo === END"
S="$S;echo === E again: off"; settle screen; double; S="$S;wait60;echo === END"
S="$S;echo === F a swing across then a double tap;vr_mock_hand_glide 1;vr_mock_hand_to main screen 3 -30;wait120;vr_mock_hand_to main screen 3 -30;wait60"
S="$S;vr_mock_hand_glide 0.15;vr_mock_hand_to main screen 3 30;wait11;vr_mock_hand_glide 0.06;vr_mock_hand_to main screen 10 0;wait5"; double; S="$S;wait60;echo === END"
S="$S;echo === G the gadget's hand swung then a double tap"; settle screen
S="$S;vr_mock_hand_glide 0.08;vr_mock_hand_to off by 0 0 8;wait6;vr_mock_hand_to off by 0 0 -8;wait6;vr_mock_hand_glide 0.03;vr_mock_hand_to main screen 10 0;wait3"; double; S="$S;wait60;echo === END"
S="$S;echo === H the guards off: F again;vr_bullettime_tap_calm_speed 0;vr_bullettime_tap_blow_quiet 0;vr_mock_hand_glide 1;vr_mock_hand_to main screen 3 -30;wait120;vr_mock_hand_to main screen 3 -30;wait60"
S="$S;vr_mock_hand_glide 0.15;vr_mock_hand_to main screen 3 30;wait11;vr_mock_hand_glide 0.06;vr_mock_hand_to main screen 10 0;wait5"; double; S="$S;wait60;echo === END"
S="$S;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "===|screen tapped|first tap|no second|never stopped|ignored|dropped|bullet time: (on|off)|rror|no gadget|usage" -Timeout 300 "$@" 2>&1 | tr -d '\r' > "$OUT"
fail=0
check() { if [ "$1" = 1 ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
case_of() { sed -n "/=== $1 /,/=== END/p" "$OUT"; }
has() { case_of "$1" | grep -q "$2" && echo 1 || echo 0; }
hasnt() { case_of "$1" | grep -q "$2" && echo 0 || echo 1; }
check $(has A "double tap: screen tapped") "A a calm double tap counts"
check $(has B "double tap: screen tapped") "B again (off)"
check $(hasnt C "double tap: screen tapped") "C a double tap just after a shove: ignored"
check $(has C "after a blow") "C ... for the shove"
check $(has D "double tap: screen tapped") "D 1 s after a shove: counts"
check $(has E "double tap: screen tapped") "E again (off)"
check $(hasnt F "double tap: screen tapped") "F a swing across then a double tap: ignored"
check $(has F "tapping hand") "F ... for the swing"
check $(hasnt G "double tap: screen tapped") "G the gadget's hand swung then a double tap: ignored"
check $(has G "gadget's hand") "G ... for the gadget's hand"
check $(has H "double tap: screen tapped") "H the guards off: counts as before"
grep -c "ignored\|dropped" "$OUT" | sed 's/^/taps ignored or dropped: /'
exit $fail
