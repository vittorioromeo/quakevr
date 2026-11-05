#!/bin/bash
# magchain_stretch_test.sh <agent> -- a fast swing's magnetic holster chain stays together (ROUND24.md, "Magnetic holster
# chain on fast swings"). vrfiringrange, the shotgun bound to the right hip holster, style Both; the main hand draws it and
# moves it along magchain_test.sh's path: slowly (1 s glides), fast (0.12 s), then whipped (0.05 s). Run twice, the piece
# limit off (the old chain) and on, same moves; magchain_stretch_summary.py prints the verdict line
# ("magchain_stretch: ... PASS"). Frames counted to the moves, fixed clock, ends with the script. RATE=72 for 72 Hz.
AGENT=$1
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
WT=C:/OHWorkspace/qvr-agents/$AGENT
BIND="vr_mock_hand_to off holster 3;wait5;vr_mock_button off stickclick 1;wait3;vr_mock_button off stickclick 0;wait3;vr_mock_hand off -0.3 1.0 -0.1 0 0 0;"
DRAW="vr_mock_hand_to main holster 3;wait5;+grabright;vr_mock_button main grip 1;wait10;"
PATH1="vr_mock_hand_to main by 20 0 12;WAIT;vr_mock_hand_to main by -25 15 -8;WAIT;vr_mock_hand_to main by 10 -25 -10;WAIT;vr_mock_hand_to main by -5 10 6;WAIT;"
SLOW=${PATH1//WAIT/wait100}
FAST=${PATH1//WAIT/wait12}
WHIP=${PATH1//WAIT/wait6}
RUNRATE=${RATE:-90}
PRECMD=${PRE:-}
go() {
    S="vr_fixed_frames_rate $RUNRATE;host_maxfps $RUNRATE;map vrfiringrange;wait60;developer 1;god;notarget;vr_magholster_style 2;vr_magholster_return_time 0;vr_magholster_return_distance 0;vr_magholster_chain_limit $1;$PRECMD;${BIND}${DRAW}vr_mock_hand main 0.35 1.2 -0.3 70 0 0;wait60;vr_magholster_chain_log 1;echo magphase slow;vr_mock_hand_glide 1;${SLOW}echo magphase fast;vr_mock_hand_glide 0.12;${FAST}${FAST}echo magphase fastwhip;vr_mock_hand_glide 0.05;${WHIP}${WHIP}echo magphase still;vr_mock_hand_glide 0;wait60;echo magphase end;vr_magholster_chain_log 0;toggleconsole;quit"
    bash $KIT/run.sh $AGENT -Script "$S" -Filter "magpiece|magsway|magphase|magchain [0-9] jump|exit=|ERROR|CRASH" > $WT/scratch/magstretch_$2.txt
}
go 0 off
go ${LIMIT:-0.1} on
python $WT/Misc/quakevr/magchain_stretch_summary.py $WT/scratch/magstretch_off.txt $WT/scratch/magstretch_on.txt
