#!/bin/bash
# flatseated_test.sh <agent> -- flat-screen play always stands (his note 2026-11-10: "Flatscreen should ignore the 'seated
# position' value for VR settings and always assume we are in standing position"; vr_seated.cpp seated::active). Two
# headless flat-screen runs in vrtesthall, Position: Standing (vr_bodycal_seated 0) and Seated (1), each printing
# vr_seated_status standing, crouched (+flatcrouch) and lying down (vr_prone_toggle): seated play off in both, and every
# number the same -- the head over the floor, the server's .vr_stand, lying down, the legs, the holsters and pouches on
# the body.
AGENT=${1:-slideback}
QT_NAME=flatseated
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch; mkdir -p "$S"

declare -A got
for SEAT in 0 1; do
    out=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -QuitOn "^FLATSEATED_DONE" -Timeout 120 -Filter "^seated:|^FLATSEATED" \
        -Script "vr_bodycal_seated $SEAT; vr_flat_holsters 0; map vrtesthall; wait60; god; notarget; impulse 9; wait5; impulse 3; wait30; echo FLATSEATED standing; vr_seated_status; +flatcrouch; wait60; echo FLATSEATED crouched; vr_seated_status; -flatcrouch; wait60; vr_prone_toggle; wait150; echo FLATSEATED prone; vr_seated_status; echo FLATSEATED_DONE; toggleconsole; quit" 2>&1)
    got[$SEAT]=$(echo "$out" | grep -a "^seated:\|^FLATSEATED")
    n=$(echo "$out" | grep -ac "^seated: off, standing")
    check $([ "$n" = 3 ] && echo 1 || echo 0) "vr_bodycal_seated $SEAT: flat-screen play is standing (seated play off in $n of 3 states)"
done
echo "${got[0]}" > "$S/flatseated_0.txt"; echo "${got[1]}" > "$S/flatseated_1.txt"
n=$(echo "${got[0]}" | grep -ac "^seated:")
check $([ "$n" -ge 12 ] && [ "${got[0]}" = "${got[1]}" ] && echo 1 || echo 0) "seated or standing set, flat play stands the same: eye height, crouch, prone, legs, holsters ($n status lines; diff the two in scratch/flatseated_*.txt)"
st=$(echo "${got[1]}" | grep -a "server stand" | sed -n 2p)
check $(grep -q "stand 0\.[0-8]" <<<"$st" && echo 1 || echo 0) "seated set: the crouch key lowers QC's .vr_stand (seated play kept it 1): ${st#seated: }"
st=$(echo "${got[1]}" | grep -a "server stand" | sed -n 3p)
check $(grep -q "prone 1" <<<"$st" && echo 1 || echo 0) "seated set: lying down lies down: ${st#seated: }"
verdict; exit $fail
