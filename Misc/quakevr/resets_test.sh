#!/bin/bash
# resets_test.sh -- the registered resets (Quake/vr/vr_mem.hpp mem::Reset, `vr_resets`; REVIEW_NEXT_2026-11-18.md,
# finding 20): the registry runs its own test resets, made out of order, in their declared order (phase, then name),
# each once a run; the real lists are in order with box3d first and scenario last; and in play every run of the
# server's world and of the client's state runs each registered reset of its event exactly once, in that order, as many
# times as the old lists ran (traced on the code before the registry): a first map, a map change and a changelevel 2
# and 2, a loaded game 2 and 2, a disconnect none; a dedicated server's map 2 server runs and no client run.
# Prints `resets: PASS|FAIL <what>` per check and the verdict `resets: N passed, M failed`.
# Linux (the game run directly, hidden, as LINUX.md's): QVR_ENGINE the game (default build/ironwail), QUAKE_DIR Quake's
# data (default Steam's).
TREE=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
QT_NAME=resets
source "$TREE/Misc/quakevr/qvrtest.sh"
ENGINE="${QVR_ENGINE:-$TREE/build/ironwail}"
QUAKE_DIR="${QUAKE_DIR:-$HOME/.local/share/Steam/steamapps/common/Quake}"
OUT="$TREE/scratch/resets"
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
        timeout 240 "$ENGINE" -nosound -noconfigwrite -basedir "$QUAKE_DIR" -basedir . -game quakevr "$@" \
        +"${expanded%;}" > "$OUT/$tag.log" 2>&1
    echo "exit=$?" >> "$OUT/$tag.log")
}

game play "vr_backend mock;vr_enabled 1;vr_fixed_frames 1;vr_mock_fast 1;wait10;vr_resets test;vr_resets record;\
map e1m1;wait60;vr_resets check map 2 2;map e1m2;wait60;vr_resets check map-change 2 2;\
changelevel e1m3;wait60;vr_resets check changelevel 2 2;save resets_test;wait10;vr_resets record;\
load resets_test;wait60;vr_resets check load 2 2;disconnect;wait20;vr_resets check disconnect 0 0;vr_resets verdict;quit"
rm -f "$TREE/quakevr/resets_test.sav"
game dedicated "vr_resets record;map e1m1;wait60;vr_resets check dedicated 2 0;vr_resets verdict;quit" -dedicated

for tag in play dedicated; do
    grep -a -E "^vr_resets: (PASS|FAIL)" "$OUT/$tag.log" | sed "s/^vr_resets: /  ($tag) /"
    expect_verdict "$(cat "$OUT/$tag.log")" "vr_resets" "$tag: the game's vr_resets checks"
    check $(grep -q -a "^exit=0" "$OUT/$tag.log" && echo 1 || echo 0) "$tag: the game ran to its quit"
done
verdict
exit $fail
