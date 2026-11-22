#!/bin/bash
# rope_pause_test.sh <agent> -- the climbable ropes and chains stand still while the game is paused (ropes5;
# Quake/vr/vr_climbrope.cpp serverFrame's worldRan, host.c Host_ServerFrame; docs/vr-port/ROPES.md, "Tests"), headless,
# flat-screen, in vrropes, the client drawing faster than the server (144 Hz, as a monitor). Prints PASS/FAIL lines and
# a verdict, exits 1 on a failure.
#   vr_rope_pause_test: the map's ropes pushed swinging; `pause` (the server's and the client's points still over a
#   second), unpaused (swinging on), the menu open (single player's pause: still), closed (swinging on).
AGENT=${1:-posebake}
QT_NAME=rope_pause
source "$(dirname "$0")/../qvrtest.sh"
out=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -QuitOn "^vr_rope_pause_test: [0-9]+ passed" -Timeout 180 \
    -Filter "^vr_rope_pause_test" -Script "vr_fixed_frames_rate 144; map vrropes; wait60; vr_rope_pause_test" 2>&1)
echo "$out" | grep -a "^vr_rope_pause_test: \(PASS\|FAIL\)"
expect_verdict "$out" "vr_rope_pause_test" "ropes stand still while paused (pause, the menu)"
verdict; exit $fail
