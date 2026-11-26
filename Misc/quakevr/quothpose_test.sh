#!/bin/bash
# quothpose_test.sh <agent> -- an armed enforcer's arms on the Centurion's plasma rifle: spawned on vrragtriggers (the
# reference, its pose pinned) and made from Quoth's kellbase1 eliminators at the map's spawn (their guns once measured
# before sv.active at the models' own size, three times too long: the left elbow flung out; vr_weapons.cpp
# modelTransform, ROUND26.md "quothpose"). Scenes agrunt_enf_centurion, quoth_kellbase_enforcer (frozen: their arms
# settled in the counted frames). Needs Quoth 2.2 in the test base. Prints the cases and a verdict, exits 1 on a failure.
AGENT=${1:-quest}
QT_NAME=quothpose
source "$(dirname "$0")/qvrtest.sh"
GUN="v_gruntgun.mdl#adv5"
# (the reference's left hand, left elbow, right hand, right elbow off their shoulders, model space: 2026-11-26)
PIN="12.5 -11.1 -11.0 6.0 -3.0 -6.1 11.2 4.4 -11.9 2.7 -1.5 -7.7"
S="vr_scenario agrunt_enf_centurion;wait150;vr_agholds_pose save $GUN;vr_agholds_pose pin $GUN 1 vrragtriggers $PIN"
S="$S;vr_scenario quoth_kellbase_enforcer;wait150;vr_agholds_pose check $GUN 1.5 kellbase1;vr_agholds_pose done"
out=$(bash $KIT/run.sh $AGENT -QuitOn "^agholdpose: [0-9]+ passed" -Timeout 240 -Filter "^agholdpose" \
    -Script "$S;wait3;toggleconsole;quit" 2>&1)
echo "$out" | grep -a "^agholdpose"
expect_verdict "$out" "agholdpose" "the Centurion's plasma rifle enforcer: vrragtriggers' pose as pinned, kellbase1's eliminators as it"
verdict; exit $fail
