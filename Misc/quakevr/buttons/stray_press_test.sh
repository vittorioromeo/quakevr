#!/bin/bash
# stray_press_test.sh <agent> [run.sh options] -- no wall button pressed by a map load, a game load or a hand at rest
# (ROUND21.md, "A far button pressed at a map load"). Loads vrstart2 and vrstart 5 times each with the mock hands at
# rest, then a save made on vrstart2's pavilion with the off hand held up beside the Turning button 20 times (before the
# fix each of those loads pressed it: the engine's line from the hand to its muzzle, left at the world's origin, crossed
# it), and finally presses Turning with the off hand right after a load (a real press must still work).
# Expected: "stray presses 0", "real presses 1", "long lines 0" (vr_debug_wallbuttons 1 prints each press with the
# hand's position and its distance from the button and the player; developer 1 a hand's line to its muzzle that is
# longer than any weapon, refused).
A=${1:?agent}
shift
K=${KIT:-C:/OHWorkspace/qvr-kit}
W=C:/OHWorkspace/qvr-agents/$A
python - "$W/quakevr/scratch_straytest.cfg" <<'PY'
import sys
out = ['alias w10 "wait;wait;wait;wait;wait;wait;wait;wait;wait;wait"', 'developer 1', 'vr_debug_wallbuttons 1']
def w(n): return ['w10'] * (n // 10) + ['wait'] * (n % 10)
for i in range(5):
    out += ['echo LOAD map vrstart2 %d' % i, 'map vrstart2'] + w(60) + ['echo LOAD map vrstart %d' % i, 'map vrstart'] + w(60)
out += ['map vrstart2'] + w(100) + ['setpos -272 -30 160 0 90 0'] + w(20) + ['vr_mock_hand_to off -272 10 197'] + w(30)
out += ['save straytest'] + w(5)
for i in range(20):
    out += ['echo LOAD save %d' % i, 'load straytest'] + w(60)
out += ['echo REAL', 'setpos -228 -30 160 0 90 0'] + w(5) + ['vr_mock_hand_to off -228 5 168'] + w(40)
out += ['echo LOAD done', 'toggleconsole', 'quit']
open(sys.argv[1], 'w', newline='\n').write('\n'.join(out) + '\n')
PY
bash "$K/run.sh" "$A" -Timeout 300 "$@" -Script "exec scratch_straytest.cfg" \
    -Filter "^LOAD|^REAL|button: \*|button:   hand|units long|ENGINE|TIMEOUT" > "$W/scratch/stray_press_test.txt" 2>&1
awk '/^REAL/{real=1} /pressed by/{if(real) r++; else s++; print} /units long/{l++} /ENGINE|TIMEOUT/{print}
     /^LOAD/{n++} END{printf "loads %d, stray presses %d, real presses %d, long lines %d\n", n-1, s, r, l}' \
    "$W/scratch/stray_press_test.txt"
