#!/bin/bash
# emptyreload_test.sh <agent> -- R (impulse 55) with no ammo in storage, for every weapon (ROUND24.md, "Flat fixes,
# round 2": "Assertion failure! 'VR_AmmoUtil_GetAmmoCountByAmmoId' at 34" reloading the nail pistol with no nails).
# Headless, fast mode, flat-screen (-flatscreenmode) and the VR mock. For each weapon id (0..19 and 29..31; the AD guns,
# 20..28, need AD's data): given in the main hand, every ammo set to 0, fired for 1.5 s (the clip runs dry where it
# has one), then R twice. Checks: no assertion or QC error in the whole run, and every weapon's line reached.
AGENT=${1:-blendmodels}
QT_NAME=emptyreload
source "$(dirname "$0")/qvrtest.sh"
WIDS="0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 29 30 31"
S="map vrtesthall; wait60; god; notarget; developer 1; setpos 0 300 25 0 90 0; wait5"
for w in $WIDS; do
    S="$S; vr_test_give_wid $w; impulse 149; wait10; give s 0; give n 0; give r 0; give c 0; wait2"
    S="$S; +attack; wait108; -attack; wait20; impulse 55; wait10; impulse 55; wait10; echo emptyreload_test: wid $w"
done
S="$S; echo emptyreload_test: done; toggleconsole; quit"
for mode in flat vr; do
    extra=()
    [ "$mode" = flat ] && extra=(-ExtraArgs "-flatscreenmode")
    log=$(bash $KIT/run.sh $AGENT "${extra[@]}" -QuitOn "^emptyreload_test: done" -Timeout 240 \
        -Filter "Assertion|^emptyreload_test|PROGS|Host_Error|runaway" -Script "$S" 2>&1)
    asserts=$(grep -a -c "Assertion\|Host_Error\|runaway" <<<"$log")
    reached=$(grep -a -c "^emptyreload_test: wid" <<<"$log")
    first=$(grep -a -m1 -B1 "Assertion" <<<"$log" | tr '\n' ' ' | cut -c1-200)
    check $([ "$asserts" = 0 ] && echo 1 || echo 0) "$mode: R with empty storage, every weapon: no assertion or QC error ($asserts${first:+: $first})"
    check $([ "$reached" = 23 ] && grep -aq "^emptyreload_test: done" <<<"$log" && echo 1 || echo 0) "$mode: all 23 weapons run to the end ($reached)"
done
verdict; exit $fail
