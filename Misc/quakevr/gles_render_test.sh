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
# The noperspective fallback (-glesnoext, QVR_GLES_NOEXT=1: an ES driver without NV_shader_noperspective_interpolation,
# the light tile coordinate and the retro affine texturing w-multiplied): the ES scene with r_softemu_mdl_warp 1 (the
# models' affine texturing) drawn with and without the extension; the eye differs in under 0.01% of its pixels and by
# under 0.1 per channel on average (measured: identical; the fallback without its gl_FragCoord.w: 0.02% and 0.81), and
# every program compiled.
# The world's textures in arrays (vr_world_texarrays, QUEST_PLAN.md C2f: on under the ES profile): the ES scene drew
# the world from them (draws of many calls; few calls left on the bound path), and the same scene with them off
# (vr_world_texarrays 0: a draw a texture) differs in under 0.01% of its pixels and by under 0.1 per channel on
# average (measured: identical), every texture the same.
# Prints PASS/FAIL lines and `gles_render: N passed, M failed`; exits 1 on a failure.
AGENT=${1:-quest}
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch/gles_render; mkdir -p "$S/pc" "$S/es" "$S/esw" "$S/esnoext"
GAME=$KIT/bases/$AGENT/qbase/quakevr
BASE="sv_random_seed 7; vr_particle_seed 7; vr_taa 1; map e1m1; wait120; vr_eyeshot 1; wait3; vr_envmap_dump; imagehash; vr_gles_report; vr_world_texarrays_report; toggleconsole; quit"
SCRIPT=$BASE
FILTER="Wrote|imagehash|shader errors|need|GLES compat|rror|world arrays|texarrays"

run() { # run <mode> [env...]: the scene, its outputs copied to $S/<mode>
    local mode=$1; shift
    # (the outputs this run writes, not an earlier run's: a run that dies first leaves none, and its checks fail)
    rm -f "$GAME/envmap.tga" "$GAME/imagehash.txt" "$S/$mode/eye.png" "$S/$mode/envmap.tga" "$S/$mode/imagehash.txt"
    # (new: written after the run started. Not by its name: every run's first eyeshot is <map>_000, the same file)
    local after started="$S/$mode.started"
    touch "$started"
    env "$@" bash $KIT/run.sh $AGENT -Timeout 300 -Filter "$FILTER" -Script "$SCRIPT" > "$S/$mode.log" 2>&1
    after=$(ls -t "$GAME"/eyeshots/*_L.png 2>/dev/null | head -1)
    if [ -n "$after" ] && [ "$after" -nt "$started" ]; then cp "$after" "$S/$mode/eye.png"; else echo "run $mode: no new eyeshot ($S/$mode.log)"; fi
    [ -f "$GAME/envmap.tga" ] && cp "$GAME/envmap.tga" "$S/$mode/envmap.tga"
    [ -f "$GAME/imagehash.txt" ] && cp "$GAME/imagehash.txt" "$S/$mode/imagehash.txt"
    return 0
}
# (the PC run's shaders queued as the ES ones are, QUEST_PLAN.md 6zi: the same start-up, so the 2D pics fill the scrap
# in the same order; the PC's default, one call, is every other test's)
run pc QVR_SHADERS_ASYNC=1
run es QVR_GLES_COMPAT=1 QVR_GLES_CLIP_CONTROL=1
SCRIPT="vr_world_texarrays 0; $BASE"
mkdir -p "$S/esoff"; run esoff QVR_GLES_COMPAT=1 QVR_GLES_CLIP_CONTROL=1
SCRIPT="r_softemu_mdl_warp 1; $BASE"
run esw QVR_GLES_COMPAT=1 QVR_GLES_CLIP_CONTROL=1
run esnoext QVR_GLES_COMPAT=1 QVR_GLES_CLIP_CONTROL=1 QVR_GLES_NOEXT=1
python "$TREE/Misc/quakevr/gles_render_check.py" "$S/esw" "$S/esnoext" > "$S/check_noext.txt"
cat "$S/check_noext.txt"
read -r _ nxDiff nxMoved nxA nxB < <(grep "^eye " "$S/check_noext.txt")
nxErrors=$(grep -aoE "[0-9]+ shader errors" "$S/esnoext.log" | tail -1)
nxOn=$(grep -ac "treated as absent" "$S/esnoext.log")
python "$TREE/Misc/quakevr/gles_render_check.py" "$S/pc" "$S/es" > "$S/check.txt"
cat "$S/check.txt"
read -r _ eyeDiff eyeMoved eyePc eyeEs < <(grep "^eye " "$S/check.txt")
read -r _ envDiff envMoved envPc envEs < <(grep "^envmap " "$S/check.txt")
read -r _ texN texDiff < <(grep "^imagehash " "$S/check.txt")
esErrors=$(grep -aoE "[0-9]+ shader errors" "$S/es.log" | tail -1)
esOff=$(grep -acE "need (GL_|EXT_)|needs? .*clip" "$S/es.log")
python "$TREE/Misc/quakevr/gles_render_check.py" "$S/es" "$S/esoff" > "$S/check_arrays.txt"
cat "$S/check_arrays.txt"
read -r _ arDiff arMoved arA arB < <(grep "^eye " "$S/check_arrays.txt")
read -r _ arTexN arTexDiff < <(grep "^imagehash " "$S/check_arrays.txt")
read -r _ _ _ _ arMany _ _ _ arCalls _ _ _ arAlone _ arBound _ < <(grep -a "^world arrays last frame" "$S/es.log" | tail -1)
arSets=$(grep -aoE "world arrays: [0-9]+ sets, [0-9]+ textures" "$S/es.log" | tail -1)
arOffDraws=$(grep -a "^world arrays last frame" "$S/esoff.log" | tail -1)
# The eye's sub-image (vr_eye_subimage, QUEST_PLAN.md C2g; on under the ES profile): the Quest preset (render scale 0.9,
# bilinear; dynamic resolution off: a fixed size) drawn into a part of the image, no resample pass; its final image
# (vr_eyeshot 1: the part) against the same scene's resample target with it off (vr_eyeshot 2): the same size,
# identical. The PC (vr_render_scale 0.9, bilinear, the setting at its default) still fills the image.
SUBRUN="sv_random_seed 7; vr_particle_seed 7; map e1m1; wait120"
SCRIPT="vr_standalone_preset 0; vr_dynres 0; vr_eye_subimage 0; $SUBRUN; vr_eyeshot 2; wait3; toggleconsole; quit"
mkdir -p "$S/qoff" "$S/qsub" "$S/pcsub"; run qoff QVR_GLES_COMPAT=1 QVR_GLES_CLIP_CONTROL=1
SCRIPT="vr_standalone_preset 0; vr_dynres 0; $SUBRUN; vr_eyeshot 1; wait3; toggleconsole; quit"
run qsub QVR_GLES_COMPAT=1 QVR_GLES_CLIP_CONTROL=1
SCRIPT="vr_render_scale 0.9; vr_upscale 0; vr_dynres 0; $SUBRUN; vr_eyeshot 1; wait3; toggleconsole; quit"
run pcsub
python "$TREE/Misc/quakevr/gles_render_check.py" "$S/qoff" "$S/qsub" > "$S/check_subimage.txt"
python "$TREE/Misc/quakevr/gles_render_check.py" "$S/pcsub" "$S/qsub" > "$S/check_subimage_pc.txt"
grep "^eye" "$S/check_subimage.txt"
read -r _ sbDiff sbMoved sbA sbB < <(grep "^eye " "$S/check_subimage.txt")
read -r _ sbW sbH _ _ sbExact < <(grep "^eyesize " "$S/check_subimage.txt")
read -r _ pcW pcH _ _ _ < <(grep "^eyesize " "$S/check_subimage_pc.txt")
lt() { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a != "" && a >= 0 && a < b)}' && echo 1 || echo 0; }
check $(awk -v a="$eyePc" -v b="$eyeEs" 'BEGIN{exit !(a > 8 && b > 8)}' && echo 1 || echo 0) "both runs drew the eye (means ${eyePc:-?}, ${eyeEs:-?})"
check $([ "$esErrors" = "0 shader errors" ] && echo 1 || echo 0) "ES compiled every program (${esErrors:-no report})"
check $([ "$esOff" = "0" ] && echo 1 || echo 0) "ES turned no feature off for a missing extension ($esOff lines)"
check $(lt "$eyeMoved" 0.02) "eye: ${eyeMoved:-?} of the pixels moved (under 0.02)"
check $(lt "$eyeDiff" 3) "eye: ${eyeDiff:-?} mean difference per channel (under 3)"
check $(lt "$envMoved" 0.02) "envmap: ${envMoved:-?} of the texels moved (under 0.02)"
check $(lt "$envDiff" 3) "envmap: ${envDiff:-?} mean difference per channel (under 3)"
check $([ "${texDiff:--1}" = "0" ] && [ "${texN:-0}" -gt 100 ] && echo 1 || echo 0) "textures: ${texDiff:-?} of ${texN:-?} differ (none)"
check $([ "${nxOn:-0}" -gt 0 ] && [ "$nxErrors" = "0 shader errors" ] && echo 1 || echo 0) "-glesnoext: on, every program compiled (${nxErrors:-no report})"
check $(awk -v a="$nxA" -v b="$nxB" 'BEGIN{exit !(a > 8 && b > 8)}' && echo 1 || echo 0) "-glesnoext: both drew (means ${nxA:-?}, ${nxB:-?})"
check $(lt "$nxMoved" 0.0001) "-glesnoext: eye ${nxMoved:-?} of the pixels moved (under 0.0001)"
check $(lt "$nxDiff" 0.1) "-glesnoext: eye ${nxDiff:-?} mean difference per channel (under 0.1)"
check $([ "${arMany:-0}" -gt 0 ] && [ "${arCalls:-0}" -ge 50 ] && [ "${arBound:-99}" -le 10 ] && echo 1 || echo 0) "arrays: ES drew ${arCalls:-?} calls as ${arMany:-?} draws (${arAlone:-?} alone, ${arBound:-?} on the bound path; ${arSets:-no report})"
check $(echo "$arOffDraws" | grep -q "0 draws of many, 0 calls in them, 0 alone" && echo 1 || echo 0) "arrays off: none drawn from them (${arOffDraws:-no report})"
check $(awk -v a="$arA" -v b="$arB" 'BEGIN{exit !(a > 8 && b > 8)}' && echo 1 || echo 0) "arrays: both drew (means ${arA:-?}, ${arB:-?})"
check $(lt "$arMoved" 0.0001) "arrays: eye ${arMoved:-?} of the pixels moved against a draw a texture (under 0.0001)"
check $(lt "$arDiff" 0.1) "arrays: eye ${arDiff:-?} mean difference per channel (under 0.1)"
check $([ "${arTexDiff:--1}" = "0" ] && echo 1 || echo 0) "arrays: textures ${arTexDiff:-?} of ${arTexN:-?} differ (none)"
check $(awk -v a="$sbA" -v b="$sbB" 'BEGIN{exit !(a > 8 && b > 8)}' && echo 1 || echo 0) "sub-image: both drew (means ${sbA:-?}, ${sbB:-?})"
check $([ "${sbExact:--1}" = "0" ] && echo 1 || echo 0) "sub-image: the eye's part (${sbW:-?} x ${sbH:-?}) identical to the resample target's (${sbExact:-?} pixels differ)"
check $([ "${pcW:-0}" -gt "${sbW:-0}" ] && [ "${pcH:-0}" -gt "${sbH:-0}" ] && echo 1 || echo 0) "sub-image: the PC fills its image (${pcW:-?} x ${pcH:-?}, the ES part ${sbW:-?} x ${sbH:-?})"
verdict; exit $fail
