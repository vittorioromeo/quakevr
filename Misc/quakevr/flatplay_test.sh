#!/bin/bash
# flatplay_test.sh <agent> -- flatscreen play (-flatscreenmode; docs/vr-port/FLATPLAY.md), headless:
#   1. the hands are locked to the view: with vr_flat_sway, vr_flat_bob and vr_flat_turnlag 0, the drawn main weapon
#      in the camera's frame (vr_flatplay_debug 2's "flatview:" lines) stays within 0.05 units and degrees of
#      where it stands still while he strafes and turns (it was 23.7 units strafing, 7.5 degrees turning:
#      ROUND22.md, "Flatscreen hands: locked to the view"); with the defaults, it sways, bobs and trails the turn
#      within bounds and settles, and the shots the moves send stay on the crosshair (Misc/quakevr/flatplay_lock.py).
#   2. the mode is the player's own (.vr_flatscreen from his move): with the flag, "the player reloads 0 (flatscreen 1)";
#      without it, "reloads 1 (flatscreen 0)".
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=${1:-fixgame3}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"
for MODE in locked feel; do
  SET=""; [ "$MODE" = "locked" ] && SET="vr_flat_sway 0; vr_flat_bob 0; vr_flat_turnlag 0;"
  # (setpos turns noclip on, noclip off again: walking. The waits are the phases' frames: strafe, stop, turn, settle.)
  bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map e1m1; wait60; god; notarget; setpos 480 -352 88 0 0 0; noclip; impulse 2; $SET cl_yawspeed 360; wait30; vr_flatplay_debug 2; wait20; +moveleft; wait80; -moveleft; wait30; +right; wait60; -right; wait80; vr_flatplay_debug 0; toggleconsole; quit" -Filter "^flat(view|move):" > "$HERE/scratch/flatlock_$MODE.log" 2>&1
  while read -r line; do
    echo "$line"; case "$line" in FAIL*) fail=1;; esac
  done < <(python "$HERE/flatplay_lock.py" "$HERE/scratch/flatlock_$MODE.log" $MODE)
done
log=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map e1m1; wait60; vr_reload_test 0; impulse 125; wait3; toggleconsole; quit" -Filter "^reload: the player" 2>&1)
check $(echo "$log" | grep -q "reloads 0 (flatscreen 1)" && echo 1 || echo 0) "flatscreen: the player's own mode, no reloading"
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1; wait60; vr_reload_test 0; impulse 125; wait3; toggleconsole; quit" -Filter "^reload: the player" 2>&1)
check $(echo "$log" | grep -q "reloads 1 (flatscreen 0)" && echo 1 || echo 0) "in the headset: reloading, not flatscreen"
log=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map e1m1; wait60; vr_test_weaponinst 0; impulse 2; wait5; impulse 2; wait5; impulse 2; wait5; impulse 120; wait3; impulse 2; wait5; impulse 120; wait3; toggleconsole; quit" -Filter "^weaponinst: main" 2>&1)
check $(echo "$log" | grep -q "main - |" && echo "$log" | grep "^weaponinst: main" | tail -1 | grep -q "main weapon" && echo 1 || echo 0) "flatscreen: cycling reaches the empty hand and comes back"
exit $fail
