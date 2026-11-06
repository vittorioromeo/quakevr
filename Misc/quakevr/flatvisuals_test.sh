#!/bin/bash
# flatvisuals_test.sh <agent> [label]: flatscreen play's visuals from the headset (ROUND24.md, "Flatscreen: the headset's
# crosshair, the death view"), real path, pixels and the death view's own numbers.
#   crosshair: the shotgun in hand, the flat crosshair off, vr_crosshair 0 / 1 (dot) / 2 (laser) / 3 (soft laser), red
#              (hue 0). Each mode against 0: the red pixels it adds. Dot: a few, near the screen's middle (where shots go);
#              laser: many, from the lower right (the gun) to the middle; soft laser: fewer than the laser, as long.
#   death:     impulse 195 (dead, not gibbed; vr_death_view 2, the fade off), 120 frames for the ragdoll to fall (the view goes
#              in on its first drawn frame); vr_flat_death_vomit 0, vr_death_view_status: immersive, not Vomit Mode, the
#              camera at the head's eyes, no roll. vr_flat_death_vomit 1 (Vomit Mode off in its own setting): Vomit Mode,
#              the camera at the eyes. Then vr_death_view 1 (Third Person: Quake's view on the floor again; not forced
#              into Vomit Mode): the frames differ.
# Ends on its own line (-QuitOn "flatvisuals: shots done"); flatvisuals_check.py prints `flatvisuals: N passed, M failed`.
AGENT=${1:-embed5}; LABEL=${2:-now}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
HERE=$(dirname "$0"); mkdir -p "$HERE/../../scratch"
S="map e1m1; god; notarget; viewsize 120; crosshair 0; vr_flat_bob 0; vr_flat_sway 0; vr_crosshair_hue 0; vr_crosshair_size 1"
S="$S; vr_crosshair_alpha 1; vr_crosshair_depth 0; vr_test_give_wid 4; impulse 149; wait120"
S="$S; vr_crosshair 0; wait3; screenshot; vr_crosshair 1; wait3; screenshot; vr_crosshair 2; wait3; screenshot"
S="$S; vr_crosshair 3; wait3; screenshot; vr_crosshair 0"
S="$S; vr_death_view 2; vr_death_view_vomit 0; vr_flat_death_vomit 0; vr_death_view_fade 0; developer 1; impulse 195; wait120"
S="$S; vr_death_view_status; screenshot; vr_flat_death_vomit 1; wait3; vr_death_view_status; vr_death_view 1; wait3; vr_death_view_status; screenshot"
S="$S; echo flatvisuals: shots done; toggleconsole; quit"
LOG="$HERE/../../scratch/flatvisuals_$LABEL.log"
bash "$KIT/run.sh" "$AGENT" -ExtraArgs "-flatscreenmode" -Clean -Script "$S" -QuitOn "flatvisuals: shots done" \
    -Filter "deathview|flatvisuals|ENGINE|TIMEOUT|composed|exit=" -Timeout 300 -Out "flatvisuals_$LABEL.png" > "$LOG" 2>&1
grep -a -E "ENGINE|TIMEOUT" "$LOG" | cut -c1-160
SHOTS=$(grep -a -h 'full size in' "$LOG" | sed 's/.*full size in //' | tr -d '\r' | tail -1)
python "$HERE/flatvisuals_check.py" "$SHOTS" "$LOG"
