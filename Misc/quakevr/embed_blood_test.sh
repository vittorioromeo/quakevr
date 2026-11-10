#!/bin/bash
# embed_blood_test.sh <agent> -- a blade left in the training dummy or a grunt is bloodied where it went in and keeps that
# blood when pulled out (his note 2026-11-10; vr_wounds.cpp: OwedBlood, payOwedBlood, wearWeaponMask). `vr_embed_test 28`
# cases 7-10 (an axe swung, a sword's stab; the dummy, then a grunt) print vr_gore_hands_info's gear lines stuck and back
# in the hand, and vr_wounds_debug's "on the part that went in"; embed_blood_check.py judges them. Ends
# `embedblood: N passed, M failed` (after the QC's own `embed1108:` verdict).
AGENT=${1:?worktree name}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
OUT=$(bash $KIT/run.sh $AGENT -Timeout 900 -Filter "embedblood|gear #|blow blood|embed1108: [0-9]+ passed|FAIL" \
    -QuitOn "^embed1108: [0-9]+ passed, [0-9]+ failed" -Script "map vrtesthall;wait5;god;notarget;vr_embed_test 28" 2>&1)
echo "$OUT" | grep "embedblood\|gear #\|blow blood\|embed1108\|FAIL"
echo "$OUT" | python "$(dirname "$0")/embed_blood_check.py"
