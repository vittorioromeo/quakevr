#!/bin/bash
# magchain_sway_test.sh <agent> [label] [sway] -- the simulated tether's calm as you move with the sticks (ROUND22.md,
# "Magnetic holster chain: the stick's moves carry it"): vrfiringrange, the shotgun bound to the right hip holster (3)
# and held out in the main hand; the player stands, walks forward, strafes, turns smoothly, snap-turns, each followed
# by a stop. vr_magholster_chain_log's magsway lines (the tether's points in the play space) summed per phase by
# magchain_sway_summary.py: the points' most movement and jerk a frame (mm, seen from you) and the middle point's most
# horizontal excursion from where it hung standing still. `sway`: vr_magholster_chain_sway (default: the cvar's).
# Writes scratch/magsway_<label>.txt.
AGENT=$1
LABEL=${2:-now}
SWAY=${3:+vr_magholster_chain_sway $3;}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
WT=C:/OHWorkspace/qvr-agents/$AGENT
BIND="vr_mock_hand_to off holster 3;wait5;vr_mock_button off stickclick 1;wait3;vr_mock_button off stickclick 0;wait3;vr_mock_hand off -0.3 1.0 -0.1 0 0 0;"
DRAW="vr_mock_hand_to main holster 3;wait5;+grabright;vr_mock_button main grip 1;wait10;"
SNAP="vr_mock_stick main 1 0;wait3;vr_mock_stick main 0 0;wait30;"
S="vr_fixed_frames_rate 90;host_maxfps 90;map vrfiringrange;wait60;developer 1;god;notarget;${SWAY}vr_snap_turn 0;vr_magholster_style 2;vr_magholster_return_time 0;vr_magholster_return_distance 0;${BIND}${DRAW}vr_mock_hand main 0.35 1.2 -0.3 70 0 0;wait120;vr_magholster_chain_log 1;echo magphase still;wait60;echo magphase walk;vr_mock_stick off 0 1;wait90;echo magphase stopw;vr_mock_stick off 0 0;wait90;echo magphase strafe;vr_mock_stick off 1 0;wait90;echo magphase stops;vr_mock_stick off 0 0;wait90;echo magphase turn;vr_mock_stick main -1 0;wait90;echo magphase stopt;vr_mock_stick main 0 0;wait90;echo magphase snap;vr_snap_turn 45;${SNAP}${SNAP}${SNAP}echo magphase stopn;wait90;echo magphase end;vr_magholster_chain_log 0;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "magsway|magchain [0-9] (now|jump)|magphase|exit=|ERROR|CRASH" > $WT/scratch/magsway_raw_$LABEL.txt
python $WT/Misc/quakevr/magchain_sway_summary.py $WT/scratch/magsway_raw_$LABEL.txt | tee $WT/scratch/magsway_$LABEL.txt
