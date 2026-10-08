#!/bin/bash
# swim_sound_test.sh <agent> -- swimming strokes sound as water, not as slaps (the author's note
# vrfiringrange_2026-10-08_22-26-37), headless, on vrfiringrange (its pool; docs/vr-port/TESTING.md, "Swimming"):
#   1. an open hand swung across on dry land: the slap's whoosh (the control) and it wakes monsters (show_hostile)
#   2. in the pool, 6 s of strokes (swim_plays.py cycle_edge.txt): each hand's strokes heard (vr/swim_* or vr/stroke*),
#      no slap's whoosh, no knights' swing, show_hostile not renewed
#   3. the same open hand swung across under water: no whoosh, no slap, show_hostile not renewed
#   4. a closed fist punched under water: the swing counts (show_hostile set) but it doesn't whoosh
# Prints PASS/FAIL per check; exits 1 on a failure. Saves in quakevr/ (qvr_swim_*.sav).
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
W=C:/OHWorkspace/qvr-agents/$AGENT
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
mkdir -p "$W/scratch/swimsnd"
python -c "import os,runpy; os.chdir(r'$W/scratch/swimsnd'); runpy.run_path(r'$W/Misc/quakevr/swim/swim_plays.py', run_name='__main__')" > /dev/null
D=$(cd "$W/scratch/swimsnd" && pwd -W 2>/dev/null || pwd)
# An open main hand swung across the body, right to left, 0.7 m in 0.12 s (about 6 m/s); a closed fist punched ahead.
printf '0.000 main 0.40 1.05 -0.35 -20 0 0\n0.300 main 0.40 1.05 -0.35 -20 0 0\n0.420 main -0.30 1.05 -0.35 -20 0 0\n1.000 main -0.30 1.05 -0.35 -20 0 0\n' > "$D/slap.txt"
printf '0.000 main 0.20 1.05 -0.20 0 0 0\n0.300 main 0.20 1.05 -0.20 0 0 0\n0.450 main 0.20 1.05 -1.00 0 0 0\n1.200 main 0.20 1.05 -1.00 0 0 0\n' > "$D/punch.txt"
hostile() { awk 'BEGIN{RS="}"} /"classname" "player"/' "$W/quakevr/$1.sav" | grep -E '"show_hostile"' | sed -E 's/.*"([0-9.]+)"/\1/'; }
S="vr_fixed_frames 1;vr_fixed_frames_rate 90;map vrfiringrange;wait60;god;notarget;developer 2;wait5"
S="$S;echo PHASE dry;vr_mock_play $D/slap.txt;wait120;save qvr_swim_dry"
S="$S;sv_gravity 0;setpos 370 474 -180 0 0 0;noclip;vr_swim_stick_speed 0.001;vr_mock_stick off 0 1;wait90"
S="$S;echo PHASE cycle;vr_mock_play $D/cycle_edge.txt;wait700;save qvr_swim_cycle"
S="$S;vr_mock_stick off 0 0;setpos 612 474 -180 0 0 0;noclip;wait10;echo PHASE slap;viewpos;vr_mock_play $D/slap.txt;wait120;save qvr_swim_slap"
S="$S;vr_mock_stick off 0 0;setpos 612 474 -180 0 0 0;noclip;wait10;echo PHASE punch;viewpos;+grabmain;vr_mock_button main grip 1;wait10;vr_mock_play $D/punch.txt;wait120;save qvr_swim_punch"
S="$S;vr_mock_button main grip 0;-grabmain;wait5;toggleconsole;quit"
log=$(bash $KIT/run.sh $AGENT -Script "$S" -Filter "PHASE|VR water sound|melee sound|rror|Player pos|melee debug: hand" 2>&1)
echo "$log" > "$W/scratch/swimsnd/log.txt"
phase() { awk -v p="PHASE $1" '/PHASE /{on=($0 ~ p)} on' "$W/scratch/swimsnd/log.txt"; }
dry=$(phase dry); cyc=$(phase cycle); slp=$(phase slap); pun=$(phase punch)
hd=$(hostile qvr_swim_dry); hc=$(hostile qvr_swim_cycle); hs=$(hostile qvr_swim_slap); hp=$(hostile qvr_swim_punch)
check $(echo "$dry" | grep -q "slap_whoosh" && echo 1 || echo 0) "dry land: an open hand swung across whooshes"
check $([ -n "$hd" ] && echo 1 || echo 0) "dry land: the slap wakes monsters ($hd)"
n=$(echo "$cyc" | grep -cE "VR water sound: vr/(swim_|stroke)")
check $([ "$n" -ge 4 ] && [ "$n" -le 16 ] && echo 1 || echo 0) "swimming: the strokes heard, not spammy ($n in 6 s, both hands)"
echo "$cyc" | grep -E "VR water sound: vr/(swim_|stroke)" | sed -E 's/.*vr\/([a-z_]+)[0-9].wav, volume/\1/' | sort | uniq -c | head -5
check $(echo "$cyc" | grep -qE "slap_whoosh|knight/sword" && echo 0 || echo 1) "swimming: no slap whoosh, no swing"
check $([ "$hc" = "$hd" ] && echo 1 || echo 0) "swimming: wakes nothing ($hd -> $hc)"
check $(echo "$slp" | grep -qE "slap_whoosh|vr/slap[0-9]" && echo 0 || echo 1) "under water: an open hand swung across neither whooshes nor slaps"
check $([ "$hs" = "$hd" ] && echo 1 || echo 0) "under water: the open hand wakes nothing ($hs)"
check $([ -n "$hp" ] && [ "$hp" != "$hd" ] && echo 1 || echo 0) "under water: a punch counts ($hp)"
check $(echo "$pun" | grep -q "under water" && echo 1 || echo 0) "under water: the punch doesn't whoosh"
check $(echo "$log" | grep -q "rror" && echo 0 || echo 1) "no error"
exit $fail
