#!/bin/bash
# lean_turn_test.sh <agent> [modes] [cases] -- the body kept on the real one through stick turns while leaning
# (vr_lean_turn; ROUND21.md, "Leaning through stick turns"). The mock stands still in the room, hands hanging, marks
# where the game has its body (vr_body_error mark), leans (forward 0.25 m, head down and pitched; or right 0.22 m,
# rolled), turns or moves with the stick, straightens up. Each line is the body's drift in the room from the mark
# (pos cm, yaw degrees) and the lean in the body's frame; the truth is 0 drift throughout (the mock's feet never move).
# Expected with vr_lean_turn 2 (and 1): about 1 cm (the lean's first instants walked before it is known: a 3-point
# ambiguity) at every step, 0 after; with 0: 35-49 cm after the turn, 25 cm straightened up, gone after about 2 s (or
# kept for good by a wall: wall180).
#   modes: default "0 2"; cases: default "smooth180 smooth90 snap90 snap180 side180 move turnmove wall180"
A=${1:?agent}
MODES=${2:-0 2}
CASES=${3:-smooth180 smooth90 snap90 snap180 side180 move turnmove wall180}
K=${KIT:-C:/OHWorkspace/qvr-kit}
HANDS="vr_mock_hand off -0.25 0.9 -0.05; vr_mock_hand main 0.25 0.9 -0.05"
LEANF="vr_mock_hand head 0 1.68 -0.05 -4 0 0; wait3; vr_mock_hand head 0 1.66 -0.1 -8 0 0; wait3; vr_mock_hand head 0 1.64 -0.15 -12 0 0; wait3; vr_mock_hand head 0 1.62 -0.2 -16 0 0; wait3; vr_mock_hand head 0 1.6 -0.25 -20 0 0; wait30"
LEANR="vr_mock_hand head 0.05 1.68 0 0 0 -3; wait3; vr_mock_hand head 0.1 1.66 0 0 0 -6; wait3; vr_mock_hand head 0.15 1.64 0 0 0 -9; wait3; vr_mock_hand head 0.2 1.62 0 0 0 -12; wait3; vr_mock_hand head 0.22 1.61 0 0 0 -15; wait30"
BACKF="vr_mock_hand head 0 1.65 -0.15 0 0 0; wait3; vr_mock_hand head 0 1.7 -0.05 0 0 0; wait3; vr_mock_hand head 0 1.7 0 0 0 0"
BACKR="vr_mock_hand head 0.1 1.66 0 0 0 0; wait3; vr_mock_hand head 0 1.7 0 0 0 0"
SMOOTH="vr_snap_turn 0; vr_mock_stick main -1 0"
for c in $CASES; do
    MAP=start; MARK="vr_body_error mark;"
    case $c in
        smooth180) LEAN=$LEANF; ACT="$SMOOTH; wait40; vr_mock_stick main 0 0; wait10"; B=$BACKF;;
        smooth90)  LEAN=$LEANF; ACT="$SMOOTH; wait20; vr_mock_stick main 0 0; wait10"; B=$BACKF;;
        snap90)    LEAN=$LEANF; ACT="vr_snap_turn 90; vr_mock_stick main -1 0; wait5; vr_mock_stick main 0 0; wait10"; B=$BACKF;;
        snap180)   LEAN=$LEANF; ACT="vr_snap_turn 90; vr_mock_stick main -1 0; wait5; vr_mock_stick main 0 0; wait5; vr_mock_stick main -1 0; wait5; vr_mock_stick main 0 0; wait10"; B=$BACKF;;
        side180)   LEAN=$LEANR; ACT="$SMOOTH; wait40; vr_mock_stick main 0 0; wait10"; B=$BACKR;;
        move)      LEAN=$LEANF; ACT="vr_mock_stick off 0 1; wait60; vr_mock_stick off 0 0; wait20"; B=$BACKF;;
        turnmove)  LEAN=$LEANR; ACT="$SMOOTH; vr_mock_stick off 0 1; wait40; vr_mock_stick main 0 0; vr_mock_stick off 0 0; wait20"; B=$BACKR;;
        # e1m1's start, strafed against the wall on the right, leaning to it: the box can't swing (round the body).
        wall180)   MAP=e1m1; MARK="vr_mock_stick off 1 0; wait180; vr_mock_stick off 0 0; wait30; vr_body_error mark;"
                   LEAN=$LEANR; ACT="$SMOOTH; wait40; vr_mock_stick main 0 0; wait10"; B=$BACKR;;
        *) echo "unknown case $c"; continue;;
    esac
    for m in $MODES; do
        S="vr_lean_turn $m; map $MAP; wait60; $HANDS; wait60; $MARK $LEAN; vr_body_error leaned; $ACT; vr_body_error turned; $B; wait5; vr_body_error back; wait30; vr_body_error back+0.5s; wait150; vr_body_error back+2.5s; toggleconsole; quit"
        bash "$K/run.sh" "$A" -Script "$S" -Filter "body error [a-z]|ENGINE|rror" 2>&1 | grep -E "body error [a-z]|ENGINE|rror" |
            grep -v "error mark" | sed -E "s/ \(t [0-9.]+\)//; s/^/[$m $c] /"
    done
done
