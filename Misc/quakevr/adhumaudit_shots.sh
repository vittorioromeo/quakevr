#!/usr/bin/env bash
# adhumaudit_shots.sh [first] [last] [out.png]: the audit of AD's remaining humanoids (docs/vr-port/ARMED_MONSTERS.md,
# "Audit 2"): vr_ad_test 222 sets one of AD's humanoid models beside the armed row's model nearest its build (ours, the
# two overlaid with AD's translucent, AD's, the overlay side-on), pairs first..last (default 0..11), a screenshot each, in
# vrtesthall with AD's data (vr_campaign_native ad). Composed into scratch/<out.png> (default adhumaudit.png).
set -u
K=C:/OHWorkspace/qvr-kit
W=$(cd "$(dirname "$0")/../.." && pwd)
NAME=$(basename "$W")
FIRST=${1:-0}
LAST=${2:-11}
OUT=${3:-adhumaudit.png}
S="sv_random_seed 7;vr_campaign_native ad;wait30;map vrtesthall;wait60;god;notarget"
for k in $(seq "$FIRST" "$LAST"); do
    S="$S;scratch1 $k;vr_ad_test 222;wait3;screenshot"
done
S="$S;echo adhumaudit_shots: done;toggleconsole;quit"
bash "$K/run.sh" "$NAME" -Clean -Script "$S" -Filter "humoverlay|adhumaudit_shots|Host_Error|SV_Error|PR_|precache" -QuitOn "adhumaudit_shots: done" -Out "$OUT" -Timeout 240
