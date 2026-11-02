#!/bin/bash
# ghost_climb_test.sh <agent>: no ghost hand while climbing with vr_ghost_hands_ignore_climb 1, and the same hold shows it
# with it 0 (ROUND23.md, "Ghost hands: none while climbing"). A large crate 40 units ahead on vrclimb's open floor, both
# hands on its lip (climb_plays.py's pull up, pc_hang: prop_climb_test.sh's hang), Ghost Hands on with no fade margin; 60
# frames each phase, in the hang. Ends on "vr_ghost_climb_test: N passed, M failed".
KIT=C:/OHWorkspace/qvr-kit
AGENT=${1:-posepromo}
W=C:/OHWorkspace/qvr-agents/$AGENT
D=$W/scratch/climb
mkdir -p "$D"
python $W/Misc/quakevr/climb/ghost_climb_play.py "$D/pc_hang.txt"
S="map vrclimb;wait30;god;notarget;vr_climb 1;setpos -300 -100 24 0 90 0;noclip;wait20;vr_test_spawn_dist 40;vr_test_spawn 120;impulse 241;wait90"
S="$S;vr_ghost_hands 1;vr_ghost_hands_fade_from 0;vr_ghost_hands_fade_range 0.1;vr_ghost_hands_ignore_climb 1;vr_mock_play $D/pc_hang.txt;wait150"
S="$S;vr_ghost_climb_test start;wait60;vr_ghost_climb_test hidden;vr_ghost_hands_ignore_climb 0;wait60;vr_ghost_climb_test end;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "vr_ghost_climb_test" -QuitOn "vr_ghost_climb_test: \d+ passed" -Timeout 120
