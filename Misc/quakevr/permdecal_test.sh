#!/bin/bash
# permdecal_test.sh <agent>: the permanent decals (vr_permdecals; docs/vr-port/PERM_DECALS.md) on vrclimb. Marks laid
# in bursts past a live cap of 16 are all baked (none left live), two marks on brush entities (the lift, the plat) are
# baked on their brush models, and a save loaded in the same game and in a new one bakes the same stamps again.
set -eu
AGENT=${1:?worker name}
K=C:/OHWorkspace/qvr-kit
TREE=C:/OHWorkspace/qvr-agents/$AGENT
OUT=$TREE/scratch/permdecal_test.log
S='vr_permdecals 1;vr_decal_max 16;map vrclimb;wait20;setpos 20 0 24 0 0 0;noclip;wait5;vr_mock_look 30 0;wait5'
S="$S;vr_decal_stress 32 40;wait3;vr_decal_stress 32 30 wall;wait3"
S="$S;vr_decal_at -330 88 40 1 0 0 blood 30;vr_decal_at -330 88 40 1 0.15 -0.1 hole 8;wait120"
S="$S;vr_decal_count;vr_permdecal_info;save permdecal_test;wait30;load permdecal_test;wait60;vr_permdecal_info;toggleconsole;quit"
bash "$K/run.sh" "$AGENT" -Script "$S" -Filter '^permdecals:|^vr_decal_at|^[0-9]+ decals:|Host_Error|ENGINE ERROR|CRASH|exit=' > "$OUT"
S2='load permdecal_test;wait60;vr_permdecal_info;toggleconsole;quit'
bash "$K/run.sh" "$AGENT" -Script "$S2" -Filter '^permdecals:|Host_Error|ENGINE ERROR|CRASH|exit=' >> "$OUT"
grep -c '^exit=0' "$OUT" | grep -q '^2$'
grep -c '^vr_decal_at: laid at .* entity [1-9][0-9]*$' "$OUT" | grep -q '^2$'
grep -q '^0 decals:' "$OUT"
# The stamps kept after the marks, after the load in the same game, after the load in a new one: the same, 40 or more,
# 2 of them on brush entities.
KEPT=$(grep -o '^permdecals: [0-9]* stamps kept (2 on brush models)' "$OUT" | grep -o '[0-9]* stamps' | sort -u)
[ "$(printf '%s\n' "$KEPT" | wc -l)" = 1 ]
[ "$(grep -c '^permdecals: [0-9]* stamps kept (2 on brush models)' "$OUT")" = 3 ]
[ "${KEPT%% *}" -ge 40 ]
if grep -qE 'Host_Error|ENGINE ERROR|CRASH' "$OUT"; then exit 1; fi
printf 'permdecal PASS: %s baked past a live cap of 16 (none live), 2 on brush entities, the same after a load (same game, new game)\n' "$KEPT"
