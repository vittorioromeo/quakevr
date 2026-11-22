#!/usr/bin/env bash
# humrepl2_test.sh: the recolours' masks (docs/vr-port/ARMED_MONSTERS.md, "Recolours by weapon"): in vrtesthall a row of
# each armed humanoid type lined up (the Texture Studio's Line Up, 30 frames each to be drawn: the masks are made as a
# skin is first drawn), a stroke painted where the view's centre meets the last row (one undo step, texels changed) and
# undone, then vr_humstudio_check: every mask made, none dyeing a texel on the flesh's or blood's palette rows (7, 4:
# those texels are never changed, the shader dyes only a mask's clothes and pack), none dyeing nothing. The engine's run
# ends on the check's line; this prints "humrepl2_test: PASS" or "FAIL" from the three.
set -u
K=C:/OHWorkspace/qvr-kit
W=$(cd "$(dirname "$0")/../.." && pwd)
NAME=$(basename "$W")
S="sv_random_seed 7;map vrtesthall;wait60;god;notarget"
for k in 0 1 2 3; do
    S="$S;vr_humstudio_lineup $k;wait30"
done
S="$S;vr_humstudio_paint 1;vr_humstudio_class 1;vr_humstudio_brush 6;vr_humstudio_stroke 1 2;vr_humstudio_stroke 0"
S="$S;vr_humstudio_undo;vr_humstudio_paint 0;vr_humstudio_check 5;toggleconsole;quit"
OUT=$(bash "$K/run.sh" "$NAME" -Script "$S" -Filter "humstudio|humlineup: row|Host_Error|SV_Error" -QuitOn "humstudio_check: [0-9]+ masks" -Timeout 180 2>&1)
echo "$OUT" | grep -v "skin 0 "
V=FAIL
if echo "$OUT" | grep -q "[1-9][0-9]* changed, undo steps 1" && echo "$OUT" | grep -q "undone (0 left)" &&
    echo "$OUT" | grep -q "on flesh, 0 dyeing nothing: PASS"; then
    V=PASS
fi
echo "humrepl2_test: $V"
