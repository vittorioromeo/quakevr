#!/bin/bash
# teleport_frames_test.sh <agent> [rate] [fade] [case...]: a screenshot of every frame drawn round a teleporter's crossing
# (vr_screenshot_frames), on fixed frames at <rate> Hz (72 by default; 120: frames between the server's 72 Hz ticks),
# with the comfort fade's seconds <fade> (vr_comfort_teleport_fade; 0 by default). Each frame is compared with its
# neighbours: a frame unlike both while they are alike is a transient (a hole: the first frame after the crossing
# showed start's pentagram floor missing, its func_bossgate not sent; ROUND21.md, "A frame seen through after a
# teleporter"). Prints, per case, each frame's share of pixels changed from the last and its transient score (the share
# of pixels unlike both neighbours where the neighbours agree), the worst one, and writes the 4 frames round the
# crossing side by side to <worktree>/scratch/tpframes_<case>_<rate>.png, and each frame's mean luminance (a step of
# lighting at the crossing: the room through the gate against the room itself).
#   start    start's middle skill gate (Normal), jumped into; the pentagram's floor (a func_bossgate) beyond it
#   e1m1     e1m1's teleporter trigger_teleport (t6), jumped into
#   flush    vrteleporters' flush player gate, walked into
AGENT=${1:?agent}; RATE=${2:-72}; FADE=${3:-0}; set -- "${@:4}" # (shift 3 with fewer arguments shifted nothing: the agent was taken for a case)
CASES=${*:-start e1m1 flush}
KIT=C:/OHWorkspace/qvr-kit
HERE=$(cd "$(dirname "$0")" && pwd)
TREE=$(cd "$HERE/../../.." && pwd -W)
SHOTS=$KIT/bases/$AGENT/qbase/quakevr/screenshots
mkdir -p "$TREE/scratch"
N=$(( 14 * RATE / 72 ))
for c in $CASES; do
    case $c in
        start) S="map start;wait120;setpos 544 1330 24 0 90 0;wait10;noclip 0;wait80;vr_mock_stick off 0 0.5;wait20;+jump;wait36;-jump" ;;
        e1m1)  S="map e1m1;wait120;setpos 1312 1040 -380 0 90 0;wait10;noclip 0;wait60;vr_mock_stick off 0 0.5;wait20;+jump;wait40;-jump" ;;
        flush) S="map vrteleporters;wait60;setpos -256 576 24 0 90 0;wait5;noclip 0;wait5;vr_mock_stick off 0 1;wait12" ;;
        *) echo "unknown case $c"; continue ;;
    esac
    S="vr_fixed_frames 1;vr_fixed_frames_rate $RATE;vr_comfort_teleport_fade $FADE;developer ${DEV:-1};${EXTRA:-};god;notarget;$S;vr_screenshot_frames $N;wait20;vr_comfort_teleport_fade 0.6;toggleconsole;quit"
    START=$(date +%s)
    bash $KIT/run.sh $AGENT -Clean -Script "$S" -Filter "carried edict 1" | grep -o "carried.*" | head -1
    py -3.13 - "$SHOTS" "$START" "$TREE/scratch/tpframes_${c}_${RATE}.png" <<'PY'
import sys, os, glob
import numpy as np
from PIL import Image
shots, start, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]
fs = sorted((f for f in glob.glob(os.path.join(shots, '*.png')) if os.path.getmtime(f) >= start - 1), key=os.path.getmtime)
if len(fs) < 4:
    print('  too few screenshots:', len(fs)); sys.exit()
im = [np.asarray(Image.open(f).convert('RGB')).astype(np.int16) for f in fs]
changed = lambda a, b: np.abs(a - b).sum(2) > 48
step = [0.0] + [changed(im[i - 1], im[i]).mean() * 100 for i in range(1, len(im))]
trans = [0.0] * len(im)
for i in range(1, len(im) - 1):
    trans[i] = (changed(im[i - 1], im[i]) & changed(im[i], im[i + 1]) & ~changed(im[i - 1], im[i + 1])).mean() * 100
print('  frames', len(im), ' changed %:', ' '.join('%.1f' % s for s in step))
print('  transient %:', ' '.join('%.1f' % t for t in trans))
lum = [(a[..., 0] * 0.299 + a[..., 1] * 0.587 + a[..., 2] * 0.114).mean() for a in im]
print('  mean luminance:', ' '.join('%.1f' % l for l in lum))
w = int(np.argmax(trans))
print('  worst transient %.2f%% at frame %d' % (trans[w], w))
k = int(np.argmax(step))  # the crossing: the largest step (or the worst transient's)
k = w if trans[w] > 0.5 else k
lo = max(0, min(k - 2, len(im) - 4))
Image.fromarray(np.concatenate([im[i] for i in range(lo, lo + 4)], 0).astype(np.uint8)).save(out)
print('  frames %d-%d -> %s' % (lo, lo + 3, out))
PY
done
