#!/bin/bash
# animrec_test.sh <agent> -- the animation recorder (vr_animrec.cpp; docs/vr-port/ANIMATIONS.md), headless:
#   1. record (the mock headset): the shotgun in the main hand, the mock swing (vr_mock_swing 1) and a shaky head
#      (vr_mock_shake 1); vr_ar_selftest record: the countdown (0.5 s), 2 s recorded through the real path (the hands'
#      state each host frame, the shot from the weapon effects, the auto pump), saved to quakevr/anims/selftest.anim and
#      read back: the frames, the main hand's path, the head and body frames agreeing, the weapons and hotspots, the shot
#      event; smoothed (under half the frames kept, the spline within the tolerances, jitter taken out) and the smoothed
#      file written and read back, its spline near the raw hands.
#   2. offline: Misc/quakevr/anim_smooth.py --check on the same recording (frames dropped, within the tolerances).
#   3. play (-flatscreenmode): vr_ar_selftest play selftest: the smoothed recording on the flat-screen hands once each
#      level, looking up (-80), looking down (80) and crouched (+flatcrouch): at full weight the hands are on it, and
#      blending in and out of the game's own pose moves them no more a frame than the recording does (1.5x + 0.5 units).
# Each run ends on its "vr_ar_selftest: N passed, M failed" line (-QuitOn); prints PASS/FAIL per check; exits 1 on a failure.
AGENT=${1:-animrec}
source "$(dirname "$0")/qvrtest.sh"
END='vr_ar_selftest: [0-9]+ passed'

log=$(bash $KIT/run.sh $AGENT -Script "map e1m1;wait60;god;vr_weapon_grip_mode 1;impulse 9;wait2;impulse 154;wait3;vr_mock_swing 1;vr_mock_shake 1;vr_ar_countdown 0.5;vr_ar_selftest record selftest;wait60;+attack;wait3;-attack;wait600;toggleconsole;quit" -QuitOn "$END" -Filter "^vr_ar_selftest" -Timeout 180 2>&1)
grep -a "^vr_ar_selftest: FAIL" <<<"$log"
expect_verdict "$log" vr_ar_selftest "record, save, read back, smooth (mock headset)"

py=$(python "$TREE/Misc/quakevr/anim_smooth.py" "$TREE/quakevr/anims/selftest.anim" --out "$TREE/scratch/animrec_test.smooth.anim" --check 2>&1)
echo "$py" | head -1
check $(grep -q "anim_smooth: PASS" <<<"$py" && echo 1 || echo 0) "anim_smooth.py: fewer frames, within the tolerances"

log=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map e1m1;wait60;god;vr_ar_play_blend 0.3;vr_ar_selftest play selftest;wait1200;toggleconsole;quit" -QuitOn "$END" -Filter "^vr_ar_selftest" -Timeout 180 2>&1)
grep -a "^vr_ar_selftest: \(FAIL\|blend\)" <<<"$log"
expect_verdict "$log" vr_ar_selftest "played on the flat-screen hands (level, up, down, crouched)"

verdict; exit $fail
