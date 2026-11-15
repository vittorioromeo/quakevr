#!/bin/bash
# flattp_test.sh <agent> [shots] -- flatscreen play's over-the-shoulder third-person camera (vr_flat_tp; docs/vr-port/FLATPLAY.md,
# "The third-person camera"; Quake/vr/vr_flattp.cpp), headless, at e1m1's start with a gun in hand: vr_flat_tp_test checks
# the option off leaves the view as it was; on, in the open the camera is at its offsets; backed into a wall it is pulled in
# over the shoulder; a walk turning and strafing along the walls never has the camera in a wall or behind one; the gun's shot
# lands on the camera's crosshair point; aiming down the sights goes into first person and back (vr_flat_tp_aim 0) or stays
# over the shoulder drawn in with the gun still on the point (vr_flat_tp_aim 1). `shots`: a screenshot strip of the open
# view, the wall, and both aims (scratch/flattp_strip.png). Prints the checks and a verdict, exits 1 on a failure.
AGENT=${1:-thirdperson}
SHOTS=${2:-}
QT_NAME=flattp_sh
source "$(dirname "$0")/qvrtest.sh"
out=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Clean -Out flattp_strip.png -QuitOn "^flattp: [0-9]+ passed" -Timeout 240 \
    -Filter "^flattp: [^s]" \
    -Script "map e1m1; god; notarget; r_norefresh 0; setpos 480 -352 88; impulse 154; wait60; vr_flat_tp_test $SHOTS; wait9000; toggleconsole; quit" 2>&1)
echo "$out" | grep -a "^flattp: \(FAIL\|PASS\)"
expect_verdict "$out" "flattp" "third-person camera: off as before, at its offsets, pulled in at walls, never in one, shots on the crosshair, both sight modes"
verdict; exit $fail
