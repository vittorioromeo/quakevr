#!/bin/bash
# dynres_ceiling_test.sh -- dynamic resolution draws a part of the eye targets allocated once at its ceiling, not new
# targets at each step (vr_dynres.hpp; stereo::ViewSize, vr_stereo.hpp; REVIEW_NEXT_2026-11-18.md finding 14). With the
# mock headset (its eyes 1024x1024), on e1m1:
# - the game's own checks (vr_stereo_targets_test begin / stepped): with vr_dynres 2, its ceiling 1 (vr_dynres_max),
#   a low budget (vr_dynres_target 0.3) and a test load (vr_dynres_test_load, which changes no pixel; a light one: a
#   frame over 0.1 s is a gap dynres does not read) make it step down to its floor and stay: first to 0.9 (where the
#   resample target is made, the first time the eyes are smaller than their images), then, from `begin`, to 0.6 (the
#   floor lowered); then a step happened, the
#   eyes are drawn at another size, a part of their targets, the eye targets were not made again (their generation) and
#   no render target was (gfx::targetsMade: the resample target, the glow's chain, the particles');
# - the eyes' images then (vr_eyeshot 1, after the resample to the image's size) the same as those of a game drawing at
#   Render Scale 0.6 with dynamic resolution off, whose targets are that size: 0 pixels differ (ImageMagick's compare;
#   skipped without it).
# Prints `dynres_ceiling: PASS|FAIL <what>` per check and the verdict `dynres_ceiling: N passed, M failed`.
# Linux (the game run directly, hidden, as taa_attach_test.sh): QVR_ENGINE the game (default build/ironwail), QUAKE_DIR
# Quake's data (default Steam's).
TREE=${TREE:-$(cd "$(dirname "$0")/../.." && pwd)}
QT_NAME=dynres_ceiling
source "$TREE/Misc/quakevr/qvrtest.sh"
ENGINE="${QVR_ENGINE:-$TREE/build/ironwail}"
QUAKE_DIR="${QUAKE_DIR:-$HOME/.local/share/Steam/steamapps/common/Quake}"
OUT="$TREE/scratch/dynres_ceiling"
SHOTS="$TREE/quakevr/eyeshots"
mkdir -p "$OUT"

# game <tag> <script> [args...]: one hidden run (waitN: N waits), its console in $OUT/<tag>.log, its eyeshots in $OUT/<tag>.
game() {
    local tag="$1" script="$2" expanded="" c i
    shift 2
    IFS=';' read -ra parts <<<"$script"
    for c in "${parts[@]}"; do
        if [[ "$c" =~ ^wait([0-9]+)$ ]]; then
            for ((i = 0; i < BASH_REMATCH[1]; i++)); do expanded+="wait;"; done
        else
            expanded+="$c;"
        fi
    done
    rm -rf "$SHOTS" "$OUT/$tag"
    mkdir -p "$OUT/$tag"
    (cd "$TREE" && env QVR_TEST_HIDDEN=1 QVR_TEST_BACKGROUND=1 QVR_NO_ERROR_DIALOG=1 \
        timeout 300 "$ENGINE" -nosound -noconfigwrite -basedir "$QUAKE_DIR" -basedir . -game quakevr "$@" \
        +"${expanded%;}" > "$OUT/$tag.log" 2>&1
    echo "exit=$?" >> "$OUT/$tag.log")
    mv "$SHOTS"/*.png "$OUT/$tag/" 2>/dev/null
}

COMMON="vr_backend mock;vr_enabled 1;vr_setup_pending 0;vr_fixed_frames 1;vr_particle_seed 7;sv_random_seed 7"
game fixed "$COMMON;vr_dynres 0;vr_render_scale 0.6;map e1m1;wait5;vr_dynres_test_load 10;wait295;vr_eyeshot 1;wait3;quit"
game dynres "$COMMON;vr_dynres 2;vr_dynres_log 1;vr_render_scale 1;vr_dynres_max 1;vr_dynres_min 0.9;vr_dynres_target 0.3;\
map e1m1;wait5;vr_dynres_test_load 10;wait60;vr_stereo_targets_test begin;vr_dynres_min 0.6;wait235;\
vr_stereo_targets_test stepped;vr_stereo_targets_test verdict;vr_eyeshot 1;wait3;quit"

grep -a -E "^stereo_targets: (PASS|FAIL)" "$OUT/dynres.log" | sed "s/^stereo_targets: /  (game) /"
expect_verdict "$(cat "$OUT/dynres.log")" "stereo_targets" "the game's vr_stereo_targets_test checks"
steps=$(grep -a -c -E "^dynres: [0-9.]+ -> [0-9.]+" "$OUT/dynres.log")
check $([ "$steps" -ge 1 ] && echo 1 || echo 0) "dynamic resolution stepped ($steps steps logged)"
if command -v magick > /dev/null; then
    for side in L R; do
        a="$OUT/fixed/e1m1_000_$side.png" b="$OUT/dynres/e1m1_000_$side.png"
        n=$( [ -f "$a" ] && [ -f "$b" ] && magick "$a" "$b" -compose difference -composite -separate -evaluate-sequence max \
            -threshold 0 -format "%[fx:round(mean*w*h)]" info: 2>/dev/null)
        check $([ "$n" = "0" ] && echo 1 || echo 0) "eye $side at the floor (0.6 of the ceiling's targets) as at Render Scale 0.6 (${n:-no image} pixels differ)"
    done
else
    echo "dynres_ceiling: (no ImageMagick: the images not compared)"
fi
check $(grep -q -a "^exit=0" "$OUT/fixed.log" && grep -q -a "^exit=0" "$OUT/dynres.log" && echo 1 || echo 0) "the games ran to their quit"
verdict
exit $fail
