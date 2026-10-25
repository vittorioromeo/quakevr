#!/bin/bash
# magchain_test.sh <agent> [label] -- the magnetic holsters' chains' calm (ROUND22.md, "Magnetic holster chains
# fixed"): vrfiringrange, the shotgun bound to the right hip holster (3), style Both; the main hand draws it and moves it
# along a path slowly (1 s glides), then fast (0.12 s), then holsters it and the player walks. vr_magholster_chain_log
# prints each chain every frame; magchain_summary.py sums each phase per chain (L lanyard, S stub, T tether): the
# springs' worst stretch from relaxed (%), the nodes' most movement off the anchor a frame and their jerk (mm), and the
# frames the chain started afresh. Writes scratch/magchain_<label>.txt.
AGENT=$1
LABEL=${2:-now}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
WT=C:/OHWorkspace/qvr-agents/$AGENT
BIND="vr_mock_hand_to off holster 3;wait5;vr_mock_button off stickclick 1;wait3;vr_mock_button off stickclick 0;wait3;vr_mock_hand off -0.3 1.0 -0.1 0 0 0;"
DRAW="vr_mock_hand_to main holster 3;wait5;+grabright;vr_mock_button main grip 1;wait10;"
PATH1="vr_mock_hand_to main by 20 0 12;WAIT;vr_mock_hand_to main by -25 15 -8;WAIT;vr_mock_hand_to main by 10 -25 -10;WAIT;vr_mock_hand_to main by -5 10 6;WAIT;"
SLOW=${PATH1//WAIT/wait100}
FAST=${PATH1//WAIT/wait12}
S="vr_fixed_frames_rate ${RATE:-90};host_maxfps ${RATE:-90};map vrfiringrange;wait60;developer 1;god;notarget;vr_magholster_style 2;vr_magholster_return_time 0;vr_magholster_return_distance 0;${BIND}${DRAW}vr_mock_hand main 0.35 1.2 -0.3 70 0 0;wait60;vr_magholster_chain_log 1;echo magphase slow;vr_mock_hand_glide 1;${SLOW}echo magphase fast;vr_mock_hand_glide 0.12;${FAST}${FAST}echo magphase still;vr_mock_hand_glide 0;wait60;echo magphase holster;vr_mock_hand_to main holster 3;wait5;-grabright;vr_mock_button main grip 0;wait30;vr_mock_hand main 0.3 1.0 -0.2 70 0 0;wait60;echo magphase walk;vr_mock_stick off 0 1;wait120;vr_mock_stick off 0 0;wait60;echo magphase end;vr_magholster_chain_log 0;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "magchain|magphase|exit=|ERROR|CRASH" > $WT/scratch/magchain_raw_$LABEL.txt
python $WT/Misc/quakevr/magchain_summary.py $WT/scratch/magchain_raw_$LABEL.txt | tee $WT/scratch/magchain_$LABEL.txt
