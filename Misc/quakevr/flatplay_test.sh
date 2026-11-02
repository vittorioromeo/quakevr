#!/bin/bash
# flatplay_test.sh <agent> -- flatscreen play (-flatscreenmode; docs/vr-port/FLATPLAY.md), headless:
#   1. the hands are locked to the view: with vr_flat_sway, vr_flat_bob and vr_flat_turnlag 0, the drawn main weapon
#      in the camera's frame (vr_flatplay_debug 2's "flatview:" lines) stays within 0.05 units and degrees of
#      where it stands still while he strafes and turns (it was 23.7 units strafing, 7.5 degrees turning:
#      ROUND22.md, "Flatscreen hands: locked to the view"); with the defaults, it sways, bobs and trails the turn
#      within bounds and settles, and the shots the moves send stay on the crosshair (Misc/quakevr/flatplay_lock.py).
#   2. the mode is the player's own (.vr_flatscreen from his move): with the flag, "the player reloads 0 (flatscreen 1)";
#      without it, "reloads 1 (flatscreen 0)".
#   3. a punch from the fire buttons: +attack (what LMB is bound to) punches with the main hand and +offhandattack
#      (RMB) with the off hand when that hand holds a weapon a swing strikes with, and the punch deals the game's own
#      melee damage to what stands within the fist's arc (vr_test_spawn 5, vr_test_spawn_dist 16, and vr_test_hurt 1
#      read twice: what its line finds before its own damage is what the punch left the monster at). A gun's fire
#      button is its own shot (the shotgun's clip drops) and never a punch; a chainsaw's trigger runs its chain; and a
#      hand that already punched this frame is joined by the other only when both hold a swinging weapon.
#   4. the drawn hand stays on its arm (the probe's "flatarm:" lines, vr_flatplay_debug 2): no more than 0.6 units of
#      the hand past what the arm can hold it at, and no more than 60 mm off its wrist — through a punch and at a look
#      straight up (it was 14 units past, 401 mm off, at the off hand's punch: the hand hung off the wrist).
#   5. none of it runs in the headset: vr_flat_punch says the mode is not running and "flatarm:" prints nothing.
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=${1:-fixgame3}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
source "$(dirname "$0")/qvrtest.sh"
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
# 3 and 4: both hands' fire buttons punch, deal the damage, and stay on the arm. (The waits are the phases' frames at
# the mock's 72 a second: a punch is 0.13 + 0.12 + 0.22 s, so its fist is at its fastest around wait14 after the press.
# The screenshots are the fist at its fastest, and the hands after the look straight up.)
log=$HERE/scratch/flatpunch.log
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map e1m1; wait60; god; notarget; developer 1; vr_test_spawn 5; vr_test_spawn_dist 16; impulse 241; wait10; impulse 152; wait15; vr_flatplay_debug 2; wait10; vr_test_hurt 1; wait3; +attack; wait14; screenshot; vr_test_hurt 1; wait8; -attack; impulse 172; wait15; +offhandattack; wait14; screenshot; vr_test_hurt 1; wait8; -offhandattack; setpos 480 -352 88 -89 0 0; wait20; vr_flatplay_status; toggleconsole; quit" -Filter "flatarm:|test hurt:|punches:|flatplay: " -Out scratch/flatpunch.png > "$log" 2>&1
read -r okmain okoff okarm probes past mm <<<"$(grep -a "" "$log" | awk '
/test hurt:/ { if (match($0, /health [0-9.]+ -> [0-9.]+/)) { split(substr($0, RSTART + 7, RLENGTH - 7), b, " ->"); if (seen) g[++n] = prev - b[1]; prev = b[2]; seen = 1 } }
/flatarm:/ { for (i = 1; i <= NF; i++) if ($i == "past") { v = $(i + 2) + 0; if (v > past) past = v } if (match($0, /\(([0-9]+) mm\)/)) { m = substr($0, RSTART + 1, RLENGTH - 4) + 0; if (m > mm) mm = m } }
END { printf "%d %d %d %d %.2f %.0f\n", (n > 0 && g[1] > 0), (n > 1 && g[2] > 0), (past <= 0.6 && mm <= 60), n, past, mm }')"
echo "  flatarm worst: $past units of the hand past its arm, $mm mm off its wrist; the punches' damage: $probes health probes ($log)"
check $okmain "flatscreen melee: +attack (LMB) punches with the axe in the main hand and hurts the monster in front of it"
check $okoff "flatscreen melee: +offhandattack (RMB) punches with the axe in the off hand and hurts the monster in front of it"
check $okarm "flatscreen arm: the drawn hand stays on its arm through a punch and looking straight up"
log=$HERE/scratch/flatgate.log
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "map e1m1; wait60; god; notarget; developer 1; impulse 152; impulse 174; wait15; vr_flatplay_debug 2; wait10; vr_flatplay_status; +attack; +offhandattack; wait20; -attack; -offhandattack; wait10; vr_flatplay_status; impulse 154; wait15; vr_test_weaponinst 0; wait3; +attack; wait10; -attack; wait5; impulse 120; wait3; vr_flatplay_status; impulse 164; wait15; +attack; wait10; -attack; wait5; vr_flatplay_status; vr_flat_punch main; wait3; vr_flat_punch off; wait3; toggleconsole; quit" -Filter "punches:|weaponinst:|vr_flat_punch:|flatplay: " > "$log" 2>&1
counts=$(grep -a "punches: the main hand" "$log" | grep -ao "main hand [0-9]*, the off hand [0-9]*" | awk '{ print $3 + 0, $7 + 0 }')
read -r s1m s1o <<<"$(echo "$counts" | sed -n 1p)"; read -r s2m s2o <<<"$(echo "$counts" | sed -n 2p)"
read -r s3m s3o <<<"$(echo "$counts" | sed -n 3p)"; read -r s4m s4o <<<"$(echo "$counts" | sed -n 4p)"
clips=$(sed -n 's/.*main weapon[^|]*clip \([0-9]*\).*/\1/p' "$log")
c1=$(echo "$clips" | sed -n 1p); c2=$(echo "$clips" | sed -n 2p)
echo "  punches after each step: $s1m/$s1o then $s2m/$s2o then $s3m/$s3o then $s4m/$s4o; the shotgun's clip $c1 then $c2 ($log)"
check $([ "${s2m:-0}" -gt "${s1m:-0}" ] && [ "${s2o:-0}" = "${s1o:-0}" ] && echo 1 || echo 0) "flatscreen melee: the other hand's fire button does not disturb a hand that already punched, unless both hold a swinging weapon"
check $([ -n "$c1" ] && [ -n "$c2" ] && [ "$c1" -gt "$c2" ] && [ "$s3m" = "$s2m" ] && [ "$s3o" = "$s2o" ] && echo 1 || echo 0) "flatscreen melee: a gun's fire button is its own shot (the shotgun's clip drops) and not a punch"
check $([ "$s4m" = "$s3m" ] && [ "$s4o" = "$s3o" ] && grep -q "does not strike with" "$log" && echo 1 || echo 0) "flatscreen melee: a chainsaw's trigger runs its chain, and vr_flat_punch says so"
# 5: in the headset none of this is running.
log=$HERE/scratch/flatvr.log
bash $KIT/run.sh $AGENT -Script "map e1m1; wait60; developer 1; vr_flatplay_debug 2; wait10; vr_flat_punch main; wait3; vr_flat_punch off; wait5; +attack; wait10; -attack; wait10; vr_flatplay_status; toggleconsole; quit" -Filter "vr_flat_punch:|flatarm:|flatscreen play|punches:" > "$log" 2>&1
check $(grep -q "vr_flat_punch: flatscreen play is not running" "$log" && ! grep -q "flatarm:" "$log" && grep -q "the mode off" "$log" && echo 1 || echo 0) "in the headset: vr_flat_punch does nothing, the arm probe prints nothing, the mode is off"
verdict
exit $fail
