#!/bin/bash
# flatplay_test.sh <agent> -- flatscreen play (-flatscreenmode; docs/vr-port/FLATPLAY.md), headless:
#   1. the arms' walking lag is back along the way he moves, whatever way he faces: flying forward (noclip) facing yaw 0
#      and yaw 90, the main hand's offset from the head across the view stays within 1.5 units of where it stands still
#      (vr_flatplay_debug's lines; it was ~11 units sideways at yaw 90).
#   2. the mode is the player's own (.vr_flatscreen from his move): with the flag, "the player reloads 0 (flatscreen 1)";
#      without it, "reloads 1 (flatscreen 0)".
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=${1:-fixgame3}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
for Y in 0 90; do
  log=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map e1m1; wait60; god; notarget; noclip; setpos 480 -352 120 0 $Y 0; wait40; vr_flatplay_debug 1; wait35; vr_flatplay_debug 0; +forward; wait90; vr_flatplay_debug 1; wait35; vr_flatplay_debug 0; -forward; wait5; toggleconsole; quit" -Filter "^flatplay: aim" 2>&1)
  res=$(echo "$log" | python -c "
import sys, re
rows = [(float(m.group(1)), float(m.group(2)), float(m.group(3))) for m in
        re.finditer(r'speed (\d+) .*?main \(([-\d.]+), ([-\d.]+),', sys.stdin.read())]
yaw = $Y
side = lambda r: r[2] if yaw == 0 else r[1]  # (across the view: world y facing yaw 0, x facing 90)
still = [side(r) for r in rows if r[0] == 0]
moving = [side(r) for r in rows if r[0] > 250]
ok = bool(still) and bool(moving) and max(abs(m - still[0]) for m in moving) <= 1.5
print(('1' if ok else '0') + ' across %s standing, %s moving' % (still[:1], [round(m, 1) for m in moving]))
")
  check ${res%% *} "yaw $Y: the main hand ${res#* }"
done
log=$(bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map e1m1; wait60; vr_reload_test 0; impulse 125; wait3; toggleconsole; quit" -Filter "^reload: the player" 2>&1)
check $(echo "$log" | grep -q "reloads 0 (flatscreen 1)" && echo 1 || echo 0) "flatscreen: the player's own mode, no reloading"
log=$(bash $KIT/run.sh $AGENT -Script "map e1m1; wait60; vr_reload_test 0; impulse 125; wait3; toggleconsole; quit" -Filter "^reload: the player" 2>&1)
check $(echo "$log" | grep -q "reloads 1 (flatscreen 0)" && echo 1 || echo 0) "in the headset: reloading, not flatscreen"
exit $fail
