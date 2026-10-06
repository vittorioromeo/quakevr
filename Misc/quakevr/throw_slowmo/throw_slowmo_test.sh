#!/bin/bash
# throw_slowmo_test.sh <agent> "<cvars>" -- throw_plays.py's four throws (ROUND21.md, "Throws in bullet time: the
# release's windows in real time"), played with `vr_debug_throw 1` after the cvars (e.g. "vr_bullettime;wait" for bullet
# time at 0.3x; add "vr_timescale_hand_speed 0;vr_timescale_hand_spin 0" for unslowed hands: each throw then 1/0.3 of
# the full-speed one, the same direction; "vr_throw_slowmo_real_time 0": as before).
NAME="$1"; ROOT="C:/OHWorkspace/qvr-agents/$NAME"; mkdir -p "$ROOT/scratch"
python "$ROOT/Misc/quakevr/throw_plays.py" --out "$ROOT/scratch/throws.txt" > /dev/null
bash C:/OHWorkspace/qvr-kit/run.sh "$NAME" -Script "map e1m1;wait60;god;notarget;vr_fixed_frames 1;vr_gunangle 70;vr_debug_throw 1;vr_mock_grip_velocity 1;vr_bullettime_duration 30;$2;vr_mock_play $ROOT/scratch/throws.txt;wait600;toggleconsole;quit" -Timeout 300 -Filter "ENGINE|throw main" 2>&1 | cut -c1-120
