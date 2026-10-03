#!/bin/bash
# gib_2h_models_test.sh <agent> [out dir] -- each gib and head model (impulse 252, vr_test_held_pick 0-8) taken in both
# hands and pushed forward (6 and 9 m/s), the hands not quite together (the off hand 50 ms behind, its path 0.6 as
# long) and let go together, the off hand 40 ms or a frame first, or the main hand 22 ms first (NOTES.md
# vrfiringrange_2026-10-03_02-02-13: the big gibs burst in front of him; ROUND21.md "Two-handed throws of big gibs").
# CFG=<a config in the game folder> is exec'd first (the author's: his gib masses). gib_2h_throw_test.sh's lines; the
# ones held in both ("both 1") must not burst. A head is too small to be taken by the second hand ("both 0"): it is
# held in one, the other hand alongside, which strikes it (the fist's strike on a gib held in the other hand).
HERE=$(cd "$(dirname "$0")" && pwd)
KINDS=${KINDS:-"gib0 gib2 gib3 gib1 gib4 gib5 gib6 gib7 gib8"} THROWS=${THROWS:-"push hard"} STAGGERS=${STAGGERS:-"0 1 2 3"} \
    LAG=${LAG:-0.05} OSCALE=${OSCALE:-0.6} bash "$HERE/gib_2h_throw_test.sh" "$@"
OUT=${2:-}
[ -n "$OUT" ] && echo "held in both and burst: $(grep "both [1-9] burst [1-9]" "$OUT/summary.txt" | wc -l)"
