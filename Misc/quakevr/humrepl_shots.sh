#!/usr/bin/env bash
# humrepl_shots.sh [out.png] [overlay]: the recolours by weapon (docs/vr-port/ARMED_MONSTERS.md, "Recolours by weapon") on
# each armed humanoid row: vr_ad_test 221 lines up five of a row before you (a shotgun as it is, a rocket launcher, the
# thunderbolt, AD's flamethrower, AD's poison sprayer), two screenshots a row (grunts, enforcers, knights, death knights):
# before (vr_humlook 0) and after (in vrtesthall; the AD campaign for AD's guns). Composed into scratch/<out.png>
# (default humrepl_rows.png). A second argument "overlay" shows the masks instead of "before" (vr_humstudio_overlay: the
# clothes green, the pack blue, the rest dark red). Each row is given the frames its monsters' start and stand take (60)
# before its shots, the look's change a frame (wait2).
set -u
K=C:/OHWorkspace/qvr-kit
W=$(cd "$(dirname "$0")/../.." && pwd)
NAME=$(basename "$W")
OUT=${1:-humrepl_rows.png}
BEFORE="vr_humlook 0"
AFTER="vr_humlook 1"
if [ "${2:-}" = "overlay" ]; then
    BEFORE="vr_humstudio_overlay 1"
    AFTER="vr_humstudio_overlay 0"
fi
S="sv_random_seed 7;vr_campaign_native ad;wait30;map vrtesthall;wait60;god;notarget"
for k in 0 1 2 3; do
    S="$S;scratch1 $k;vr_ad_test 221;wait60;$BEFORE;wait2;screenshot;$AFTER;wait2;screenshot"
done
S="$S;echo humrepl_shots: done;toggleconsole;quit"
bash "$K/run.sh" "$NAME" -Clean -Script "$S" -Filter "humlineup: row|humrepl_shots|Host_Error|SV_Error" -QuitOn "humrepl_shots: done" -Out "$OUT" -Timeout 240
