#!/bin/bash
# plates_model_test.sh <agent> -- the player's armour plates drawn as the body's own armour model across a campaign
# switch (ROUND24.md, "The plates drawn as a gib"): for each body build 1-3, armour in e1m1 (id1), then the switch to
# rogue and armour in r2m6 (a map with many models precached: the slots a switch empties given to its models). Checks:
# vr_player_plates_model_check (the plates' model is "<body>#armor", an alias model) on each map, and the pixels
# (plates_model_pixels.py: a camera in front of the player, the plates on against off, the same silhouette in r2m6 as
# in e1m1). Needs rogue mounted. Ends `plates_model_test: N passed, M failed`.
NAME=${1:?agent name}
KIT=C:/OHWorkspace/qvr-kit
HERE=$(dirname "$0")
OUT="C:/OHWorkspace/qvr-agents/$NAME/scratch/plates_model_test.log"
S="vr_player_plates 1;vr_player_plates_morph_time 0;r_flatlightstyles 2;r_dynamic 0"
# (the camera set once the armour shows: set with it, the first r2m6 after a switch drew from the head)
shot() {
    S="$S;give a 100;wait10;vr_player_plates_model_check;vr_mock_camera 0 1.0 -1.7 0 180;wait5"
    S="$S;echo ppshot: build $1 $2 on;screenshot;vr_player_plates 0;wait5"
    S="$S;echo ppshot: build $1 $2 off;screenshot;vr_player_plates 1;vr_mock_camera"
}
for b in 1 2 3; do
    S="$S;vr_campaign_select id1;wait30;vr_body_build $b;map e1m1;wait30"
    shot $b e1m1
    S="$S;vr_campaign_select rogue;wait30;map r2m6;wait120" # (the first visit: the view settles for about a second)
    shot $b r2m6
done
S="$S;vr_campaign_select id1;wait30;resetcvar vr_player_plates_morph_time;resetcvar vr_body_build;resetcvar r_flatlightstyles;resetcvar r_dynamic"
S="$S;vr_player_plates_model_check end;toggleconsole;quit"
bash "$KIT/run.sh" "$NAME" -Clean -Script "$S" -Filter "ppmodel|ppshot|Wrote|rror" -QuitOn "ppmodel: [0-9]+ passed" -Timeout 900 > "$OUT" 2>&1
grep -E "ppmodel|rror|exit=" "$OUT"
python "$HERE/plates_model_pixels.py" "$OUT" "$NAME"
