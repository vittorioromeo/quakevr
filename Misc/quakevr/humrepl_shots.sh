#!/usr/bin/env bash
# humrepl_shots.sh [out.png]: the recolours by weapon (docs/vr-port/ARMED_MONSTERS.md, "Recolours by weapon") on each
# armed humanoid row: vr_ad_test 221 lines up five of a row before you (a shotgun as it is, a rocket launcher, the
# thunderbolt, AD's flamethrower, AD's poison sprayer), one screenshot a row (grunts, enforcers, knights, death knights),
# then the grunts again with vr_humlook 0 (in vrtesthall; the AD campaign for AD's guns). Composed into scratch/<out.png> (default humrepl_rows.png). Each row is given
# the frames its monsters' start and stand take (60) before its shot.
set -u
K=C:/OHWorkspace/qvr-kit
W=$(cd "$(dirname "$0")/../.." && pwd)
NAME=$(basename "$W")
OUT=${1:-humrepl_rows.png}
S="sv_random_seed 7;vr_campaign_native ad;wait30;map vrtesthall;wait60;god;notarget"
for k in 0 1 2 3; do
    S="$S;scratch1 $k;vr_ad_test 221;wait60;screenshot"
done
S="$S;vr_humlook 0;scratch1 0;vr_ad_test 221;wait60;screenshot;vr_humlook 1;echo humrepl_shots: done;toggleconsole;quit"
bash "$K/run.sh" "$NAME" -Clean -Script "$S" -Filter "humlineup|humrepl_shots|Host_Error|SV_Error" -QuitOn "humrepl_shots: done" -Out "$OUT" -Timeout 240
