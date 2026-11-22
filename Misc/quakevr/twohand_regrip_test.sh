#!/bin/bash
# twohand_regrip_test.sh <agent> -- every prop in both hands (ROUND21.md, "Every prop in both hands"): a prop put into the
# off hand (impulse 252, vr_test_held_pick: 4 a grunt's head, 1 the player's head, 0 a small gib, 9 the nearest rock, 10
# a hand grenade from the pouch, 11 the nearest box of shells, 12 the nearest brick), the main hand's fist on it gripping
# (taken in both), then a hand-over: the off hand lets go (held in the main), grips again (both), the main lets go (held
# in the off), grips again (both), and both let go. Prints per prop: how many times it was held in both (3), kept by one
# hand (2), and the most it moved when one hand let go (units; no snap: ~0).
# Ends on "twohand_regrip_test: N passed, M failed" (each pick: held in both 3, kept by one 2, moved under a unit, let go by
# both 1).
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
pass=0; fail=0
# pick and the main hand's place (tracking space, metres) where its fist touches what the off hand holds. (The test pick puts
# the prop's origin on the off hand's fist, not fitted to its surface as a real pickup: the grunt's head (4, Size 0.7) and
# the grenade (10) sit centred on it, so the main hand comes closer for them: at 0.06 / 0.03 neither was ever taken in
# both, "the other hand is not within reach", since ROUND22.)
for spec in "4 0.03 1.29" "1 0.06 1.29" "0 0.06 1.25" "9 0.06 1.25" "10 0.0 1.25" "11 0.06 1.25" "12 0.06 1.25"; do
    set -- $spec; pick=$1; mx=$2; my=$3
    log=$(bash $KIT/run.sh $AGENT -Script "map vrfiringrange;wait60;god;notarget;developer 1;vr_mock_hand off -0.07 1.25 -0.25 70 0 0;vr_mock_hand main 0.3 1.25 -0.25 70 0 0;wait10;+graboff;vr_mock_button off grip 1;vr_test_held_pick $pick;impulse 252;wait20;vr_mock_hand main $mx $my -0.25 70 0 0;wait3;+grabright;vr_mock_button main grip 1;wait15;-graboff;vr_mock_button off grip 0;wait15;+graboff;vr_mock_button off grip 1;wait15;-grabright;vr_mock_button main grip 0;wait15;+grabright;vr_mock_button main grip 1;wait15;-graboff;vr_mock_button off grip 0;-grabright;vr_mock_button main grip 0;wait30;toggleconsole;quit" \
        -Filter "^carry: (both hands|kept by|both hands let go|one hand)|^test: " 2>&1)
    what=$(echo "$log" | grep -m1 "^test: " | sed 's/^test: //; s/ in the off hand//')
    both=$(echo "$log" | grep -c "^carry: both hands$")
    kept=$(echo "$log" | grep -c "^carry: kept by")
    moved=$(echo "$log" | grep "^carry: kept by" | sed 's/.*moved \([0-9.]*\) units.*/\1/' | sort -n | tail -1)
    letgo=$(echo "$log" | grep -c "^carry: both hands let go")
    echo "pick $pick ($what): both $both kept $kept moved ${moved:-none} let-go-both $letgo"
    ok=$(awk -v b="$both" -v k="$kept" -v m="${moved:-99}" -v l="$letgo" 'BEGIN{print (b == 3 && k == 2 && m < 1 && l == 1) ? 1 : 0}')
    if [ "$ok" = 1 ]; then pass=$((pass+1)); else fail=$((fail+1)); echo "FAIL pick $pick"; fi
done
echo "twohand_regrip_test: $pass passed, $fail failed"
exit $fail
