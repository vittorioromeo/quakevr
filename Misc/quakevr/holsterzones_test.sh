#!/bin/bash
# holsterzones_test.sh <agent> [shot] -- ROUND22.md, "Holster zones: boxes where they are drawn". A gun drawn from the
# right hip, held muzzle down (mock pitch -40), its grip put at the right hip holster as drawn (vr_mock_hand_to main
# zonegrip 3 <out> <side> 0: units in the holster's frame, whose `out` is sideways at the hip: negative is in, towards the
# ammo pouch), let go: IN (back in the holster) or DROP. vr_holster_zones 0 (the old spheres) drops it 1 unit in from
# the drawn holster (no hotspot) and 2 in (the ammo pouch's hotspot); the zones take it in, but at the pouch itself.
# Then the empty off hand at every holster's and pouch's reach point: its hotspot. "shot": the wireframes
# (scratch/holsterzones.png: nothing held; the gun held 2 units in, its zone green). Prints PASS/FAIL; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
P="developer 1;map e1m1;wait60;god;notarget;vr_reload_mode 3;vr_holster_mode 0;vr_handgrenade 1;"
DRAW="vr_mock_hand_to main holster 3;wait5;+grabright;vr_mock_button main grip 1;wait10;vr_mock_hand main 0.3 1.0 -0.2 -40 0 0;wait3;"
fail=0
if [ "$2" = "shot" ]; then
    CAM="0.25 1.45 -1.0 30 180"
    AT="vr_mock_hand_to main zonegrip 3 -2 0 0;wait3;vr_mock_hand_to main zonegrip 3 -2 0 0;wait3;"
    S="${P}vr_holster_zones_show 1;wait10;vr_mock_camera $CAM;wait20;screenshot;$DRAW$AT vr_mock_camera $CAM;wait20;screenshot;vr_mock_camera off;"
    bash $KIT/run.sh $AGENT -Clean -Out holsterzones.png -Script "${S}toggleconsole;quit" -Filter "composed|ERROR"
    exit 0
fi
# mode out,side expected
CASES="0:0,0:IN 0:-1,0:DROP 0:-2,0:DROP 0:-3,0:DROP 1:2,0:IN 1:0,0:IN 1:-1,0:IN 1:-2,0:IN 1:-3,0:IN 1:-2,2:IN 1:-7,0:DROP"
for c in $CASES; do
    mode=${c%%:*}; rest=${c#*:}; pos=${rest%%:*}; want=${rest#*:}
    AT="vr_mock_hand_to main zonegrip 3 ${pos%,*} ${pos#*,} 0;wait3;vr_mock_hand_to main zonegrip 3 ${pos%,*} ${pos#*,} 0;wait3;"
    S="${P}vr_holster_zones $mode;wait10;$DRAW$AT vr_holster_zones_report;-grabright;vr_mock_button main grip 0;wait30;vr_holster_zones_report;"
    OUT=$(bash $KIT/run.sh $AGENT -Script "${S}toggleconsole;quit" -Filter "^zone (rhip|hand main:)")
    HS=$(echo "$OUT" | grep "^zone hand main:" | head -1 | sed 's/.*hotspot \([0-9]*\).*points (\([^)]*\)).*/hotspot \1, hand at \2/')
    if echo "$OUT" | grep "^zone rhip" | tail -1 | grep -q "a gun in it"; then got=IN; else got=DROP; fi
    if [ "$got" = "$want" ]; then r=PASS; else r=FAIL; fail=1; fi
    echo "$r zones $mode, grip at the drawn hip holster + ($pos): $HS -> $got (want $want)"
done
S="$P wait10;"
for h in 0 1 2 3 4 5; do S="$S vr_mock_hand_to off holster $h;wait3;vr_mock_hand_to off holster $h;wait3;vr_holster_zones_report;"; done
S="$S vr_mock_hand_to off ammopouch;wait3;vr_mock_hand_to off ammopouch;wait3;vr_holster_zones_report;"
S="$S vr_mock_hand_to off grenadepouch;wait3;vr_mock_hand_to off grenadepouch;wait3;vr_holster_zones_report;"
GOT=$(bash $KIT/run.sh $AGENT -Script "${S}toggleconsole;quit" -Filter "^zone hand off:" | grep "^zone hand off:" | sed 's/.*hotspot \([0-9]*\).*/\1/' | tr '\n' ' ')
if [ "$GOT" = "3 4 5 6 8 9 12 11 " ]; then r=PASS; else r=FAIL; fail=1; fi
echo "$r the empty hand at the holsters' and pouches' points: hotspots $GOT(want 3 4 5 6 8 9 12 11)"
exit $fail
