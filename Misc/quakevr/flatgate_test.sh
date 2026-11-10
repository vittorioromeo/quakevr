#!/bin/bash
# flatgate_test.sh <agent> -- flat-screen walks through teleporters (docs/vr-port/FLATPLAY.md, "Teleporters"), headless.
#   seamless  vr_flatgate_test 1 (QC: pitch, yaw by the gate's turn alone, walking on the way he looks, body, aim), and
#             from the same run's flatview lines (vr_flatplay_debug 2): each frame's view step and the gun's step in the
#             camera's frame, so a one-frame jump of the look or the gun at the crossing shows as a number.
#   turning   the mouse's turn stands in as +right held through the gate: the step at the crossing is the gate's turn plus
#             a frame's turn, not the server's older angle.
#   plain     vr_portals_walk 0: Quake's teleport still sets the view (level), it is not meant to be seamless.
#   level     a map changed to right after a crossing: the level's start, level.
# Prints PASS/FAIL; exits 1 on a failure. Ends with `flatgate: N passed, M failed` (the script's own tally).
AGENT=${1:?agent}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch; mkdir -p "$S"
QT_NAME=flatgate_sh
FLAT="-flatscreenmode"
P="vr_flat_holsters 0; map vrteleporters; wait60; developer 1; god; notarget; r_norefresh 0; vr_mock_eye_size 256; impulse 2; wait10; impulse 2; wait20"
run() # run <tag> <script> <quit-on>
{
    bash "$KIT/run.sh" "$AGENT" -Low -ExtraArgs "$FLAT" -Timeout 240 -QuitOn "$3" -Script "$2" \
        -Filter "^flatview:|^flatgate:|VR portal: carried|Assertion|ENGINE CRASH|Host_Error" > "$S/$1.log" 2>&1
}

run fg_seamless "$P; vr_flatplay_debug 2; vr_flatgate_test 1" "flatgate: [0-9]+ passed"
grep -a "^flatgate:" "$S/fg_seamless.log" | cut -c1-150
expect_verdict "$(cat "$S/fg_seamless.log")" "flatgate" "the seamless gates (QC)"
# The steps: the largest pitch change in a frame, and the gun's largest step in the camera's frame, frame by frame.
steps=$(python "$(dirname "$0")/flatgate_steps.py" "$S/fg_seamless.log")
echo "flatgate_sh: steps $steps"
check $(python -c "import sys; v=dict(a.split('=') for a in sys.argv[1:]); print(1 if float(v['pitch'])<3 and float(v['gun'])<15 else 0)" $steps) \
    "no jump at a crossing: in the view's pitch (under 3 degrees a frame, what setpos sets is left out) or the gun in the camera's frame (under 15 units: a fresh gate 0.1, while the walk's sway runs 9.5 for one frame; it was 708, the gun in the room he left)"

run fg_turning "$P; vr_flatplay_debug 2; setpos -1280 704 24 10 90 0; wait20; cl_yawspeed 40; +right; +forward; wait120; -right; -forward; wait5; echo ff_gate_done" "ff_gate_done"
turn=$(python "$(dirname "$0")/flatgate_steps.py" "$S/fg_turning.log" --turn 90)
echo "flatgate_sh: turning $turn"
check $(python -c "import sys; v=dict(a.split('=') for a in sys.argv[1:]); print(1 if float(v['crossings'])>=1 and float(v['gatestep'])<1.5 else 0)" $turn) \
    "turning through a turned gate: the step at the crossing is the gate's turn and a frame's own (off by under 1.5 degrees)"

run fg_plain "$P; vr_portals_walk 0; vr_flatgate_test 2" "flatgate: [0-9]+ passed"
grep -a "^flatgate:" "$S/fg_plain.log" | cut -c1-150
expect_verdict "$(cat "$S/fg_plain.log")" "flatgate" "a plain teleporter stays Quake's"

run fg_level "$P; setpos -256 576 24 25 90 0; wait20; +forward; wait40; -forward; changelevel e1m1; wait60; developer 1; vr_flatgate_test 3" "flatgate: [0-9]+ passed"
grep -a "^flatgate:" "$S/fg_level.log" | cut -c1-150
expect_verdict "$(cat "$S/fg_level.log")" "flatgate" "a level change is the level's start"

verdict; exit $fail
