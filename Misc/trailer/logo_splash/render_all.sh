#!/usr/bin/env bash
# render_all.sh -- the whole logo splash: Blender's layers, the 2D fields, the frames, the movies.
#   bash Misc/trailer/logo_splash/render_all.sh [width=3840] [jobs=6]
# Outputs in $QVR_SPLASH_OUT (default C:\OHWorkspace\qvr-trailer\logo_splash); see README.md.
set -euo pipefail
WIDTH=${1:-3840}
JOBS=${2:-6}
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${QVR_SPLASH_OUT:-C:/OHWorkspace/qvr-trailer/logo_splash}"
BLENDER="${BLENDER:-C:/Program Files/Blender Foundation/Blender 5.2/blender.exe}"
PY="${PYTHON:-C:/Python313/python.exe}"
WORK="$OUT/work"
[ "$WIDTH" = 3840 ] || WORK="$OUT/work$WIDTH"
cd "$HERE"
[ -f "$OUT/letters.json" ] || { echo "letters"; "$PY" trace_logo.py --out "$OUT/letters.json"; }
t() { local s=$(date +%s); "$@"; echo "  ($(( $(date +%s) - s )) s)"; }
for p in stills:128 letters:64 fg:64; do
  echo "blender ${p%%:*}"
  t "$BLENDER" -b --factory-startup -P blender_scene.py -- --pass "${p%%:*}" --width "$WIDTH" --samples "${p##*:}" \
      --out "$WORK" > "$WORK.${p%%:*}.log" 2>&1
done
echo "fields"; t "$PY" fx_precompute.py --width "$WIDTH" --work "$WORK"
echo "frames"; t "$PY" composite.py --width "$WIDTH" --work "$WORK" --out "$OUT/png_$WIDTH" --jobs "$JOBS"
echo "movies"
t "$PY" encode.py --png "$OUT/png_$WIDTH" --sheet "$OUT/contact_sheet.png" --alphatest "$OUT/alpha_test.png" \
    --preview "$OUT/preview_over_checker.mp4" --prores "$OUT/logo_splash_${WIDTH}_prores4444.mov"
if [ "$WIDTH" = 3840 ]; then
  t "$PY" encode.py --png "$OUT/png_3840" --downscale "$OUT/png_1920" --to 1920
  t "$PY" encode.py --png "$OUT/png_1920" --prores "$OUT/logo_splash_1920_prores4444.mov"
fi
