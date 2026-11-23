#!/bin/bash
# rope_flat_test.sh <agent> -- flatscreen play's climbable ropes (-flatscreenmode; vr_flat_rope; Quake/vr/vr_climb.cpp
# flatRope, flatRopeFrame; docs/vr-port/ROPES.md, "Climbing (flat-screen)"), headless, in vrropes. Prints PASS/FAIL lines
# and a verdict, exits 1 on a failure.
#   vr_rope_flat_test: a rope hung near the floor walked into with forward held (taken), forward held looking level
#   pumps its swing (ropes9: to near the cap, no climb), climbed with forward looking up (the feet up 80, hand over hand,
#   both hands drawn on it), climbed down with back looking down (off it standing; ropes9: past vr_flat_rope_climb_pitch
#   the keys climb); then rope to rope on the swinging row: set in the air by its first rope
#   (taken), facing across the row the strafe key pumps the swing towards the second, jump at
#   the swing's forward peak lets go, the second caught in the air. (Climbing stamina off: the pump takes a while.)
# rope_flat_test.sh <agent> pole (ropes7): a wooden pole standing there instead: walked into, up (looking level: a pole
#   is only climbed), down, it never moved.
# rope_flat_test.sh <agent> strung (ropes7): a rope strung level overhead: jumped up to, hand over hand along it both
#   ways (the forward and back keys), the legs hanging, dropped from (crouch), jumped off.
# rope_flat_test.sh <agent> strungpole (ropes8a): the same on a level pole (it never sags).
# rope_flat_test.sh <agent> chain (ropes8b): a chain walked into and climbed (it clinks), held still 2 s (no clink).
# (ropes8b) Every mode hanging still checks the hip holsters stay on the hips; strung, the hands overhand under it.
# `vr_rope_flat_test <mode> shots` (in the console) screenshots the body from outside while hanging.
# rope_flat_test.sh <agent> wall (ropes8e): vr_flat_rope 0, walked into an upright pole: stopped by its boxes, not taken.
# rope_flat_test.sh <agent> wallvr (ropes8e): the same with the mock headset (VR play: forward along the view).
# rope_flat_test.sh <agent> polejump (ropes10): up a pole 300 units, jump: a normal jump up and pushed off it
#   (vr_pole_jump, vr_pole_jump_push), a free fall to the floor (no frame held back), not taken again in the air.
# rope_flat_test.sh <agent> slantjump (ropes10): the same off the upper side of a pole slanted 27 degrees.
# rope_flat_test.sh <agent> strungside (ropes10): the level pole faced across, the strafe keys along it: the hands keep
#   their sides (the right palm never left of the left one), shuffling.
# rope_flat_test.sh <agent> ropejump (ropes13): up a still rope, jump: straight up, not taken again in the air (he never
#   leaves its reach: the retake time alone took it again at the jump's top); landed, walked into again: taken.
# rope_flat_test.sh <agent> slide|poleslide (ropes13): up the rope / pole 300 units, crouch held: down to the floor at
#   vr_flat_rope_slide_speed (80% at least), no hand over hand, its scrape played and stopped.
# rope_flat_test.sh <agent> strungcrouch (ropes13): strungpole dropped from with crouch instead of the move-down key.
# (ropes13) The thumbs: round a rope or chain (vr_rope_thumb_wrap), along a pole's side.
AGENT=${1:-posebake}
MODE=${2:-}
QT_NAME=rope_flat${MODE:+_$MODE}
source "$(dirname "$0")/../qvrtest.sh"
FLAT=(-ExtraArgs "-flatscreenmode")
if [ "$MODE" = wallvr ]; then FLAT=(); MODE=wall; fi
out=$(bash $KIT/run.sh $AGENT "${FLAT[@]}" -QuitOn "^vr_rope_flat_test: [0-9]+ passed" -Timeout 240 \
    -Filter "^vr_rope_flat_test" -Script "map vrropes; wait60; god; notarget; vr_climb_stamina 0; vr_rope_flat_test $MODE" 2>&1)
echo "$out" | grep -a "^vr_rope_flat_test: \(PASS\|FAIL\)"
expect_verdict "$out" "vr_rope_flat_test" "flat ropes ${MODE:-rope}: walked into, up, down, rope to rope"
verdict; exit $fail
