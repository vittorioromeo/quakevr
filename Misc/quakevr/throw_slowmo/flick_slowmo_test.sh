#!/bin/bash
# flick_slowmo_test.sh <agent> "<cvars>" [stretch] -- flick_plays.py's throws (three forward wrist flicks, three upward
# ones, two upward with the hand drifting up, an overhand throw without and with its wrist) let go of with an empty
# hand, vr_debug_throw 1; one line a throw: its speed, how far it would go at 45 degrees on level ground and, in slow
# motion, the release's windows and the flick's share and factor. ROUND21.md, "Wrist flicks in bullet time" and "...:
# the arm tells the tempo": run it with "" (full speed), with "vr_bullettime;wait10" (bullet time at 0.3x: every flick
# as far as at full speed; the overhands as before) and with "vr_bullettime;wait10" 3.333 (made slowly with the world:
# the overhands as at full speed; a flick as gently as it was made). flick_slowmo_compare.sh runs all three, one table.
# "vr_throw_slowmo_flick 0": as before 10-07 (a flick that went 0.9 m at full speed went 7.9 m in bullet time).
NAME="$1"; ROOT="C:/OHWorkspace/qvr-agents/$NAME"; S="${3:-1}"; mkdir -p "$ROOT/scratch"
TAG="${S}_$(echo "$2" | md5sum | cut -c1-6)" # runs in parallel don't share files
OUT="$ROOT/scratch/flicks_$TAG.txt"; LOG="$ROOT/scratch/flicks_$TAG.log"
T=$(python "$ROOT/Misc/quakevr/throw_slowmo/flick_plays.py" "$OUT" "$ROOT/Misc/quakevr" "$S")
WAIT=$(python -c "print(int(($T + 1) * 90 * 1.5))")
bash C:/OHWorkspace/qvr-kit/run.sh "$NAME" -Script "map e1m1;wait60;god;notarget;vr_fixed_frames 1;vr_gunangle 70;vr_debug_throw 1;vr_mock_grip_velocity 1;vr_bullettime_duration 600;$2;vr_mock_play $OUT;wait$WAIT;toggleconsole;quit" -Timeout 400 -Filter "THROW|^throw main|slow motion:" 2>&1 > "$LOG"
python - "$LOG" <<'PY'
import sys, re
name = None; last = None
def out():
    if last: print(last)
lines = []
for l in open(sys.argv[1]):
    if lines and not re.match(r'THROW|throw main|  slow motion', l): lines[-1] = lines[-1].rstrip() + ' ' + l
    else: lines.append(l)
for l in lines:
    if l.startswith('THROW'): name = l.split()[1]
    m = re.match(r'throw main: ([\d.]+) m/s', l)
    if m and name:
        out(); v = float(m.group(1)); last = f'{name:12s} {v:6.2f} m/s  at 45 degrees {v * v / 9.81:6.2f} m'; name = None
    m = re.search(r'windows x([\d.]+).*the flick \((\d+)% of it\) x([\d.]+)', l)
    if m and last: last += f'  (windows x{m.group(1)}, flick {m.group(2)}% x{m.group(3)})'
out()
PY
