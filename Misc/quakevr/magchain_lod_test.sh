#!/bin/bash
# magchain_lod_test.sh <agent> [shot] -- ROUND22.md, "Magnetic holsters: chain origin, long throws, sounds, the hand
# point". The shotgun bound to the right hip holster (slot 3), drawn, dropped at your feet (it never comes back:
# vr_magholster_return_time 0, _distance 0), then you walk away from it: at about 3 m the tether is links ("tether
# chain"), at about 10 m a plain line ("tether line"), never gone; Full Chain Up To (vr_magholster_chain_lod) 12 at 10
# m: links again; 0: a line at 3 m. The chain origin sliders (vr_magholster_chain_x/y/z and the hips') move the chain's
# end: "chain end ... off the reach point" changes. "shot": the tether seen from behind you at 3 m and 10 m
# (scratch/magchain_lod.png). Prints PASS/FAIL; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
P="map e1m1;wait60;developer 1;god;notarget;vr_magholster_return_time 0;vr_magholster_return_distance 0;"
BIND="vr_mock_hand_to off holster 3;wait5;vr_mock_button off stickclick 1;wait3;vr_mock_button off stickclick 0;wait3;"
DRAW="vr_mock_hand_to main holster 3;wait5;+grabright;vr_mock_button main grip 1;wait10;"
DROP="vr_mock_hand_glide 0.4;vr_mock_hand_to main by 20 0 15;wait90;vr_mock_hand_glide 0;-grabright;vr_mock_button main grip 0;wait60;vr_mock_hand main 0.3 1.0 -0.2 0 0 0;"
CAM="vr_mock_camera 0.8 1.2 -0.2 15 180;wait10;" # (beside you on the right, looking back past your hip to the gun)
walk() { echo "vr_mock_stick off 0 1;wait$1;vr_mock_stick off 0 0;wait30;"; }
INFO="vr_magholster_info;"
fail=0
F="^magholster 3:|tether|composed"
if [ "$2" = "shot" ]; then
    S="$P$BIND$DRAW$DROP$(walk 8)$INFO${CAM}screenshot;vr_mock_camera off;$(walk 28)$INFO${CAM}screenshot;vr_mock_camera off;"
    bash $KIT/run.sh $AGENT -Clean -Out magchain_lod.png -Script "${S}toggleconsole;quit" -Filter "$F" | cut -c1-200
    exit 0
fi
S="$P$BIND$DRAW$DROP$(walk 8)$INFO vr_magholster_chain_lod 0;wait3;$INFO vr_magholster_chain_lod 5;wait3;$(walk 28)$INFO"
S="$S vr_magholster_chain_lod 12;wait3;${INFO}vr_magholster_chain_lod 5;vr_magholster_chain_x 2;vr_magholster_chain_hip_z 3;wait3;$INFO"
OUT=$(bash $KIT/run.sh $AGENT -Script "${S}toggleconsole;quit" -Filter "^magholster 3:|tether")
# (The info line wraps: join each "magholster 3:" line with the next.)
L=$(echo "$OUT" | grep -v "^$" | grep -v "^exit=" | awk '/^magholster 3:/{if(l)print l; l=$0; next}{l=l" "$0}END{print l}')
check() { # $1 line number, $2 regex, $3 what
    if echo "$L" | sed -n "$1p" | grep -q "$2"; then r=PASS; else r=FAIL; fail=1; fi
    echo "$r $3: $(echo "$L" | sed -n "$1p" | sed 's/.*(entity [0-9]*, \([0-9.]*\) m.*tether \([a-z]*\) \([0-9]*\) links.*/\1 m, tether \2 \3 links/')"
}
check 1 "tether chain [1-9]" "about 3 m: links"
check 2 "tether line" "about 3 m, Full Chain Up To 0: a line"
check 3 "tether line" "about 10 m: a line (never gone)"
check 4 "tether chain [1-9]" "about 10 m, Full Chain Up To 12: links"
E1=$(echo "$L" | sed -n 4p | sed 's/.*chain end \([0-9.]*\) cm.*/\1/'); E2=$(echo "$L" | sed -n 5p | sed 's/.*chain end \([0-9.]*\) cm.*/\1/')
if [ "$E1" != "$E2" ] && [ -n "$E2" ]; then r=PASS; else r=FAIL; fail=1; fi
echo "$r the chain origin moved (all forward 2, hip up 3): chain end $E1 -> $E2 cm off the reach point"
exit $fail
