#!/bin/bash
# toss_catch_test.sh <agent> [toss cm] ["<cvars>"] -- the axe tossed up from the main hand and caught again
# (toss_catch_plays.py), the grip closed 0.05 .. 0.4 game seconds after the release; at full speed and in bullet time
# (0.3x, the toss at real speed). One line a trial: the throw's speed and whether the hand took it back (developer 1's
# "taken into the main hand", how far from its handle). ROUND22.md, "Mid-air catches".
NAME="$1"; CM="${2:-8}"; X="${3:-}"; ROOT="C:/OHWorkspace/qvr-agents/$NAME"; D="$ROOT/Misc/quakevr/throw_slowmo"
mkdir -p "$ROOT/scratch"
P="$ROOT/scratch/tosscatch_${CM}_$(echo "$X" | md5sum | cut -c1-6)"
DELAYS="0.05 0.1 0.15 0.2 0.25 0.3 0.4"
one() { # <tag> <slow> <cvars>
    local T WAIT
    T=$(python "$D/toss_catch_plays.py" "${P}_$1.play" "$ROOT/Misc/quakevr" "$2" "$CM" $DELAYS)
    WAIT=$(python -c "print(int(($T + 1) * 90 * 1.5))")
    bash C:/OHWorkspace/qvr-kit/run.sh "$NAME" -Script "map vrfiringrange;wait60;god;notarget;developer 1;vr_fixed_frames 1;vr_weapon_grip_mode 0;vr_controller_legacy_pose 0;vr_debug_throw 1;vr_mock_grip_velocity 1;vr_bullettime_duration 600;$X;$3;vr_mock_play ${P}_$1.play;wait$WAIT;toggleconsole;quit" -Timeout 400 -Filter "^TRIAL|^CHECK|taken into the main|^throw main" > "${P}_$1.log" 2>&1
    echo "== $1"
    python "$D/toss_catch_table.py" "${P}_$1.log"
}
one full 1 ""
one bt 3.333 "vr_bullettime;wait10"
