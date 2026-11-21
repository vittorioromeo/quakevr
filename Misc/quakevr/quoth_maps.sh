#!/bin/bash
# quoth_maps.sh [frames]: Quoth's acceptance (docs/vr-port/QUOTH.md). The 14 sample Quoth map packages (15 maps) from
# the Map Library's path (quoth_setup.py installs them and Quoth 2.2 into the test base: no download), played in the
# stock game with Quoth's content mounted; then Quoth's campaign (vr_campaign_native quoth) and its own 12 maps. Each
# map loaded (skill 1), saved 15 frames after `map`, its ruleset printed, `frames` game frames run (default 600); then
# vr_ad_test 211 (quothkeys), Quoth's start exit, three saves loaded back, and a verdict line. quoth_maps.py reads the log: missing spawn functions
# and unknown function keys per map, errors, ruleset (AD's on every map), monsters live and waiting.
# Needs Arcane Dimensions 1.80 p1 in the base dir (the kit's agent folder has it) and the kit (run.sh).
set -u
FR=${1:-600}
K=C:/OHWorkspace/qvr-kit
W=$(cd "$(dirname "$0")/../.." && pwd)
NAME=$(basename "$W")
OUT=$W/scratch/quoth_maps
mkdir -p "$OUT"
BASE=$K/bases/$NAME/qbase${QVR_LANE:+.lane$QVR_LANE}
python -I "$W/Misc/quakevr/quoth_setup.py" "$BASE"
# (metmon1d's map, metmon, is inside its zip's pak0.pak: quoth_setup.py unpacks it, as the Map Library's install does)
PKG="apsp2 dis_sp6 e1m5quoth gatetohell ion02 kellmet1 lpqsp1 metmon mfxsp10 mhsp01 ne_marb ne_marb2 quarantine sksp2 tris"
OWN="start e1m1quoth e1m2quoth ne_basetest kelltest1 kelltest2 kelltest3 kelltest4 kelltest5 kellbase1 breakable"
S="developer 0;sv_random_seed 7;skill 1;coop 0;maxplayers 1"
for m in $PKG; do
    S="$S;echo quoth: map $m;map $m;wait15;save quc_$m;vr_ruleset_info;wait$FR;echo quoth: ran $m"
done
# (the campaign switch loads its start; the loop then loads it as the others)
S="$S;vr_campaign_native quoth;wait30"
for m in $OWN; do
    S="$S;echo quoth: map $m;map $m;wait15;save quc_$m;vr_ruleset_info;wait$FR;echo quoth: ran $m"
done
# vr_ad_test 211 on kellbase1 (its exit lock is a spawner): targetname2 and the spawner (120 frames: its monsters
# come 0.3 s apart, the check 0.5 s after the last); then start's first exit.
S="$S;echo quoth: map kellbase1;map kellbase1;wait15;vr_ad_test 211;wait120;echo quoth: ran kellbase1"
S="$S;echo quoth: chain start;map start;wait30;vr_goto_exit;wait90;vr_ruleset_info"
for m in lpqsp1 ne_marb kellbase1; do
    S="$S;map e1m1;wait10;echo quoth: load $m;load quc_$m;wait10;vr_ruleset_info"
done
S="$S;echo quoth: done;toggleconsole;quit"
bash "$K/run.sh" "$NAME" -ExtraArgs "-nomapindex -noaddons" -Timeout 2400 -QuitOn "quoth: done" -Script "$S" \
    -Filter "quoth|Ruleset:|No spawn function|classname|Host_Error|SV_Error|OBJECT ERROR|rror|names a function|vr_goto_exit|\.qc :|couldn't|not found|exit=|TIMEOUT|timed out" \
    > "$OUT/run.log" 2>&1
python -I "$W/Misc/quakevr/quoth_maps.py" "$OUT/run.log"
