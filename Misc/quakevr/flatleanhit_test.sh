#!/bin/bash
# flatleanhit_test.sh <agent> -- flatscreen play's lean (Q / E, +flatleanleft / +flatleanright) is the game moving his head
# and hands, never a blow of his: crouched, looking ahead and down, and standing, leant left and right again and again,
# no melee event at all (no headbutt of the floor or a wall, no hand's hit). His note 2026-11-13: a crouched lean looking
# down sounded the fist's thud and his own pain cry -- the head's drop at 2.7 m/s read as a headbutt of the floor
# (client.qc PlayerVRMeleeHeadbutt; this very script gave four "headbutt a wall" events before the gate). Headless.
AGENT=${1:-takeslight}
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch; mkdir -p "$S"
L="+flatleanleft; wait30; -flatleanleft; wait30; +flatleanright; wait30; -flatleanright; wait30"
SCRIPT="map e1m1; wait60; developer 1; god; notarget; +flatcrouch; wait40; $L; cl_pitchspeed 600; +lookdown; wait30; -lookdown; $L; $L; centerview; -flatcrouch; wait30; $L; vr_flatplay_status; echo flatleanhit: leans done; toggleconsole; quit"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$SCRIPT" -Filter "melee event|flatleanhit:|^flatscreen play:" \
    -QuitOn "flatleanhit: leans done" -Timeout 240 > "$S/flatleanhit.log" 2>&1
check $(grep -qa "flatleanhit: leans done" "$S/flatleanhit.log" && grep -qa "^flatscreen play:" "$S/flatleanhit.log" && echo 1 || echo 0) \
    "the run is flatscreen play and every lean was played"
n=$(grep -ac "melee event" "$S/flatleanhit.log")
check $([ "$n" = 0 ] && echo 1 || echo 0) \
    "crouched and standing, looking ahead and down, 16 leans: no melee event ($n: $(grep -a 'melee event' "$S/flatleanhit.log" | head -2 | tr '\n' ';'))"
verdict; exit $fail
