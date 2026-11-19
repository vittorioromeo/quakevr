#!/bin/bash
# taa_attach_test.sh -- the TAA motion target stays attached to the eyes' scene and composite framebuffers when the eye
# targets are made anew (vr_temporal.cpp attach, vr_temporal_attachcheck; REVIEW_NEXT_2026-11-18.md finding 14's note):
# its cache was keyed by GL names alone, and a remade set given back the same framebuffer and texture names (as this
# Mesa does) kept none after the first remake, TAA without motion for the rest of the session. Now keyed by the eye
# targets' generation too. With the mock headset, TAA on (vr_taa 1, no MSAA): GL asked after the first map, after Render
# Scale changes (0.5, 1), after dynamic resolution steps (vr_dynres 2 with vr_dynres_test_load: a step down, then the load
# gone; each step logged, vr_dynres_log 1, and at least one checked to have happened) and after VR stopped and started
# again. Prints `taa_attach: PASS|FAIL <what>` per check and the verdict `taa_attach: N passed, M failed`.
# Linux (the game run directly, hidden, as resets_test.sh): QVR_ENGINE the game (default build/ironwail), QUAKE_DIR
# Quake's data (default Steam's).
TREE=${TREE:-$(cd "$(dirname "$0")/../.." && pwd)}
QT_NAME=taa_attach
source "$TREE/Misc/quakevr/qvrtest.sh"
ENGINE="${QVR_ENGINE:-$TREE/build/ironwail}"
QUAKE_DIR="${QUAKE_DIR:-$HOME/.local/share/Steam/steamapps/common/Quake}"
OUT="$TREE/scratch/taa_attach"
mkdir -p "$OUT"

# game <tag> <script> [args...]: one hidden run (waitN: N waits), its console in $OUT/<tag>.log.
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
    (cd "$TREE" && env QVR_TEST_HIDDEN=1 QVR_TEST_BACKGROUND=1 QVR_NO_ERROR_DIALOG=1 \
        timeout 300 "$ENGINE" -nosound -noconfigwrite -basedir "$QUAKE_DIR" -basedir . -game quakevr "$@" \
        +"${expanded%;}" > "$OUT/$tag.log" 2>&1
    echo "exit=$?" >> "$OUT/$tag.log")
}

game play "vr_backend mock;vr_enabled 1;vr_taa 1;vid_fsaa 0;vr_dynres 0;vr_setup_pending 0;map start;wait60;\
vr_temporal_attachcheck start;vr_render_scale 0.5;wait30;vr_temporal_attachcheck render-scale-0.5;\
vr_render_scale 1;wait30;vr_temporal_attachcheck render-scale-1;\
vr_dynres 2;vr_dynres_log 1;vr_dynres_test_load 40;wait150;vr_temporal_attachcheck dynres-loaded;\
vr_dynres_test_load 0;wait300;vr_temporal_attachcheck dynres-unloaded;vr_dynres 0;wait30;\
vr_temporal_attachcheck dynres-off;vr_enabled 0;wait10;vr_enabled 1;wait30;vr_temporal_attachcheck vr-restarted;\
vr_temporal_attachcheck end;quit"

grep -a -E "^taa_attach: (PASS|FAIL)" "$OUT/play.log" | sed "s/^taa_attach: /  (game) /"
expect_verdict "$(cat "$OUT/play.log")" "taa_attach" "the game's vr_temporal_attachcheck checks"
steps=$(grep -a -c -E "^dynres: [0-9.]+ -> [0-9.]+" "$OUT/play.log")
check $([ "$steps" -ge 1 ] && echo 1 || echo 0) "dynamic resolution stepped the eye size ($steps steps logged)"
check $(grep -q -a "^exit=0" "$OUT/play.log" && echo 1 || echo 0) "the game ran to its quit"
verdict
exit $fail
