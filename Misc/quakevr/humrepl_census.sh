#!/usr/bin/env bash
# humrepl_census.sh: the maps placing AD's and Quoth's humanoid variants (23 of AD's, 10 of Quoth's and its sample
# packages: scratch/humrepl_census.py on their BSPs) loaded with vr_agrunt_repl_variants 0 and then 1; each map's census
# (vr_ad_test 220: its monster count, armed ones, variants replaced, variants left AD's) a line. humrepl_census.py
# compares the two runs: the monster counts must match (the maps' flow: every variant still counted once), none left
# with the setting on (each census once its player is in: vr_ad_test 220 waits for him; 60 frames a map). Needs AD in the base dir and Quoth installed by quoth_setup.py (as quoth_maps.sh does).
set -u
K=C:/OHWorkspace/qvr-kit
W=$(cd "$(dirname "$0")/../.." && pwd)
NAME=$(basename "$W")
OUT=$W/scratch/humrepl_census
mkdir -p "$OUT"
BASE=$K/bases/$NAME/qbase${QVR_LANE:+.lane$QVR_LANE}
python -I "$W/Misc/quakevr/quoth_setup.py" "$BASE" > /dev/null
AD="ad_ac ad_akalakha ad_azad ad_crucial ad_dm1 ad_dm5 ad_e1m1 ad_e2m2 ad_e2m7 ad_grendel ad_lavatomb ad_metmon ad_mountain ad_necrokeep ad_s1m1 ad_scastle ad_sepulcher ad_swampy ad_tears ad_test1 ad_test3 ad_test6 ad_tfuma"
PKG="apsp2 dis_sp6 e1m5quoth mhsp01 quarantine metmon"
OWN="e1m1quoth e1m2quoth kellbase1 ne_basetest"
for on in 0 1; do
    S="sv_random_seed 7;vr_agrunt_repl_variants $on;map start;wait60" # (a first map: the start-up's own lines aside)
    for m in $PKG; do S="$S;map $m;vr_ad_test 220;wait60"; done
    S="$S;vr_campaign_native ad;wait30"
    for m in $AD; do S="$S;map $m;vr_ad_test 220;wait60"; done
    S="$S;vr_campaign_native quoth;wait30"
    for m in $OWN; do S="$S;map $m;vr_ad_test 220;wait60"; done
    S="$S;vr_agrunt_repl_variants 1;echo humrepl_census: run done;toggleconsole;quit"
    bash "$K/run.sh" "$NAME" -Script "$S" -Filter "humcensus:|humrepl_census|Host_Error|SV_Error|Couldn't spawn" -QuitOn "humrepl_census: run done" -Timeout 900 > "$OUT/run$on.txt" 2>&1
done
python -I "$W/Misc/quakevr/humrepl_census.py" "$OUT/run0.txt" "$OUT/run1.txt"
