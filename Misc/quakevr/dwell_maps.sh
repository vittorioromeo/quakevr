#!/bin/bash
# dwell_maps.sh [frames]: Dwell P1b's acceptance (DWELL.md P1b). Two Copper custom maps from the Map Library (Ancient,
# Underdark Overbright's udob1: copper_custom_setup.py installs them into the test base) played the same way first;
# then Dwell's campaign (vr_campaign_native dwell), each of its
# 24 maps loaded (skill 1), saved 3 frames after spawn (scratch/dwell_maps/), its ruleset printed, then run `frames`
# game frames (default 600); then the exit chain (start's first exit, d1m1's exit), three saves loaded back, and a
# verdict line. dwell_maps.py reads the log and the saves: missing spawn functions per map (only Dwell's own 14
# classnames may miss: P2), errors, monsters (live + waiting for their trigger), coop-only placements present.
# Needs Dwell v2.2 in a base dir as <base>/dwell or <base>/dwellv2p2 (the kit's agent folder) and the kit (run.sh).
set -u
FR=${1:-600}
K=C:/OHWorkspace/qvr-kit
W=$(cd "$(dirname "$0")/../.." && pwd)
NAME=$(basename "$W")
OUT=$W/scratch/dwell_maps
mkdir -p "$OUT"
MAPS="start d1m1 d1m2 d1m3 d1m4 d1m5 d1m6 d1m7 d1m8 d1m9 d1end d2m1 d2m2 d2m3 d2m4 d2m5 d2m6 d2m7 d2m8 d2m9 d2end dwelldm2 dwelldm3 dwelldm7"
# Two Copper custom maps through the Map Library's path first, in the stock game (Dwell not mounted): Ancient and
# Underdark Overbright's udob1, installed into the test base's qvr_addons by copper_custom_setup.py (no download).
CUSTOM="ancient udob1"
BASE=$K/bases/$NAME/qbase${QVR_LANE:+.lane$QVR_LANE}
python -I "$W/Misc/quakevr/copper_custom_setup.py" "$BASE"
S="developer 0;sv_random_seed 7;skill 1;coop 0;maxplayers 1"
for m in $CUSTOM; do
    S="$S;echo dwell: map $m;map $m;wait15;save dwc_$m;vr_ruleset_info;wait$FR;echo dwell: ran $m"
done
S="$S;vr_campaign_native dwell;wait30"
for m in $MAPS; do
    # (saved 15 frames after `map`: d2m8's 4,000 edicts' signon puts the player in about 8 frames after it, and a save
    # without him fails. vr_waitprint on "entered the game" does not see that line: it is printed off the main thread.)
    S="$S;echo dwell: map $m;map $m;wait15;save dwc_$m;vr_ruleset_info;wait$FR;echo dwell: ran $m"
done
S="$S;map start;wait30;vr_goto_exit;wait90;echo dwell: chain start exit;vr_ruleset_info"
S="$S;map d1m1;wait30;god;vr_goto_exit;wait90;echo dwell: chain d1m1 exit;vr_ruleset_info"
for m in d1m3 d2m4 d2end; do
    S="$S;map e1m1;wait10;load dwc_$m;wait10;echo dwell: loaded $m;vr_ruleset_info"
done
S="$S;echo dwell: done;toggleconsole;quit"
bash "$K/run.sh" "$NAME" -ExtraArgs "-nomapindex -noaddons" -Timeout 2100 -QuitOn "dwell: done" -Script "$S" \
    -Filter "dwell:|Ruleset:|No spawn function|classname|Host_Error|SV_Error|OBJECT ERROR|rror|VR music|vr_goto_exit|Warning: key|\.qc :|exit=|TIMEOUT|timed out" \
    > "$OUT/run.log" 2>&1
python -I "$W/Misc/quakevr/dwell_maps.py" "$OUT/run.log" "$W/quakevr"
