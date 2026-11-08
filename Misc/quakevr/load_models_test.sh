#!/bin/bash
# load_models_test.sh <agent> [base] -- every holdable weapon (ids 1..31, AD's melee kinds, AD's three upgrades: the
# ones installed) in each hand and each holster through a save and a load (vr_pickup_test 12, vr_load_models_test.qc;
# a scratch save qvr_loadmodels.sav in quakevr/), headless on e1m1 with vr_ad_upgrades 2. Checks:
#   1. the QC test's verdict (each slot's weapon and model the same after the load: 8 per weapon)
#   2. no hand's or holster's model reached SV_ModelIndex unprecached (the engine's "precached late" safety net, or
#      the Sys_Error it replaces: the Widowmaker's "progs/v_shot3.mdl#vr not precached" quickload crash, 2026-11-08)
#   3. no model that can't be loaded, no error
# [base]: run.sh -Base (qbase: id's data alone, no AD). Exits 1 on a failure.
AGENT=$1; BASE=$2
source "$(dirname "$0")/qvrtest.sh"
args=()
[ -n "$BASE" ] && args=(-Base "$BASE")
log=$(bash $KIT/run.sh $AGENT "${args[@]}" -Script "vr_ad_upgrades 2;map e1m1;wait30;god;notarget;vr_pickup_test 12" \
    -QuitOn "loadmodels: [0-9]+ passed" -Timeout 600 -Filter "loadmodels|not precached|can't be loaded|rror|exit=" 2>&1 | tr -d '\r')
echo "$log" | grep -E "loadmodels: [0-9]+ weapons" | cut -c1-200
echo "$log" | grep -E "loadmodels: FAIL" | head -5
expect_verdict "$log" loadmodels "every weapon in each slot, saved and loaded"
check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "the run ended on the verdict (not a timeout or a crash)"
check $(grep -q "not precached" <<<"$log" && echo 0 || echo 1) "no model unprecached at SV_ModelIndex"
check $(grep -qE "can't be loaded|rror" <<<"$log" && echo 0 || echo 1) "no unloadable model, no error"
verdict
exit $fail
