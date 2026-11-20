#!/bin/bash
# rules_numbers_test.sh <agent>: the rulesets' numbers table (QC/vr_rules_numbers.qc, DWELL.md S6) through our own
# touches (vr_ad_test 209): Quake's numbers in e1m1, Copper's in Dwell's d1m1 (needs Dwell v2.2 in the test base, as
# dwell_maps.sh). Each run ends on the test's verdict line (`rulesnum: N passed, M failed`, after its 7.5 s rot check).
AGENT=$1
source "$(dirname "$0")/qvrtest.sh"
for where in id copper; do
    if [ "$where" = id ]; then
        pre="map e1m1;wait10"
    else
        pre="vr_campaign_native dwell;wait30;map d1m1;wait30"
    fi
    log=$(bash $KIT/run.sh $AGENT -ExtraArgs "-nomapindex -noaddons" -Script "developer 0;$pre;god;notarget;vr_ad_test 209" \
        -QuitOn "rulesnum: [0-9]+ passed" -Timeout 300 -Filter "rulesnum|rror|exit=" 2>&1 | tr -d '\r')
    echo "$log" | grep -E "rulesnum: (ruleset|FAIL)|FAIL" | head -12
    expect_verdict "$log" rulesnum "$where: the ruleset's numbers"
    check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "$where: the run ended on the verdict"
done
verdict
exit $fail
