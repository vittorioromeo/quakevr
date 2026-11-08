#!/bin/bash
# plasma_ball_test.sh <agent> -- AD's Plasma Gun carried into a new server, then its first ball (vr_ad_test 104-105,
# vr_ad_upgrades_test.qc): headless on e1m1 (outside AD's rules: nothing of AD's map setup precaches its ball or its
# hit's sprites), the gun given to the main hand, then (1) a save and a load (a scratch save qvr_plasmaball.sav in
# quakevr/), (2) a level change to e1m2; each time a ball down onto the floor ahead. Checks per case:
#   1. the QC test's verdict (the gun still held as the Plasma Gun, the ball hit)
#   2. no "late precache:" line after "adplasmahit: fired" (developer 1): its assets were precached when the new server
#      first drew the gun (AD_EnsureVRModel's AD_WeaponsOnDemand), not at its first shot or hit (his hitch, 2026-11-08)
# Needs AD's data (Additional Content); without it the verdicts are missing and it fails. Exits 1 on a failure.
AGENT=$1
source "$(dirname "$0")/qvrtest.sh"
for mode in load level; do
    if [ "$mode" = load ]; then
        move="save qvr_plasmaball;wait5;load qvr_plasmaball"
    else
        move="changelevel e1m2"
    fi
    log=$(bash $KIT/run.sh $AGENT -Script "vr_ad_upgrades 2;map e1m1;wait10;god;notarget;developer 1;vr_ad_test 104;wait5;$move;wait10;vr_ad_test 105" \
        -QuitOn "adplasmahit: [0-9]+ passed" -Timeout 300 -Filter "adplasmahit|late precache|rror|exit=" 2>&1 | tr -d '\r')
    echo "$log" | grep -E "adplasmahit: (FAIL|fired)" | head -3
    expect_verdict "$log" adplasmahit "$mode: the Plasma Gun carried, its ball hit"
    check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "$mode: the run ended on the verdict"
    late=$(awk '/adplasmahit: fired/ {on = 1} on && /late precache:/' <<<"$log")
    [ -n "$late" ] && echo "$late" | head -5
    check $([ -z "$late" ] && grep -q "adplasmahit: fired" <<<"$log" && echo 1 || echo 0) "$mode: nothing loaded at the first ball or its hit"
done
verdict
exit $fail
