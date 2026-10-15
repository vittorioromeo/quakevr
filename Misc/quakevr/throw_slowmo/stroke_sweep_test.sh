#!/bin/bash
# stroke_sweep_test.sh <agent> [lob|toss] ["<cvars>"] -- stroke_sweep_plays.py's strokes (3 .. 70 cm, each in its
# real-speed time) let go of with an empty hand, vr_debug_throw 1: at full speed, and in bullet time (0.3x) made at real
# speed and 1.5, 2, 2.5 and 3.333 times slower (3.333: slowly with the world). One table (stroke_sweep_table.py): each
# throw's game-time speed and how far it goes (lob: at 45 degrees on level ground; toss: how high it rises), the
# bullet-time columns over full speed, and the gain (vr_throw_slowmo_stroke). ROUND22.md, "Bullet-time throws: the
# stroke's tempo": real speed x1.00, slowly with the world about x1.00 for the arm's strokes, no step anywhere.
NAME="$1"; KIND="${2:-lob}"; X="${3:-}"; ROOT="C:/OHWorkspace/qvr-agents/$NAME"; D="$ROOT/Misc/quakevr/throw_slowmo"
mkdir -p "$ROOT/scratch"
P="$ROOT/scratch/sweep_${KIND}_$(echo "$X" | md5sum | cut -c1-6)"
one() { # <tag> <stretch> <cvars>
    local T WAIT
    T=$(python "$D/stroke_sweep_plays.py" "${P}_$1.play" "$ROOT/Misc/quakevr" "$2" "$KIND")
    WAIT=$(python -c "print(int(($T + 1) * 90 * 1.5))")
    bash C:/OHWorkspace/qvr-kit/run.sh "$NAME" -Script "map e1m1;wait60;god;notarget;vr_fixed_frames 1;vr_gunangle 70;vr_debug_throw 1;vr_mock_grip_velocity 1;vr_bullettime_duration 600;$X;$3;vr_mock_play ${P}_$1.play;wait$WAIT;toggleconsole;quit" -Timeout 400 -Filter "THROW|^throw main|slow motion:" > "${P}_$1.log" 2>&1
}
one full 1 ""
one bt1 1 "vr_bullettime;wait10"
one bt1.5 1.5 "vr_bullettime;wait10"
one bt2 2 "vr_bullettime;wait10"
one bt2.5 2.5 "vr_bullettime;wait10"
one bt3.3 3.333 "vr_bullettime;wait10"
python "$D/stroke_sweep_table.py" "$P" "$KIND"
