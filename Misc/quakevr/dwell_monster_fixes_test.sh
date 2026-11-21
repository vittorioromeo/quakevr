#!/bin/bash
# dwell_monster_fixes_test.sh <agent> -- his notes 2026-11-21 on Dwell's monsters (ROUND25.md "expbugs"), headless:
# (1) the Kamikaze (vrfiringrange, vr_ad_test 215): it chases and bursts near the player, its explosion drawn (it burst
# at once: EntitiesTouching on monsters' relative boxes; no TE_EXPLOSION); (2) the Brute's rig: no triangle across bones
# not joined (vr_ragdoll_rig_spans: the cannon's piece split between his hands; quakevr/rigs/mon_brute.rig.txt);
# (3) the Sculptor (d2end, vr_ad_test 216): a player inside its fighting box put just outside it, free. Needs Dwell v2.2
# in the base dir (<base>/dwellv2p2). Each case ends on its verdict line. Exits 1 on a failure.
AGENT=$1
source "$(dirname "$0")/qvrtest.sh"
log=$(bash $KIT/run.sh $AGENT -Script "map vrfiringrange;wait10;god;vr_ad_test 215" -QuitOn "dwkami: [0-9]+ passed" \
    -Timeout 300 -Filter "dwkami|rror|exit=" 2>&1 | tr -d '\r')
echo "$log" | grep -E "dwkami: (FAIL|PASS it)" | head -6
expect_verdict "$log" dwkami "the Kamikaze chases, bursts near the player, its explosion drawn"
check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "kamikaze: the run ended on the verdict"
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait5;vr_ragdoll_rig_spans progs/e2/mon_brute.mdl;echo rigspans: done" \
    -QuitOn "rigspans: done" -Timeout 200 -Filter "rig_spans|rigdata|exit=" 2>&1 | tr -d '\r')
echo "$log" | grep -E "rig_spans: (triangle|progs)" | head -4
check $(grep -q "mon_brute.mdl: 0 spanning triangles" <<<"$log" && echo 1 || echo 0) "the Brute's rig: no triangle across unjoined bones"
log=$(bash $KIT/run.sh $AGENT -ExtraArgs "-nomapindex -noaddons" -Script "vr_campaign_native dwell;wait30;map d2end;wait10;god;notarget;vr_ad_test 216" \
    -QuitOn "dwstuck: [0-9]+ passed" -Timeout 300 -Filter "dwstuck|rror|exit=" 2>&1 | tr -d '\r')
echo "$log" | grep -E "dwstuck: FAIL" | head -6
expect_verdict "$log" dwstuck "a player inside the Sculptor's box put out of it, free"
check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "sculptor: the run ended on the verdict"
verdict
exit $fail
