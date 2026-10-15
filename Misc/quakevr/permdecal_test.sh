#!/bin/bash
# permdecal_test.sh <agent>: the permanent decals (vr_permdecals; docs/vr-port/PERM_DECALS.md) on vrclimb.
#  1. Marks laid in bursts past a live cap of 16 are all baked (none left live), two marks on brush entities (the lift,
#     the plat) are baked on their brush models, and a save loaded in the same game and in a new one bakes the same
#     stamps again.
#  2. Burst budget: 128 marks settling at once with vr_permdecal_bake_max 64 bake over several frames, no flush past
#     the budget and one mark's pages, all baked in the end.
#  3. Atlas full keeps marks: at vr_permdecal_density 0.25, a 1024 atlas (1024 pages a layer) grows layers instead of
#     giving pages up: none given up, and the frame is the same as with one 4096 layer.
#  4. On/off rule: switched off, the newest vr_decal_max marks show live, the frame the same as with the layer off from
#     the start (the old live ring); on again, the same frame as before; vr_decals 0 hides them all (the frame before
#     any mark) and 1 brings them back; marks made while off are kept and baked once it is on.
#  5. A mark shown later on the lift waits and is baked when it shows; a mark on a box made in the game stays live and
#     goes when the box changes.
set -eu
AGENT=${1:?worker name}
K=C:/OHWorkspace/qvr-kit
TREE=C:/OHWorkspace/qvr-agents/$AGENT
OUT=$TREE/scratch/permdecal_test.log
SHOTS=$TREE/quakevr/screenshots
fail() { printf 'permdecal FAIL: %s (%s)\n' "$1" "$OUT"; exit 1; }

# 1. Cap, brush entities, saves.
S='vr_permdecals 1;vr_decal_max 16;map vrclimb;wait20;setpos 20 0 24 0 0 0;noclip;wait5;vr_mock_look 30 0;wait5'
S="$S;vr_decal_stress 32 40;wait3;vr_decal_stress 32 30 wall;wait3"
S="$S;vr_decal_at -330 88 40 1 0 0 blood 30;vr_decal_at -330 88 40 1 0.15 -0.1 hole 8;wait120"
S="$S;vr_decal_count;vr_permdecal_info;save permdecal_test;wait30;load permdecal_test;wait60;vr_permdecal_info;toggleconsole;quit"
bash "$K/run.sh" "$AGENT" -Clean -Script "$S" -Filter '^permdecals:|^vr_decal_at|^[0-9]+ decals:|Host_Error|ENGINE ERROR|CRASH|exit=' > "$OUT"
S2='load permdecal_test;wait60;vr_permdecal_info;toggleconsole;quit'
bash "$K/run.sh" "$AGENT" -Script "$S2" -Filter '^permdecals:|Host_Error|ENGINE ERROR|CRASH|exit=' >> "$OUT"
[ "$(grep -c '^exit=0' "$OUT")" = 2 ] || fail "a run failed"
[ "$(grep -c '^vr_decal_at: laid at .* entity [1-9][0-9]*$' "$OUT")" = 2 ] || fail "marks on brush entities"
grep -q '^0 decals:' "$OUT" || fail "marks left live"
# The stamps kept after the marks, after the load in the same game, after the load in a new one: the same, 40 or more,
# 2 of them on brush entities.
KEPT=$(grep -o '^permdecals: [0-9]* stamps kept (2 on brush models)' "$OUT" | grep -o '[0-9]* stamps' | sort -u)
[ "$(printf '%s\n' "$KEPT" | wc -l)" = 1 ] || fail "stamps differ after the loads"
[ "$(grep -c '^permdecals: [0-9]* stamps kept (2 on brush models)' "$OUT")" = 3 ] || fail "stamps after the loads"
[ "${KEPT%% *}" -ge 40 ] || fail "too few stamps"

# 2. Burst budget.
B="$TREE/scratch/permdecal_test_budget.log"
S='vr_fixed_frames 1;vr_permdecals 1;vr_permdecal_bake_max 64;vr_decal_max 768;map vrclimb;wait20;setpos 20 0 24 0 0 0;noclip;wait5'
S="$S;vr_mock_look 30 0;wait5;vr_decal_stress 64 40;wait1;vr_decal_stress 64 30 wall;wait900;vr_decal_count;vr_permdecal_info;toggleconsole;quit"
bash "$K/run.sh" "$AGENT" -Script "$S" -Filter '^permdecals:|^[0-9]+ decals:|exit=' > "$B"
grep -q '^exit=0' "$B" || fail "budget run"
grep -q '^0 decals:' "$B" || fail "budget: marks left live"
WORST=$(grep -o '([0-9]* the most in one' "$B" | grep -o '[0-9]*')
FLUSHES=$(grep -o '^permdecals: [0-9]* flushes' "$B" | grep -o '[0-9]*')
[ "$WORST" -le 96 ] || fail "budget: $WORST pages in one flush"
[ "$FLUSHES" -ge 8 ] || fail "budget: only $FLUSHES flushes"

# 3 and 4: exact frames (screenshots compared, permdecal_shots.py).
X='vr_fixed_frames 1;vr_dither 0;r_drawentities 0;r_particles 0;vr_particles 0;r_dynamic 0;con_notifytime 0'
P='map vrclimb;wait20;setpos 20 0 24 0 0 0;noclip;wait5;vr_mock_look 30 0;wait5'
M='vr_decal_stress 32 40;wait3;vr_decal_stress 32 30 wall;wait900'
T="$TREE/scratch/permdecal_test_toggle.log"
# Shots 0 on, 1 off, 2 on again, 3 vr_decals 0, 4 vr_decals 1; then 5 the layer off from the start, 6 no marks
# (vr_decals 0 from the start; the view still turns for a while after the start: each shot after the same frames).
S="$X;vr_permdecals 1;vr_decal_max 16;$P;$M;screenshot;wait2;vr_permdecals 0;wait5;vr_decal_count;screenshot;wait2"
S="$S;vr_permdecals 1;wait5;vr_decal_count;screenshot;wait2;vr_decals 0;wait5;screenshot;wait2;vr_decals 1;wait5;screenshot;wait2"
S="$S;vr_permdecal_info;vr_permdecals 0;wait3;vr_decal_stress 24 20;wait3;vr_decal_stress 24 20 wall;wait900;vr_decal_count;vr_permdecals 1;wait5;vr_decal_count;vr_permdecal_info"
S="$S;toggleconsole;quit"
bash "$K/run.sh" "$AGENT" -Script "$S" -Filter '^permdecals: [0-9]+ stamps|^[0-9]+ decals:|exit=' > "$T"
S="$X;vr_permdecals 0;vr_decal_max 16;$P;$M;vr_decal_count;screenshot;wait2;toggleconsole;quit"
bash "$K/run.sh" "$AGENT" -Script "$S" -Filter '^[0-9]+ decals:|exit=' >> "$T"
S="$X;vr_decals 0;$P;$M;screenshot;wait2;vr_decals 1;toggleconsole;quit"
bash "$K/run.sh" "$AGENT" -Script "$S" -Filter 'exit=' >> "$T"
# Shots 7 one 4096 layer, 8 1024 layers growing.
A="$TREE/scratch/permdecal_test_atlas.log"
: > "$A"
M2='vr_decal_stress 64 90;wait3;vr_decal_stress 64 70 wall;wait3;vr_decal_stress 64 50 pool;wait3;vr_decal_stress 64 80 cluster;wait900'
for SIZE in 4096 1024; do
  S="$X;vr_permdecals 1;vr_permdecal_density 0.25;vr_permdecal_atlas $SIZE;vr_permdecal_atlas_max 512;vr_decal_max 16;$P;$M2"
  S="$S;vr_permdecal_info;screenshot;wait2;vr_permdecal_density 0.5;vr_permdecal_atlas 4096;toggleconsole;quit"
  bash "$K/run.sh" "$AGENT" -Script "$S" -Filter '^permdecals: (atlas|[0-9]+ stamps)|exit=' >> "$A"
done
[ "$(grep -c '^exit=0' "$T")" = 3 ] && [ "$(grep -c '^exit=0' "$A")" = 2 ] || fail "a frame run failed"
# Off: 16 live; on again: none; marks made while off kept and baked (more stamps, none live).
[ "$(grep -o '^[0-9]* decals:' "$T" | tr '\n' ' ')" = "16 decals: 0 decals: 16 decals: 0 decals: 16 decals: " ] || fail "live counts"
ST=$(grep -o '^permdecals: [0-9]* stamps' "$T" | grep -o '[0-9]* stamps' | grep -o '[0-9]*' | tr '\n' ' ')
set -- $ST
[ "$#" = 2 ] && [ "$2" -gt "$1" ] || fail "marks made while off not kept ($ST)"
grep -q '^permdecals: atlas 1024^2 x [2-9] layers' "$A" || fail "the 1024 atlas did not grow"
if grep -q ' [1-9][0-9]* given up' "$A"; then fail "pages given up"; fi
python "$TREE/Misc/quakevr/permdecal_shots.py" "$SHOTS" 9 6:0 0:1 0:2 0:4 6:3 1:5 7:8 > "$TREE/scratch/permdecal_test_shots.log"
pk() { grep "^$1 peak" "$TREE/scratch/permdecal_test_shots.log" | grep -o '[0-9]*$'; }
[ "$(pk 6:0)" -gt 32 ] || fail "no marks on screen"
[ "$(pk 0:1)" -gt 32 ] || fail "off shows the same as on"
[ "$(pk 0:2)" -le 2 ] || fail "on again differs ($(pk 0:2))"
[ "$(pk 0:4)" -le 2 ] || fail "vr_decals 1 again differs ($(pk 0:4))"
[ "$(pk 6:3)" -le 2 ] || fail "vr_decals 0 shows marks ($(pk 6:3))"
[ "$(pk 1:5)" -le 2 ] || fail "off differs from the old live ring ($(pk 1:5))"
[ "$(pk 7:8)" -le 2 ] || fail "layered atlas differs ($(pk 7:8))"
# 5. A mark shown later on a brush entity (the gore's flying blood, 3 s) waits and is baked when it shows, not at once;
#    a mark on a box made in the game (vr_brushes.cpp) stays live past every bake delay (not baked) and goes when the
#    box changes, the world's beside it baked.
E="$TREE/scratch/permdecal_test_late.log"
S='vr_fixed_frames 1;vr_permdecals 1;map vrclimb;wait20;setpos 20 0 24 0 0 0;noclip;wait5'
S="$S;vr_decal_at -330 80 20 1 0 0 blood 24 3;wait5;vr_permdecal_info;wait400;vr_permdecal_info"
S="$S;vr_brush_box 50 -16 0 66 16 48;wait5;vr_decal_at 20 0 30 1 0 0 blood 16;vr_decal_at 20 0 30 0 0 -1 blood 16;wait900"
S="$S;vr_brush_list;vr_permdecal_info;vr_brush_set 1 50 -16 0 66 16 64;wait5;vr_brush_list;toggleconsole;quit"
bash "$K/run.sh" "$AGENT" -Script "$S" -Filter '^permdecals: ([0-9]+ stamps|[0-9]+ marks on brush)|^  [0-9]+: entity|Host_Error|ENGINE ERROR|CRASH|exit=' > "$E"
grep -q '^exit=0' "$E" || fail "late/box run"
grep -q '^permdecals: 1 marks on brush models waiting' "$E" || fail "the late mark on the lift not waiting"
[ "$(grep -o '^permdecals: [0-9]* stamps kept ([0-9]* on' "$E" | grep -o '[0-9]* stamps kept ([0-9]*' | tr -d 'a-z (' | tr '\n' ' ')" = "00 11 21 " ] ||
  fail "late mark / box stamps ($(grep -o '[0-9]* stamps kept ([0-9]* on' "$E" | tr '\n' ' '))"
[ "$(grep -o '[0-9]* decals$' "$E" | tr '\n' ' ')" = "1 decals 0 decals " ] || fail "the box's mark not live, or not gone with the change"
if grep -qE 'Host_Error|ENGINE ERROR|CRASH' "$OUT" "$B" "$T" "$A" "$E"; then fail "engine error"; fi
printf 'permdecal PASS: %s baked past a live cap of 16, 2 on brush entities, the same after loads; budget 64: %s pages the most in a flush, %s flushes; ' "$KEPT" "$WORST" "$FLUSHES"
printf '1024 atlas grew layers, none given up, same frame; off = the old live ring, on/off/on and vr_decals 0/1 frames exact; '
printf 'a late mark on the lift baked when it shows; a box'"'"'s mark live, gone with the box\n'
