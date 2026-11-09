#!/usr/bin/env bash
# flatlook_test.sh <agent> -- flatscreen play's look (vr_flatlook.cpp) is flat-only (ROUND24.md, "Flat-screen look").
# 1. The mock headset, the game paused: the eyes' images with the flat look's settings off and at their extremes are the
#    same (one frame apart, nothing moving; at most a stray pixel off by 2, as two shots of the same view can be), and
#    the look ran no pass (vr_flatlook_info: passes 0).
# 2. -flatscreenmode: the look runs (passes > 0) and changes the window's view (exposure 2: the view brighter).
# Ends with "flatlook: N passed, M failed" (each run quits itself). Needs py -3.13 with Pillow.
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
WT=C:/OHWorkspace/qvr-agents/$AGENT
OFF="vr_flat_grade 0;vr_flat_vignette 0;vr_flat_bloom 0"
BRIGHT="vr_flat_grade 1;vr_flat_grade_exposure 2;vr_flat_grade_contrast 1;vr_flat_grade_shadows 0;vr_flat_grade_warmth 0;vr_flat_grade_saturation 1"
MAX="vr_flat_grade 1;vr_flat_grade_exposure 2;vr_flat_grade_contrast 2;vr_flat_grade_warmth 1;vr_flat_grade_saturation 2;vr_flat_grade_shadows -1;vr_flat_vignette 1;vr_flat_vignette_size 1;vr_flat_bloom 1.5"
# compare <a.png> <b.png>: "<pixels differing by more than 2> <mean luma of b over a>"
compare() {
  py -3.13 - "$1" "$2" <<'EOF'
import sys
from PIL import Image, ImageChops, ImageStat
try:
    a, b = (Image.open(p).convert('RGB') for p in sys.argv[1:3])
    big = sum(ImageChops.difference(a, b).convert('L').point(lambda v: 255 if v > 2 else 0).histogram()[255:])
    la, lb = (ImageStat.Stat(i.convert('L')).mean[0] for i in (a, b))
    print(big, '%.3f' % (lb / max(la, 0.001)))
except Exception:
    print(-1, 0)
EOF
}

# 1. VR: eyeshots (the worktree's quakevr/eyeshots) with the look off, then at its extremes.
OUT=$(bash $KIT/run.sh "$AGENT" -Filter "vr_flatlook_info|Wrote eyeshots|exit=|TIMEOUT|ENGINE" -Script \
  "map start;wait30;pause;wait5;$OFF;wait2;vr_eyeshot 1;wait3;$MAX;wait2;vr_eyeshot 1;wait3;vr_flatlook_info;toggleconsole;quit")
grep -E "flatlook_info|TIMEOUT|ENGINE" <<<"$OUT"
SHOTS=$(grep -o "eyeshots/start_[0-9]*_L" <<<"$OUT" | sort -u)
A=$(sed -n 1p <<<"$SHOTS")
B=$(sed -n 2p <<<"$SHOTS")
read -r BIG RATIO <<<"$(compare "$WT/quakevr/$A.png" "$WT/quakevr/$B.png")"
check $([ -n "$B" ] && [ "$BIG" = 0 ] && echo 1 || echo 0) "VR: the eyes unchanged by the flat look's settings ($A vs $B: $BIG pixels off by more than 2)"
check $(grep -q "vr_flatlook_info: passes 0, active 0, flatscreen play 0" <<<"$OUT" && echo 1 || echo 0) "VR: no flat look pass"

# 2. Flat: the window's view with the look off, then brightened by its exposure alone.
OUT=$(bash $KIT/run.sh "$AGENT" -Clean -ExtraArgs "-flatscreenmode" -Filter "vr_flatlook_info|Wrote screenshots|exit=|TIMEOUT|ENGINE" -Script \
  "map start;wait30;$OFF;wait2;screenshot;$BRIGHT;wait3;screenshot;wait2;vr_flatlook_info;toggleconsole;quit")
grep -E "flatlook_info|TIMEOUT|ENGINE" <<<"$OUT"
check $(grep -Eq "vr_flatlook_info: passes [1-9][0-9]*, active 1, flatscreen play 1" <<<"$OUT" && echo 1 || echo 0) "flat: the look runs"
SHOTS=$(grep -o "screenshots/start_[0-9_-]*\.png" <<<"$OUT")
A=$(sed -n 1p <<<"$SHOTS")
B=$(sed -n 2p <<<"$SHOTS")
read -r BIG RATIO <<<"$(compare "$WT/quakevr/$A" "$WT/quakevr/$B")"
check $(py -3.13 -c "print(1 if $RATIO > 1.3 else 0)") "flat: the look brightens the view at exposure 2 (luma x$RATIO)"
verdict
exit $fail
