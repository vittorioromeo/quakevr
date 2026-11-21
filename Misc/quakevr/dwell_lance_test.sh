#!/bin/bash
# dwell_lance_test.sh <agent> -- Dwell's Crystal Lance as a two-handed lance (dwellwpn2; DWELL.md 9.9): vr_dw_lance_pole,
# its model made from Dwell's world model (vr_adweapons.cpp, weapons.txt slot 55), the engine's pole grip (vr_twohand.cpp
# FREE_POLE). Needs Dwell v2.2 in the base dir (<base>/dwellv2p2). Headless on e1m1:
#  1. vr_ad_test 217: the Lance drawn as a lance and dealt its crystal head's blow; with vr_dw_lance_pole 0 the pistol
#     and the gun's club (dwlance verdict); the seven models made (vr_adweapon_models).
#  2. Mock VR, the Lance in the main hand (vr_ad_test 207), vr_debug_2h_grip 2: the off hand closes on its pole 0.75 of
#     the way to the tip (a pole grip taken), slides to 0.95 and back to 0.35 still gripping (the place along the pole
#     follows it: further, then nearer; never let go), and the two-handed hold counts (vr_status: "on its pole").
# Exits 1 on a failure.
AGENT=$1
source "$(dirname "$0")/qvrtest.sh"
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait10;god;notarget;vr_adweapon_models;vr_ad_test 217" \
    -QuitOn "dwlance: [0-9]+ passed" -Timeout 300 -Filter "dwlance|vr_adweapon_models:|progs/dw_lance|rror|exit=" 2>&1 | tr -d '\r')
echo "$log" | grep -E "FAIL|vr_adweapon_models:|progs/dw_lance" | cut -c1-200 | head -6
expect_verdict "$log" dwlance "the Lance as a lance, and as the pistol with the setting off"
check $(grep -q "vr_adweapon_models: 8 made, 0 not made" <<<"$log" && echo 1 || echo 0) "the eight models made (the lance from Dwell's world model)"
check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "models: the run ended on its verdict"
OFF="+grableft;vr_mock_button off grip 1;wait30"
SLIDE="vr_mock_hand_to off held 0.95;wait30;echo dwlance-mark: far;vr_mock_hand_to off held 0.35;wait30;echo dwlance-mark: near;vr_status"
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait10;god;notarget;vr_mock_hand main 0.2 1.2 -0.3 70 0 0;vr_ad_test 207;wait10;vr_debug_2h_grip 2;vr_mock_hand_to off held 0.75;wait5;vr_mock_hand_to off held 0.75;wait5;$OFF;echo dwlance-mark: took;$SLIDE;echo dwlance-test: done" \
    -QuitOn "dwlance-test: done" -Timeout 300 -Filter "2h grip|dwlance-mark|off .*angles|rror|exit=" 2>&1 | tr -d '\r')
took=$(grep -m1 "took the weapon anywhere" <<<"$log")
echo "  $took" | cut -c1-160
check $(grep -q "took the weapon anywhere (pole)" <<<"$log" && echo 1 || echo 0) "the off hand took the Lance's pole (a pole grip)"
along() { awk -v m="$1" '/dwlance-mark/ { if (index($0, m)) { print last; exit } } /on the pole/ { for (i = 1; i <= NF; i++) if ($i == "from" && $(i+1) == "the" && $(i+2) == "holding") last = $(i-1) }' <<<"$log"; }
a0=$(along took); a1=$(along far); a2=$(along near)
echo "  along the pole from the holding hand: took $a0, slid out $a1, slid in $a2"
check $(awk -v a="$a0" -v b="$a1" -v c="$a2" 'BEGIN { print (a != "" && b != "" && c != "" && b > a + 3 && c < a - 3) ? 1 : 0 }') "the grip slid along the pole with the hand (out, then in)"
check $(grep -q "let go of the weapon held anywhere" <<<"$log" && echo 0 || echo 1) "never let go while sliding"
check $(grep -q "anywhere: on its pole" <<<"$log" && echo 1 || echo 0) "vr_status: the off hand helping on its pole"
check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "pole: the run ended on its last line"
verdict
exit $fail
