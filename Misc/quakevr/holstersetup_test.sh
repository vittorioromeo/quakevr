#!/bin/bash
# holstersetup_test.sh <agent> [shot] -- ROUND22.md, "Holster Setup: drag and drop". Holster Setup (vr_holstersetup) with the
# mock hands: entered (the game paused), the right hip holster's box gripped (vr_mock_hand_to main zonebox 3) and dragged
# 6 units down along it, Done: both hips 6 units lower (Symmetry), vr_hip_offset_z set, left after Done. A gun drawn from
# it then put back at its new place: IN; at its old place (6 up): DROP; without the drag, at the new place: DROP. Symmetry
# off: the drag moves only that holster (its trim); two hands on it spread from 3 to 6 units apart: its size 2, its pair's
# 1; Symmetry on, both hands on the right chest holster: both chest holsters' sizes; a held holster's stick up: larger;
# Cancel: every setting back. The panel: the hand at Undo and at Done, the trigger: undone, left. "shot": the mode from
# the eyes looking down and from a camera in front (scratch/holstersetup.png). Prints PASS/FAIL; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
P="developer 1;map e1m1;wait60;god;notarget;vr_reload_mode 3;vr_holster_mode 0;vr_handgrenade 1;wait10;"
GRAB="vr_mock_hand_to main zonebox 3;wait3;vr_mock_hand_to main zonebox 3;wait3;vr_mock_button main grip 1;wait5;"
DRAG="vr_holstersetup;wait5;${GRAB}vr_mock_hand_to main zonebox 3 8 0 0;wait20;vr_mock_button main grip 0;wait5;"
draw="vr_mock_hand_to main zone 3;wait5;vr_mock_hand_to main zone 3;wait5;+grabright;vr_mock_button main grip 1;wait10;vr_mock_hand main 0.3 1.0 -0.2 -40 0 0;wait3;"
fail=0
check() { # $1 PASS condition (0/1), $2 text
    if [ "$1" = 1 ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi
}
z() { # $1 output, $2 zone, $3 which report (1 first): the zone's up from the eyes
    echo "$1" | grep "^setup $2 at" | sed -n "${3}p" | awk '{print $6}'
}
if [ "$2" = "shot" ]; then
    S="${P}vr_holstersetup;wait5;vr_mock_hand_to main zonebox 3;wait3;vr_mock_hand_to main zonebox 3;wait3;vr_mock_hand_to off zonebox 2;wait3;vr_mock_hand_to off zonebox 2;wait5;wait20;screenshot;vr_mock_look 65 0;wait20;screenshot;vr_mock_look 0 0;vr_mock_camera 0 1.3 -1.1 15 180;wait20;screenshot;vr_mock_camera;vr_mock_look 0 0;"
    bash $KIT/run.sh $AGENT -Clean -Out holstersetup.png -Script "${S}toggleconsole;quit" -Filter "composed|ERROR"
    exit 0
fi

# 1. Enter, drag, Done.
OUT=$(bash $KIT/run.sh $AGENT -Script "${P}vr_holstersetup;wait5;vr_holstersetup_report;${GRAB}vr_mock_hand_to main zonebox 3 0 0 -6;wait20;vr_mock_button main grip 0;wait5;vr_holstersetup_report;vr_holstersetup_done;wait5;vr_holstersetup_report;vr_hip_offset_z;toggleconsole;quit" -Filter "^setup|vr_hip_offset_z")
A=$(echo "$OUT" | grep "^setup active" | tr '\n' '|')
R0=$(z "$OUT" rhip 1); R1=$(z "$OUT" rhip 2); L1=$(z "$OUT" lhip 2)
OK=$(awk -v a=$R0 -v b=$R1 -v c=$L1 'BEGIN{d=b-a; print (d<-5.5 && d>-6.5 && (c-b)<0.05 && (b-c)<0.05) ? 1 : 0}')
echo "$A" | grep -q "active 1 paused 1.*active 1 paused 1.*active 0 paused 0" && E=1 || E=0
check $E "entered (paused), left with Done"
check $OK "dragged 6 down: right hip $R0 -> $R1, left hip $L1 (mirrored)"
echo "$OUT" | grep -q '"vr_hip_offset_z" is "0"' && check 0 "vr_hip_offset_z kept" || check 1 "Done kept it: $(echo "$OUT" | grep vr_hip_offset_z)"

# 2. Dragged 8 units out (sideways at the hip): a gun at the new place holsters, at the old place not; without the drag, the new place doesn't.
for c in "drag:0:IN" "drag:-8:DROP" "none:8:DROP"; do
    IFS=: read how dz want <<< "$c"
    if [ $how = drag ]; then D="${DRAG}vr_holstersetup_done;wait10;"; else D=""; fi
    S="${P}${D}${draw}vr_mock_hand_to main zonegrip 3 $dz 0 0;wait3;vr_mock_hand_to main zonegrip 3 $dz 0 0;wait3;-grabright;vr_mock_button main grip 0;wait30;vr_holster_zones_report;"
    OUT=$(bash $KIT/run.sh $AGENT -Script "${S}toggleconsole;quit" -Filter "^zone rhip")
    if echo "$OUT" | grep "^zone rhip" | tail -1 | grep -q "a gun in it"; then got=IN; else got=DROP; fi
    [ "$got" = "$want" ] && r=1 || r=0
    check $r "gun at the right hip ($how) + $dz out: $got (want $want)"
done

# 3. Symmetry off, pinch, Symmetry on pinch, stick, Cancel.
PINCH="vr_mock_hand_to off zonebox ZZ 0 -1.5 0;vr_mock_hand_to main zonebox ZZ 0 1.5 0;wait3;vr_mock_hand_to off zonebox ZZ 0 -1.5 0;vr_mock_hand_to main zonebox ZZ 0 1.5 0;wait3;vr_mock_button off grip 1;wait3;vr_mock_button main grip 1;wait3;vr_mock_hand_to off zonebox ZZ 0 -3 0;vr_mock_hand_to main zonebox ZZ 0 3 0;wait10;vr_mock_button off grip 0;vr_mock_button main grip 0;wait5;"
S="${P}vr_holstersetup;wait5;vr_holstersetup_symmetry 0;${GRAB}vr_mock_hand_to main zonebox 3 0 0 5;wait20;vr_mock_button main grip 0;wait5;vr_holstersetup_report;vr_holster_trim_rhip_z;${PINCH//ZZ/3}vr_holstersetup_report;vr_holstersetup_symmetry 1;${PINCH//ZZ/5}vr_holstersetup_report;vr_mock_hand_to main zonebox 5;wait3;vr_mock_hand_to main zonebox 5;wait3;vr_mock_button main grip 1;wait3;vr_mock_stick main 0 1;wait30;vr_mock_stick main 0 0;vr_mock_button main grip 0;wait3;vr_holstersetup_report;vr_holstersetup_cancel;wait5;vr_holstersetup_report;vr_holster_trim_rhip_z;vr_hip_offset_z;vr_holster_size_rhip;vr_holster_size_rupper;"
OUT=$(bash $KIT/run.sh $AGENT -Script "${S}toggleconsole;quit" -Filter "^setup|vr_holster_trim_rhip_z|vr_hip_offset_z|vr_holster_size_r")
R1=$(z "$OUT" rhip 1); L1=$(z "$OUT" lhip 1)
OK=$(awk -v r=$R1 -v l=$L1 'BEGIN{print (r-l>4.5 && r-l<5.5) ? 1 : 0}')
check $OK "Symmetry off: the right hip alone 5 up (right $R1, left $L1); $(echo "$OUT" | grep vr_holster_trim_rhip_z | head -1)"
SZ=$(echo "$OUT" | grep "^setup [rl]hip at" | sed -n '3,4p' | awk '{print $8}' | tr '\n' ' ')
[ "$SZ" = "1.00 2.00 " ] && r=1 || r=0
check $r "pinch, Symmetry off: left, right hip sizes $SZ(want 1.00 2.00)"
SZ=$(echo "$OUT" | grep "^setup [rl]upper at" | sed -n '5,6p' | awk '{print $8}' | tr '\n' ' ')
[ "$SZ" = "2.00 2.00 " ] && r=1 || r=0
check $r "pinch, Symmetry on: both chest sizes $SZ(want 2.00 2.00)"
SZ=$(echo "$OUT" | grep "^setup rupper at" | sed -n '4p' | awk '{print $8}')
OK=$(awk -v s=$SZ 'BEGIN{print (s>2.05) ? 1 : 0}')
check $OK "stick up while held: right chest size 2.00 -> $SZ"
echo "$OUT" | grep -q '"vr_holster_trim_rhip_z" is "0"' && echo "$OUT" | grep -q '"vr_hip_offset_z" is "0"' &&
    echo "$OUT" | grep -q '"vr_holster_size_rhip" is "1"' && echo "$OUT" | grep -q '"vr_holster_size_rupper" is "1"' &&
    echo "$OUT" | grep "^setup active" | tail -1 | grep -q "active 0" && r=1 || r=0
check $r "Cancel: every setting back, left"

# 4. The panel: Undo, then Done, by the hand at the button and the trigger.
S="${P}${DRAG}vr_mock_hand_to off setupbutton 1;wait3;vr_mock_hand_to off setupbutton 1;wait3;vr_mock_button off trigger 1;wait3;vr_mock_button off trigger 0;wait3;vr_hip_offset_y;vr_mock_hand_to off setupbutton 5;wait3;vr_mock_hand_to off setupbutton 5;wait3;vr_mock_button off trigger 1;wait3;vr_mock_button off trigger 0;wait5;vr_holstersetup_report;"
OUT=$(bash $KIT/run.sh $AGENT -Script "${S}toggleconsole;quit" -Filter "^setup active|vr_hip_offset_y|Holster Setup:")
echo "$OUT" | grep '"vr_hip_offset_y"' | grep -q "(default)" && echo "$OUT" | grep -q "Holster Setup: done" &&
    echo "$OUT" | grep "^setup active" | tail -1 | grep -q "active 0" && r=1 || r=0
check $r "panel: Undo put the hip back, Done left"

# 5. Seated: the drag moves the seated layout's hips (vr_hip_holster_seated_*), the standing ones kept.
S="${P}vr_bodycal_seated 1;wait10;${DRAG}vr_holstersetup_done;wait5;vr_hip_offset_y;vr_hip_holster_seated_y;vr_bodycal_seated 0;"
OUT=$(bash $KIT/run.sh $AGENT -Script "${S}toggleconsole;quit" -Filter "vr_hip_offset_y|vr_hip_holster_seated_y")
echo "$OUT" | grep '"vr_hip_offset_y"' | grep -q "(default)" && echo "$OUT" | grep '"vr_hip_holster_seated_y"' | grep -qv "(default)" && r=1 || r=0
check $r "seated: the seated hips moved, the standing kept ($(echo "$OUT" | tr '\n' ' '))"
exit $fail
