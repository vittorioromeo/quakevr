#!/bin/bash
# music_test.sh <agent> -- the CD tracks each campaign plays (his notes 2026-11-21: Scourge of Armagon's and Quake's
# start played Dwell's music): Dwell's and Quoth's folders are mounted in every campaign for their models and sounds only
# (vr_gamedir.cpp skipOutside), so their music/trackNN never plays outside their campaign (vr_music.cpp honours
# VR_SkipSearchPath); in Dwell's campaign its own tracks play, its numbers above 127 included (start's 254), not read as
# Quake VR's campaign-track encoding. Headless (vr_music_which prints the file each track resolves to; no sound):
# Quake's start (its track and tracks 2-5), hip1m1, then Dwell's campaign (start's 254, its track 3). Needs Dwell v2.2
# in the base dir (<base>/dwellv2p2) for the last checks (skipped without it). Exits 1 on a failure.
AGENT=$1
source "$(dirname "$0")/qvrtest.sh"
S="map start;wait5;echo music: quake;vr_music_which;vr_music_which 2;vr_music_which 3;vr_music_which 4;vr_music_which 5"
S="$S;map hip1m1;wait5;echo music: hipnotic;vr_music_which"
S="$S;vr_campaign_native dwell;wait30;echo music: dwell;vr_music_which;vr_music_which 3;echo music: done"
log=$(bash $KIT/run.sh $AGENT -ExtraArgs "-nomapindex -noaddons" -Script "$S" -QuitOn "music: done" -Timeout 300 \
    -Filter "^music:|VR music which|exit=" 2>&1 | tr -d '\r' | tr '\n' ' ' | sed 's/VR music which/\nVR music which/g; s/music: /\nmusic: /g')
echo "$log" | grep -E "^VR music which" | cut -c1-160
section() { sed -n "/^music: $1/,/^music: /p" <<<"$log" | grep "^VR music which"; }
q=$(section quake); h=$(section hipnotic); d=$(section dwell)
check $([ "$(grep -c . <<<"$q")" -ge 5 ] && echo 1 || echo 0) "Quake's start: its track and tracks 2-5 resolved"
check $(grep -qi -e dwell -e quoth <<<"$q" && echo 0 || echo 1) "Quake's start: no track from Dwell's or Quoth's folder"
check $(grep -qi -e dwell -e quoth <<<"$h" && echo 0 || echo 1) "hip1m1: its track not Dwell's or Quoth's"
check $(grep -q "hipnotic" <<<"$h" && echo 1 || echo 0) "hip1m1: Scourge of Armagon's own track"
if [ -d "$KIT/bases/$AGENT/qbase/dwellv2p2" ]; then
    check $(grep -q "track 254 -> .*dwellv2p2" <<<"$d" && echo 1 || echo 0) "Dwell's start: its track 254 from Dwell's folder"
    check $(grep -q "track 3 -> .*dwellv2p2" <<<"$d" && echo 1 || echo 0) "Dwell's campaign: its track 3 its own"
fi
check $(grep -q "exit=quit-on" <<<"$log" && echo 1 || echo 0) "the run ended on its last line"
verdict
exit $fail
