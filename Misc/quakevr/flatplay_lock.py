"""flatplay_lock.py <log> <mode>: flatplay_test.sh's analysis of vr_flatplay_debug 2's lines.

The log's "flatview:" lines are the drawn main weapon in the camera's frame, one a frame, while the player strafes and
then turns (the script in flatplay_test.sh); "flatmove:" lines are the main hand's shot as each move sends it, against
the view the move carries.

mode "locked" (vr_flat_sway, vr_flat_bob and vr_flat_turnlag 0): the drawn weapon never moves off its place in the view
(0.05 units, 0.05 degrees), strafing or turning.
mode "feel" (the defaults): it moves (the sway, the bob, the turn's lag) but within bounds (3 units, 4.5 degrees), and is
back within 0.05 degrees of the view a second after the turn; the shots stay on the crosshair (0.1 degrees) throughout.
Prints "PASS ..." or "FAIL ..." lines.
"""
import math
import re
import sys

text = open(sys.argv[1], encoding='utf-8', errors='replace').read()
mode = sys.argv[2]
rows = [[float(x) for x in m.groups()] for m in re.finditer(
    r'flatview: t ([-\d.]+) local ([-\d.]+) ([-\d.]+) ([-\d.]+) yaw ([-\d.]+) pitch ([-\d.]+) view ([-\d.]+) ([-\d.]+) '
    r'speed (\d+)', text)]
shots = [float(m.group(1)) for m in re.finditer(r'flatmove: .*shot off the view ([\d.]+)', text)]
if len(rows) < 100:
    print('FAIL flatscreen %s: %d flatview lines (want 100 or more)' % (mode, len(rows)))
    sys.exit(0)

ref = rows[3]
strafe, turn, after = [], [], []
turned = False
for p, r in zip(rows, rows[1:]):
    dyaw = abs(((r[7] - p[7]) + 180) % 360 - 180)
    if dyaw > 0.5:
        turn.append(r)
        turned = True
    elif turned:
        after.append(r)
    elif r[8] > 100:
        strafe.append(r)


def worst(rs):
    return (max(math.dist(r[1:4], ref[1:4]) for r in rs), max(abs(r[4] - ref[4]) for r in rs)) if rs else (-1, -1)


ws, wt = worst(strafe), worst(turn)
say = 'strafing %d frames: %.3f units %.3f deg; turning %d frames: %.3f units %.3f deg' % (
    len(strafe), ws[0], ws[1], len(turn), wt[0], wt[1])
if not strafe or not turn or not after:
    print('FAIL flatscreen %s: a phase is missing (%s, %d after)' % (mode, say, len(after)))
elif mode == 'locked':
    ok = max(ws[0], wt[0]) <= 0.05 and max(ws[1], wt[1]) <= 0.05
    print('%s flatscreen hands locked to the view (sway, bob, turn lag 0): %s' % ('PASS' if ok else 'FAIL', say))
else:
    settled = abs(after[min(len(after) - 1, 71)][4] - ref[4])
    ok = 0.2 <= max(ws[0], wt[0]) <= 3.0 and 0.5 <= wt[1] <= 4.5 and settled <= 0.05
    print('%s flatscreen hands sway, bob and trail a turn, then settle: %s; %.3f deg off a second after' % (
        'PASS' if ok else 'FAIL', say, settled))
    off = max(shots) if shots else -1
    print('%s flatscreen shots on the crosshair with the sway on: %d moves, at most %.4f deg off the view' % (
        'PASS' if shots and off <= 0.1 else 'FAIL', len(shots), off))
