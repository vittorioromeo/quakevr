#!/bin/bash
# dwell_lance_test.sh <agent> -- Dwell's Crystal Lance as a two-handed lance (dwellwpn2; DWELL.md 9.9): vr_dw_lance_pole,
# its model made from Dwell's world model (vr_adweapons.cpp, weapons.txt slot 55), the engine's pole grip (vr_twohand.cpp
# FREE_POLE). Needs Dwell v2.2 in the base dir (<base>/dwellv2p2). Headless on e1m1:
#  1. vr_ad_test 217: the Lance drawn as a lance and dealt its crystal head's blow; with vr_dw_lance_pole 0 the pistol
#     and the gun's club (dwlance verdict); the seven models made (vr_adweapon_models).
#  2. Mock VR, the Lance in the main hand held like a spear (lancespear: the pole through the fist, the controller
#     pitched so the pole lies level; vr_ad_test 207), vr_debug_2h_grip 2: the off hand closes on its pole 0.75 of
#     the way to the tip (a pole grip taken), slides to 0.95 and back to 0.35 still gripping (the place along the pole
#     follows it: further, then nearer; never let go), the two-handed hold counts (vr_status: "on its pole") and the
#     beam goes along the pole (vr_status: the shot within 6 degrees of the line from the hand to the tip).
# Exits 1 on a failure.
AGENT=$1
source "$(dirname "$0")/qvrtest.sh"
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait10;god;notarget;vr_adweapon_models;vr_ad_test 217" \
    -QuitOn "dwlance: [0-9]+ passed" -Timeout 300 -Filter "dwlance|vr_adweapon_models:|progs/dw_lance|rror|exit=" 2>&1 | tr -d '\r')
echo "$log" | grep -E "FAIL|vr_adweapon_models:|progs/dw_lance" | cut -c1-200 | head -6
expect_verdict "$log" dwlance "the Lance as a lance, and as the pistol with the setting off"
check $(grep -q "vr_adweapon_models: 7 made, 0 not made" <<<"$log" && echo 1 || echo 0) "the seven models made (the lance from Dwell's world model)"
check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "models: the run ended on its verdict"
OFF="vr_mock_fingers main 0 1;+grableft;vr_mock_button off grip 1;wait30" # (the main hand's grip closed: it holds, the off hand slides)
SLIDE="vr_mock_hand_to off held 0.95;wait30;echo dwlance-mark: far;vr_mock_hand_to off held 0.35;wait30;echo dwlance-mark: near;vr_status"
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait10;god;notarget;vr_mock_hand main 0.2 1.1 -0.3 -22 0 0;vr_ad_test 207;wait10;vr_debug_2h_grip 2;vr_mock_hand_to off held 0.75;wait5;vr_mock_hand_to off held 0.75;wait5;$OFF;echo dwlance-mark: took;$SLIDE;echo dwlance-test: done" \
    -QuitOn "dwlance-test: done" -Timeout 300 -Filter "2h grip|dwlance-mark|off .*angles|the shot|rror|exit=" 2>&1 | tr -d '\r')
took=$(grep -m1 "took the weapon anywhere" <<<"$log")
echo "  $took" | cut -c1-160
check $(grep -q "took the weapon anywhere (pole)" <<<"$log" && echo 1 || echo 0) "the off hand took the Lance's pole (a pole grip)"
along() { awk -v m="$1" '/dwlance-mark/ { if (index($0, m)) { print last; exit } } /on the pole/ { for (i = 1; i <= NF; i++) if ($i == "from" && $(i+1) == "the" && $(i+2) == "holding") last = $(i-1) }' <<<"$log"; }
a0=$(along took); a1=$(along far); a2=$(along near)
echo "  along the pole from the holding hand: took $a0, slid out $a1, slid in $a2"
check $(awk -v a="$a0" -v b="$a1" -v c="$a2" 'BEGIN { print (a != "" && b != "" && c != "" && b > a + 3 && c < a - 3) ? 1 : 0 }') "the grip slid along the pole with the hand (out, then in)"
check $(grep -q "let go of the weapon held anywhere" <<<"$log" && echo 0 || echo 1) "never let go while sliding"
check $(grep -q "anywhere: on its pole" <<<"$log" && echo 1 || echo 0) "vr_status: the off hand helping on its pole"
shot=$(grep -m1 "main  the shot" <<<"$log" | awk '{ print $4 }')
echo "  the beam off the pole (the line from the hand to the tip): $shot deg"
check $(awk -v d="$shot" 'BEGIN { print (d != "" && d + 0 < 6) ? 1 : 0 }') "the beam goes along the pole (Shot Pitch: the spear held through the fist)"
check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "pole: the run ended on its last line"
# 3. dwellweap: its shot (no magazine: 6 cells from the pouch's 100, the beam's print), lancecharge: charged (held 30
#    frames and let go: no shot; held 90, a 1 s charge: the shot as it is let go, its bolt lingering; the charge drawn from the middle crystal's tip);
#    dwellweap2: its grip flipped by
#    the eject button (B, vr_revgrip.qc VR_RevGrip_HandFrame), reversed then upright again. (B held 10 frames: a 2-frame
#    press, dwellweap's, never reached a player frame, so it did not flip.)
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait10;god;notarget;developer 1;vr_mock_hand main 0.2 1.3 -0.3 -22 0 0;vr_debug_gunfx 1;vr_ad_test 207;wait60;+attack;wait30;-attack;wait30;+attack;wait90;-attack;wait5;edict 1;vr_debug_gunfx 0;echo dwlance-mark: flips;vr_mock_button main secondary 1;wait10;vr_mock_button main secondary 0;wait30;vr_mock_button main secondary 1;wait10;vr_mock_button main secondary 0;wait30;echo dwlance-test: done" \
    -QuitOn "dwlance-test: done" -Timeout 300 -Filter "dwlance:|lancecharge:|ammo_cells|revgrip:|rror|exit=" 2>&1 | tr -d '\r')
echo "$log" | grep -E "dwlance:|lancecharge: (let|fired|bolt)|ammo_cells|revgrip:" | cut -c1-160 | head -9
check $(grep -q "dwlance: the beam through" <<<"$log" && echo 1 || echo 0) "shot: the beam fired"
check $(grep -q "dwlance: let go at 0\.[0-9]* (hand 1): no shot" <<<"$log" && grep -q "lancecharge: let go short ent 1 hand 1" <<<"$log" && echo 1 || echo 0) "charge: held 30 frames and let go: no shot (the client fades it out)"
check $(grep -q "lancecharge: fired ent 1 hand 1" <<<"$log" && grep -q "dwlance: charged (hand 1) after 1\.0" <<<"$log" && echo 1 || echo 0) "charge: held 90 frames, charged in 1 s, fired as let go"
check $(grep -q "lancecharge: bolt ent 1 hand 1 lingers 0\.35 s" <<<"$log" && echo 1 || echo 0) "charge: its bolt lingers 0.35 s (vr_dw_lance_bolt)"
check $(grep -c "dwlance: the beam through" <<<"$log" | grep -qx 1 && echo 1 || echo 0) "charge: one shot only (none while held)"
# (weaponfx, his note 2026-11-23: the charge drawn from the middle crystal's tip: the spear's (28.16 5.27 77.41, on its
# pole's line; its muzzle anchor between the two outer prongs binds to a prong, 4 units off), vertexPosition's own place.)
check $(grep -qE "lancecharge: origin at the middle crystal's tip \(progs/dw_lance.mdl#vr vertex [0-9]+, at rest 28\.1[0-9] 5\.2[0-9] 77\.4[0-9]\)" <<<"$log" && echo 1 || echo 0) "charge: drawn from the middle crystal's tip"
# (lancebolt, his decision 2026-11-23: the bolt starts at the middle crystal's tip too, VR and flat, spear and gun: the
# beam's start as drawn, vr_client.cpp VR_UpdateBeam, within 1 unit of where the charge was drawn as it fired.)
boltcheck() { # <log> <label> <model regex> <min. units from the muzzle>
    local o b d m
    o=$(grep -oE "lancecharge: the charge fired ent 1 hand 1 at [-0-9.]+ [-0-9.]+ [-0-9.]+" <<<"$1" | tail -1 | awk '{ print $(NF-2), $(NF-1), $NF }')
    b=$(grep -oE "lancecharge: bolt ent 1 hand 1 from the tip \($3 vertex [0-9]+\) [-0-9.]+ [-0-9.]+ [-0-9.]+, [0-9.]+ off" <<<"$1" | tail -1 | tr -d ',' | awk '{ print $(NF-4), $(NF-3), $(NF-2), $(NF-1) }')
    d=$(awk -v o="$o" -v b="$b" 'BEGIN { if(split(o, p, " ") < 3 || split(b, q, " ") < 4) { print 99; exit } printf "%.2f", sqrt((p[1]-q[1])^2 + (p[2]-q[2])^2 + (p[3]-q[3])^2) }')
    m=$(awk -v b="$b" 'BEGIN { split(b, q, " "); print q[4] + 0 }')
    check $(awk -v d="$d" -v m="$m" -v n="$4" 'BEGIN { print (d + 0 <= 1 && m + 0 >= n) ? 1 : 0 }') "$2: the bolt starts at the middle crystal's tip ($d from the charge as it fired; $m from the muzzle)"
}
boltcheck "$log" "bolt (VR, spear)" "progs/dw_lance.mdl#vr" 2
check $(grep -qE "ammo_cells +94" <<<"$log" && echo 1 || echo 0) "shot: 6 cells taken from the pouch's 100 (no magazine)"
check $(grep -q "revgrip: hand 1 flips its grip: reversed" <<<"$log" && echo 1 || echo 0) "flip: B turned the Lance's grip over (head down)"
check $(grep -q "revgrip: hand 1 flips its grip: upright" <<<"$log" && echo 1 || echo 0) "flip: B again, upright"
check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "shot: the run ended on its last line"
# 4. dwellweap2: its cord (vr_cellcord.cpp Weapon::Lance; vr_cellcord_lance): from the butt of its pole into a cell in
#    the pouch while there are cells (QC VR_Reload_PlayerFrame: the pouch shows cells), loose and hanging under the
#    butt with none, plugging back in when cells come again.
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait10;god;notarget;vr_mock_hand main 0.2 1.3 -0.3 -22 0 0;vr_ad_test 207;wait60;echo STEP1;vr_cellcord_info;give c 0;wait150;echo STEP2;vr_cellcord_info;give c 50;wait3;echo STEP3;vr_cellcord_info;wait40;echo STEP4;vr_cellcord_info;vr_cellcord_lance 0;wait5;echo STEP5;vr_cellcord_info;echo dwlance-test: done" \
    -QuitOn "dwlance-test: done" -Timeout 300 -Filter "^cellcord|^cell cords|^STEP|rror|exit=" 2>&1 | tr -d '\r')
step() { echo "$log" | sed -n "/^STEP$1/,/^STEP/p"; }
cval() { echo "$1" | grep "^cellcord main: lance [a-z]* cell=" | sed -E "s/.* $2=([-0-9.]+).*/\\1/"; }
near() { awk -v a="$1" 'BEGIN { print (a != "" && a + 0 <= 0.05) ? 1 : 0 }'; }
s=$(step 1); echo "$s" | grep "^cellcord main" | cut -c1-160
check $(grep -q "^cellcord main: lance plugged cell=0" <<<"$s" && [ "$(near "$(cval "$s" gunend)")" = 1 ] && [ "$(near "$(cval "$s" plug)")" = 1 ] && echo 1 || echo 0) "cord: from the butt into cell 0 (gunend $(cval "$s" gunend), plug $(cval "$s" plug))"
check $(grep -q "^cell cords: .* the pouch shows [1-9][0-9]* cells" <<<"$s" && echo 1 || echo 0) "cord: the pouch shows cells while the Lance is held"
s=$(step 2); below=$(cval "$s" below)
check $(grep -q "^cellcord main: lance loose cell=-1" <<<"$s" && awk -v b="$below" 'BEGIN { exit !(b > 4) }' && echo 1 || echo 0) "cord: no cells, loose, hanging $below units under the butt"
s=$(step 3)
check $(grep -q "^cellcord main: lance plugging cell=0" <<<"$s" && echo 1 || echo 0) "cord: cells again, plugging in"
s=$(step 4)
check $(grep -q "^cellcord main: lance plugged cell=0" <<<"$s" && [ "$(near "$(cval "$s" plug)")" = 1 ] && echo 1 || echo 0) "cord: then plugged"
s=$(step 5)
check $(grep -q "^cellcord main: none" <<<"$s" && echo 1 || echo 0) "cord: Crystal Lance Cord off, none"
check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "cord: the run ended on its last line"
# 5. dwellweap2: the main hand slides too (vr_twohand.cpp poleShift, vr_pole_slide): the off hand closed on the pole,
#    the main hand's grip loose (analog 0.3 < vr_pole_slide_grip 0.6): the off hand moved towards the tip carries the
#    Lance with it, the main hand sliding back towards the butt (the tip drawn further ahead of it); its grip closed
#    again, the off hand slides and the main hand's place stays.
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait10;god;notarget;vr_mock_hand main 0.2 1.1 -0.3 -22 0 0;vr_ad_test 207;wait10;vr_debug_2h_grip 2;vr_mock_hand_to off held 0.75;wait5;vr_mock_hand_to off held 0.75;wait5;$OFF;echo dwlance-mark: took;vr_mock_fingers main 0 0.3;vr_mock_hand_to off held 0.95;wait30;echo dwlance-mark: slid;vr_mock_fingers main 0 1;vr_mock_hand_to off held 0.5;wait30;echo dwlance-mark: held;vr_status;echo dwlance-test: done" \
    -QuitOn "dwlance-test: done" -Timeout 300 -Filter "2h grip|dwlance-mark|the shot|rror|exit=" 2>&1 | tr -d '\r')
mainAt() { awk -v m="$1" '/dwlance-mark/ { if (index($0, m)) { print last; exit } } /the holding hand .* along it from the handle/ { for (i = 1; i <= NF; i++) if ($i == "hand" && $(i+2) == "along" && $(i+6) == "handle") last = $(i+1) }' <<<"$log"; }
tipAt() { awk -v m="$1" '/dwlance-mark/ { if (index($0, m)) { print last; exit } } /the tip drawn/ { for (i = 1; i <= NF; i++) if ($i == "drawn") last = $(i+1) }' <<<"$log"; }
m0=$(mainAt took); m1=$(mainAt slid); m2=$(mainAt held); t0=$(tipAt took); t1=$(tipAt slid)
echo "  the main hand along the pole from its handle: took $m0, slid $m1, held $m2; the tip ahead of it: $t0, then $t1"
check $(awk -v a="$m0" -v b="$m1" 'BEGIN { print (a != "" && b != "" && a + 0 > -0.5 && a + 0 < 0.5 && b + 0 < -3) ? 1 : 0 }') "main slide: loose, the main hand slid back along the pole (the off hand carrying the Lance)"
check $(awk -v a="$t0" -v b="$t1" -v m="$m1" 'BEGIN { print (a != "" && b != "" && b - a > -m - 2 && b - a < -m + 2) ? 1 : 0 }') "main slide: the Lance drawn moved with it (the tip further ahead by the slide)"
check $(awk -v a="$m1" -v b="$m2" 'BEGIN { print (a != "" && b != "" && b - a < 0.5 && a - b < 0.5) ? 1 : 0 }') "main slide: closed again, the main hand's place kept (the off hand slides)"
check $(grep -q "let go of the weapon held anywhere" <<<"$log" && echo 0 || echo 1) "main slide: never let go"
check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "main slide: the run ended on its last line"
# 6. dwellweap2: a thrust (QC dw_items.qc DW_LanceMelee): the Lance held level (lancespear) driven forward along its
#    pole at 4.7 m/s (vr_mock_play) into vrmelee-recording's grunt 0.95 m ahead: read as a stab, landing with the crystal
#    head (vr_dw_lance_head 0.55), its damage times vr_dw_lance_thrust (1.5).
MOT=C:/OHWorkspace/qvr-agents/$AGENT/scratch; mkdir -p "$MOT"
printf '0.000 main 0.20 1.10 0.10 -22 0 0\n0.300 main 0.20 1.10 0.10 -22 0 0\n0.450 main 0.20 1.10 -0.60 -22 0 0\n1.200 main 0.20 1.10 -0.60 -22 0 0\n2.000 main 0.20 1.10 0.10 -22 0 0\n' > "$MOT/dwlance_thrust.txt"
log=$(bash $KIT/run.sh $AGENT -Script "sv_random_seed 7;map vrmelee-recording;wait60;god;notarget;developer 1;vr_debug_axestick 1;vr_melee_push 0;vr_dummy_attacks 0;setpos 3.3 -31.2 24 0 90 0;wait30;vr_mock_hand main 0.20 1.10 0.10 -22 0 0;vr_ad_test 207;wait30;vr_mock_fingers main 0 1;vr_mock_play $MOT/dwlance_thrust.txt;wait200;echo dwlance-test: done" \
    -QuitOn "dwlance-test: done" -Timeout 300 -Filter "dwlance: a thrust|melee event|embedhold: Thunderbolt held|rror|exit=" 2>&1 | tr -d '\r')
echo "$log" | grep -E "dwlance: a thrust|melee event|embedhold: Thunderbolt held" | cut -c1-200 | head -3
check $(grep -q "melee event: gun, hand 1, jab" <<<"$log" && echo 1 || echo 0) "thrust: read as a jab (a stab along the pole)"
check $(grep -qE "dwlance: a thrust, the head \(" <<<"$log" && echo 1 || echo 0) "thrust: landed with the crystal head"
# (lance3: the thrust by its very tip stays in, shallow: vr_dw_lance_embed_length 5 units, a sword's depth shares of it.)
din=$(grep -m1 -oE "embedhold: Thunderbolt held by hand 1, [0-9.]+ units in" <<<"$log" | awk '{ print $(NF-2) }')
check $(awk -v d="$din" 'BEGIN { print (d != "" && d + 0 > 0 && d + 0 <= 5) ? 1 : 0 }') "thrust: the tip stab stayed in, shallow ($din units in)"
check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "thrust: the run ended on its last line"
# 7. lancecharge: vr_ad_test 224 (dw_test.qc DW_ChargeTest): let go at half charge (no shot, no cells), the charge time
#    (vr_dw_lance_charge 1, then 0.5), held charged without a shot, fired as let go (150 to a grunt, 6 cells), no charge
#    without its 6 cells.
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait10;god;notarget;vr_positional_damage 0;vr_mock_hand main 0.2 1.3 -0.3 -22 0 0;vr_ad_test 224" \
    -QuitOn "dwcharge: [0-9]+ passed" -Timeout 300 -Filter "dwcharge|rror|exit=" 2>&1 | tr -d '\r')
echo "$log" | grep -E "FAIL" | cut -c1-200 | head -6
expect_verdict "$log" dwcharge "the charged shot: let go short, the charge time, fired as let go, no cells"
check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "charge: the run ended on its verdict"
# 8. lancebolt: a charged shot in flat play (the spear as drawn, its sway and all) and in VR as the gun (vr_dw_lance_pole
#    0: its muzzle already on its middle crystal's tip): the bolt from that tip (boltcheck).
for mode in flat gun; do
    extra=""; pre="vr_mock_hand main 0.2 1.3 -0.3 -22 0 0;"; model="progs/dw_lance.mdl#vr"; away=2
    [ "$mode" = flat ] && extra="-flatscreenmode" && pre=""
    [ "$mode" = gun ] && pre="$pre""vr_dw_lance_pole 0;" && model="[^ ]*dw_rail[^ ]*" && away=0
    log=$(bash $KIT/run.sh $AGENT -ExtraArgs "$extra" -Script "map e1m1;wait10;god;notarget;developer 1;$pre""vr_debug_gunfx 1;vr_ad_test 207;wait60;+attack;wait90;-attack;wait10;echo dwlance-test: done" \
        -QuitOn "dwlance-test: done" -Timeout 300 -Filter "lancecharge: (bolt|the charge)|dwlance: charged|rror|exit=" 2>&1 | tr -d '\r')
    echo "$log" | grep -E "lancecharge: (bolt .* from the tip|the charge)" | cut -c1-160 | head -2
    boltcheck "$log" "bolt ($mode)" "$model" $away
    check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "bolt ($mode): the run ended on its last line"
done
# 9. lance3 (his VR notes 2026-11-23): its grip flipped, it shoots the way its crystal head points (vr_status: the shot
#    within 6 degrees of the line from the hand to the tip, as drawn); its beam strikes as a bolt (vr_debug_shots: its push
#    on what it meets, "shot push: kind 4"), its impact drawn on the wall behind; it shakes as it charges and more as it
#    fires (vr_debug_gunfx: the charge's prints' "shake most", the buzzes); without cells and fired on the pull
#    (vr_dw_lance_charge 0) the trigger held makes no empty click.
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait10;god;notarget;developer 1;vr_mock_hand main 0.2 1.3 -0.3 -22 0 0;vr_ad_test 207;wait60;vr_mock_button main secondary 1;wait10;vr_mock_button main secondary 0;wait60;echo dwlance-mark: flipped;vr_status;vr_debug_shots 1;vr_debug_gunfx 1;+attack;wait90;-attack;wait120;+attack;wait90;-attack;wait30;vr_debug_gunfx 0;give c 0;wait5;tune vr_dw_lance_charge 0;wait5;+attack;wait40;-attack;tune vr_dw_lance_charge 1;wait5;echo dwlance-test: done" \
    -QuitOn "dwlance-test: done" -Timeout 300 -Filter "dwlance|lancecharge: (charging|fired)|shot push: kind 4|empty click|revgrip:|the shot|rror|exit=" 2>&1 | tr -d '\r')
echo "$log" | grep -E "revgrip:|main  the shot|lancecharge: (charging|fired)|shot push" | cut -c1-170 | head -7
check $(grep -q "revgrip: hand 1 flips its grip: reversed" <<<"$log" && echo 1 || echo 0) "flipped: B turned the Lance over"
shot=$(sed -n '/dwlance-mark: flipped/,$p' <<<"$log" | grep -m1 "main  the shot" | awk '{ print $4 }')
# (Upright 3.4 deg; flipped, the grip's pivot moves the pole off the hand's point: 6.3 measured, 174.5 before lance3.)
check $(awk -v d="$shot" 'BEGIN { print (d != "" && d + 0 < 10) ? 1 : 0 }') "flipped: it shoots along its crystal head ($shot deg off the line from the hand to the tip)"
check $(grep -c "dwlance: the beam through" <<<"$log" | grep -qx 2 && echo 1 || echo 0) "flipped: two charged shots fired"
check $(grep -q "shot push: kind 4" <<<"$log" && echo 1 || echo 0) "beam: it pushes what it meets as a bolt (props, ragdolls)"
chs=$(grep -m1 -oE "lancecharge: fired ent 1 hand 1 .*shake most [0-9.]+" <<<"$log" | awk '{ print $NF }')
fis=$(grep -oE "lancecharge: charging ent 1 hand 1 .*shake most [0-9.]+, buzzes [0-9]+" <<<"$log" | sed -n 2p | tr -d ',' | awk '{ print $(NF-2), $NF }')
check $(awk -v c="$chs" -v f="$fis" 'BEGIN { split(f, q, " "); print (c + 0 >= 0.02 && q[1] + 0 >= 0.1 && q[2] + 0 >= 1) ? 1 : 0 }') "shake: charging $chs kicks, firing (shake, buzzes) $fis"
check $(grep -q "empty click: hand 1" <<<"$log" && echo 0 || echo 1) "empty: no click with the trigger held"
check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "lance3: the run ended on its last line"
verdict
exit $fail
