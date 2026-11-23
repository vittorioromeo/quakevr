#!/bin/bash
# dwell_weapons_test.sh <agent> -- Dwell's weapons as ours (DWELL.md P3; vr_ad_test 201-202, dwell/dw_test.qc):
# headless on e1m1, the Rotary Shotgun given to the main hand, the Crystal Lance to the off hand, a shotgun holding the
# retired Rifle's variant to holster 0, then (1) a save and a load (a scratch save qvr_dwweapons.sav in quakevr/), (2) a
# level change to e1m2; each time the two checked still Dwell's (their records' variants: the save, parm76), the
# shotgun plain; after the load (e1m1's room) each fired at grunts held on its line:
# the Rotary Shotgun's pellets of 2, the Crystal Lance's 150 through two (the mock's hands levelled:
# they point at the floor; positional damage off: exact numbers). Checks per case: the QC
# verdict, and the run ended on it. Needs Dwell v2.2 in the base dir (<base>/dwellv2p2): without it the pickups'
# models are missing but the weapons still fire (the variants are ours). Then vr_ad_test 204 (an armed grunt arming a
# Dwell weapon's record, the powerups' screen tints). Exits 1 on a failure.
AGENT=$1
source "$(dirname "$0")/qvrtest.sh"
for mode in load level; do
    if [ "$mode" = load ]; then
        move="save qvr_dwweapons;wait5;load qvr_dwweapons"; req=202
    else
        move="changelevel e1m2"; req=203
    fi
    log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait10;god;notarget;vr_positional_damage 0;vr_ad_test 201;wait5;$move;wait10;vr_mock_hand_turn main 70 0 0;vr_mock_hand_turn off 70 0 0;wait3;vr_ad_test $req" \
        -QuitOn "dwweapons: [0-9]+ passed" -Timeout 300 -Filter "dwweapons|rror|exit=" 2>&1 | tr -d '\r')
    echo "$log" | grep -E "dwweapons: (FAIL|given)|FAIL" | head -6
    expect_verdict "$log" dwweapons "$mode: Dwell's weapons kept and fired"
    check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "$mode: the run ended on the verdict"
done
# vr_ad_test 204: an armed grunt arming a Dwell weapon's record (named, the variant kept, Dwell's shot), the powerups' tints.
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait10;vr_ad_test 204" -QuitOn "dwgrunt: [0-9]+ passed" -Timeout 300 \
    -Filter "dwgrunt|rror|exit=" 2>&1 | tr -d '\r')
echo "$log" | grep -E "FAIL" | head -6
expect_verdict "$log" dwgrunt "an armed grunt keeps a Dwell weapon; the powerups' tints"
check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "grunt: the run ended on the verdict"
# vr_ad_test 213 (his notes 2026-11-21): Dwell's map pickups taken (they asserted and vanished), the toolgun's Dwell
# weapons and MG3 bloody shotguns laid as props (they floated as pickups), a bloody prop taken by the hand.
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait10;vr_ad_test 213" -QuitOn "dwpickup: [0-9]+ passed" -Timeout 300 \
    -Filter "dwpickup|Assertion|rror|exit=" 2>&1 | tr -d '\r')
echo "$log" | grep -E "FAIL|Assertion" | head -6
expect_verdict "$log" dwpickup "Dwell's pickups taken; the toolgun's Dwell and bloody weapons as props"
check $(grep -q "Assertion failure" <<<"$log" && echo 0 || echo 1) "pickups: no assertion failure"
check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "pickups: the run ended on the verdict"
# vr_ad_test 208 (his note 2026-11-23, dwrifle): Dwell's Rifle gone: the armed humanoids' random pool never yields it, a
# shotgun no Dwell weapon; the toolgun has no Rifle row (its Crystal Lance row still found).
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait10;vr_toolgun_select Rifle;vr_toolgun_select Crystal Lance;vr_toolgun_select clear;vr_ad_test 208" \
    -QuitOn "dwrifle: [0-9]+ passed" -Timeout 300 -Filter "dwrifle|vr_toolgun_select|rror|exit=" 2>&1 | tr -d '\r')
echo "$log" | grep -E "FAIL|vr_toolgun_select" | head -6
expect_verdict "$log" dwrifle "Dwell's Rifle in no random pool"
check $(grep -q 'no entry labelled "Rifle"' <<<"$log" && echo 1 || echo 0) "the toolgun has no Rifle row"
check $(grep -q 'no entry labelled "Crystal Lance"' <<<"$log" && echo 0 || echo 1) "the toolgun's Crystal Lance row still there"
check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "rifle: the run ended on the verdict"
verdict
exit $fail
