#!/bin/bash
# throw_slowmo_test.sh <agent> "<cvars>" [stretch] [keyrate] -- throw_plays.py's four throws (ROUND21.md, "Throws in bullet time:
# the release's windows in real time", "Throws in bullet time: the way the controller moved"), played with
# `vr_debug_throw 1` after the cvars (e.g. "vr_bullettime;wait" for bullet time at 0.3x; add
# "vr_timescale_hand_speed 0;vr_timescale_hand_spin 0" for unslowed hands: each throw then 1/0.3 of the full-speed one,
# the same direction; "vr_throw_slowmo_real_time 0": as before; "vr_throw_slowmo_aim 0": the slowed hand's way).
# `stretch` (default 1): every motion that many times slower (3.333: the same throws made slowly in bullet time at 0.3x).
# `keyrate` (default 90): the motion's keys a second (throw_plays.py --rate; 1000: smooth between the frames, the
# throws much less dependent on where the frames fall, which moved the 90-key throws by up to 10 degrees).
NAME="$1"; ROOT="C:/OHWorkspace/qvr-agents/$NAME"; STRETCH="${3:-1}"; KEYRATE="${4:-90}"; mkdir -p "$ROOT/scratch"
OUT="$ROOT/scratch/throws_${STRETCH}_$KEYRATE.txt"
python "$ROOT/Misc/quakevr/throw_plays.py" --stretch "$STRETCH" --rate "$KEYRATE" --out "$OUT" > /dev/null
WAIT=$(python -c "print(int(600 * $STRETCH) + 60)")
bash C:/OHWorkspace/qvr-kit/run.sh "$NAME" -Script "map e1m1;wait60;god;notarget;vr_fixed_frames 1;vr_gunangle 70;vr_debug_throw 1;vr_mock_grip_velocity 1;vr_bullettime_duration 60;$2;vr_mock_play $OUT;wait$WAIT;toggleconsole;quit" -Timeout 400 -Filter "ENGINE|throw main|THROW|slow motion:|released" 2>&1 | cut -c1-140
