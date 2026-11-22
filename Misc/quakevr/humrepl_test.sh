#!/usr/bin/env bash
# humrepl_test.sh: AD's and Quoth's humanoid variants as armed humanoids and their recolours (docs/vr-port/
# ARMED_MONSTERS.md, "AD's and Quoth's variants"): vr_ad_test 198 (AD's replacements: its rocket soldier now armed),
# then vr_ad_test 219 on e1m1 in the AD campaign (needs AD in the base dir). Ends on 219's verdict line
# ("humrepl: N passed, M failed"; it waits for the client's looks itself, a 3 s give-up).
set -u
K=C:/OHWorkspace/qvr-kit
W=$(cd "$(dirname "$0")/../.." && pwd)
NAME=$(basename "$W")
S="sv_random_seed 7;vr_campaign_native ad;wait30;map e1m1;wait60;god;notarget;vr_ad_test 198;wait2;vr_ad_test 219;wait900;toggleconsole;quit"
bash "$K/run.sh" "$NAME" -Script "$S" -Filter "adrepl|humrepl|humlook|  look|  entity|Host_Error|SV_Error" -QuitOn "humrepl: [0-9]+ passed" -Timeout 240
