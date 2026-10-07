#!/bin/bash
# flick_slowmo_test.sh <agent> "<cvars>" [stretch] -- flick_plays.py's throws (three wrist flicks, an overhand throw
# without and with its wrist) let go of with an empty hand, vr_debug_throw 1; one line a throw: its speed and how far it
# would go at 45 degrees on level ground. ROUND21.md, "Wrist flicks in bullet time": run it with "" (full speed), with
# "vr_bullettime;wait10" (bullet time at 0.3x: the flicks within about 1.5 times as far as at full speed; the overhand
# without its wrist as before) and with "vr_bullettime;wait10" 3.333 (made slowly with the world: as at full speed).
# "vr_throw_slowmo_flick 0": as before (a flick that went 0.9 m at full speed went 7.9 m in bullet time).
NAME="$1"; ROOT="C:/OHWorkspace/qvr-agents/$NAME"; S="${3:-1}"; mkdir -p "$ROOT/scratch"
OUT="$ROOT/scratch/flicks_$S.txt"; LOG="$ROOT/scratch/flicks_$S.log"
T=$(python "$ROOT/Misc/quakevr/throw_slowmo/flick_plays.py" "$OUT" "$ROOT/Misc/quakevr" "$S")
WAIT=$(python -c "print(int(($T + 1) * 90 * 1.5))")
bash C:/OHWorkspace/qvr-kit/run.sh "$NAME" -Script "map e1m1;wait60;god;notarget;vr_fixed_frames 1;vr_gunangle 70;vr_debug_throw 1;vr_mock_grip_velocity 1;vr_bullettime_duration 600;$2;vr_mock_play $OUT;wait$WAIT;toggleconsole;quit" -Timeout 400 -Filter "THROW|^throw main" 2>&1 > "$LOG"
python - "$LOG" <<'PY'
import sys, re
name = None
for l in open(sys.argv[1]):
    if l.startswith('THROW'): name = l.split()[1]
    m = re.match(r'throw main: ([\d.]+) m/s', l)
    if m and name:
        v = float(m.group(1)); print(f'{name:12s} {v:6.2f} m/s  at 45 degrees {v * v / 9.81:6.2f} m'); name = None
PY
