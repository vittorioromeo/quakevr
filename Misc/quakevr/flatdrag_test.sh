#!/bin/bash
# flatdrag_test.sh <agent> -- flat-screen hands steady on a crate dragged with H (his flat note 2026-11-27: "Hands/arms
# jitter/shake a lot when dragging a crate with H"). The hold's point was the Box3D body's on the server's tick while the
# crate is drawn as the client has it (lerped between ticks): with the client faster than the server (a 144 Hz monitor) the
# hands stepped every other frame, 2-3.5 cm off a smooth path on average and sliding on the crate by up to 6 cm a frame.
# The hold's point is now put in the crate's frame as drawn (vr_view_hands.cpp limbHold). A crate grabbed with both hands
# at his default reach, then walked back, forward, strafed and turned with, at 144 and 90 frames a second (the same game
# logic: vr_fixed_frames_rate), the hold-arm probe (vr_hold_arm_probe) measuring each frame how far the drawn hand left a
# smooth path (the middle of three frames off the line through its neighbours) and how far it slid on the crate as drawn.
# (Before the fix at 144: off its path at most 4.85 cm, mean 2.12; slid at most 5.77 cm. After: 0.82, 0.06; 1.73.)
# Prints "flatdrag: N passed, M failed".
AGENT=${1:?agent}
QT_NAME=flatdrag
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch/flatdrag_test
mkdir -p "$S"
FILTER='^(flatdrag:|holdarm:|flat grab|propdrag:)'
HD="vr_mock_key h down"; HU="vr_mock_key h up"

run() {
    local tag=$1 rate=$2
    bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Timeout 180 \
        -Script "map vrtesthall; god; notarget; developer 1; vr_fixed_frames_rate $rate; wait15; vr_flat_grab_hold 0; \
            setpos 0 -200 25 35 90 0; noclip 0; wait5; vr_physics_spawn vr_crate 14; wait25; $HD; wait3; $HU; wait30; \
            setpos 0 -200 25 0 90 0; noclip 0; wait3; vr_hold_arm_probe 1; \
            +back; wait60; -back; wait20; +forward; wait40; -forward; wait20; +moveleft; wait40; -moveleft; wait20; \
            +left; wait30; -left; wait20; vr_hold_arm_probe report; vr_hold_arm_probe 0; echo flatdrag: $tag done; toggleconsole; quit" \
        -QuitOn "^flatdrag: $tag done" -Filter "$FILTER" > "$S/$tag.log" 2>&1
}

# The frames are counted in game time (vr_fixed_frames_rate): the waits are the same logic at each rate, more frames at 144.
for rate in 144 90; do
    run r$rate $rate
    L=$S/r$rate.log
    check $(grep -aq "flatdrag: r$rate done" "$L" && echo 1 || echo 0) "$rate fps: run completed"
    check $(grep -aq 'flat grab: drags vr_crate (40 kg) by its surface with both hands' "$L" && echo 1 || echo 0) "$rate fps: H drags the crate with both hands"
    check $(grep -aq 'propdrag: prop [0-9]*: hand [01] lets go' "$L" && echo 0 || echo 1) "$rate fps: no hand lets go while walking with it"
    set -- $(sed -n 's/^holdarm: jitter \([0-9]*\) frames, off its path at most \([0-9.]*\) cm (mean \([0-9.]*\)), slid at most \([0-9.]*\) cm (\([0-9]*\) frames).*/\1 \2 \3 \4 \5/p' "$L" | head -1)
    check $(awk -v n="$1" 'BEGIN{exit !(n != "" && n > 300)}' && echo 1 || echo 0) "$rate fps: the hands measured while dragging (${1:-?} frames)"
    check $(awk -v m="$3" 'BEGIN{exit !(m != "" && m < 0.4)}' && echo 1 || echo 0) "$rate fps: the hands on a smooth path (off it by ${3:-?} cm on average, under 0.4)"
    check $(awk -v h="$2" 'BEGIN{exit !(h != "" && h < 2.0)}' && echo 1 || echo 0) "$rate fps: no shake (off its path at most ${2:-?} cm, under 2)"
    check $(awk -v s="$4" -v f="$5" 'BEGIN{exit !(s != "" && s < 1.5 && f > 50)}' && echo 1 || echo 0) "$rate fps: the hands stay put on the crate where they reach it (slid at most ${4:-?} cm a frame over ${5:-?} frames, under 1.5)"
done

verdict
exit $fail
