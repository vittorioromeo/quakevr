#!/bin/bash
# gles_render_test.sh <agent> -- the render paths under the Quest's OpenGL ES profile (QUEST_PLAN.md, C2c): the same
# scene drawn on the PC and under -glescompat -glesclipcontrol (QVR_GLES_COMPAT=1 QVR_GLES_CLIP_CONTROL=1: an ES
# driver with EXT_clip_control), e1m1's start with TAA on (vr_taa 1), the map lights' shadows, the weapon
# reflections (the envmap) and the PBR companions at their defaults, the particles and the server's random numbers
# seeded. Each run: the left eye's image (vr_eyeshot 1) after 120 frames (the TAA history and the envmap settled),
# vr_envmap_dump, imagehash; gles_render_check.py compares them.
# Checks: both runs drew (eyes not black); ES compiled every program and turned no feature off ("need" lines);
# the eye differs in under 2% of its pixels by more than 60 of 765 and by under 3 per channel on average; the
# envmap the same within that; every texture (PBR companions included) the same bits.
# Prints PASS/FAIL lines and `gles_render: N passed, M failed`; exits 1 on a failure.
AGENT=${1:-quest}
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch/gles_render; mkdir -p "$S/pc" "$S/es"
GAME=$KIT/bases/$AGENT/qbase/quakevr
SCRIPT="sv_random_seed 7; vr_particle_seed 7; vr_taa 1; map e1m1; wait120; vr_eyeshot 1; wait3; vr_envmap_dump; imagehash; vr_gles_report; toggleconsole; quit"
FILTER="Wrote|imagehash|shader errors|need|GLES compat|rror"

run() { # run <mode> [env...]: the scene, its outputs copied to $S/<mode>
    local mode=$1; shift
    env "$@" bash $KIT/run.sh $AGENT -Timeout 300 -Filter "$FILTER" -Script "$SCRIPT" > "$S/$mode.log" 2>&1
    cp "$(ls -t "$GAME"/eyeshots/*_L.png | head -1)" "$S/$mode/eye.png"
    cp "$GAME/envmap.tga" "$S/$mode/envmap.tga"
    cp "$GAME/imagehash.txt" "$S/$mode/imagehash.txt"
}
run pc
run es QVR_GLES_COMPAT=1 QVR_GLES_CLIP_CONTROL=1
python "$TREE/Misc/quakevr/gles_render_check.py" "$S/pc" "$S/es" > "$S/check.txt"
cat "$S/check.txt"
read -r _ eyeDiff eyeMoved eyePc eyeEs < <(grep "^eye " "$S/check.txt")
read -r _ envDiff envMoved envPc envEs < <(grep "^envmap " "$S/check.txt")
read -r _ texN texDiff < <(grep "^imagehash " "$S/check.txt")
esErrors=$(grep -aoE "[0-9]+ shader errors" "$S/es.log" | tail -1)
esOff=$(grep -acE "need (GL_|EXT_)|needs? .*clip" "$S/es.log")
lt() { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a != "" && a >= 0 && a < b)}' && echo 1 || echo 0; }
check $(awk -v a="$eyePc" -v b="$eyeEs" 'BEGIN{exit !(a > 8 && b > 8)}' && echo 1 || echo 0) "both runs drew the eye (means ${eyePc:-?}, ${eyeEs:-?})"
check $([ "$esErrors" = "0 shader errors" ] && echo 1 || echo 0) "ES compiled every program (${esErrors:-no report})"
check $([ "$esOff" = "0" ] && echo 1 || echo 0) "ES turned no feature off for a missing extension ($esOff lines)"
check $(lt "$eyeMoved" 0.02) "eye: ${eyeMoved:-?} of the pixels moved (under 0.02)"
check $(lt "$eyeDiff" 3) "eye: ${eyeDiff:-?} mean difference per channel (under 3)"
check $(lt "$envMoved" 0.02) "envmap: ${envMoved:-?} of the texels moved (under 0.02)"
check $(lt "$envDiff" 3) "envmap: ${envDiff:-?} mean difference per channel (under 3)"
check $([ "${texDiff:--1}" = "0" ] && [ "${texN:-0}" -gt 100 ] && echo 1 || echo 0) "textures: ${texDiff:-?} of ${texN:-?} differ (none)"
verdict; exit $fail
